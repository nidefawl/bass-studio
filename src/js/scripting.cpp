#include "scripting.h"
#include "host/mainctrl.h"
#include "js/interface/duk_daw_interface.h"
extern "C" {
#include "duktape.h"
}
#define DUK_CMDLINE_CONSOLE_SUPPORT
#define DUK_CMDLINE_LOGGING_SUPPORT
#define DUK_CMDLINE_MODULE_SUPPORT
#if defined(DUK_CMDLINE_CONSOLE_SUPPORT)
#include "duk_console.h"
#endif
#if defined(DUK_CMDLINE_LOGGING_SUPPORT)
#include "duk_logging.h"
#endif
#if defined(DUK_CMDLINE_MODULE_SUPPORT)
#include "duk_module_duktape.h"
#endif

duk_int_t safeCall(duk_context* ctx, void* userdata);

static void cmdline_fatal_handler(void *udata, const char *msg) {
	(void) udata;
	fprintf(stderr, "*** FATAL ERROR: %s\n", msg ? msg : "no message");
	fprintf(stderr, "Causing intentional segfault...\n");
	fflush(stderr);
	dbgassert(0);
}
static duk_ret_t native_print(duk_context *ctx) {
	duk_push_string(ctx, " ");
	duk_insert(ctx, 0);
	duk_join(ctx, duk_get_top(ctx) - 1);
	log_printf("%s\n", duk_to_string(ctx, -1));
	return 0;
}
/*
 *  String.fromBufferRaw()
 */

static duk_ret_t string_frombufferraw(duk_context *ctx) {
	duk_buffer_to_string(ctx, 0);
	return 1;
}
class JSContext::Impl {
	duk_context *ctx;
public:
	Impl() {

		/*
		 *  Create heap
		 */
		ctx = duk_create_heap(NULL, NULL, NULL, NULL, cmdline_fatal_handler);
		/* Register String.fromBufferRaw() which does a 1:1 buffer-to-string
		 * coercion needed by testcases.  String.fromBufferRaw() is -not- a
		 * default built-in!  For stripped builds the 'String' built-in
		 * doesn't exist and we create it here; for ROM builds it may be
		 * present but unwritable (which is ignored).
		 */
		duk_eval_string(ctx,
			"(function(v){"
			    "if (typeof String === 'undefined') { String = {}; }"
			    "Object.defineProperty(String, 'fromBufferRaw', {value:v, configurable:true});"
			"})");
		duk_push_c_function(ctx, string_frombufferraw, 1 /*nargs*/);
		(void) duk_pcall(ctx, 1);
		duk_pop(ctx);
	/* Register console object. */
#if defined(DUK_CMDLINE_CONSOLE_SUPPORT)
	duk_console_init(ctx, DUK_CONSOLE_FLUSH /*flags*/);
#endif

	/* Register Duktape.Logger (removed in Duktape 2.x). */
#if defined(DUK_CMDLINE_LOGGING_SUPPORT)
	duk_logging_init(ctx, 0 /*flags*/);
#endif

	/* Register require() (removed in Duktape 2.x). */
#if defined(DUK_CMDLINE_MODULE_SUPPORT)
	duk_module_duktape_init(ctx);
#endif

		/* Stash a formatting function for evaluation results. */
		duk_push_global_stash(ctx);
		duk_eval_string(ctx,
			"(function (E) {"
			    "return function format(v){"
			        "try{"
			            "return E('jx',v);"
			        "}catch(e){"
			            "return ''+v;"
			        "}"
			    "};"
			"})(Duktape.enc)");
		duk_put_prop_string(ctx, -2, "dukFormat");
		duk_pop(ctx);
		duk_push_c_function(ctx, native_print, DUK_VARARGS);
		duk_put_global_string(ctx, "print");
		using NU::SCRIPTING::DawInterface;
		NU::SCRIPTING::registerInterfaceToContext(ctx);
	}
	~Impl() {
		duk_destroy_heap(ctx);
	}

	void init() {
	}
	String eval(const String& srcJS, call_context_t& ctxt) {
		NU::SCRIPTING::setGlobalInstance(ctx, MainCtrl::get());

		duk_push_pointer(ctx, (void *) StringAsCStr(srcJS));
		duk_push_uint(ctx, (duk_uint_t) srcJS.length());
		duk_push_string(ctx, "input");
		String strRet = "";
		duk_int_t rc = 1;
		ctxt.returnVal = 0;
		try {

			rc = safeCall(ctx, &ctxt);
			ctxt.returnVal = 1;
			if (rc != DUK_EXEC_SUCCESS) {
				/* in interactive mode, write to stdout */
				const char* tmpStackTrace = duk_safe_to_stacktrace(ctx, -1);
				if (tmpStackTrace) {
					auto excDesc = StringFormat("%s\r\n", tmpStackTrace);
					strRet = excDesc;
		//			fwrite(excDesc.c_str(), excDesc.length(), 1, stdout);
		//			fflush(stdout);
				}
			} else {
				ctxt.returnVal = 2;
				auto jsStdOut = StringFormat("%s\r\n", ctxt.strOut.c_str());
				strRet = jsStdOut;
			}
			duk_pop(ctx);
		} catch (std::exception& e) {
			ctxt.returnVal = -1;
			auto excDesc = StringFormat("Exception on CLI: %s\r\n", e.what());
			strRet = excDesc;
		} catch (...) {
			ctxt.returnVal = -2;
			auto excDesc = StringFormat("Exception in duk_safe_call\r\n");
			strRet = excDesc;
		}
		return strRet;
	}
};
JSContext::JSContext() : impl(new JSContext::Impl{}) {
	impl->init();
}
JSContext::~JSContext() {
	delete impl;
}

JSContext::Impl* JSContext::getImpl() {
	return impl;
}
String JSContext::eval(const String& js, call_context_t& calltxt) {

	return impl->eval(js, calltxt);
}

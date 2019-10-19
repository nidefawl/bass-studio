#include "scripting.h"
#include "host/mainctrl.h"
#include "js/interface/duk_daw_interface.h"
extern "C" {
#include "duktape.h"
}

duk_int_t safeCall(duk_context* ctx, void* userdata);

static void cmdline_fatal_handler(void *udata, const char *msg) {
	(void) udata;
	fprintf(stderr, "*** FATAL ERROR: %s\n", msg ? msg : "no message");
	fprintf(stderr, "Causing intentional segfault...\n");
	fflush(stderr);
	*((volatile unsigned int *) 0) = (unsigned int) 0xdeadbeefUL;
	abort();
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
	}
	~Impl() {
		duk_destroy_heap(ctx);
	}

	void init() {
		using NU::SCRIPTING::DawInterface;
		NU::SCRIPTING::setGlobalInstance(ctx, MainCtrl::get());
		NU::SCRIPTING::registerInterfaceToContext(ctx);
		fflush(stdin);
	}
	String eval(const String& srcJS, call_context_t& ctxt) {

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

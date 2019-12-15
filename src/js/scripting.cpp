#include "scripting.h"
#include "fileio.h"
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
#include "track.h"
#include "track_impl.h"
#include "host/vst_host.h"


duk_int_t safeCall(duk_context* ctx, void* userdata);
namespace {

duk_ret_t js_testClass_dtor(duk_context *ctx)
{
	uint64_t hInst = 0;

    // The object to delete is passed as first argument
	//retrieve this.hInst numeric value
    duk_get_prop_string(ctx, 0, "nativeHandle");
	hInst = duk_to_number(ctx, -1);
    duk_pop(ctx);

	printf("textstream instance 0x%06X released!\n", hInst);
    return 0;

}

int getTrackInfo(duk_context *ctx) {
	int i;
	const char* arg0 = 0;
	//int n = duk_get_top(ctx);  //arg count

	arg0 = duk_safe_to_string(ctx, 0);

	printf("getAudioGraph(%s) returning a new AudioGraph instance\n", arg0);


	const char* mClass = "function TrackInfo(){\n"
				"   this.nativeHandle = null;\n"
				"	this.tracks = [];\n"
				"}";

	//add this script fragment to the current context (used latter)
	//safe to call eval to avoid fatal panic handler on syntax error
	duk_push_string(ctx, mClass);
	if (duk_peval(ctx) != 0) {
		printf("eval failed: %s\n", duk_safe_to_string(ctx, -1));
	}
	//create a new testClass javascript object instance
	duk_get_global_string(ctx, "TrackInfo");
	duk_new(ctx, 0);

	//set this.nativeHandle = 12345
	duk_push_number(ctx, 12345);
	duk_put_prop_string(ctx, -2, "nativeHandle");
	vsthost* host = daw_tls::getTls().host;
	project_t* project = project_controller_t::get();
	if (host && project) {
//		DAW::track_graph_t graph = host->lastTrackGraph;
//		{
//			duk_idx_t  arr_idx = duk_push_array(ctx);
////			duk_uarridx_t idx = 0;
//			for (auto node : graph.nodes) {
//				audio_stage_t* stage = host->getAudioStage(audio_stage_ref_t{node.stageId});
//				if (stage && stage->type == 0) {
//					track_impl_t* trImpl = dynamic_cast<track_impl_t*>(stage);
//					duk_push_string(ctx, StringAsCStr(trImpl->track->name));
//					duk_put_prop_index(ctx, arr_idx, static_cast<int32_t>(node.stageId));
////					duk_put_prop_index(ctx, arr_idx, idx++);
//				}
//			}
//			duk_put_prop_string(ctx, -2, "tracks");
//		}
	}



	//register a C function to run when js obj released
	duk_push_c_function(ctx, js_testClass_dtor, 1);
	duk_set_finalizer(ctx, -2);

	return 1;
}
int getAudioGraph(duk_context *ctx) {

	try {
		int i;
		const char* arg0 = 0;
		//int n = duk_get_top(ctx);  //arg count

		arg0 = duk_safe_to_string(ctx, 0);

		printf("getAudioGraph(%s) returning a new AudioGraph instance\n", arg0);


		const char* mClass = "function AudioGraph(){\n"
					"   this.nativeHandle = null;\n"
					"   this.maxLatency = 0;\n"
					"	this.roots = [];\n"
					"	this.nodes = [];\n"
					"}";

		//add this script fragment to the current context (used latter)
		//safe to call eval to avoid fatal panic handler on syntax error
		duk_push_string(ctx, mClass);
		if (duk_peval(ctx) != 0) {
			printf("eval failed: %s\n", duk_safe_to_string(ctx, -1));
		}
		//create a new testClass javascript object instance
		duk_get_global_string(ctx, "AudioGraph");
		duk_new(ctx, 0);

		//set this.nativeHandle = 12345
		duk_push_number(ctx, 12345);
		duk_put_prop_string(ctx, -2, "nativeHandle");
		if (daw_tls::getTls().host) {
			auto graph = vsthost::getInstance()->lastTrackGraph;
//			{
//
//				duk_idx_t  arr_idx = duk_push_array(ctx);
//				duk_uarridx_t idx = 0;
//				for (auto n : graph.nodes) {
//					duk_idx_t  obj_idx = duk_push_object(ctx);
//						duk_push_int(ctx, static_cast<int32_t>(n.stageId));
//						duk_put_prop_string(ctx, -2, "stageId");
//						duk_push_int(ctx, n.numDependants);
//						duk_put_prop_string(ctx, -2, "numDependants");
//						duk_push_int(ctx, n.internalLatency);
//						duk_put_prop_string(ctx, -2, "internalLatency");
//						duk_push_int(ctx, n.inputLatency);
//						duk_put_prop_string(ctx, -2, "inputLatency");
//						duk_idx_t  arr_idx2 = duk_push_array(ctx);
//						duk_uarridx_t idx2 = 0;
//						for (auto n2 : n.dependencies) {
//							duk_push_int(ctx, static_cast<int32_t>(n2));
//							duk_put_prop_index(ctx, arr_idx2, idx2++);
//						}
//						duk_put_prop_string(ctx, -2, "dependencies");
//
//	//				duk_put_prop_index(ctx, arr_idx, idx++);
//					duk_put_prop_index(ctx, arr_idx, static_cast<int32_t>(n.stageId));
//				}
//				duk_put_prop_string(ctx, -2, "nodes");
//			}

//			{
//
//				duk_idx_t  arr_idx = duk_push_array(ctx);
//				duk_uarridx_t idx = 0;
//				for (auto n : graph.roots) {
//					duk_push_int(ctx, static_cast<int32_t>(n));
//					duk_put_prop_index(ctx, arr_idx, idx++);
//				}
//				duk_put_prop_string(ctx, -2, "roots");
//			}
		}



		//register a C function to run when js obj released
		duk_push_c_function(ctx, js_testClass_dtor, 1);
		duk_set_finalizer(ctx, -2);
	} catch (...) {
		log_printf("Exception in getAudioGraph()\n", 0);
	}

	return 1;
}
}
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
	bool hasInit = false;
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


		{
//			/**
//			 * track_node_t - represents a node in the audio chain dependency graph
//			 *
//			 */
//			struct track_node_t {
//				audiostageid_i32 stageId;
//				std::vector<audiostageid_i32> dependencies;
//				int32_t numDependants;
//				uint64_t latencyBefore;
//				uint64_t latencyToMaster;
//			};
//			/**
//			 * track_graph_t - represents the audio chain dependency graph build from I/O configuration of all loaded tracks
//			 *
//			 */
//			struct track_graph_t {
//				std::vector<audiostageid_i32> roots; // outbut nodes (Master, )
//				std::vector<track_node_t> nodes;
//				uint64_t maxLatency;
//			};
			//register a new native function for use in JS
			duk_push_global_object(ctx);
			duk_push_c_function(ctx, getAudioGraph, DUK_VARARGS);
			duk_put_prop_string(ctx, -2, "getAudioGraph");
			duk_pop(ctx);
			duk_push_global_object(ctx);
			duk_push_c_function(ctx, getTrackInfo, DUK_VARARGS);
			duk_put_prop_string(ctx, -2, "getTrackInfo");
			duk_pop(ctx);

		}

	}
	~Impl() {
		duk_destroy_heap(ctx);
	}

	void init() {
		if (!MainCtrl::get()) {
			log_printf("WARN: MainCtrl::get() == nullptr\n", 0);
			return;
		}
		if (!hasInit) {
			hasInit = true;
			NU::SCRIPTING::setGlobalInstance(ctx, MainCtrl::get());
			String srcJS;
			String contextInitScript = "daw_context_init.js";
			int64_t ret = ReadFileText(contextInitScript, srcJS);
			if (ret > 0) {
				call_context_t ctxt;
				String response = eval(srcJS, ctxt);
				if (response.length()) {
					fwrite(response.c_str(), response.length(), 1, stdout);
					fflush(stdout);
				}
			} else {
				my_printf("failed loading %s\n", StringAsCStr(contextInitScript));
			}
		}
	}
	String eval(const String& srcJS, call_context_t& ctxt) {
		init();
		if (!hasInit)
			return "";
		dbgassert(hasInit);
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

#include "scripting.h"
#include "host/mainctrl.h"
#include "js/interface/duk_daw_interface.h"
extern "C" {
#include "duktape.h"
}
#ifdef _WIN32
#include "windows.h"


#ifdef __MINGW32__
#include <excpt.h>
#include "platform/mingw/mingw.exc.h"
extern "C" int exchandler(_In_ EXCEPTION_POINTERS *lpEP);
#endif
#endif // _WIN32
/** NOT PRETTY **/

#include <algorithm>
#include "str_util.h"
#include "automation.h"
#include "logging.h"


#ifdef _WIN32
#include "windows.h"


#ifdef __MINGW32__
#include <excpt.h>
#include "platform/mingw/mingw.exc.h"
extern "C" int exchandler(_In_ EXCEPTION_POINTERS *lpEP);
#endif
#endif // _WIN32

#define  MEM_LIMIT_NORMAL   (128*1024*1024)   /* 128 MB */
#define  MEM_LIMIT_HIGH     (2047*1024*1024)  /* ~2 GB */
#define ALLOW_BYTECODE_INPUT true

namespace {

void dealWithJSException() {
	auto jsStdOut = StringFormat("Exception in REP_TCP js execution\r\n");
	fwrite(jsStdOut.c_str(), jsStdOut.length(), 1, stdout);
}


/* Print error to stderr and pop error. */
static void print_pop_error(duk_context *ctx, FILE *f) {
	fprintf(f, "%s\n", duk_safe_to_stacktrace(ctx, -1));
	fflush(f);
	duk_pop(ctx);
}

static duk_ret_t wrapped_compile_execute(duk_context *ctx, void *udata) {
	const char *src_data;
	duk_size_t src_len;
	duk_uint_t comp_flags;

	call_context_t* ctxt = (call_context_t*) udata;

	/* XXX: Here it'd be nice to get some stats for the compilation result
	 * when a suitable command line is given (e.g. code size, constant
	 * count, function count.  These are available internally but not through
	 * the public API.
	 */

	/* Use duk_compile_lstring_filename() variant which avoids interning
	 * the source code.  This only really matters for low memory environments.
	 */

	/* [ ... bytecode_filename src_data src_len filename ] */

	src_data = (const char *) duk_require_pointer(ctx, -3);
	src_len = (duk_size_t) duk_require_uint(ctx, -2);

	if (src_data != NULL && src_len >= 1 && src_data[0] == (char) 0xbf) {
		/* Bytecode. */
		if (ALLOW_BYTECODE_INPUT) {
			void *buf;
			buf = duk_push_fixed_buffer(ctx, src_len);
			memcpy(buf, (const void *) src_data, src_len);
			duk_load_function(ctx);
		} else {
			(void) duk_type_error(ctx, "bytecode input rejected (use -b to allow bytecode inputs)");
		}
	} else {
		/* Source code. */
		comp_flags = DUK_COMPILE_SHEBANG;
		duk_compile_lstring_filename(ctx, comp_flags, src_data, src_len);
	}

	/* [ ... bytecode_filename src_data src_len function ] */

	/* Optional bytecode dump. */
	if (duk_is_string(ctx, -4)) {
		FILE *f;
		void *bc_ptr;
		duk_size_t bc_len;
		size_t wrote;
		char fnbuf[256];
		const char *filename;

		duk_dup_top(ctx);
		duk_dump_function(ctx);
		bc_ptr = duk_require_buffer_data(ctx, -1, &bc_len);
		filename = duk_require_string(ctx, -5);
		snprintf(fnbuf, sizeof(fnbuf), "%s", filename);
		fnbuf[sizeof(fnbuf) - 1] = (char) 0;

		f = fopen(fnbuf, "wb");
		if (!f) {
			(void) duk_generic_error(ctx, "failed to open bytecode output file");
		}
		wrote = fwrite(bc_ptr, 1, (size_t) bc_len, f);  /* XXX: handle partial writes */
		(void) fclose(f);
		if (wrote != bc_len) {
			(void) duk_generic_error(ctx, "failed to write all bytecode");
		}

		return 0;  /* duk_safe_call() cleans up */
	}

#if 0
	/* Manual test for bytecode dump/load cycle: dump and load before
	 * execution.  Enable manually, then run "make ecmatest" for a
	 * reasonably good coverage of different functions and programs.
	 */
	duk_dump_function(ctx);
	duk_load_function(ctx);
#endif

	duk_push_global_object(ctx);  /* 'this' binding */
	duk_call_method(ctx, 0);


	/*
	 *  In interactive mode, write to stdout so output won't
	 *  interleave as easily.
	 *
	 *  NOTE: the ToString() coercion may fail in some cases;
	 *  for instance, if you evaluate:
	 *
	 *    ( {valueOf: function() {return {}},
	 *       toString: function() {return {}}});
	 *
	 *  The error is:
	 *
	 *    TypeError: coercion to primitive failed
	 *            duk_api.c:1420
	 *
	 *  These are handled now by the caller which also has stack
	 *  trace printing support.  User code can print out errors
	 *  safely using duk_safe_to_string().
	 */

	duk_push_global_stash(ctx);
	duk_get_prop_string(ctx, -1, "dukFormat");
	duk_dup(ctx, -3);
	duk_call(ctx, 1);  /* -> [ ... res stash formatted ] */
	const char* szStr = duk_to_string(ctx, -1);
	ctxt->returnVal = 0;
	ctxt->strOut = szStr;


	return 0;  /* duk_safe_call() cleans up */
}

}
duk_int_t safeCall(duk_context* ctx, void* userdata) {
//	#ifdef _WIN32
//	#if defined(_MSC_VER)
//			__try
//	#elif defined(__MINGW32__)
//			__mingw_try("ehjsproc", exchandler)
//	#endif
//	#endif //ifdef _WIN32
//			{

				duk_int_t rc = 1;
				rc = duk_safe_call(ctx, wrapped_compile_execute, userdata /*udata*/, 3 /*nargs*/, 1 /*nret*/);
				return rc;
//			}
//
//	#ifdef _WIN32
//	#if defined(_MSC_VER)
//			__except(isHandledExc(GetExceptionCode()) ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
//			{
//				dealWithPluginException(this);
//			}
//	#elif defined(__MINGW32__)
//			__mingw_except_begin("ehjsproc")
//			{
//				dealWithJSException();
//			}
//			__mingw_except_end("ehjsproc")
//	#endif
//	#endif //ifdef _WIN32
}

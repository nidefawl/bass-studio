/** NOT PRETTY **/

#include <algorithm>
#include "vst_plugin.h"
#include "str_util.h"
#include "logging.h"
#include "vst_plugin_handles.h"


#ifdef _WIN32
#include "windows.h"
#ifdef __MINGW32__
#include <excpt.h>
#include "platform/mingw/mingw.exc.h"
extern "C" int exchandler(_In_ EXCEPTION_POINTERS *lpEP);
int exchandler(_In_ EXCEPTION_POINTERS *lpEP)
{
//	my_printf("Exception code: %u  Flags: %u\n", lpEP->ExceptionRecord->ExceptionCode, lpEP->ExceptionRecord->ExceptionFlags);
	if (lpEP->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
	    return EXCEPTION_EXECUTE_HANDLER;
	}
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif
#endif // _WIN32

void dealWithPluginException(effectbase* eff) {
	if (!eff->isBypass()) {
		eff->setParamValue(PARAM_ENABLE, 0, 0);
		my_printf("segfault/fatal exception on %s\n", StringAsCStr(eff->getName()));
	}
	logEveryMsec(4, 1000, String("EXCEPTION_ACCESS_VIOLATION on " + eff->getName()));
}

long vstplugin::dispatch(
	long opcode,
	long index,
	long value,
	void *ptr,
	float opt) {
	long l = 0;
#ifdef _WIN32
#if defined(_MSC_VER)
		__try
#elif defined(__MINGW32__)
		__mingw_try("ehvstdisp", exchandler)
#endif
#endif //ifdef _WIN32
		{
			l = handle->aeffect->dispatcher(handle->aeffect, opcode, index, value, ptr, opt);
		}
#ifdef _WIN32
#if defined(_MSC_VER)
		__except(GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
		{
			dealWithPluginException(this);
		}
#elif defined(__MINGW32__)
		__mingw_except_begin("ehvstdisp")
		{
			dealWithPluginException(this);
		}
		__mingw_except_end("ehvstdisp")
#endif
#endif //ifdef _WIN32
	return l;
}

float vst_getParameter(vstplugin* plugin, AEffect* aeffect, int32_t idx) {
	float f = 0;
#ifdef _WIN32
#if defined(_MSC_VER)
		__try
#elif defined(__MINGW32__)
		__mingw_try("ehvstgetp", exchandler)
#endif
#endif //ifdef _WIN32
		{
			f = aeffect->getParameter(aeffect, idx);
		}
#ifdef _WIN32
#if defined(_MSC_VER)
		__except(GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
		{
			dealWithPluginException(plugin);
		}
#elif defined(__MINGW32__)
		__mingw_except_begin("ehvstgetp")
		{
			dealWithPluginException(plugin);
		}
		__mingw_except_end("ehvstgetp")
#endif
#endif //ifdef _WIN32
	return f;
}
void vst_setParameter(vstplugin* plugin, AEffect* aeffect, int32_t idx, float value) {
#ifdef _WIN32
#if defined(_MSC_VER)
		__try
#elif defined(__MINGW32__)
		__mingw_try("ehvstsetp", exchandler)
#endif
#endif //ifdef _WIN32
		{
			aeffect->setParameter(aeffect, idx, value);
		}
#ifdef _WIN32
#if defined(_MSC_VER)
		__except(GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
		{
			dealWithPluginException(plugin);
		}
#elif defined(__MINGW32__)
		__mingw_except_begin("ehvstsetp")
		{
			dealWithPluginException(plugin);
		}
		__mingw_except_end("ehvstsetp")
#endif
#endif //ifdef _WIN32
}

void vstplugin::process(AudioBlock* in, AudioBlock* out, int32_t samples) {
	if (handle->aeffect != NULL) {

#ifdef _WIN32
#if defined(_MSC_VER)
		__try
#elif defined(__MINGW32__)
		__mingw_try("ehvstproc", exchandler)
#endif
#endif //ifdef _WIN32
	    {

			if (handle->aeffect->flags & effFlagsCanReplacing) {
				handle->aeffect->processReplacing(handle->aeffect, in->buf, out->buf, samples);
			} else {
				handle->aeffect->process(handle->aeffect, in->buf, out->buf, samples);
			}

		}

#ifdef _WIN32
#if defined(_MSC_VER)
		__except(GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
		{
			dealWithPluginException(this);
		}
#elif defined(__MINGW32__)
		__mingw_except_begin("ehvstproc")
		{
			dealWithPluginException(this);
		}
		__mingw_except_end("ehvstproc")
#endif
#endif //ifdef _WIN32
	}
}

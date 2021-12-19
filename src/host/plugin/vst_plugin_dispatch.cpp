/** NOT PRETTY **/

#include <algorithm>
#include "vst_plugin.h"
#include "str_util.h"
#include "automation.h"
#include "logging.h"
#include "vst_plugin_handles.h"
#include "track_impl.h"


#ifdef _WIN32
#include "windows.h"


bool isHandledExc(int n) {
	return true;
}

#ifdef __MINGW32__
#include <excpt.h>
#include "platform/mingw/mingw.exc.h"
extern "C" int exchandler(_In_ EXCEPTION_POINTERS *lpEP);
int exchandler(_In_ EXCEPTION_POINTERS *lpEP)
{
	if (isHandledExc(lpEP->ExceptionRecord->ExceptionCode)) {
		return EXCEPTION_EXECUTE_HANDLER;
	}
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif
#endif // _WIN32

void dealWithPluginException(effectbase* eff) {
	log_printf("segfault/fatal exception\n", 0);
	if (!eff->isBypass()) {
		eff->setParamValue(PARAM_ENABLE, 0, FLG_PAR_UPDATE_NOSTORE);
		log_printf("segfault/fatal exception on %s\n", StringAsCStr(eff->getName()));
	}
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
		__except(isHandledExc(GetExceptionCode()) ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
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
		__except(isHandledExc(GetExceptionCode()) ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
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
		__except(isHandledExc(GetExceptionCode()) ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
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

void vstplugin::process(AudioBlock* in, AudioBlock* out, double tick, int32_t samplePos, int32_t numSamples, playback_state state) {
	dbgassert(!isInSuspend);
	dbgassert(getTrackLink()->sampleFormat == this->format && in->samples == format.blockSize && out->samples == format.blockSize && format.blockSize > 0 && format.sampleRate > 0);
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
				handle->aeffect->processReplacing(handle->aeffect, in->buf, out->buf, numSamples);
			} else {
				handle->aeffect->process(handle->aeffect, in->buf, out->buf, numSamples);
			}

		}

#ifdef _WIN32
#if defined(_MSC_VER)
		__except(isHandledExc(GetExceptionCode()) ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
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

#ifdef _WIN32
HMODULE safeLoadLib(const char* szLibName) {
	HMODULE hmodule = NULL;
#if defined(_MSC_VER)
		__try
#elif defined(__MINGW32__)
		__mingw_try("ehvstload", exchandler)
#endif
	{
		hmodule = LoadLibrary(szLibName);
	}
#if defined(_MSC_VER)
__except(isHandledExc(GetExceptionCode()) ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
{
	hmodule = NULL;
}
#elif defined(__MINGW32__)
__mingw_except_begin("ehvstload")
{
	hmodule = NULL;
}
__mingw_except_end("ehvstload")
#endif //defined(__MINGW32__)

return hmodule;
}
#endif //_WIN32

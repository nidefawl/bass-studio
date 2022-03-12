#include <vstsdk-host-2.4/aeffect.h>
#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#include "platform/mingw/mingw.exc.h"
#include <Windows.h>
#endif

class vstplugin;


__declspec(noinline) void vst_onException(vstplugin* eff);
#ifdef _WIN32
extern "C" {
static bool isHandledExc(int n) {
    return true;
}

#ifdef __MINGW32__
__attribute__((__used__))
int vstdispatch_exchandler(_In_ EXCEPTION_POINTERS* lpEP) {
    if (isHandledExc(lpEP->ExceptionRecord->ExceptionCode)) {
        return EXCEPTION_EXECUTE_HANDLER;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
#define SEH_EXC_HANDLER vstdispatch_exchandler
#endif // __MINGW32__

}
#endif // _WIN32

__declspec(noinline)
int64_t vst_dispatch(
    vstplugin* plugin,
    AEffect* aeffect,
    int32_t opcode,
    int32_t index,
    int64_t value,
    void* ptr,
    float opt)
{
    volatile int64_t l = 0;
    seh_try("ehvstdisp")
    {
        l = aeffect->dispatcher(aeffect, opcode, index, value, ptr, opt);
    }
    seh_catch("ehvstdisp")
    {
        vst_onException(plugin);
    }
    seh_finally("ehvstdisp")
    return l;
}

__declspec(noinline)
float vst_getParameter(vstplugin* plugin, AEffect* aeffect, int32_t idx) {
    volatile float f = 0;
    seh_try("ehvstgetp")
    {
        f = aeffect->getParameter(aeffect, idx);
    }
    seh_catch("ehvstgetp")
    {
        vst_onException(plugin);
    }
    seh_finally("ehvstgetp")
    return f;
}

__declspec(noinline)
void vst_setParameter(vstplugin* plugin, AEffect* aeffect, int32_t idx, float value) {
    seh_try("ehvstsetp")
    {
        aeffect->setParameter(aeffect, idx, value);
    }
    seh_catch("ehvstsetp")
    {
        vst_onException(plugin);
    }
    seh_finally("ehvstsetp")
}

__declspec(noinline)
void vst_process(vstplugin* plugin, AEffect* aeffect, float** bufIn, float** bufOut, int32_t numSamples) {
    seh_try("ehvstproc")
    {

        if (aeffect->flags & effFlagsCanReplacing) {
            aeffect->processReplacing(aeffect, bufIn, bufOut, numSamples);
        } else {
            aeffect->process(aeffect, bufIn, bufOut, numSamples);
        }

    }
    seh_catch("ehvstproc")
    {
        vst_onException(plugin);
    }
    seh_finally("ehvstproc")
}

#ifdef _WIN32
__declspec(noinline)
HMODULE safeLoadLib(const char* szLibName) {
    volatile HMODULE hmodule = nullptr;
    seh_try("ehsafeLoadLib")
    {
        hmodule = LoadLibrary(szLibName);
    }
    seh_catch("ehsafeLoadLib")
    {
        hmodule = nullptr;
    }
    seh_finally("ehsafeLoadLib")
    return hmodule;
}
#endif//_WIN32

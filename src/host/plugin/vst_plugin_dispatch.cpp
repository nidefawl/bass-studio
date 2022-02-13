/** NOT PRETTY **/

#include <vstsdk-host-2.4/aeffect.h>

#include <cstddef>
#include <cstdint>

using namespace std;

class vstplugin;

extern "C" {
void vst_onException(vstplugin* eff);

#ifdef _WIN32
#include <Windows.h>

static bool isHandledExc(int n) {
    return true;
}

#ifdef __MINGW32__
#include <excpt.h>
#include "platform/mingw/mingw.exc.h"
int exchandler(_In_ EXCEPTION_POINTERS* lpEP) {
    if (isHandledExc(lpEP->ExceptionRecord->ExceptionCode)) {
        return EXCEPTION_EXECUTE_HANDLER;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

#endif // __MINGW32__

#endif // _WIN32

}

int64_t vst_dispatch(
    vstplugin* plugin,
    AEffect* aeffect,
    int32_t opcode,
    int32_t index,
    int64_t value,
    void* ptr,
    float opt)
{
    int64_t l = 0;
#ifdef _WIN32
#if defined(_MSC_VER)
    __try
#elif defined(__MINGW32__)
    __mingw_try("ehvstdisp", exchandler)
#endif
#endif//ifdef _WIN32
    {
        l = aeffect->dispatcher(aeffect, opcode, index, value, ptr, opt);
    }
#ifdef _WIN32
#if defined(_MSC_VER)
    __except (isHandledExc(GetExceptionCode()) ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        vst_onException(plugin);
    }
#elif defined(__MINGW32__)
    __mingw_except_begin("ehvstdisp") {
        vst_onException(plugin);
    }
    __mingw_except_end("ehvstdisp")
#endif
#endif//ifdef _WIN32
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
#endif//ifdef _WIN32
    {
        f = aeffect->getParameter(aeffect, idx);
    }
#ifdef _WIN32
#if defined(_MSC_VER)
    __except (isHandledExc(GetExceptionCode()) ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        vst_onException(plugin);
    }
#elif defined(__MINGW32__)
    __mingw_except_begin("ehvstgetp") {
        vst_onException(plugin);
    }
    __mingw_except_end("ehvstgetp")
#endif
#endif//ifdef _WIN32
    return f;
}
void vst_setParameter(vstplugin* plugin, AEffect* aeffect, int32_t idx, float value) {
#ifdef _WIN32
#if defined(_MSC_VER)
    __try
#elif defined(__MINGW32__)
    __mingw_try("ehvstsetp", exchandler)
#endif
#endif//ifdef _WIN32
    {
        aeffect->setParameter(aeffect, idx, value);
    }
#ifdef _WIN32
#if defined(_MSC_VER)
    __except (isHandledExc(GetExceptionCode()) ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        vst_onException(plugin);
    }
#elif defined(__MINGW32__)
    __mingw_except_begin("ehvstsetp") {
        vst_onException(plugin);
    }
    __mingw_except_end("ehvstsetp")
#endif
#endif//ifdef _WIN32
}

void vst_process(vstplugin* plugin, AEffect* aeffect, float** bufIn, float** bufOut, int32_t numSamples) {

    if (aeffect != nullptr) {

#ifdef _WIN32
#if defined(_MSC_VER)
        __try
#elif defined(__MINGW32__)
        __mingw_try("ehvstproc", exchandler)
#endif
#endif//ifdef _WIN32
        {

            if (aeffect->flags & effFlagsCanReplacing) {
                aeffect->processReplacing(aeffect, bufIn, bufOut, numSamples);
            } else {
                aeffect->process(aeffect, bufIn, bufOut, numSamples);
            }

        }

#ifdef _WIN32
#if defined(_MSC_VER)
        __except (isHandledExc(GetExceptionCode()) ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
            vst_onException(plugin);
        }
#elif defined(__MINGW32__)
        __mingw_except_begin("ehvstproc") {
            vst_onException(plugin);
        }
        __mingw_except_end("ehvstproc")
#endif
#endif//ifdef _WIN32
    }
}

#ifdef _WIN32
HMODULE safeLoadLib(const char* szLibName) {
    HMODULE hmodule = nullptr;
#if defined(_MSC_VER)
    __try
#elif defined(__MINGW32__)
    __mingw_try("ehvstload", exchandler)
#endif
    {
        hmodule = LoadLibrary(szLibName);
    }
#if defined(_MSC_VER)
    __except (isHandledExc(GetExceptionCode()) ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        hmodule = nullptr;
    }
#elif defined(__MINGW32__)
    __mingw_except_begin("ehvstload") {
        hmodule = NULL;
    }
    __mingw_except_end("ehvstload")
#endif//defined(__MINGW32__)

    return hmodule;
}
#endif//_WIN32

#ifdef _WIN32
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <excpt.h>
#include "assert_dbg.h"
#include "msgbox.h"
#include <Windows.h>
#ifdef __MINGW32__
#include "mmsystem.h"
#else
#include <timeapi.h>
#endif
#include <fcntl.h>
#include <io.h>
#include <vector>

#include "math/seq_math.h"
#include "str_util.h"
#include "logging.h"
#include "error.h"
#include "str_win32.h"
#include <shlobj.h>//for knownFolder
#include <sstream>


static HWND mainHWND = nullptr;
extern "C" {
HWND getMainHWND() {
    return mainHWND;
}
void setMainHWND(HWND hwnd) {
    mainHWND = hwnd;
}
}

/* windows typedefs LONGLONG as __int64_t */
struct time_perf_count {
    int64_t count;
    int64_t freq;
};

time_perf_count getPerfCount() {
    static LARGE_INTEGER frequency{};
    static LARGE_INTEGER begin{};
    if (frequency.QuadPart == 0) {
        ::QueryPerformanceFrequency(&frequency);
        ::QueryPerformanceCounter(&begin);
    }
    LARGE_INTEGER now{};
    ::QueryPerformanceCounter(&now);
    return {now.QuadPart - begin.QuadPart, frequency.QuadPart};
}

double getTimeSecondsD() {
    time_perf_count time = getPerfCount();
    return static_cast<double>(time.count) / static_cast<double>(time.freq);
}

int64_t getTimeMillis() {
    time_perf_count time = getPerfCount();
    auto freqOver1K = time.freq / 1'000LL;
    return time.count / freqOver1K;
}

double getTimeMillisD() {
    time_perf_count time = getPerfCount();
    auto freqOver1K = time.freq / 1'000LL;
    return static_cast<double>(time.count) / static_cast<double>(freqOver1K);
}

float getTimeMillisF() {
    return static_cast<float>(getTimeMillisD());
}

int64_t getTimeMicros() {
    time_perf_count time = getPerfCount();
    auto freqOver1M = time.freq / 1'000'000LL;
    return time.count / freqOver1M;
}

String getModuleName(HMODULE module) {
    std::vector<TCHAR> pathBuf;
    DWORD copied;
    do {
        pathBuf.resize(pathBuf.size() + MAX_PATH);
        copied = GetModuleFileName(module, &pathBuf.at(0), pathBuf.size());
    } while (copied >= pathBuf.size());

    pathBuf.resize(copied);

    String path(pathBuf.begin(), pathBuf.end());
    return path;
}
String getCurrentWorkingDirectory() {
    std::vector<TCHAR> pathBuf;
    DWORD copied;
    do {
        pathBuf.resize(pathBuf.size() + MAX_PATH);
        copied = GetCurrentDirectory(pathBuf.size(), &pathBuf.at(0));
    } while (copied >= pathBuf.size());
    pathBuf.resize(copied);
    String path(pathBuf.begin(), pathBuf.end());
    return path;
}
void setMinimumResolutionTimer() {
#define TARGET_RESOLUTION 1u// 1-millisecond target resolution

    TIMECAPS tc;
    UINT wTimerRes;

    if (timeGetDevCaps(&tc, sizeof(TIMECAPS)) == TIMERR_NOERROR) {
        wTimerRes = math::min(math::max((uint32_t) tc.wPeriodMin, (uint32_t) TARGET_RESOLUTION), (uint32_t) tc.wPeriodMax);
        timeBeginPeriod(wTimerRes);
    } else {
        log_printf("timeGetDevCaps failed, cannot call timeBeginPeriod\n", 0);
    }
}


bool determineUserdataPath(String& path) {
    wchar_t *wPath = nullptr;
    if ((S_OK == SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_CREATE, nullptr, &wPath))) {
#ifdef USE_WSTRING
        path = wPath;
#else
        std::vector<char> convertedStr;
        auto errorCode = wcharToSring(CP_UTF8, wPath, ::wcslen(wPath), convertedStr);
        if (!errorCode){
            path.assign(convertedStr.begin(), convertedStr.end());
        }
#endif
        ::CoTaskMemFree(wPath);
        return true;
    }
    return false;
}

void allocConsole() {
#ifndef __MINGW32__
    AllocConsole();
    AttachConsole(GetCurrentProcessId());
    FILE* f;
    freopen_s(&f, "CON", "w", stdout);
#endif
}

#ifdef USE_WIN32_EXC_HOOKS
static const char* exc_as_str(DWORD excCode) {
    switch (excCode) {
        case EXCEPTION_ACCESS_VIOLATION:
            return "EXCEPTION_ACCESS_VIOLATION";
        case EXCEPTION_DATATYPE_MISALIGNMENT:
            return "EXCEPTION_DATATYPE_MISALIGNMENT";
        case EXCEPTION_BREAKPOINT:
            return "EXCEPTION_BREAKPOINT";
        case EXCEPTION_SINGLE_STEP:
            return "EXCEPTION_SINGLE_STEP";
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
            return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
        case EXCEPTION_FLT_DENORMAL_OPERAND:
            return "EXCEPTION_FLT_DENORMAL_OPERAND";
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:
            return "EXCEPTION_FLT_DIVIDE_BY_ZERO";
        case EXCEPTION_FLT_INEXACT_RESULT:
            return "EXCEPTION_FLT_INEXACT_RESULT";
        case EXCEPTION_FLT_INVALID_OPERATION:
            return "EXCEPTION_FLT_INVALID_OPERATION";
        case EXCEPTION_FLT_OVERFLOW:
            return "EXCEPTION_FLT_OVERFLOW";
        case EXCEPTION_FLT_STACK_CHECK:
            return "EXCEPTION_FLT_STACK_CHECK";
        case EXCEPTION_FLT_UNDERFLOW:
            return "EXCEPTION_FLT_UNDERFLOW";
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
            return "EXCEPTION_INT_DIVIDE_BY_ZERO";
        case EXCEPTION_INT_OVERFLOW:
            return "EXCEPTION_INT_OVERFLOW";
        case EXCEPTION_PRIV_INSTRUCTION:
            return "EXCEPTION_PRIV_INSTRUCTION";
        case EXCEPTION_IN_PAGE_ERROR:
            return "EXCEPTION_IN_PAGE_ERROR";
        case EXCEPTION_ILLEGAL_INSTRUCTION:
            return "EXCEPTION_ILLEGAL_INSTRUCTION";
        case EXCEPTION_NONCONTINUABLE_EXCEPTION:
            return "EXCEPTION_NONCONTINUABLE_EXCEPTION";
        case EXCEPTION_STACK_OVERFLOW:
            return "EXCEPTION_STACK_OVERFLOW";
        case EXCEPTION_INVALID_DISPOSITION:
            return "EXCEPTION_INVALID_DISPOSITION";
        case EXCEPTION_GUARD_PAGE:
            return "EXCEPTION_GUARD_PAGE";
        case EXCEPTION_INVALID_HANDLE:
            return "EXCEPTION_INVALID_HANDLE";
        default:
            break;
    }
    return "UKNOWN_EXCEPTION";
}
static int toErrorCode(DWORD excCode) {
    switch (excCode) {
        case EXCEPTION_ACCESS_VIOLATION:
            return ERR_ACCESSVIOLATION;
        default:
            return ERR_UNKNOWN;
    }
}
void logStackTrace();
extern volatile bool fataError;
#define WINAPI __stdcall
static LONG WINAPI TopLevelExceptionHandler(PEXCEPTION_POINTERS pExceptionInfo) {
    DWORD excCode = pExceptionInfo && pExceptionInfo->ExceptionRecord ? pExceptionInfo->ExceptionRecord->ExceptionCode : 0;
    if (handleFatalError(toErrorCode(excCode), static_cast<int32_t>(excCode))) {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    log_out("Fatal exception: %s (0x%08X)\n", exc_as_str(excCode), (int) excCode);
    logStackTrace();
    fataError = true;
    return EXCEPTION_CONTINUE_SEARCH;
}
#if defined(_MSC_VER)
int __cdecl DebugReportHook(int nReportType, char*, int* pnRet) {
    int ret = nReportType == _CRT_ASSERT || nReportType == _CRT_ERROR;
    *pnRet  = ret;
    return ret ? TRUE : FALSE;
}
#endif

void SignalHandler(int signal) {
    if (signal == SIGABRT) {
        if (!fataError) {
            logStackTrace();
        }
        (void) 0;
        abort();
    }
}
void setExceptionHandler() {
#if defined(_MSC_VER) || (defined(__MSVCRT_VERSION__) && __MSVCRT_VERSION__ > 0x800)
    //_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _set_error_mode(_OUT_TO_STDERR);// we have to supress the assert dialog, since it will spawn a message loop inside onRefresh
    signal(SIGABRT, SignalHandler);

    //_set_abort_behavior(0, _WRITE_ABORT_MSG);
#endif
#if defined(_MSC_VER)
    //this is here to trigger a breakpoint when dbgassert(0) is called using the ms c-runtime
    //by default ms crt throws an exception on dbgassert(0) and opens a dialog that interferes with our wndProc
    _CrtSetReportHook2(_CRT_RPTHOOK_INSTALL, DebugReportHook);
#endif
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOALIGNMENTFAULTEXCEPT | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    SetUnhandledExceptionFilter(TopLevelExceptionHandler);
}
#endif

String getKeyName(int scancode) {
    TCHAR strBuf[512];
    GetKeyNameText(scancode << 16, strBuf, 512);
    return strBuf;
}

String FormatErrorMessage(uint32_t error, const String& msg) {
    static const int BUFFERLENGTH = 1024;
    std::vector<char> buf(BUFFERLENGTH);
        FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, error, 0, buf.data(), BUFFERLENGTH - 1, nullptr);
    if (msg.empty())
        return { buf.data() };
    return msg + " (" + StringTrim(String(buf.data())) + ")";
}

#endif

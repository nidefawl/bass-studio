#ifdef _WIN32
#include "config.h"
#include "str_util.h"
#include "math/seq_math.h"
#include "error.h"
#include "str_win32.h"
#include "logging.h"
#include "assert_dbg.h"
#include "msgbox.h"
#include <shlobj.h>//for knownFolder
#include <sstream>
#include "types.h"
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <excpt.h>
#include <windows.h>
#ifdef __MINGW32__
#include "mmsystem.h"
#else
#include <timeapi.h>
#endif
#include <fcntl.h>
#include <io.h>
#include <vector>

#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include "platform.h"

bool saveWindowPos(GLFWwindow* glfw, appwindow_size_t* size) {
    HWND hwnd = glfwGetWin32Window(glfw);
    size->valid = GetWindowPlacement(hwnd, &(size->p)) != 0;
    return true;
}

bool restoreWindowPos(GLFWwindow* glfw, appwindow_size_t* size) {
    if (size->valid) {
        HWND hwnd = glfwGetWin32Window(glfw);
        return SetWindowPlacement(hwnd, &(size->p)) != 0;
    }
    return false;
}


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
        copied = GetModuleFileName(module, &pathBuf.at(0), static_cast<DWORD>(pathBuf.size()));
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
        log_printf("timeGetDevCaps failed, cannot call timeBeginPeriod\n");
    }
}

void allocConsole() {
#ifndef __MINGW32__
    AllocConsole();
    AttachConsole(GetCurrentProcessId());
    FILE* f;
    freopen_s(&f, "CON", "w", stdout);
#endif
}
void enableVirtTermProc() {
    // Set output mode to handle virtual terminal sequences
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE)
    {
        DWORD dwOriginalOutMode = 0;
        if (GetConsoleMode(hOut, &dwOriginalOutMode)) {
            if (!SetConsoleMode(hOut, dwOriginalOutMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN)
                && !SetConsoleMode(hOut, dwOriginalOutMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
            {
                log_lf(Log::L_TRACE, "Failed enabling console ENABLE_VIRTUAL_TERMINAL_PROCESSING: SetConsoleMode failed\n");
            }
        } else {
            log_lf(Log::L_TRACE, "Failed enabling console ENABLE_VIRTUAL_TERMINAL_PROCESSING: Not a console\n");
        }
    }
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

extern volatile bool fatalError;
#define WINAPI __stdcall
static LONG WINAPI TopLevelExceptionHandler(PEXCEPTION_POINTERS pExceptionInfo) {
    DWORD excCode = pExceptionInfo && pExceptionInfo->ExceptionRecord ? pExceptionInfo->ExceptionRecord->ExceptionCode : 0;
    log_out("Fatal exception: %s (0x%08X)\n", exc_as_str(excCode), (int) excCode);
#ifndef PROJECT_UNITTEST
    logStackTrace();
#endif
    fatalError = true;
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
        if (!fatalError) {
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

namespace App::Platform {

    String GetExecutablePath() {
        TCHAR szFileName[MAX_PATH + 1];
        GetModuleFileName(nullptr, szFileName, MAX_PATH + 1);
        String exeName = szFileName;
        return exeName;
    }

    String getCurrentWorkingDirectory() {
        static std::vector<TCHAR> pathBuf;
        if (pathBuf.empty()) {
            DWORD copied = 0;
            do {
                pathBuf.resize(pathBuf.size() + MAX_PATH);
                copied = GetCurrentDirectory(static_cast<DWORD>(pathBuf.size()), &pathBuf.at(0));
            } while (copied >= pathBuf.size());
            pathBuf.resize(copied);
        }
        return {pathBuf.begin(), pathBuf.end()};
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
                convertedStr.pop_back();
                path.assign(convertedStr.begin(), convertedStr.end());
            }
#endif
            ::CoTaskMemFree(wPath);
            return true;
        }
        return false;
    }

    void sanitizePathToDirectory(String& pathString) {
        replaceString(pathString, "/", FILE_PATHSEP_STR);
        if (pathString.length() && !StrEndsWith(pathString, FILE_PATHSEP_STR))
            pathString += FILE_PATHSEP_STR;
    }

    void sanitizePathToFile(String& pathString) {
        replaceString(pathString, "/", FILE_PATHSEP_STR);
    }
    
    void shellExpandPath(String& pathString) {
    }

} // namespace App::Platform

#endif

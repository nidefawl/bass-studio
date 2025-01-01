
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <wincon.h> // Ensure this is included for WriteConsoleA
#endif
#include "str_util.h"
#include "util/testing_environment.h"
#include <ctime>
#include <vector>
#include <mutex>
#include "logging.h"
#include "assert_dbg.h"
#include "fileio.h"
#include "thread.h"
#ifdef HAVE_SLOWSTACKTRACE
#include <slowstacktrace.h>
#endif

#ifdef __GNUC__
#include <cxxabi.h>
#include <ctime>

String demangleName(const char* to_demangle) {
    constexpr size_t SIZE_TEMP_BUF = 128;
    size_t length = SIZE_TEMP_BUF;
    int status = 0;
    char * szDemangled = __cxxabiv1::__cxa_demangle(to_demangle, nullptr, &length, &status);
    String demangled;
    if (szDemangled && length && status == 0) {
        demangled = szDemangled;
    }
    std::free(szDemangled);
    return demangled;
}
#else
String demangleName(const char* to_demangle)
{
    return to_demangle;
}
#endif
static const char* const TERM_COL_YELLOW = "\x1b[93m";
static const char* const TERM_COL_RED = "\x1b[91m";
static const char* const TERM_COL_RESET = "\x1b[0m";
class StdOutLogger final : public Logger {
public:
    StdOutLogger() noexcept = default;
    ~StdOutLogger() override = default;
    void write(const char* data, size_t len) {
#ifdef _WIN32
        HANDLE stdOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (stdOut && stdOut != INVALID_HANDLE_VALUE) {
            DWORD dwOriginalOutMode = 0;
            if (GetConsoleMode(stdOut, &dwOriginalOutMode)) {
                auto codepage = CP_UTF8;
                int lenWide = MultiByteToWideChar(codepage, 0, data, (int)len, nullptr, 0);
                if (lenWide  > 0) {
                    std::basic_string<wchar_t> converted;
                    converted.reserve(lenWide);
                    converted.resize(lenWide);
                    converted.front() = 0;
                    lenWide  = MultiByteToWideChar(codepage, 0, data, (int)len, converted.data(), (int)converted.size());
                    if (lenWide > 0) {
                        DWORD lpNumberOfCharsWritten = 0;
                        WriteConsoleW(stdOut, converted.data(), lenWide, &lpNumberOfCharsWritten, nullptr);
                    }
                }
            } else {
                DWORD lpNumberOfCharsWritten = 0;
                WriteFile(stdOut, data, len, &lpNumberOfCharsWritten, nullptr);
            }
        }
#else
        fwrite(data, len, 1, stdout);
#endif
    }
    void log(Log::Level lvl, const char* data, size_t len) override {
        if (Log::LEVEL_ALL != getLevel() && lvl < getLevel())
            return;
        if (lvl >= Log::L_ERROR)
            write(TERM_COL_RED, 5);
        else if (lvl >= Log::L_WARN)
            write(TERM_COL_YELLOW, 5);
        write(data, len);
        if (lvl >= Log::L_WARN)
            write(TERM_COL_RESET, 4);
        fflush(stdout);
    }
    void logStr(Log::Level lvl, String s) override {
        if (Log::LEVEL_ALL != getLevel() && lvl < getLevel())
            return;
        if (lvl >= Log::L_ERROR)
            write(TERM_COL_RED, 5);
        else if (lvl >= Log::L_WARN)
            write(TERM_COL_YELLOW, 5);
        if (s.length() && s.back() != '\n')
            s+='\n';
        const char* str = StringAsCStr(s);
        write(str, s.length());
        if (lvl >= Log::L_WARN)
            write(TERM_COL_RESET, 4);
        fflush(stdout);
    }
};
#ifndef PROJECT_UNITTEST
class ThreadSafeFileLogger final : public Logger {
    std::recursive_mutex mutex;
    IOFile* handle = nullptr;
public:
    ThreadSafeFileLogger() /*noexcept*/ = default; // Clang 9 doesn't agree on noexcept
    ~ThreadSafeFileLogger() override {
        delete handle;
    }
    //Not threadsafe
    void openFile(const String& filename) {
        closeLog();
        handle = IOFile::openFile(filename, OpenFileMode::READWRITE);
        if (handle && !handle->isValid()) {
            closeLog();
        }
    }
    //Not threadsafe
    void closeLog() {
        if (handle) {
            delete handle;
            handle = nullptr;
        }
    }

public:
    //threadsafe
    void log(Log::Level lvl, const char* data, size_t len) override {
        if (Log::LEVEL_ALL != getLevel() && lvl < getLevel())
            return;
        if (handle) {
            std::lock_guard<std::recursive_mutex> lockguard(mutex);
            std::time_t t = std::time(nullptr);
            struct tm tm{};
            localtime_s(&tm, &t);
            char mbstr[100];
            size_t posDateTime = std::strftime(
                    mbstr,
                    sizeof(mbstr),
                    "%Y-%m-%dT%H:%M:%S ",
                    &tm);
            if (posDateTime > 0) {
                handle->write(mbstr, posDateTime);
            }
            handle->write(data, len);
            handle->flush();
            if (!handle->isValid()) {
                closeLog();
            }
        }
    }
    void logStr(Log::Level lvl, String s) override {
        if (s.back() != '\n')
            s += '\n';
        const char* str = StringAsCStr(s);
        log(lvl, str, s.length());
    }
};
namespace {
static ThreadSafeFileLogger& getFileLogger() noexcept {
    static ThreadSafeFileLogger gGlobalLogger;
    return gGlobalLogger;
}
}
#endif
MultiLogger& getMultiLogger() noexcept {
    static StdOutLogger gMultiLoggerStdOutInstance;
    static MultiLogger gMultiLogger(&gMultiLoggerStdOutInstance);
    return gMultiLogger;
}
namespace {
Logger** getGlobalLoggerRef() noexcept {
    static Logger* globalLogger = &getMultiLogger();
    return &globalLogger;
}
}
void setGlobalLogger(Logger* logger) noexcept {
    *getGlobalLoggerRef() = logger;
}
Logger* getGlobalLogger() noexcept {
    return *getGlobalLoggerRef();
}
void closeGlobalLog() {
#ifndef PROJECT_UNITTEST
    getFileLogger().logStr(Log::L_DEBUG, "End of logfile\n");
    getFileLogger().closeLog();
    getMultiLogger().removeLogger(&getFileLogger());
#endif
}
void openGlobalLog(const String& logFileName) {
#ifndef PROJECT_UNITTEST
    // if logfile exists move it to a backup
    // if a backup with that name exists, delete it
    if (FileExists(logFileName)) {
        String backupName = logFileName + ".bak";
        if (FileExists(backupName)) {
            DeleteAbsoluteFile(backupName);
        }
        MoveAbsoluteFile(logFileName, backupName);
    }
    getFileLogger().openFile(logFileName);
    getFileLogger().logStr(Log::L_DEBUG, "Begin of logfile\n");
    getMultiLogger().addLogger(&getFileLogger());
#else
    (void)logFileName;
#endif
}

namespace Log {

enum : int16_t {
    LOG_BUF_SIZE = 4096,
    MAX_LEN_FILENAME = 512
};

void log_fmt(Logger* logger, Level lvl, const char* file, int line, const char* func, const char* fmt, ...) noexcept {
    dbgassert(logger);
    if (Log::LEVEL_ALL != logger->getLevel() && lvl < logger->getLevel())
        return;
    char szLogStr[LOG_BUF_SIZE]{ 0 };
    char szFileShort[MAX_LEN_FILENAME]{ 0 };
    char szLogBuf[LOG_BUF_SIZE]{ 0 };
    va_list args;
#ifdef _WIN32
    va_start(args, fmt);
    int ret = vsnprintf_s(szLogStr, LOG_BUF_SIZE, _TRUNCATE, fmt, args);
    va_end(args);
    if (ret == -1) {
        ret = LOG_BUF_SIZE - 1;
    }
    if (ret < 0 || ret >= LOG_BUF_SIZE) {
        dbgassert(0);
        return;
    }
#else
    //TODO test truncation on apple
    va_start(args, fmt);
    int ret = vsnprintf(szLogStr, LOG_BUF_SIZE, fmt, args);
    va_end(args);
    if (ret == -1) {
        dbgassert(0);
        return;
    }
    // linux does the right thing: 
    // write up to LOG_BUF_SIZE-2 chars and put \0 at LOG_BUF_SIZE-1
    if (ret >= LOG_BUF_SIZE) {
        ret = LOG_BUF_SIZE;
        dbgassert(szLogStr[ret - 1] == '\0');
    }
#endif
    const char* szLogStatement = nullptr;
    if (file && line && func) {
        replaceBackslashWithForwardslash(relFileName(file), szFileShort, MAX_LEN_FILENAME);
        String threadName        = seqthreads::getCurrentThreadName();
        const char* szThreadName = StringAsCStr(threadName);
#ifndef _WIN32
        ret = snprintf(szLogBuf, LOG_BUF_SIZE - 1, "%s %s:%d %s  %s", szThreadName, szFileShort, line, func, szLogStr);
        if (ret >= LOG_BUF_SIZE) {
            ret = LOG_BUF_SIZE;
            dbgassert(szLogStr[ret - 1] == '\0');
        }
#else
        ret = _snprintf_s(szLogBuf, LOG_BUF_SIZE-1, _TRUNCATE, "%s %s:%d %s  %s", szThreadName, szFileShort, line, func, szLogStr);
        if (ret == -1) {
            ret = LOG_BUF_SIZE - 1;
            szLogBuf[LOG_BUF_SIZE-2] = '\n';
        }
        szLogBuf[LOG_BUF_SIZE-1] = '\0';
#endif
        if (ret >= 0 && ret < LOG_BUF_SIZE) {
            szLogStatement = szLogBuf;
        }
    } else {
        szLogStatement = szLogStr;
    }
    if (szLogStatement) {
        logger->log(lvl, szLogStatement, ret);
    }
}

} // namespace Log 

extern "C" {

#ifndef _WIN32
void __attribute__((constructor(1000))) C_logger_init()
{
	getGlobalLogger();
}
#endif

static bool g_FailedAssert = false;
void C_failedAssert(const char* expr, const char* file, int line) noexcept {
    if (!g_FailedAssert) {
        g_FailedAssert = true;
        ::Log::log_fmt(getGlobalLogger(), ::Log::L_FATAL, file, line, "dbgassert", "Assertion failed: %s\n", expr);
        logStackTrace();
    }
    abort();
}
}

void CPP_failedAssert(const char* expr, const char* file, int line) {
    if (daw_test::testThrowAssertEnabled) {
        throw daw_test::failed_assert_exception(StringAsCStr(StringFormat("Assertion failed: %s", expr)));
    }
    if (!g_FailedAssert) {
        g_FailedAssert = true;
        ::Log::log_fmt(getGlobalLogger(), ::Log::L_FATAL, file, line, "dbgassert", "Assertion failed: %s\n", expr);
        logStackTrace();
    }
    abort();
}




void getStackTrace(std::vector<String>& vec) {
#ifdef HAVE_SLOWSTACKTRACE
    char buf[4096]{};
    get_thread_stacktrace(buf, sizeof(buf), nullptr);
    auto bufPtr = &buf[0];
    while (bufPtr < buf + sizeof(buf)) {
        auto lineEnd = std::strstr(bufPtr, "\n");
        if (!lineEnd)
            return;
        vec.emplace_back(bufPtr, lineEnd);
        bufPtr = lineEnd + 1;
    }
#else
    vec.emplace_back("Stacktrace not available\n");
#endif
}

void logStackTrace() {
#ifdef HAVE_SLOWSTACKTRACE
    char buf[4096]{};
    get_thread_stacktrace(buf, sizeof(buf), nullptr);
    ::getGlobalLogger()->log(Log::L_INFO, buf, strnlen(buf, sizeof(buf)));
#else
    String warn = "Stacktrace not available\n";
    ::getGlobalLogger()->log(Log::L_INFO, StringAsCStr(warn), warn.length());
#endif
    // print_thread_stacktrace();
}
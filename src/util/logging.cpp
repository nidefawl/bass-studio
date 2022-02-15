#include "str_util.h"
#include <string_view>
#include "util/testing_environment.h"

#ifdef __GNUC__

#include <cxxabi.h>


String demangleName(String to_demangle) {
    int status = 0;
    size_t buff_size = 128;
    auto buff = reinterpret_cast<char*>(std::malloc(buff_size));
    char * buff2 = __cxxabiv1::__cxa_demangle(to_demangle.c_str(), buff, &buff_size, &status);
    String demangled;
    if (buff2) {
        buff = buff2;
        demangled = buff2;
    }
    std::free(buff);
    return demangled;
}

#else
String demangleName(String to_demangle)
{
    return to_demangle;
}
#endif
#include <ctime>
#include <unordered_map>
#include <vector>
#include <mutex>
#include "logging.h"
#include "assert_dbg.h"
#include "fileio.h"
#include "thread.h"


class StdOutLogger : public Logger {
public:
    StdOutLogger() noexcept = default;
    ~StdOutLogger() override = default;
    void log(const char* data, size_t len) override {
        fwrite(data, len, 1, stdout);
        fflush(stdout);
    }
    void logStr(String s) override {
        if (s.length() && s.back() != '\n')
            s+='\n';
        fprintf(stdout, "%s", StringAsCStr(s));
        fflush(stdout);
    }
};
class MultiLogger : public Logger {
    std::vector<Logger*> loggers;
public:
    explicit MultiLogger(Logger* handle = nullptr) noexcept {
        if (handle)
            loggers.push_back(handle);
    }
    void addLogger(Logger* _logger) {
        loggers.push_back(_logger);
    }
    void removeLogger(Logger* _logger) {
        loggers.erase(std::remove(loggers.begin(), loggers.end(), _logger), loggers.end());
        loggers.push_back(_logger);
    }
    ~MultiLogger() override = default;
    void log(const char* data, size_t len) override {
        for (auto* logger : loggers) {
            logger->log(data, len);
        }
    }
    void logStr(String s) override {
        for (auto* logger : loggers) {
            logger->logStr(s);
        }
    }
};
class ThreadSafeFileLogger : public Logger {
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
    void log(const char* data, size_t len) override {
        if (handle) {
            std::lock_guard<std::recursive_mutex> lockguard(mutex);
            std::time_t t = std::time(nullptr);
            char mbstr[100];
            size_t posDateTime = std::strftime(
                    mbstr,
                    sizeof(mbstr),
                    "%Y-%m-%dT%H:%M:%S ",
                    std::localtime(&t));
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
    void logStr(String s) override {
        if (s.back() != '\n')
            s += '\n';
        const char* str = StringAsCStr(s);
        log(str, s.length());
    }
};
static ThreadSafeFileLogger& getFileLogger() noexcept {
    static ThreadSafeFileLogger gGlobalLogger;
    return gGlobalLogger;
}
static MultiLogger& getMultiLogger() noexcept {
    static StdOutLogger gMultiLoggerStdOutInstance;
    static MultiLogger gMultiLogger(&gMultiLoggerStdOutInstance);
    return gMultiLogger;
}
static Logger& getExclusiveLoggerInstance() noexcept {
    static StdOutLogger logger;
    return logger;
}
static Logger* globalLogger = &getMultiLogger();
Logger* getExclusiveLogger() {
    return &getExclusiveLoggerInstance();
}
Logger* getGlobalLogger() noexcept {
    return globalLogger;
}
void setGlobalLogger(Logger* logger) noexcept {
    globalLogger = logger;
}
void closeGlobalLog() {
    getFileLogger().logStr("End of logfile\n");
    getFileLogger().closeLog();
    getMultiLogger().removeLogger(&getFileLogger());
}
void openGlobalLog(const String& logFileName) {
    getFileLogger().openFile(logFileName);
    getFileLogger().logStr("Begin of logfile\n");
    getMultiLogger().addLogger(&getFileLogger());
}
#define LOG_BUF_SIZE 4096
#define MAX_LEN_FILENAME 512
namespace Log {

void log_fmt(Logger* logger, Level lvl, const char* file, int line, const char* func, const char* fmt, ...) noexcept {
    char szLogStr[LOG_BUF_SIZE]{ 0 };
    char szFileShort[MAX_LEN_FILENAME]{ 0 };
    char szLogBuf[LOG_BUF_SIZE]{ 0 };
    va_list args;
    va_start(args, fmt);
#ifdef _WIN32
    int ret = vsnprintf_s(szLogStr, LOG_BUF_SIZE, _TRUNCATE, fmt, args);
#else
    //TODO test truncation on linux
    int ret = vsnprintf(szLogStr, LOG_BUF_SIZE, fmt, args);
#endif
    va_end(args);
    if (ret == -1) {
        ret = LOG_BUF_SIZE - 1;
    }
    if (ret < 0 || ret >= LOG_BUF_SIZE) {
        dbgassert(0);
        return;
    }
    const char* szLogStatement = nullptr;
    if (file && line && func) {
        replaceBackslashWithForwardslash(relFileName(file), szFileShort, MAX_LEN_FILENAME);
        String threadName        = seqthreads::getCurrentThreadName();
        const char* szThreadName = StringAsCStr(threadName);
#ifndef _WIN32
        ret = snprintf(szLogBuf, LOG_BUF_SIZE - 1, "%s:%s: %s", szThreadName, id.data(), szLogStr);
#else
        ret = _snprintf_s(szLogBuf, LOG_BUF_SIZE-1, _TRUNCATE, "%s:%s::%s  %s", szThreadName, szFileShort, func, szLogStr);
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
        logger->log(szLogStatement, ret);
    }
}

} // namespace Log 

void log_format_to_logger(Logger* logger, const char* file, int line, const char* func, const char* fmt, ...) noexcept {
    char szLogStr[LOG_BUF_SIZE]{ 0 };
    char szFileShort[MAX_LEN_FILENAME]{ 0 };
    char szLogBuf[LOG_BUF_SIZE]{ 0 };
    va_list args;
    va_start(args, fmt);
#ifdef _WIN32
    int ret = vsnprintf_s(szLogStr, LOG_BUF_SIZE, _TRUNCATE, fmt, args);
#else
    //TODO test truncation on linux
    int ret = vsnprintf(szLogStr, LOG_BUF_SIZE, fmt, args);
#endif
    va_end(args);
    if (ret == -1) {
        ret = LOG_BUF_SIZE - 1;
    }
    if (ret < 0 || ret >= LOG_BUF_SIZE) {
        dbgassert(0);
        return;
    }
    const char* szLogStatement = nullptr;
    if (file && line && func) {
        replaceBackslashWithForwardslash(relFileName(file), szFileShort, MAX_LEN_FILENAME);
        String threadName        = seqthreads::getCurrentThreadName();
        const char* szThreadName = StringAsCStr(threadName);
#ifndef _WIN32
        ret = snprintf(szLogBuf, LOG_BUF_SIZE - 1, "%s:%s:%d %s: %s", szThreadName, szFileShort, line, func, szLogStr);
#else
        ret = _snprintf_s(szLogBuf, LOG_BUF_SIZE-1, _TRUNCATE, "%s:%s:%d %s: %s", szThreadName, szFileShort, line, func, szLogStr);
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
        logger->log(szLogStatement, ret);
    }
}

extern "C" {
static bool failedAssert = false;
void C_failedAssert(const char* expr, const char* file, int line) noexcept {
    if (!failedAssert) {
        failedAssert = true;
        ::Log::log_fmt(getGlobalLogger(), ::Log::L_FATAL, file, line, "dbgassert", "Assertion failed: %s\n", expr);
    }
    auto nop = []() {};
    nop();
    abort();
}
}

void CPP_failedAssert(const char* expr, const char* file, int line) {
    if (daw_test::testThrowAssertEnabled) {
        throw daw_test::failed_assert_exception(StringAsCStr(StringFormat("Assertion failed: %s", expr)));
    }
    if (!failedAssert) {
        failedAssert = true;
        ::Log::log_fmt(getGlobalLogger(), ::Log::L_FATAL, file, line, "dbgassert", "Assertion failed: %s\n", expr);
    }
    auto nop = []() {};
    nop();
    abort();
}

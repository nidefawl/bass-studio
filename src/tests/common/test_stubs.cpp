#include "test_environment.h"
#include "logging.h"
#include "thread.h"
#include "assert_dbg.h"
#include <cstdio>

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

static StdOutLogger gTestStub_globalLoggerInstance;
Logger* getGlobalLogger() noexcept {
    return &gTestStub_globalLoggerInstance;
}
void closeGlobalLog() {
    getGlobalLogger()->logStr("End of logfile\n");
}
void openGlobalLog(const String&) {
    getGlobalLogger()->logStr("Begin of logfile\n");
}
#define LOG_BUF_SIZE 4096
#define MAX_LEN_FILENAME 512

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
        ret = _snprintf_s(szLogBuf, LOG_BUF_SIZE, _TRUNCATE, "%s:%s:%d %s: %s", szThreadName, szFileShort, line, func, szLogStr);
        if (ret == -1) {
            ret               = LOG_BUF_SIZE - 1;
            szLogBuf[ret - 1] = '\n';
        }
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
static bool gTestStub_failedAssert = false;
void C_failedAssert(const char* expr, const char* file, int line) noexcept {
    if (!gTestStub_failedAssert) {
        gTestStub_failedAssert = true;
        log_format_to_logger(getGlobalLogger(), file, line, "dbgassert", "Assertion failed: %s\n", expr);
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
    if (!gTestStub_failedAssert) {
        gTestStub_failedAssert = true;
        log_format_to_logger(getGlobalLogger(), file, line, "dbgassert", "Assertion failed: %s\n", expr);
    }
    auto nop = []() {};
    nop();
    abort();
}

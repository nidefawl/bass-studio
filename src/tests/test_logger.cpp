#include "TestBase.hpp"
#include "logging.h"
#include "str_util.h"
#include <vector>

void setGlobalLogger(Logger* logger);

namespace {
#define LOG_BUFFER_SIZE 4096

    size_t safe_strlen(const char* str, size_t max_len) {
        const char* end = (const char*) memchr(str, '\0', max_len);
        if (end == nullptr)
            return max_len;
        else
            return static_cast<size_t>(end - str);
    }
    class TestLogger : public Logger {
    public:
        char* recvdLog    = nullptr;
        size_t recvdSize  = 0;
        uint32_t numCalls = 0;
        void log(const char* data, size_t len) override {
            TEST_ASSERT_THROW(numCalls == 0);
            TEST_ASSERT_THROW(data != nullptr);
            TEST_ASSERT_THROW(len > 0);
            TEST_ASSERT_THROW(len < LOG_BUFFER_SIZE);
            TEST_ASSERT_THROW(data[len] == 0);
            TEST_ASSERT_THROW(safe_strlen(data, 4096) == len);
            recvdLog  = _strdup(data);
            recvdSize = len;
            numCalls++;
        }
        void logStr(String s) override {
        }
        void reset() {
            if (recvdLog) free(recvdLog);
            recvdLog  = nullptr;
            recvdSize = 0;
            numCalls  = 0;
        }
    };

    void testLogPrintf() {
        TestLogger testLogger;
        setGlobalLogger(&testLogger);
        std::vector<char> buf;
        const size_t testLength[] = { 1, 32, 1000, 10000, LOG_BUFFER_SIZE - 2, LOG_BUFFER_SIZE - 1, LOG_BUFFER_SIZE, LOG_BUFFER_SIZE + 1 };
        for (size_t len : testLength) {
            buf.resize(len);
            memset(buf.data(), 'x', buf.size());
            buf.back() = '\0';
            log_printf("%s\n", buf.data());
            printf("log_printf strlen: %llu, Recv strlen %llu\n", safe_strlen(buf.data(), 1UL << 16U), testLogger.recvdSize);
            if (len < 100) {
                printf("STR '%s'\n", testLogger.recvdLog);
            }
            testLogger.reset();
        }
        for (size_t len : testLength) {
            buf.resize(len);
            memset(buf.data(), 'x', buf.size());
            buf.back() = '\0';
            log_format_to_logger(getGlobalLogger(), buf.data(), 0x7FFFFFFF, buf.data(), buf.data());
            printf("log_format_to_logger strlen: %llu, Recv strlen %llu\n", safe_strlen(buf.data(), 1UL << 16U), testLogger.recvdSize);
            if (len < 100) {
                printf("STR '%s'\n", testLogger.recvdLog);
            }
            testLogger.reset();
        }
#if 0
        // Generate a format string that ends with "xxxxxx%2"
        // This generates an invalid parameter error and calls a handler that terminates the application
        buf.resize(LOG_BUFFER_SIZE);
        for (size_t i = 0; i < 3; i++) {
            memset(buf.data(), 'x', buf.size());
            strcpy(&buf[LOG_BUFFER_SIZE - 4 + i], "%2f");
            buf.back() = '\0';
            log_printf(buf.data(), 1.0f / 3.0f);
            testLogger.reset();
        }
#endif
        const char* strmsg = "123123123123123\n";
        log_out(strmsg, 0);
        TEST_ASSERT_THROW(strcmp(testLogger.recvdLog, strmsg) == 0);
        testLogger.reset();
    }

}// namespace
int main() {
    testLogPrintf();
    return 0;
}

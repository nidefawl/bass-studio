#pragma once
#include "types.h"
#include <stdexcept>

namespace daw_test {
    enum TestCases : uint32_t {
        TEST_NONE = 0,
        TEST_HOST_EXCEPTIONS = 1
    };
    extern uint32_t currentTest;
    extern bool testThrowAssertEnabled;
    inline bool runTest(TestCases testcase) {
 #if defined(PROJECT_BUILD_TESTS) || defined(__CLION_IDE__)
        return currentTest == testcase;
#else
        return false;
#endif
    }

class failed_assert_exception : public std::runtime_error {
public:
    explicit failed_assert_exception(const char* msg) : runtime_error(msg) {}
};
}// namespace daw_test

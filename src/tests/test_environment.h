#pragma once
#include <cstdint>

namespace daw_test {
    enum TestCases : uint32_t {
        TEST_NONE = 0,
        TEST_HOST_EXCEPTIONS = 1
    };
    extern uint32_t currentTest;
    inline bool runTest(TestCases testcase) {
#if defined(BUILD_TESTS) || defined(__CLION_IDE_)
        return currentTest == testcase;
#else
        return false;
#endif
    }
}// namespace daw_test

#pragma once
#include <stdint.h>

namespace daw_test {
#ifdef BUILD_TESTS
constexpr bool ENABLED = true;
#else
constexpr bool ENABLED = false;
#endif
enum TestCases : uint32_t {
	TEST_NONE = 0, TEST_HOST_EXCEPTIONS = 1
};
extern uint32_t currentTest;
inline bool runTest(TestCases testcase) {
	return currentTest == testcase;
}
}

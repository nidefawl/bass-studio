#include "types.h"
#include "util/testing_environment.h"
#include <cstdio>

#undef HAVE_BUILTIN_TRAP
#ifdef __GNUC__
#define GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#if GCC_VERSION > 40203
#define HAVE_BUILTIN_TRAP
#endif
#else
#ifdef __has_builtin
#if __has_builtin(__builtin_trap)
#define HAVE_BUILTIN_TRAP
#endif
#endif
#endif


namespace daw_test {
    uint32_t currentTest = TestCases::TEST_NONE;
    bool testThrowAssertEnabled = false;
    void debugRaiseSegFault() {
#ifdef HAVE_BUILTIN_TRAP
#define debugRaiseSegFault() __builtin_trap()
#else
#define debugRaiseSegFault()        \
    do {                            \
        int* volatile iptr = 0;     \
        int i              = *iptr; \
        std::printf("%d", i);       \
    } while (0)
#endif
  }
}

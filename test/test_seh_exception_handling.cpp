#include "TestBase.hpp"
#include <vector>
#include <cstdio>
#include "str_util.hpp"
#include "seq_util.hpp"
#include "types.hpp"
#include "compiler.hpp"
#include "platform/mingw/mingw.exc.hpp"

#ifdef _WIN32
#include <windows.h>
#endif


using std::int64_t;

struct teststruct {
    int64_t data;
    int64_t data2;
    int64_t *data3;
};

void* gUserPtrExpected = nullptr;
int64_t unsafeFunction(void* userptr, int userdata, void* somePtr, int64_t someVal, void* morePtrs, void* morePtrs2, int someInt) {
    TEST_ASSERT_EQUAL(gUserPtrExpected, userptr);
    printf("gUserPtrExpected %012zx\n", (uint64_t)gUserPtrExpected);
    printf("userptr %012zx\n", (uint64_t)userptr);
    teststruct* tst = (teststruct*) userptr;
    if (userdata == 0) {
        return tst->data + tst->data2;
    }
    return *tst->data3;
}

int64_t handleException(void* userptr) {
    TEST_ASSERT_EQUAL(gUserPtrExpected, userptr);
    printf("gUserPtrExpected %012zx\n", (uint64_t)gUserPtrExpected);
    printf("userptr %012zx\n", (uint64_t)userptr);
    return 0xAA55AA;
}
extern "C" {
    static bool isHandledExc(int n) {
        return true;
    }

#ifdef __MINGW32__
    long CALLBACK ExceptionHandler(EXCEPTION_POINTERS * lpEP) {
        // std::printf("ExceptionHandler\n");
        if (isHandledExc(lpEP->ExceptionRecord->ExceptionCode)) {
            return EXCEPTION_EXECUTE_HANDLER;
        }
        return EXCEPTION_CONTINUE_SEARCH;
    }
#define SEH_EXC_HANDLER ExceptionHandler
#endif
}

FUNC_NOINLINE
int64_t invoke_unsafe_and_catch(void* userptr, int userdata)
{
    volatile int64_t l = 0;
    seh_try("testsehasm")
    {
        l = unsafeFunction(userptr, userdata, gUserPtrExpected, 123123, nullptr, userptr, userdata);
    }
    seh_catch("testsehasm")
    {
        l = handleException(userptr);
    }
    seh_finally("testsehasm")
    return l;
}

void test_seh_try_catch() {
    TEST_BEGIN("test_seh_try_catch");
    teststruct testhandleImpl{ 1, 2, nullptr };
    gUserPtrExpected = &testhandleImpl;
    TEST_ASSERT_EQUAL(invoke_unsafe_and_catch(&testhandleImpl, 0), 3);
    TEST_ASSERT_EQUAL(invoke_unsafe_and_catch(&testhandleImpl, 1), 0xAA55AA);
    TEST_END();
}

int main(int, char*[]) {
    test_seh_try_catch();
    return 0;
}

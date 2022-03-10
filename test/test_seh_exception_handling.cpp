#include "TestBase.hpp"
#include <vector>
#include <cstdint>
#include <cstdio>
#ifdef _WIN32
#include <Windows.h>

#ifdef __MINGW32__
#include <excpt.h>
#include "platform/mingw/mingw.exc.h"
#endif

using std::int64_t;

struct teststruct {
    int64_t data;
    int64_t data2;
    int64_t *data3;
};


extern "C" {
    int64_t unsafeFunction(void* userptr, int userdata) {
        teststruct* tst = (teststruct*) userptr;
        if (userdata == 0) {
            return tst->data + tst->data2;
        }
        return *tst->data3;
    }

    int64_t handleException(void* userptr) {
        return 0xAA55AA;
    }

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
#endif


}
#endif // _WIN32

int64_t invoke_unsafe_and_catch(void* userptr, int userdata)
{
    int64_t l = 0;
#if defined(_MSC_VER)
    __try
#elif defined(__MINGW32__)
    __mingw_try("testsehasm", ExceptionHandler)
#endif
    {
        l = unsafeFunction(userptr, userdata);
    }
#if defined(_MSC_VER)
    __except (isHandledExc(GetExceptionCode()) ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        l = handleException(userptr);
    }
#elif defined(__MINGW32__)
    __mingw_except_begin("testsehasm") {
        l = handleException(userptr);
    }
    __mingw_except_end("testsehasm")
#endif
    return l;
}

void test_seh_try_catch() {
    TEST_BEGIN("test_seh_try_catch");
    teststruct testhandleImpl{ 1, 2, nullptr };
    TEST_ASSERT_EQUAL(invoke_unsafe_and_catch(&testhandleImpl, 0), 3);
    TEST_ASSERT_EQUAL(invoke_unsafe_and_catch(&testhandleImpl, 1), 0xAA55AA);
    TEST_END();
}

int main(int, char*[]) {
    test_seh_try_catch();
    return 0;
}

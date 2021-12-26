#ifdef _WIN32
#include "hires_timer.h"
#include "exceptions.h"
#include <windows.h>
#include <profileapi.h>
#include "assert_dbg.h"

static double QPC_TOSECONDS(LARGE_INTEGER& iStart, LARGE_INTEGER& iStop, LARGE_INTEGER& freq) {
    return ((double) iStop.QuadPart - (double) iStart.QuadPart) / (double) freq.QuadPart;
}
static int64_t QPC_TOMICROSECONDS(LARGE_INTEGER& iStart, LARGE_INTEGER& iStop, LARGE_INTEGER& freq) {
    int64_t div = freq.QuadPart / 1000000;
    dbgassert(div > 0);
    return ((int64_t) iStop.QuadPart - (int64_t) iStart.QuadPart) / div;
}
class hires_timer_t::Impl {
    LARGE_INTEGER freq{};
    LARGE_INTEGER iStart{};
    LARGE_INTEGER iStop{};

public:
    Impl() {
        if (!QueryPerformanceFrequency(&freq)) {
            throw SystemException(GetLastError(), "QueryPerformanceFrequency failed");
        }
        reset();
    }
    void reset() {
        QueryPerformanceCounter(&iStart);
    }
    void queryStop() {
        QueryPerformanceCounter(&iStop);
    }
    int64_t getTime() {
        queryStop();
        return QPC_TOMICROSECONDS(iStart, iStop, freq);
    }
    int64_t getTimeReset() {
        queryStop();
        int64_t i = QPC_TOMICROSECONDS(iStart, iStop, freq);
        iStart    = iStop;
        return i;
    }

    double getTimeDouble() {
        queryStop();
        return QPC_TOSECONDS(iStart, iStop, freq);
    }
    double getTimeDoubleReset() {
        queryStop();
        double d = QPC_TOSECONDS(iStart, iStop, freq);
        iStart   = iStop;
        return d;
    }
};

hires_timer_t::hires_timer_t() : _M_Iimpl{ new Impl{} } {
}
hires_timer_t::~hires_timer_t() {
    delete _M_Iimpl;
}
void hires_timer_t::reset() {
    _M_Iimpl->reset();
}
/* returns time passed in int64_t MICROSECONDS */
int64_t hires_timer_t::getTime() {
    return _M_Iimpl->getTime();
}
/* returns time passed in int64_t MICROSECONDS */
int64_t hires_timer_t::getTimeReset() {
    return _M_Iimpl->getTimeReset();
}
/* returns time passed in double SECONDS */
double hires_timer_t::getTimeDouble() {
    return _M_Iimpl->getTimeDouble();
}
/* returns time passed in double SECONDS */
double hires_timer_t::getTimeDoubleReset() {
    return _M_Iimpl->getTimeDoubleReset();
}

#endif

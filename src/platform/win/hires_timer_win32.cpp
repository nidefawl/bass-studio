#ifdef _WIN32
#include "hires_timer.hpp"
#include "exceptions.hpp"
#include <windows.h>
#include <profileapi.h>
#include "assert_dbg.h"

static_assert(sizeof(LARGE_INTEGER)*3 <= HIRES_TIMER_STACK_SIZE, "struct timespec size does not match HIRES_TIMER_STACK_SIZE");

LARGE_INTEGER& getFreq(unsigned char* buf) {
    return *reinterpret_cast<LARGE_INTEGER*>(buf);
}
LARGE_INTEGER& getStart(unsigned char* buf) {
    return *(&getFreq(buf) + 1);
}
LARGE_INTEGER& getStop(unsigned char* buf) {
    return *(&getStart(buf) + 2);
}

static double QPC_TOSECONDS(LARGE_INTEGER& iStart, LARGE_INTEGER& iStop, LARGE_INTEGER& freq) {
    return ((double) iStop.QuadPart - (double) iStart.QuadPart) / (double) freq.QuadPart;
}
static int64_t QPC_TOMICROSECONDS(LARGE_INTEGER& iStart, LARGE_INTEGER& iStop, LARGE_INTEGER& freq) {
    int64_t div = freq.QuadPart / 1000000;
    dbgassert(div > 0);
    return ((int64_t) iStop.QuadPart - (int64_t) iStart.QuadPart) / div;
}

hires_timer_t::hires_timer_t() {
    if (!QueryPerformanceFrequency(&getFreq(m_state))) {
        throw SystemException(GetLastError(), "QueryPerformanceFrequency failed");
    }
    reset();
}

hires_timer_t::~hires_timer_t() = default;

void hires_timer_t::reset() { QueryPerformanceCounter(&getStart(m_state)); }
void hires_timer_t::queryStop() { QueryPerformanceCounter(&getStop(m_state)); }

/* returns time passed in int64_t MICROSECONDS */
int64_t hires_timer_t::getTime() {
    queryStop();
    return QPC_TOMICROSECONDS(getStart(m_state), getStop(m_state), getFreq(m_state));
}

/* returns time passed in int64_t MICROSECONDS */
int64_t hires_timer_t::getTimeReset() {
    queryStop();
    int64_t i = QPC_TOMICROSECONDS(getStart(m_state), getStop(m_state), getFreq(m_state));
    getStart(m_state) = getStop(m_state);
    return i;
}

/* returns time passed in double SECONDS */
double hires_timer_t::getTimeDouble() {
    queryStop();
    return QPC_TOSECONDS(getStart(m_state), getStop(m_state), getFreq(m_state));
}

/* returns time passed in double SECONDS */
double hires_timer_t::getTimeDoubleReset() {
    queryStop();
    double d = QPC_TOSECONDS(getStart(m_state), getStop(m_state), getFreq(m_state));
    getStart(m_state) = getStop(m_state);
    return d;
}

#endif

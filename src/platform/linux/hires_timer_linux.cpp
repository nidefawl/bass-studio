#if defined(__linux__) || defined(__APPLE__)
#include "hires_timer.h"
#include <ctime>

static_assert(sizeof(struct timespec)*2 <= HIRES_TIMER_STACK_SIZE, "struct timespec size does not match HIRES_TIMER_STACK_SIZE");

void timespec_diff(struct timespec* start, struct timespec* stop, struct timespec* result);
struct timespec* getStart(unsigned char* buf) {
    return reinterpret_cast<struct timespec*>(buf);
}
struct timespec* getStop(unsigned char* buf) {
    return getStart(buf) + 1;
}

hires_timer_t::hires_timer_t() {
    reset();
}

hires_timer_t::~hires_timer_t() = default;

void hires_timer_t::reset() {
    clock_gettime(CLOCK_MONOTONIC, getStart(m_state));
}

void hires_timer_t::queryStop() {
    clock_gettime(CLOCK_MONOTONIC, getStop(m_state));
}

int64_t hires_timer_t::getTime() {
    queryStop();
    struct timespec diff {};
    timespec_diff(getStart(m_state), getStop(m_state), &diff);
    return diff.tv_sec * 1'000'000L + diff.tv_nsec / 1'000L;
}

/* returns time passed in int64_t MICROSECONDS */
int64_t hires_timer_t::getTimeReset() {
    int64_t i = getTime();
    // iStart    = iStop;
    *getStart(m_state) = *getStop(m_state);
    return i;
}

double hires_timer_t::getTimeDouble() {
    queryStop();
    struct timespec diff {};
    // timespec_diff(&iStart, &iStop, &diff);
    timespec_diff(getStart(m_state), getStop(m_state), &diff);
    return diff.tv_sec + diff.tv_nsec / 1.0e9;
}

double hires_timer_t::getTimeDoubleReset() {
    double valD = getTimeDouble();
    // iStart      = iStop;
    *getStart(m_state) = *getStop(m_state);
    return valD;
}

#endif

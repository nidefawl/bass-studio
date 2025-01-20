#pragma once
#include "types.hpp"
#if defined(__linux__) || defined(__APPLE__)
#define HIRES_TIMER_STACK_SIZE 32
#elif defined(_WIN32)
#define HIRES_TIMER_STACK_SIZE 32
#endif

class hires_timer_t {
    alignas(HIRES_TIMER_STACK_SIZE) unsigned char m_state[HIRES_TIMER_STACK_SIZE]{};

    void queryStop();
public:
    hires_timer_t();
    ~hires_timer_t();

    void reset();

    /* returns time passed in int64_t MICROSECONDS */
    int64_t getTime();

    /* returns time passed in int64_t MICROSECONDS */
    int64_t getTimeReset();

    /* returns time passed in double SECONDS */
    double getTimeDouble();

    /* returns time passed in double SECONDS */
    double getTimeDoubleReset();
};

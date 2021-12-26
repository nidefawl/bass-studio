#pragma once
#include <cstdint>

class hires_timer_t {
    class Impl;
    Impl* m_impl;

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

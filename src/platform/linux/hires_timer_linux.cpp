#if defined(__linux__) || defined(__APPLE__)
#include "hires_timer.h"
#include "exceptions.h"
#include <ctime>


void timespec_diff(struct timespec* start, struct timespec* stop, struct timespec* result); 

class hires_timer_t::Impl {
    struct timespec iStart{};
    struct timespec iStop{};

public:
    Impl() {
        reset();
    }

    ~Impl() = default;

    void reset() {
        clock_gettime(CLOCK_MONOTONIC, &iStart);
    }

    void queryStop() {
        clock_gettime(CLOCK_MONOTONIC, &iStop);
    }

    int64_t getTime() {
        queryStop();
        struct timespec diff {};
        timespec_diff(&iStart, &iStop, &diff);
        return diff.tv_sec * 1'000'000L + diff.tv_nsec / 1'000L;
    }

    int64_t getTimeReset() {
        int64_t i = getTime();
        iStart    = iStop;
        return i;
    }

    double getTimeDouble() {
        queryStop();
        struct timespec diff {};
        timespec_diff(&iStart, &iStop, &diff);
        return diff.tv_sec + diff.tv_nsec / 1.0e9;
    }

    double getTimeDoubleReset() {
        double valD = getTimeDouble();
        iStart = iStop;
        return valD;
    }
};

hires_timer_t::hires_timer_t() : m_impl(new Impl{}) {
}

hires_timer_t::~hires_timer_t() {
    delete m_impl;
}

void hires_timer_t::reset() {
    m_impl->reset();
}

int64_t hires_timer_t::getTime() {
    return m_impl->getTime();
}

/* returns time passed in int64_t MICROSECONDS */
int64_t hires_timer_t::getTimeReset() {
    return m_impl->getTimeReset();
}

double hires_timer_t::getTimeDouble() {
    return m_impl->getTimeDouble();
}

double hires_timer_t::getTimeDoubleReset() {
    return m_impl->getTimeDoubleReset();
}

#endif

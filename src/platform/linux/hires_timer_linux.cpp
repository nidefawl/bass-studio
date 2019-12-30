#if defined(__linux__) || defined (__APPLE__)
#include "hires_timer.h"
#include "exceptions.h"
#include <time.h>


void timespec_diff(struct timespec *start, struct timespec *stop,
                   struct timespec *result)
{
    if ((stop->tv_nsec - start->tv_nsec) < 0) {
        result->tv_sec = stop->tv_sec - start->tv_sec - 1;
        result->tv_nsec = stop->tv_nsec - start->tv_nsec + 1000000000;
    } else {
        result->tv_sec = stop->tv_sec - start->tv_sec;
        result->tv_nsec = stop->tv_nsec - start->tv_nsec;
    }

    return;
}
class hires_timer_t::Impl {
    struct timespec iStart;
    struct timespec iStop;
public:
	Impl() {
		reset();
	}
	~Impl() {

	}
	void reset() {
	    clock_gettime(CLOCK_MONOTONIC, &iStart);
	}
	void queryStop() {
	    clock_gettime(CLOCK_MONOTONIC, &iStop);
	}
	int64_t getTime() {
		queryStop();
	    struct timespec diff;
		timespec_diff(&iStart, &iStop, &diff);
		return diff.tv_sec*1000000.0+diff.tv_nsec / 1000.0;
	}

	double getTimeDouble() {
		queryStop();
	    struct timespec diff;
		timespec_diff(&iStart, &iStop, &diff);
		return diff.tv_sec+diff.tv_nsec / 1000000000.0;
	}
	double getTimeDoubleReset() {
		double d = getTimeDouble();
		iStart = iStop;
		return d;
	}

};

hires_timer_t::hires_timer_t() : _M_Iimpl{new Impl { } } {

}
hires_timer_t::~hires_timer_t() {
	delete _M_Iimpl;
}
void hires_timer_t::reset() {
	_M_Iimpl->reset();
}
int64_t hires_timer_t::getTime() {
	return _M_Iimpl->getTime();
}
double hires_timer_t::getTimeDouble() {
	return _M_Iimpl->getTimeDouble();
}
double hires_timer_t::getTimeDoubleReset() {
	return _M_Iimpl->getTimeDoubleReset();
}
#endif

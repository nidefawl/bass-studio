#pragma once
#include <stdint.h>

class hires_timer_t {
	class Impl;
	Impl *_M_Iimpl;
public:
	hires_timer_t();
	~hires_timer_t();
	void reset();

	/* returns time passed in int64_t MICROSECONDS */
	int64_t getTime();

	/* returns time passed in double SECONDS */
	double getTimeDouble();

	/* returns time passed in double SECONDS */
	double getTimeDoubleReset();
};

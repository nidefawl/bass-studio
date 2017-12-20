#pragma once
#include <stdint.h>

class hires_timer_t {
	class Impl;
	Impl *_M_Iimpl;
public:
	hires_timer_t();
	~hires_timer_t();
	void reset();
	int64_t getTime();
	double getTimeDouble();
	double getTimeDoubleReset();
};

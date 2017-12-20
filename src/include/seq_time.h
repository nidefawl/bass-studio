#pragma once
#include <stdint.h>
#include <cmath>
#include <limits>
#include "samplerate.h"
using tick_t = int32_t;
//AKA PPQ
#define TICK_BITS 12
#define TICK_BITS_BAR (TICK_BITS+2)
#define TICKS_QUARTER (1<<TICK_BITS)
#define TICKS_16TH (TICKS_QUARTER>>2)
#define TICKS_BAR (TICKS_QUARTER<<2)
#define TICK_MASK_16TH (TICKS_16TH-1)
#define TICK_MASK_SUB_16TH ((TICKS_16TH>>1)-1)
struct beatbar16th_t {
	uint32_t bar;
	uint32_t beat;
	uint32_t th;
	int32_t operator[](const int nIndex) {
		if (nIndex == 2)
			return th;
		if (nIndex == 1)
			return beat;
		return bar;
	}
};
inline tick_t sgn(tick_t val) {
    return (0 < val) - (val < 0);
}

template<typename T>
inline bool almost_equal(T x, T y, int ulp)
{
    // the machine epsilon has to be scaled to the magnitude of the values used
    // and multiplied by the desired precision in ULPs (units in the last place)
    return std::abs(x-y) <= std::numeric_limits<T>::epsilon() * std::abs(x+y) * ulp
    // unless the result is subnormal
           || std::abs(x-y) < std::numeric_limits<T>::min();
}
inline double toMilliSeconds(tick_t tick, int32_t bpm100) {
	return ((tick)/(double)(bpm100*TICKS_QUARTER)) * 100.0 * 60.0 * 1000.0;
}
inline double toSeconds(tick_t tick, int32_t bpm100) {
	return ((tick)/(double)(bpm100*TICKS_QUARTER)) * 100.0 * 60.0;
}
inline double toSecondsPrecise(double tick, int32_t bpm100) {
	return (tick/(double)(bpm100*TICKS_QUARTER)) * 100.0 * 60.0;
}
inline tick_t toTick(double seconds, int32_t bpm100) {
	return std::floor((seconds*bpm100*TICKS_QUARTER) / 6000.0);
}
inline double toTickPrecise(double seconds, int32_t bpm100) {
	return (seconds*bpm100*TICKS_QUARTER) / 6000.0;
}
inline tick_t millisToTick(double ms, int32_t bpm100) {
	return std::floor((ms*bpm100*TICKS_QUARTER) / 6000000.0);
}
inline int32_t tickToSample(tick_t tick, int32_t bpm100, samplerate_t samplerate, int32_t blocksize) {
	double samplePos = toSeconds(tick, bpm100) * samplerate;
	return floor(samplePos);
}
inline int32_t tickToBlock(tick_t tick, int32_t bpm100, samplerate_t samplerate, int32_t blocksize) {
	double samplePos = toSeconds(tick, bpm100) * samplerate;
	double blockPos = samplePos / blocksize;
	return floor(blockPos);
}
inline double tickToBlockPrecise(double tick, int32_t bpm100, samplerate_t samplerate, int32_t blocksize) {
	double bpm = bpm100 / 100.0;
	double minutesOver100 = tick/(double)(bpm*TICKS_QUARTER);
	double seconds = minutesOver100 * 60.0;
	double samplePos = seconds * samplerate;
	double blockPos = samplePos / blocksize;
	return blockPos;
}
inline tick_t blockToTick(int32_t block, int32_t bpm100, samplerate_t samplerate, int32_t blocksize) {
	double seconds = (block * blocksize) / (double)samplerate;
	return toTick(seconds, bpm100);
}
inline double blockToTickPrecise(double block, int32_t bpm100, samplerate_t samplerate, int32_t blocksize) {
	double seconds = (block * blocksize) / (double)samplerate;
	return toTickPrecise(seconds, bpm100);
}
inline tick_t blockToPPQ24TickRounded(int32_t block, int32_t bpm100, samplerate_t samplerate, int32_t blocksize) {
	double seconds = (block * blocksize) / (double)samplerate;
	return std::round((seconds*bpm100*24.0) / 6000.0);
}
inline int32_t PPQ24TickSample(tick_t tick, int32_t bpm100, samplerate_t samplerate, int32_t blocksize) {
	double seconds = ((tick)/(double)(bpm100*24.0)) * 100.0 * 60.0;
	double samplePos = seconds * samplerate;
	return floor(samplePos);
}
enum playback_state {
	status_stop,
	status_play
};

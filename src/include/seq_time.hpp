#pragma once
#include "assert_dbg.h"
#include "types.hpp"
#include <cmath>
#include <limits>
#include "math/seq_math.hpp"
#include "samplerate.hpp"
#include "str_util.hpp"

extern const tick_t INVALID_TICK;
struct tick_minmax_t {
    tick_t min;
    tick_t max;
};
//AKA PPQ
#define TICK_BITS 12
#define TICK_BITS_BAR (TICK_BITS + 2)
#define TICKS_QUARTER (1 << TICK_BITS)
#define TICKS_16TH (TICKS_QUARTER >> 2)
#define TICKS_BAR (TICKS_QUARTER << 2)
#define TICK_MASK_16TH (TICKS_16TH - 1)
#define TICK_MASK_SUB_16TH ((TICKS_16TH >> 1) - 1)

constexpr double TPQ_OVER_MINUTE_100 = TICKS_QUARTER/6000.;
constexpr double MINUTE_100_OVER_TPQ = 6000.0/TICKS_QUARTER;

#define MIN_CLIPSIZE (TICKS_16TH >> 2)
struct beatbar16th_t {
    int32_t bar;
    int32_t beat;
    int32_t th;
    int32_t subticks;
    int32_t operator[](const int nIndex) const {
        if (nIndex == 3)
            return subticks;
        if (nIndex == 2)
            return th;
        if (nIndex == 1)
            return beat;
        if (nIndex == 0)
            return bar;
        dbgassert(0);
        return 0;
    }
    int32_t& operator[](const int nIndex) {
        if (nIndex == 3)
            return subticks;
        if (nIndex == 2)
            return th;
        if (nIndex == 1)
            return beat;
        if (nIndex == 0)
            return bar;
        dbgassert(0);
        return bar;
    }
};

beatbar16th_t tickToBarBeat16th(tick_t tick, uint32_t signatureNum, uint32_t signatureDenomBits, bool isRelative);
tick_t beatBarNthToTick(const beatbar16th_t& beatBarNth, uint32_t signatureNum, uint32_t signatureDenomBits, bool isRelative);
String tickAsBeatString(tick_t tick, bool isRelative);
String beatBarNthToString(const beatbar16th_t& tick, bool isRelative);
beatbar16th_t stringToBeatBarNth(const String& s, bool isRelative, uint32_t signatureNum, uint32_t signatureDenomBits);

inline double toSecondsDD(double tick, double oneOverBPM100) {
    return tick * MINUTE_100_OVER_TPQ * oneOverBPM100;
}

inline double toSeconds(double tick, int32_t bpm100) {
    return toSecondsDD(tick, 1.0 / bpm100);
}

inline double secondsToTicks(double seconds, int32_t bpm100) {
    return (TPQ_OVER_MINUTE_100 * bpm100) * seconds;
}

namespace roundmode {
    struct none {};
    struct round {};
    struct floor {};
    struct ceil {};
    struct floorclamp {};
}// namespace roundmode

inline double tickToSampleDD(double tick, double srOverBpm) {
    return tick * MINUTE_100_OVER_TPQ * srOverBpm;
}

template<typename ReturnType, typename RoundMode>
typename std::enable_if<std::is_same<RoundMode, roundmode::none>::value, ReturnType>::type
secondsToSamplesConvert(double seconds, samplerate_t samplerate) {
    return static_cast<ReturnType>(seconds * samplerate);
}

template<typename ReturnType, typename RoundMode>
typename std::enable_if<std::is_same<RoundMode, roundmode::round>::value, ReturnType>::type
secondsToSamplesConvert(double seconds, samplerate_t samplerate) {
    return static_cast<ReturnType>(std::lround(seconds * samplerate));
}

template<typename ReturnType, typename RoundMode>
typename std::enable_if<std::is_same<RoundMode, roundmode::floor>::value, ReturnType>::type
secondsToSamplesConvert(double seconds, samplerate_t samplerate) {
    return static_cast<ReturnType>(std::floor(seconds * samplerate));
}

template<typename ReturnType, typename RoundMode>
typename std::enable_if<std::is_same<RoundMode, roundmode::floorclamp>::value, ReturnType>::type
secondsToSamplesConvert(double seconds, samplerate_t samplerate) {
    return static_cast<ReturnType>(math::max<double>(0.0, std::floor(seconds * samplerate)));
}

template<typename ReturnType, typename RoundMode>
typename std::enable_if<std::is_same<RoundMode, roundmode::ceil>::value, ReturnType>::type
secondsToSamplesConvert(double seconds, samplerate_t samplerate) {
    return static_cast<ReturnType>(std::ceil(seconds * samplerate));
}

template<typename ReturnType, typename RoundMode, typename TickType>
typename std::enable_if<std::is_same<RoundMode, roundmode::none>::value, ReturnType>::type
tickToSampleConvert(TickType tick, int32_t bpm100, samplerate_t samplerate) {
    return static_cast<ReturnType>(tickToSampleDD(tick, samplerate / (double) bpm100));
}

template<typename ReturnType, typename RoundMode, typename TickType>
typename std::enable_if<std::is_same<RoundMode, roundmode::round>::value, ReturnType>::type
tickToSampleConvert(TickType tick, int32_t bpm100, samplerate_t samplerate) {
    return static_cast<ReturnType>(std::lround(tickToSampleDD(tick, samplerate / (double) bpm100)));
}

template<typename ReturnType, typename RoundMode, typename TickType>
typename std::enable_if<std::is_same<RoundMode, roundmode::floor>::value, ReturnType>::type
tickToSampleConvert(TickType tick, int32_t bpm100, samplerate_t samplerate) {
    return static_cast<ReturnType>(std::floor(tickToSampleDD(tick, samplerate / (double) bpm100)));
}

template<typename ReturnType, typename RoundMode, typename TickType>
typename std::enable_if<std::is_same<RoundMode, roundmode::floorclamp>::value, ReturnType>::type
tickToSampleConvert(TickType tick, int32_t bpm100, samplerate_t samplerate) {
    return static_cast<ReturnType>(math::max<double>(0.0, std::floor(tickToSampleDD(tick, samplerate / (double) bpm100))));
}

template<typename ReturnType, typename RoundMode, typename TickType>
typename std::enable_if<std::is_same<RoundMode, roundmode::ceil>::value, ReturnType>::type
tickToSampleConvert(TickType tick, int32_t bpm100, samplerate_t samplerate) {
    return static_cast<ReturnType>(std::ceil(tickToSampleDD(tick, samplerate / (double) bpm100)));
}

inline double sampleToTickDD(double sample, double bpmOverSR) {
    return sample * TPQ_OVER_MINUTE_100 * bpmOverSR;
}

template<typename ReturnType, typename RoundMode, typename SampleType>
typename std::enable_if<std::is_same<RoundMode, roundmode::none>::value, ReturnType>::type
sampleToTickConvert(SampleType sample, int32_t bpm100, samplerate_t samplerate) {
    return static_cast<ReturnType>(sampleToTickDD(sample, bpm100 / (double) samplerate));
}

template<typename ReturnType, typename RoundMode, typename SampleType>
typename std::enable_if<std::is_same<RoundMode, roundmode::round>::value, ReturnType>::type
sampleToTickConvert(SampleType sample, int32_t bpm100, samplerate_t samplerate) {
    return static_cast<ReturnType>(std::lround(sampleToTickDD( sample, bpm100 / (double) samplerate)));
}

template<typename ReturnType, typename RoundMode, typename SampleType>
typename std::enable_if<std::is_same<RoundMode, roundmode::floor>::value, ReturnType>::type
sampleToTickConvert(SampleType sample, int32_t bpm100, samplerate_t samplerate) {
    return static_cast<ReturnType>(std::floor(sampleToTickDD(sample, bpm100 / (double) samplerate)));
}

template<typename ReturnType, typename RoundMode, typename SampleType>
typename std::enable_if<std::is_same<RoundMode, roundmode::floorclamp>::value, ReturnType>::type
sampleToTickConvert(SampleType sample, int32_t bpm100, samplerate_t samplerate) {
    return static_cast<ReturnType>(math::max<double>(0.0, std::floor(sampleToTickDD(sample, bpm100 / (double) samplerate))));
}

template<typename ReturnType, typename RoundMode, typename SampleType>
typename std::enable_if<std::is_same<RoundMode, roundmode::ceil>::value, ReturnType>::type
sampleToTickConvert(SampleType sample, int32_t bpm100, samplerate_t samplerate) {
    return static_cast<ReturnType>(std::ceil(sampleToTickDD(sample, bpm100 / (double) samplerate)));
}

enum playback_state {
    status_stop,
    status_playback,
    status_render,
    status_no_process,
};

namespace DAW {
    inline bool isPlaybackState(playback_state s) {
        return s == status_playback || s == status_render;
    }
}// namespace DAW

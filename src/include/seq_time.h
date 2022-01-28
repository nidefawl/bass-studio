#pragma once
#include <cstdint>
#include <cmath>
#include <limits>
#include "math/seq_math.h"
#include "samplerate.h"

using tick_t = int32_t;
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
#define MIN_CLIPSIZE (TICKS_16TH >> 2)
struct beatbar16th_t {
    int32_t bar;
    int32_t beat;
    int32_t th;
    int32_t subticks;
    int32_t operator[](const int nIndex) const {
        if (nIndex == 2)
            return th;
        if (nIndex == 1)
            return beat;
        return bar;
    }
};

inline beatbar16th_t tickToBarBeat16th(int32_t tick, int32_t signatureNum = 4, int32_t signatureDenomBits = 2) {
    beatbar16th_t t{};
    int32_t denom     = 4 - math::clamp<int32_t>(signatureDenomBits, 0, 4);
    int32_t num       = signatureNum;
    int32_t barOffset = 0;
    if (tick < 0) {
        barOffset = -((((-tick) + TICKS_BAR - 1) / TICKS_BAR));
        tick      = tick & (TICKS_BAR - 1);
    }
    int32_t sixth    = tick / TICKS_16TH;
    t.subticks       = tick % TICKS_16TH;
    t.th             = sixth & ((1 << denom) - 1);
    int32_t quarters = (sixth >> denom);
    t.beat           = quarters % num;
    t.bar            = quarters / num;
    t.bar += barOffset;
    return t;
}

inline double toMilliSeconds(tick_t tick, int32_t bpm100) {
    return ((tick) / (double) (bpm100 * TICKS_QUARTER)) * 100.0 * 60.0 * 1000.0;
}
inline double toSeconds(tick_t tick, int32_t bpm100) {
    return ((tick) / (double) (bpm100 * TICKS_QUARTER)) * 100.0 * 60.0;
}
inline double toSecondsPrecise(double tick, int32_t bpm100) {
    return (tick / (double) (bpm100 * TICKS_QUARTER)) * 100.0 * 60.0;
}
inline tick_t toTick(double seconds, int32_t bpm100) {
    return (tick_t) std::floor((seconds * bpm100 * TICKS_QUARTER) / 6000.0);
}
inline double toTickPrecise(double seconds, int32_t bpm100) {
    return (seconds * bpm100 * TICKS_QUARTER) / 6000.0;
}
inline tick_t millisToTick(double ms, int32_t bpm100) {
    return (tick_t) std::floor((ms * bpm100 * TICKS_QUARTER) / 6000000.0);
}
inline int32_t tickToSample(tick_t tick, int32_t bpm100, samplerate_t samplerate) {
    double samplePos = toSeconds(tick, bpm100) * samplerate;
    return (int32_t) floor(samplePos);
}
inline double tickToSamplePrecise(double tick, int32_t bpm100, samplerate_t samplerate) {
    double samplePos = toSecondsPrecise(tick, bpm100) * samplerate;
    return samplePos;
}
inline double sampleToTickPrecise(double sample, int32_t bpm100, samplerate_t samplerate) {
    double seconds = sample / (double) samplerate;
    return toTickPrecise(seconds, bpm100);
}
inline int32_t sampleToTick(int32_t sample, int32_t bpm100, samplerate_t samplerate) {
    double seconds = sample / (double) samplerate;
    return (int32_t) toTick(seconds, bpm100);
}
inline int32_t tickToBlock(tick_t tick, int32_t bpm100, samplerate_t samplerate, int32_t blocksize) {
    double samplePos = toSeconds(tick, bpm100) * samplerate;
    double blockPos  = samplePos / blocksize;
    return (int32_t) floor(blockPos);
}
inline double tickToBlockPrecise(double tick, int32_t bpm100, samplerate_t samplerate, int32_t blocksize) {
    double bpm            = bpm100 / 100.0;
    double minutesOver100 = tick / (double) (bpm * TICKS_QUARTER);
    double seconds        = minutesOver100 * 60.0;
    double samplePos      = seconds * samplerate;
    double blockPos       = samplePos / blocksize;
    return blockPos;
}
inline tick_t blockToTick(int32_t block, int32_t bpm100, samplerate_t samplerate, int32_t blocksize) {
    double seconds = (block * blocksize) / (double) samplerate;
    return (tick_t) toTick(seconds, bpm100);
}
inline double blockToTickPrecise(double block, int32_t bpm100, samplerate_t samplerate, int32_t blocksize) {
    double seconds = (block * blocksize) / (double) samplerate;
    return toTickPrecise(seconds, bpm100);
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

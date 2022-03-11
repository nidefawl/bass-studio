#pragma once
#include <cmath>
#include <cstdint>
#include <iterator>
#include "assert_dbg.h"
#include "math/seq_math.h"
#include "audioblock.h"
#include "seq_util.h"
#include "seq_time.h"

namespace DAW {

//TODO: make samplerate dependent
struct meter_lvls {
    float fMax  = 0;
    float fPeak = 0;
    float fLvl  = 0;
};


template<uint32_t N>
struct runningsum {
    alignas(1024) float rsBuffer[N]{};
    double runningSum = 0;
    int rsIdx         = 0;
    float fMax        = 0;
    float fPeak       = 0;
    float fLvl        = 0;
    float fPeakFalloffDelay = 0;
    inline void update(const float* fBuf, const uint16_t samples16, const float fGain) {
        float fMaxBlock = 0.0f;
        auto it = std::begin(rsBuffer) + rsIdx;
        double newSum   = runningSum;
#ifndef _MSC_VER
#pragma unroll
#endif
        for (uint32_t i = 0; i < samples16 * 16U; i++) {
            float f = *fBuf++;
            f = f * f;
            fMaxBlock = math::max(fMaxBlock, f);
            newSum += f - *it;
            *it++ = f;
            if (it == std::end(rsBuffer)) BRANCH_UNLIKELY {
                it = std::begin(rsBuffer);
            }
        }
        rsIdx = (rsIdx + samples16 * 16) % N;
        runningSum = newSum;
        if (fMaxBlock > math::F_MIN) {
            fMax = math::max(sqrtf(fMaxBlock), fMax);
        }
        if (fMax > fPeak) {
            fPeak = fMax;
            fPeakFalloffDelay = 2.0f;
        }
        fLvl = runningSum > math::F_MIN ? (float) sqrt(runningSum / (double) N) : 0.0f;
    }

    inline void update512Fixed(const float* fBuf) {
        float fMaxBlock = 0.0f;
        double newSum = runningSum;
        auto it = std::begin(rsBuffer) + rsIdx;

#ifndef _MSC_VER
#pragma unroll
#endif
        for (uint32_t i = 0; i < 512; i++) {
            float f = *fBuf++;
            f = f * f;
            fMaxBlock = math::max(fMaxBlock, f);
            newSum += f - *it;
            *it++ = f;
            if (it == std::end(rsBuffer)) BRANCH_UNLIKELY {
                it = std::begin(rsBuffer);
            }
        }
        rsIdx = (rsIdx + 512) % N;
        runningSum = newSum;
        if (fMaxBlock > math::F_MIN) {
            fMax = math::max(sqrtf(fMaxBlock), fMax);
        }
        if (fMax > fPeak) {
            fPeak = fMax;
            fPeakFalloffDelay = 2.0f;
        }
        fLvl = runningSum > math::F_MIN ? (float) sqrt(runningSum / (double) N) : 0.0f;
    }
    void onTick(double since) {
        /*
         * TODO: parameter since is constant on calls from audio thread (blocksize/samplerate)
         * make the decay curve a state or parameter
         */
        float decayCurve = math::powf(10.0f, (float) -since);
        if (fMax > math::F_MIN) {
            fMax = math::max(0.0f, fMax * decayCurve);
        }
        if (!(fMax > math::F_MIN)) {
            fMax = 0.0f;
        }
        if (fPeakFalloffDelay > 0) {
            fPeakFalloffDelay -= since;
        } else {
            if (fPeak > math::F_MIN) {
                fPeak = math::max(0.0f, fPeak * decayCurve);
            }
            if (!(fPeak > math::F_MIN)) {
                fPeak = 0.0f;
            }
        }
    }
    meter_lvls getLevels() const {
        return meter_lvls{ fMax, fPeak, fLvl };
    }
};

using meter_runningsum = runningsum<16384>;
// static_assert(sizeof(meter_runningsum)>0, "sizeof(meter_runningsum)");
class rmsmeter {
    bool isDefaultCstr=true;
    int count = 0;
    std::vector<meter_runningsum*> channels;
public:
    rmsmeter(meter_runningsum* _channels, uint8_t _numChannels)
        : channels(_numChannels)
    {
        dbgassert(channels.size());
        isDefaultCstr = false;
        count = _numChannels;
        for (uint8_t idx = 0; idx < _numChannels; ++idx) {
            channels[idx] = _channels++;
        }
    }
    rmsmeter() = default;
    float getRms(int i) const {
        dbgassert(channels.size());
        return channels[i]->fLvl;
    }
    float getMax(int i) const {
        dbgassert(channels.size());
        return channels[i]->fMax;
    }
    float getMaxRMS() const {
        dbgassert(channels.size());
        float f = channels[0]->fLvl;
        for (auto& cn : channels) {
            f = std::max(f, cn->fLvl);
        }
        return f;
    }
    float getMaxPeak() const {
        dbgassert(channels.size());
        float f = channels[0]->fMax;
        for (auto& cn : channels) {
            f = std::max(f, cn->fMax);
        }
        return f;
    }
    float getStandingPeak(int i) const {
        dbgassert(channels.size());
        return channels[i]->fPeak;
    }
    void onTick(double since) {
        for (auto& cn : channels) {
            cn->onTick(since);
        }
    }
    std::vector<meter_lvls> getLevels() const {
        std::vector<meter_lvls> v(channels.size());
        auto it = v.begin();
        for (auto& cn : channels) {
            *it++ = cn->getLevels();
        }
        return v;
    }
    
    void update(const AudioBlock* block, float fTrackGain) {
        if (block->samples == 512 && channels.size() == 2 && fTrackGain == 1.0f) {
            channels[0]->update512Fixed(block->buf[0]);
            channels[1]->update512Fixed(block->buf[1]);
        } else {
            for (size_t i = 0; i < math::min<size_t>(block->channels, channels.size()); i++) {
                channels[i]->update(block->buf[i], block->samples/16, fTrackGain);
            }
        }
    }
    rmsmeter getSubChannelMeter(uint8_t channelOffset, uint8_t channelCount) {
        dbgassert(channels.size());
        dbgassert(channelOffset + channelCount <= channels.size());
        return {channels[channelOffset], channelCount};
    }
    uint8_t getNumChannels() const {
        return channels.size();
    }
};


}
#pragma once
#include "types.hpp"
#include "assert_dbg.h"
#include "math/seq_math.hpp"
#include "host/audiobuffer/audioblock.hpp"
#include "seq_util.hpp"
#include "seq_time.hpp"

namespace DAW::MeterOld {

//TODO: make samplerate dependent
struct meter_lvls {
    float fMax  = 0;
    float fPeak = 0;
    float fLvl  = 0;
};


template<uint32_t N>
class runningsum {
public:
    float rsBuffer[N]       = { 0 };
    double runningSum       = 0;
    int rsIdx               = 0;
    float fMax              = 0;
    float fPeak             = 0;
    float fLvl              = 0;
    float fPeakFalloffDelay = 0;
    void update(const float* fBuf, uint32_t samples, float fGain) {
        //TODO: find out if this could be done more efficiently. Think about SIMD or look at the assembly
        uint32_t i;
        float fMaxBlock = 0.0f;
        for (i = 0; i < samples; i++) {
            float f = *fBuf;
            f *= fGain;
            f = f * f;
            fMaxBlock = math::max(fMaxBlock, f);
            runningSum += f;
            runningSum -= rsBuffer[rsIdx];
            rsBuffer[rsIdx] = f;
            rsIdx++;
            if (rsIdx >= (int32_t) N) {
                rsIdx = 0;
            }
            fBuf++;
        }
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
    
    void update(const AudioBlock* block, float fTrackGain) {
        for (size_t i = 0; i < math::min<size_t>(block->channels, channels.size()); i++) {
            channels[i]->update(block->buf[i], block->samples, fTrackGain);
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
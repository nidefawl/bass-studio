#include "dsp_util.h"
#include "samplerate.h"
#include "config.h"
#include "math/seq_math.h"
#include "host/audiobuffer/audioblock.h"
#include <cstdlib>
#include "types.h"
#include <cmath>
#include <memory.h>
#include <algorithm>
#include <limits>
#include <cstring>
#include "math/vec.h"
#include "types.h"
#include <glm/geometric.hpp>

namespace dsp_util {
    const float GAIN_DB30    = math::powf(10.0f, 30.0f / 20.0f);// 2.0f
    const float GAIN_DB6     = math::powf(10.0f, 6.0f / 20.0f); // 2.0f
    const float GAIN_DBFLOOR = math::powf(10.0f, DBFS_FLOOR / 20.0f);
    const float GAIN_DBINF   = math::powf(10.0f, DBFS_INF_POS / 20.0f);

#define TABLE_SIZE (200)
    struct paTestData {
        float sine[TABLE_SIZE];
        int left_phase;
        int right_phase;
    };

    float Saturate(float input, float fMax) {
        static const float fGrdDiv = 0.5f;

        float x1 = fabsf(input + fMax);
        float x2 = fabsf(input - fMax);
        return fGrdDiv * (x1 - x2);
    }
    void fillSaturate(float** buffer, channelnum_t channels, samplecount_t samples) {
        const float maxGain = 1.0;
        const auto maxChannels = static_cast<channelnum_t>((channels + 1U) / 2U);
        for (channelnum_t ch = 0; ch < maxChannels; ch++) {
            float* output0 = buffer[ch * 2 + 0];
            float* output1 = buffer[ch * 2 + 1];
            for (samplecount_t s = 0; s < samples; s++) {
                *output0 = dsp_util::Saturate(*output0, maxGain);
                *output1 = dsp_util::Saturate(*output1, maxGain);
                output0++;
                output1++;
            }
        }
    }
    void fillAllChannelsStereo(float** buffer, channelnum_t channels, samplecount_t samples, float f = 0.0f) {
        for (channelnum_t ch = 0; ch < channels / 2; ch++) {
            float* input0 = buffer[ch * 2 + 0];
            float* input1 = buffer[ch * 2 + 1];
            for (samplecount_t s = 0; s < samples; s++) {
                *input0 = f; /* left */
                *input1 = f; /* right */
                input0++;
                input1++;
            }
        }
    }
    void fillAllChannels(float** buffer, channelnum_t channels, samplecount_t samples, float f = 0.0f) {
        for (channelnum_t ch = 0; ch < channels; ch++) {
            float* input0 = buffer[ch];
            for (samplecount_t s = 0; s < samples; s++) {
                *input0++ = f;
            }
        }
    }
    void fillChannels(float** buffer, channelnum_t channels, samplecount_t samples, float f = 0.0f) {
        if (channels % 2 == 0) {
            fillAllChannelsStereo(buffer, channels, samples, f);
        } else {
            fillAllChannels(buffer, channels, samples, f);
        }
    }
    void fillBlock(AudioBlock& block, float f) {
        if (block.channels % 2 == 0) {
            fillAllChannelsStereo(block.buf, block.channels, block.samples, f);
        } else {
            fillAllChannels(block.buf, block.channels, block.samples, f);
        }
    }
    float dBFS(float f) {
        return 20.0f * std::log10(f);
    }
    float clampGain(float f) {
        if (f > GAIN_DB6)
            return GAIN_DB6;
        if (f < GAIN_DBFLOOR)
            return GAIN_DBINF;
        return f;
    }
    float clampReadGain(float f) {
        if (f > GAIN_DB30)
            return GAIN_DB30;
        if (f < GAIN_DBFLOOR)
            return 0;
        return f;
    }
    float dBFSClampInf6(float f) {
        if (f <= GAIN_DBFLOOR)
            return -std::numeric_limits<float>::infinity();
        f = 20.0f * std::log10(f);
        return f > 6.0f ? 6.0f : f;
    }
    float fromdBFSClampInf6(float f_dBfs) {
        if (f_dBfs <= DBFS_FLOOR)
            return 0.0f;
        float f_gain = pow(10.0f, f_dBfs / 20.0f);
        if (f_gain > GAIN_DB6) {
            return GAIN_DB6;
        }
        return f_gain;
    }
    float fromdBFS(float f) {
        return pow(10.0f, f / 20.0f);
    }
    float scaledRange(float db, float lvlFloor, float lvlCeil) {
        if (db < dsp_util::DBFS_FLOOR)
            return 1.0f;
        float lvlRange = lvlFloor - lvlCeil;
        return (math::max(lvlFloor, math::min(db, lvlCeil)) - lvlCeil) / lvlRange;
    }
    void fillSine(float** buffer, samplecount_t samples) {
        static paTestData* data = NULL;
        if (data == NULL) {
            data = (paTestData*) malloc(sizeof(paTestData));
            /* initialise sinusoidal wavetable */
            for (uint32_t i = 0; i < TABLE_SIZE; i++) {
                data->sine[i] = (float) sin(((double) i / (double) TABLE_SIZE) * M_PI * 2.);
            }
            data->left_phase = data->right_phase = 0;
        }
        float gain    = 0.1f;
        float* input0 = buffer[0];
        float* input1 = buffer[1];
        for (samplecount_t i = 0; i < samples; i++) {
            *input0++ = data->sine[data->left_phase] * gain;  /* left */
            *input1++ = data->sine[data->right_phase] * gain; /* right */
            data->left_phase += 1;
            if (data->left_phase >= TABLE_SIZE) data->left_phase -= TABLE_SIZE;
            data->right_phase += 3; /* higher pitch so we can distinguish left and right. */
            if (data->right_phase >= TABLE_SIZE) data->right_phase -= TABLE_SIZE;
        }
    }
    void fillNoise(float** buffer, channelnum_t channels, samplecount_t samples) {
        //TODO: make this thread safe
        float gain = 0.1f;


        for (channelnum_t channelIdx = 0; channelIdx < channels; channelIdx++) {
            float* input0 = buffer[channelIdx];
            static float fU32_Max = 4294967296; // off by 1
            float g_fScale  = 2.0f / fU32_Max;
            static uint32_t g_x1 = 0x67452301UL;
            static uint32_t g_x2 = 0xefcdab89UL;
            gain *= g_fScale;
            for (samplecount_t i = 0; i < samples; i++) {
                g_x1 ^= g_x2;
                *input0++ = static_cast<int64_t>(g_x2) * gain;
                g_x2 += g_x1;
            }
        }
    }
    void fillNoiseBlock(AudioBlock& block) {
        fillNoise(block.buf, block.channels, block.samples);
    }
    void fillSqare(samplerate_t samplerate, float freq, float** buffer, samplecount_t samples) {
        float gain    = 0.05f;
        float* input0 = buffer[0];
        float* input1 = buffer[1];
        union sample {
            float f;
            int i;
        };
        sample one;
        one.f                   = 1.0f;
        static uint32_t intOver = 0L;
        uint32_t intIncr        = (uint32_t) ((4294967296.0 / samplerate) * freq);
        //static int lastSign = (intOver & 0x80000000);
        // loop:
        for (samplecount_t i = 0; i < samples; i++) {
            one.i &= 0x7FFFFFFF;// mask out sign bit
            one.i |= (intOver & 0x80000000);
            *input0++ = one.f * gain;
            *input1++ = one.f * gain;
            intOver += intIncr;
        }
    }


    const float GAIN_SCALE_RANGE = DBFS_MUTE_POS - MTR_CEIL;
    const float GAIN_SCALE_EXP   = 2.0f;
    float gainToLinScale(float f) {
        float db = dBFS(f);
        float f2 = ((math::max(DBFS_MUTE_POS, math::min(db, MTR_CEIL)) - MTR_CEIL) / GAIN_SCALE_RANGE);
        return 1.0f - math::powf(f2, 1.0f / GAIN_SCALE_EXP);
    }
    float linScaleToGain(float f) {
        float f1 = (1.0f - f);
        f1       = math::powf(f1, GAIN_SCALE_EXP);
        float f2 = (f1 * GAIN_SCALE_RANGE) + MTR_CEIL;
        return fromdBFS(f2);
    }
    float gainToLinScaleWithRange(float f, float MTR_CEIL, float DBFS_MUTE_POS) {
        const float GAIN_SCALE_RANGE = DBFS_MUTE_POS - MTR_CEIL;
        float db = dBFS(f);
        float f2 = ((math::max(DBFS_MUTE_POS, math::min(db, MTR_CEIL)) - MTR_CEIL) / GAIN_SCALE_RANGE);
        return 1.0f - math::powf(f2, 1.0f / GAIN_SCALE_EXP);
    }
    float linScaleToGainWithRange(float f, float MTR_CEIL, float DBFS_MUTE_POS) {
        const float GAIN_SCALE_RANGE = DBFS_MUTE_POS - MTR_CEIL;
        float f1 = (1.0f - f);
        f1       = math::powf(f1, GAIN_SCALE_EXP);
        float f2 = (f1 * GAIN_SCALE_RANGE) + MTR_CEIL;
        return fromdBFS(f2);
    }
    bool getGainLvlWithRange(float fLinGain, float MTR_CEIL, float DBFS_MUTE_POS, float& fGainOut) {
        const float DBFS_FLOOR = DBFS_MUTE_POS + 1.0f;
        const float GAIN_DBFLOOR = math::powf(10.0f, DBFS_FLOOR / 20.0f);
        float fGainRaw = dsp_util::linScaleToGainWithRange(fLinGain, MTR_CEIL, DBFS_MUTE_POS);
        if (fGainRaw < GAIN_DBFLOOR) {
            fGainOut = 0.0f;
            return false;
        }
        fGainOut = math::clamp(fGainRaw, GAIN_DBFLOOR, dsp_util::GAIN_DB30);
        return true;
    }
}// namespace dsp_util

namespace math {

float distvec2(const vec2 a, const vec2 b) {
    auto vLen = vec2(b - a);
    return glm::length(vLen);
}

float distancePointLine(const vec2 pt, const vec2 a, const vec2 b) {
    vec2 v      = b - a;
    float lenSq = glm::dot(v, v);
    if (lenSq < 1E-4F) {
        return glm::distance(vec2(pt), vec2(a));
    }
    float t      = math::max(0.0f, math::min(1.0f, glm::dot(vec2(pt - a), v) / lenSq));
    const vec2 p = vec2(a) + t * v;
    return glm::distance(vec2(pt), p);
}

}// namespace math

namespace DAW::Panning {
    void MultiplyConstant(AudioBlock* src, AudioBlock* dst, float gain, float pan) {
        auto srcSamples  = src->samples;
        auto srcChannels = src->channels;
        auto channels    = dst->channels;
        auto samples     = dst->samples;
        auto srcBuf      = src->buf;
        auto buf         = dst->buf;

        dbgassert(srcSamples <= samples);
        const auto nSamples = math::min<samplecount_t>(srcSamples, samples);
        auto nChannels      = math::min<channelnum_t>(srcChannels, channels);
        float srcGain       = 1.0f;
        if (srcChannels == 2 && channels == 1) {
            srcGain   = 0.5f;
            nChannels = 2;
        }
        if (srcChannels == 1 && channels == 2) {
            nChannels = 2;
        }
        // float sqrt2    = sqrt(2.0f);
        // float panLR[2] = {
        //     float(sqrt(1.0 - double(pan))) * sqrt2,
        //     float(sqrt(double(pan))) * sqrt2,
        // };
        float panLR[2];
        CalculatePanning<PanLaw::SIN_4_5DB>(pan, &panLR[0], &panLR[1]);
        for (channelnum_t i = 0; i < nChannels; i++) {
            channelnum_t srcChannelIdx = srcChannels < 1 ? 0 : i % srcChannels;
            channelnum_t dstChannelIdx = channels < 1 ? 0 : i % channels;
            auto srcBufChannel         = srcBuf[srcChannelIdx];
            float* dstBufChannel       = buf[dstChannelIdx];
            for (samplecount_t j = 0; j < nSamples; j++) {
                dstBufChannel[j] += srcBufChannel[j] * srcGain * gain * panLR[i % 2];
            }
        }
    }
    void MultiplyAutomation(AudioBlock* src, AudioBlock* dst, float* pGain, float** pPan) {
        auto srcSamples  = src->samples;
        auto srcChannels = src->channels;
        auto channels    = dst->channels;
        auto samples     = dst->samples;
        auto srcBuf      = src->buf;
        auto buf         = dst->buf;

        dbgassert(srcSamples <= samples);
        const auto nSamples = math::min<samplecount_t>(srcSamples, samples);
        auto nChannels      = math::min<channelnum_t>(srcChannels, channels);
        float srcGain       = 1.0f;
        if (srcChannels == 2 && channels == 1) {
            srcGain   = 0.5f;
            nChannels = 2;
        }
        if (srcChannels == 1 && channels == 2) {
            nChannels = 2;
        }
        for (channelnum_t i = 0; i < nChannels; i++) {
            channelnum_t srcChannelIdx = srcChannels < 1 ? 0 : i % srcChannels;
            channelnum_t dstChannelIdx = channels < 1 ? 0 : i % channels;
            auto srcBufChannel         = srcBuf[srcChannelIdx];
            float* dstBufChannel       = buf[dstChannelIdx];
            float* gain                = pGain;
            float* panChannel          = pPan[i % 2];
            for (samplecount_t j = 0; j < nSamples; j++) {
                dstBufChannel[j] += srcBufChannel[j] * srcGain * (*gain++) * (*panChannel++);
            }
        }
    }

} // namespace DAW::Panning
#include "dsp_util.h"
#include "samplerate.h"
#include "config.h"
#include "math/seq_math.h"
#include "audioblock.h"
#include <stdlib.h>
#include <stdint.h>
#define _USE_MATH_DEFINES
#include <cmath>
#include <memory.h>
#include <algorithm>
#include <limits>
#include <string.h>

namespace dsp_util {
    const float GAIN_DB30    = math::powf(10.0f, 30.0f / 20.0f);// 2.0f
    const float GAIN_DB6     = math::powf(10.0f, 6.0f / 20.0f); // 2.0f
    const float GAIN_DBFLOOR = math::powf(10.0f, DBFS_FLOOR / 20.0f);
    const float GAIN_DBINF   = math::powf(10.0f, DBFS_INF_POS / 20.0f);

#define TABLE_SIZE (200)
    typedef struct
    {
        float sine[TABLE_SIZE];
        int left_phase;
        int right_phase;
    } paTestData;
    float Saturate(float input, float fMax) {
        static const float fGrdDiv = 0.5f;

        float x1 = fabsf(input + fMax);
        float x2 = fabsf(input - fMax);
        return fGrdDiv * (x1 - x2);
    }
    void fillSaturate(float** buffer, int32_t channels, uint32_t samples) {
        const float maxGain = 1.0;
        for (int i = 0; i < (channels + 1) / 2; i++) {
            float* output0 = buffer[i * 2 + 0];
            float* output1 = buffer[i * 2 + 1];
            for (uint32_t i = 0; i < samples; i++) {
                *output0 = dsp_util::Saturate(*output0, maxGain);
                *output1 = dsp_util::Saturate(*output1, maxGain);
                output0++;
                output1++;
            }
        }
    }
    void fillBlock(AudioBlock& block, float f) {
        fillChannels(block.buf, block.channels, block.samples, f);
    }
    void fillNoiseBlock(AudioBlock& block) {
        fillNoise(block.buf, block.channels, block.samples);
    }
    void fillChannels(float** buffer, int32_t channels, uint32_t samples, float f = 0.0f) {
        const float maxGain = 1.0;
        for (int i = 0; i < (channels + 1) / 2; i++) {
            float* input0 = buffer[i * 2 + 0];
            float* input1 = buffer[i * 2 + 1];
            for (uint32_t i = 0; i < samples; i++) {
                *input0 = f; /* left */
                *input1 = f; /* right */
                input0++;
                input1++;
            }
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
    void fillSine(float** buffer, uint32_t samples) {
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
        for (uint32_t i = 0; i < samples; i++) {
            *input0++ = data->sine[data->left_phase] * gain;  /* left */
            *input1++ = data->sine[data->right_phase] * gain; /* right */
            data->left_phase += 1;
            if (data->left_phase >= TABLE_SIZE) data->left_phase -= TABLE_SIZE;
            data->right_phase += 3; /* higher pitch so we can distinguish left and right. */
            if (data->right_phase >= TABLE_SIZE) data->right_phase -= TABLE_SIZE;
        }
    }
    void fillNoise(float** buffer, int32_t channels, uint32_t samples) {
        //TODO: make this thread safe
        float gain = 0.1f;


        for (int channelIdx = 0; channelIdx < channels; channelIdx++) {
            float* input0 = buffer[channelIdx];

            float g_fScale  = 2.0f / 0xffffffffUL;
            static int g_x1 = 0x67452301UL;
            static int g_x2 = 0xefcdab89UL;
            gain *= g_fScale;
            for (uint32_t i = 0; i < samples; i++) {
                g_x1 ^= g_x2;
                *input0++ = g_x2 * gain;
                g_x2 += g_x1;
            }
        }
    }
    void fillSqare(samplerate_t samplerate, float freq, float** buffer, uint32_t samples) {
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
        //	static int lastSign = (intOver & 0x80000000);
        // loop:
        for (uint32_t i = 0; i < samples; i++) {
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
        return 1.0f - powf(f2, 1.0 / GAIN_SCALE_EXP);
    }
    float linScaleToGain(float f) {
        float f1 = (1.0f - f);
        f1       = powf(f1, GAIN_SCALE_EXP);
        float f2 = (f1 * GAIN_SCALE_RANGE) + MTR_CEIL;
        return fromdBFS(f2);
    }
}// namespace dsp_util

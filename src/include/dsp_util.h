#pragma once
#include <cstdint>
#include "samplerate.h"

struct AudioBlock;
namespace dsp_util {

    float Saturate(float input, float fMax);
    void fillSaturate(float** buffer, int32_t channels, uint32_t samples);
    void fillSine(float** buffer, uint32_t samples);
    void fillNoise(float** buffer, int32_t channels, uint32_t samples);
    void fillSqare(samplerate_t samplerate, float freq, float** buffer, uint32_t samples);
    void fillChannels(float** buffer, int32_t channels, uint32_t samples, float f);
    void fillBlock(AudioBlock& block, float f);
    void fillNoiseBlock(AudioBlock& block);
    void copyBuffer(float** dst, float** src, uint32_t samples);
    float clampGain(float f);
    float clampReadGain(float f);
    float dBFS(float f);
    float dBFSClampInf6(float f);
    float fromdBFSClampInf6(float f);
    float fromdBFS(float f);
    const float DBFS_FLOOR    = -80.0f;
    const float DBFS_MUTE_POS = -81.0f;
    const float DBFS_INF_POS  = -100.0f;
    const float MTR_FLOOR     = -60.0f;
    const float MTR_CEIL      = 6.0f;
    extern const float GAIN_DB6;
    extern const float GAIN_DBFLOOR;
    extern const float GAIN_DBINF;
    float scaledRange(float db, float lvlFloor, float lvlCeil);
    float gainToLinScale(float f);
    float linScaleToGain(float f);

    /**
     * Calculate mixer gain level from parameter.
     * returns: false if gain == -inf db
     */
    inline bool getGainLvl(float fLinGain, float& fGainOut) {
        float fGainRaw = dsp_util::linScaleToGain(fLinGain);
        if (fGainRaw < dsp_util::GAIN_DBFLOOR) {
            fGainOut = 0.0f;
            return false;
        }
        fGainOut = dsp_util::clampReadGain(fGainRaw);
        return true;
    }
}// namespace dsp_util

#pragma once
#include "types.h"
#include "samplerate.h"
#include "math/seq_math.h"

struct AudioBlock;
namespace dsp_util {

    float Saturate(float input, float fMax);
    void fillSaturate(float** buffer, channelnum_t channels, samplecount_t samples);
    void fillSine(float** buffer, samplecount_t samples);
    void fillNoise(float** buffer, channelnum_t channels, samplecount_t samples);
    void fillSqare(samplerate_t samplerate, float freq, float** buffer, samplecount_t samples);
    void fillChannels(float** buffer, channelnum_t channels, samplecount_t samples, float f);
    void fillBlock(AudioBlock& block, float f);
    void fillNoiseBlock(AudioBlock& block);
    void copyBuffer(float** dst, float** src, samplecount_t samples);
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

    float linScaleToGainWithRange(float f, float MTR_CEIL, float DBFS_MUTE_POS);
    float gainToLinScaleWithRange(float f, float MTR_CEIL, float DBFS_MUTE_POS);

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
    bool getGainLvlWithRange(float fLinGain, float MTR_CEIL, float DBFS_MUTE_POS, float& fGainOut);

}// namespace dsp_util

namespace DAW::Panning {
    enum class PanLaw {
        SQRT,
        SIN_3_0DB,
        SIN_4_5DB,
        SIN_6_0DB,
    };
    template<PanLaw P>
    constexpr void CalculatePanning(float pan, float* pPanL, float* pPanR) {
        if constexpr (P == PanLaw::SQRT) {
            const float sqrt2 = sqrt(2.0f);
            *pPanL = sqrt(1.0f - pan) * sqrt2;
            *pPanR = sqrt(pan) * sqrt2;
        } else if constexpr (P == PanLaw::SIN_3_0DB) {
            *pPanL = sin((1.0f - pan) * FLOAT_HALF_PI);
            *pPanR = sin(pan *FLOAT_HALF_PI);
        } else if constexpr (P == PanLaw::SIN_4_5DB) {
            *pPanL = powf(sin((1.0f - pan) * FLOAT_HALF_PI), 1.5f);
            *pPanR = powf(sin(pan * FLOAT_HALF_PI), 1.5f);
        } else if constexpr (P == PanLaw::SIN_6_0DB) {
            *pPanL = powf(sin((1.0f - pan) * FLOAT_HALF_PI), 2.f);
            *pPanR = powf(sin(pan * FLOAT_HALF_PI), 2.f);
        }
    }
    template<PanLaw P>
    constexpr float CenterGain() {
        if constexpr (P == PanLaw::SQRT) {
            return 1.0f;
        } else if constexpr (P == PanLaw::SIN_3_0DB) {
            return 0.70710678118654752440084436210485f;
        } else if constexpr (P == PanLaw::SIN_4_5DB) {
            return 0.594603539f;
        } else if constexpr (P == PanLaw::SIN_6_0DB) {
            return 0.5f;
        }
    }
    constexpr float GetCenterGain() {
        return CenterGain<PanLaw::SIN_4_5DB>();
    }

    void MultiplyAutomation(AudioBlock* src, AudioBlock* dst, float* pGain, float** pPan);

    void MultiplyConstant(AudioBlock* src, AudioBlock* dst, float gain, float pan);
}

#pragma once
#include "types.h"
#include "samplerate.h"
#include "math/seq_math.h"
#include "host/audiobuffer/audioblock.h"

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
    constexpr float DBFS_FLOOR    = -80.0f;
    constexpr float DBFS_MUTE_POS = -81.0f;
    constexpr float DBFS_INF_POS  = -100.0f;
    constexpr float MTR_FLOOR     = -60.0f;
    constexpr float MTR_CEIL      = 6.0f;
    extern const float GAIN_DB6;
    extern const float GAIN_DBFLOOR;
    extern const float GAIN_DBINF;
    constexpr float scaledRange(float db, float lvlFloor, float lvlCeil) {
        if (db < dsp_util::DBFS_FLOOR)
            return 1.0f;
        float lvlRange = lvlFloor - lvlCeil;
        return (math::max(lvlFloor, math::min(db, lvlCeil)) - lvlCeil) / lvlRange;
    }
    float gainToLinScale(float f);
    float linScaleToGain(float f);

    float linScaleToGainWithRange(float f, float MTR_CEIL, float DBFS_MUTE_POS);
    float gainToLinScaleWithRange(float f, float MTR_CEIL, float DBFS_MUTE_POS);
    float dbfsToLinScaleWithRange(float dbfs, float MTR_CEIL, float DBFS_MUTE_POS);
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
    template<typename FPTypeSrc, typename FPTypeDst>
    void MultiplyAutomation(AudioBlockBase<FPTypeSrc>* src, AudioBlockBase<FPTypeDst>* dst, float* pGain, float** pPan);

    template<typename FPTypeSrc, typename FPTypeDst>
    void MultiplyConstant(AudioBlockBase<FPTypeSrc>* src, AudioBlockBase<FPTypeDst>* dst, float gain, float pan);
}

namespace DAW {
    enum class CurveShapingFunction : int32_t {
        Linear = 0,
        Pow,
        Exp,
    };
#ifdef _MSC_VER
#define SHAPE_CONSTEXPR inline
#else
#define SHAPE_CONSTEXPR constexpr
#endif
    SHAPE_CONSTEXPR double shapeCurveSegmentExp(double x, double shape) {
        double shapeBi = 1.0 - shape * 2.0;
        return exp((1.0 - x) * shapeBi) * x;
    }
    // does not sound clean enough (noticable in short attack phase)
    SHAPE_CONSTEXPR double shapeCurveSegmentPow(double x, double shape) {
        double shapeBi  = 1.0 - shape * 2.0;
        double shapeBiAbs = fabs(shapeBi);
        if (shapeBiAbs != 0.0) {
            double shapeExp = 0.0;
            double scale2   = 0.2 + x * 0.8;
            if (shapeBi < 0.0) {
                shapeExp = 1.0 + scale2 * shapeBiAbs * 16.0;
            } else {
                shapeExp = 1.0 / (1.0 + scale2 * shapeBiAbs * 16.0);
            }
            return pow(x, shapeExp);
        }
        return x;
    }
    SHAPE_CONSTEXPR double shapeCurveSegment(CurveShapingFunction shaping, double x, double shape) {
        switch (shaping) {
            case CurveShapingFunction::Exp:
                return shapeCurveSegmentExp(x, shape);
            case CurveShapingFunction::Pow:
                return shapeCurveSegmentPow(x, shape);
            case CurveShapingFunction::Linear:
            default:
                return x;
        }
    }
#undef SHAPE_CONSTEXPR
}
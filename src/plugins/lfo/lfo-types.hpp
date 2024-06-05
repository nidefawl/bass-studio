#pragma once
#include "types.h"
#include "str_util.h"
#include "seq_time.h"
#include "math/seq_math.h"
#include "host/plugin/internal/internal-plugin.h"
#include <vector>

namespace PluginLFO {
    enum NoteRatio : uint8_t {
        STRAIGHT = 1,
        DOTTED = 2,
        TRIPLET = 4,
    };

    struct LFOSyncRatio {
        int32_t numerator;
        int32_t denominator;
        String text;
    };
    struct LFOSyncParameters {
        int32_t syncFlags;
        std::vector<LFOSyncRatio> syncRatios;
    };

    constexpr std::array LFO_PHASE_RESET_STEPS = {
        tick_t(0), tick_t(512), 1024, 1536, 2048, 2560, 3072, 3584, 4096, 6144, 8192, 12288, 16384, 20480, 24576, 28672, 32768, 40960, 49152, 57344, 65536, 81920, 98304, 114688, 131072, 163840, 196608, 229376, 262144, 327680, 393216, 458752, 524288
    };
    const double RATE_MIN = 1;
    const double RATE_MAX = TICKS_BAR*4;

    inline tick_t GetScaledResetTicks(float paramValue) {
        int32_t idx = math::clamp<int32_t>(math::floorfS32(paramValue * (LFO_PHASE_RESET_STEPS.size()-1)), 0, LFO_PHASE_RESET_STEPS.size() - 1);
        return LFO_PHASE_RESET_STEPS[idx];
    }

    inline float ResetTicksToParam(float resetTicks) {
        size_t idx = 0;
        for (size_t i = 0; i < LFO_PHASE_RESET_STEPS.size(); ++i) {
            if (resetTicks >= LFO_PHASE_RESET_STEPS[i]) {
                break;
            }
            idx = i;
        }
        return float(idx) / float(LFO_PHASE_RESET_STEPS.size() - 1);
    }

    inline String FormatResetTicks(float paramValue) {
        return StringFormat("%d", GetScaledResetTicks(paramValue));
    }

    inline double GetScaledRate(float paramValue) {
        return math::clamp(paramValue * (RATE_MAX - RATE_MIN) + RATE_MIN, RATE_MIN, RATE_MAX);
    }

    inline float RateToParam(float rate) {
        return float((rate - RATE_MIN) / (RATE_MAX - RATE_MIN));
    }

    inline std::vector<LFOSyncRatio> GetSyncRatios(int syncFlags = (STRAIGHT | DOTTED | TRIPLET)) {
        std::vector<LFOSyncRatio> syncRatios;
        for (int32_t i = 64; i >= 1; i /= 2) {
            if (syncFlags & NoteRatio::TRIPLET) {
                syncRatios.push_back({ 1, i * 3, StringFormat("%d/%d", 1, i*3) });// triplet
            }
            if (syncFlags & NoteRatio::STRAIGHT) {
                syncRatios.push_back({ 1, i, StringFormat("%d/%d", 1, i) });// straight
            }
            if (syncFlags & NoteRatio::DOTTED) {
                syncRatios.push_back({ 3, i, StringFormat("%d/%d", 3, i) });// dotted
            }
        }
        for (int32_t i = 2; i < 32; i *= 2) {
            if (syncFlags & NoteRatio::TRIPLET) {
                syncRatios.push_back({ i, 3, StringFormat("%d/%d", i, 3) });// triplet
            }
            if (syncFlags & NoteRatio::STRAIGHT) {
                syncRatios.push_back({ i, 1, StringFormat("%d/%d", i, 1) });// straight
            }
            if (syncFlags & NoteRatio::DOTTED) {
                syncRatios.push_back({ 3 * i, 1, StringFormat("%d/%d", 3, i*2) });// dotted
            }
        }
        if (syncFlags & NoteRatio::STRAIGHT) {
            for (int32_t i : {32, 64, 128}) {
                syncRatios.push_back({ i, 1, StringFormat("%d/%d", i, 1) });// straight
            }
        }
        std::sort(syncRatios.begin(), syncRatios.end(), [](const LFOSyncRatio& a, const LFOSyncRatio& b) {
            return a.numerator * b.denominator < b.numerator * a.denominator;
        });
        return syncRatios;
    }

    inline std::vector<String> GetSyncRatioLabels(int syncFlags = (STRAIGHT | DOTTED | TRIPLET)) {
        auto syncs = GetSyncRatios(syncFlags);
        std::vector<String> syncRatios;
        syncRatios.reserve(syncs.size());
        for (auto& sync : syncs) {
            syncRatios.push_back(sync.text);
        }
        return syncRatios;
    }

    inline float GetSyncRate(const std::vector<LFOSyncRatio>& syncRatios, bool bIsSync, float paramValue) {
        if (!bIsSync || syncRatios.empty()) {
            return GetScaledRate(paramValue);
        }

        int32_t index = math::clamp<int32_t>(math::floorfS32(paramValue * syncRatios.size()), 0, CtrSize(syncRatios) - 1);
        const LFOSyncRatio& syncRatio = syncRatios[index];
        return float(TICKS_BAR * syncRatio.numerator) / syncRatio.denominator;
    }

    inline String FormatSyncRate(const std::vector<LFOSyncRatio>& syncRatios, int32_t syncFlags, float paramValue) {
        if (!syncFlags) {
            return StringFormat("%.2f", GetScaledRate(paramValue));
        }
        int32_t index = math::clamp<int32_t>(math::floorfS32(paramValue * syncRatios.size()), 0, CtrSize(syncRatios) - 1);
        return syncRatios[index].text;
    }
    
    class LFORateMinMaxAutomation {
    public:
        virtual ~LFORateMinMaxAutomation() = default;
        virtual std::pair<float, float> getMinMax(double dTick) const = 0;
        virtual std::tuple<float, float, float> getRatePhase(double dTick) const = 0;
    };

    struct lfo_automation_src_synced_t final : public automated_param_t {
        DAW::Shape::shape_t* shape;
        LFORateMinMaxAutomation* rateMinMax = nullptr;
        LFOSyncParameters* sync = nullptr;
        float getPhase(double dTick) const {
            const auto [fRate, fPhase, fPhaseDuration] = rateMinMax->getRatePhase(dTick);

            double fPhaseOffset = 0.0f;
            if (fPhaseDuration > 0) {
                double phaseDuration = GetScaledResetTicks(fPhaseDuration);
                dTick = fmod(dTick, phaseDuration);
            }
            if (sync->syncRatios.empty()) {
                fPhaseOffset = dTick / GetScaledRate(fRate);
            } else {
                auto index = math::clamp<int32_t>(math::floorfS32(fRate * CtrSize(sync->syncRatios)), 0, CtrSize(sync->syncRatios) - 1);
                auto ratio = sync->syncRatios[index];
                double barPos = dTick / double(TICKS_BAR);
                fPhaseOffset = double((barPos * ratio.denominator) / ratio.numerator);
            }
            double phase = fPhaseOffset + fPhase;
            double _unused = 0.0;
            float moduloPhase = float(std::modf(phase, &_unused));
            // ensure phase is positive
            moduloPhase = moduloPhase < 0.0f ? 1.0f + moduloPhase : moduloPhase;
            return moduloPhase;
        }
        float sampleCurve(double dTick) const {
            float moduloPhase = getPhase(dTick);
            auto [valMin, valMax] = rateMinMax->getMinMax(dTick);
            auto value = shape->sampleCurve(moduloPhase, false);
            return value * (valMax - valMin) + valMin;
        }
        float modulateValue(double tick, float fIn, const DAW::modulation_scaling_t& scale) const override {
            const auto s = sampleCurve(tick);
            if(fp_math::isNanOrInfd(s)) {
                return fIn;
            }
            const auto valScaled = scale.min + s * (scale.max - scale.min);
            switch (scale.mode) {
                case DAW::ModulationMode::ADD:
                    fIn += valScaled;
                    break;
                case DAW::ModulationMode::MUL:
                    fIn *= valScaled;
                    break;
                case DAW::ModulationMode::REPLACE:
                    fIn = valScaled;
                    break;
                default:
                    break;
            }
            if (scale.bClamp) {
                fIn = math::clamp(fIn, 0.0f, 1.0f);
            }
            dbgassert(!fp_math::isNanOrInfd(fIn));
            return fIn;
        }
        void sampleAutomation(double dTickBegin, double dTickEnd, samplecount_t numSamples, const DAW::modulation_scaling_t& scale, float* inOut) const override {
            for (samplecount_t i = 0; i < numSamples; ++i) {
                const auto dTickOffset = dTickBegin + i * (dTickEnd - dTickBegin) / double(numSamples);
                const auto valScaled   = scale.min + sampleCurve(dTickOffset) * (scale.max - scale.min);
                switch (scale.mode) {
                    case DAW::ModulationMode::ADD:
                        *inOut += valScaled;
                        break;
                    case DAW::ModulationMode::MUL:
                        *inOut *= valScaled;
                        break;
                    case DAW::ModulationMode::REPLACE:
                        *inOut = valScaled;
                        break;
                    default:
                        break;
                }
                if (scale.bClamp) {
                    *inOut = math::clamp(*inOut, 0.0f, 1.0f);
                }
                ++inOut;
            }
        }
        void setRange(tick_t tickBegin, tick_t tickEnd, std::vector<automation_point_t>& data) override {
        }
        void copyRange(tick_t tickBegin, tick_t tickEnd, std::vector<automation_point_t>& data) const override {
        }
        bool isActive() const override { //??
            return true;
        }
        bool isAutomated() const override { //??
            return true; 
        }
        float getValueAt(tick_t tick) const override {
            return sampleCurve(tick);
        }
        float getValueAtExact(double dTick) const override {
            return sampleCurve(dTick);
        }
        String getName() const override {
            return StringFormat("LFO %d", paramIdx+1);
        }
        void deleteTickRange(tick_t tickBegin, tick_t tickEnd) override {
        }
        void insertTickRange(tick_t tickBegin, tick_t tickEnd, const std::vector<automation_point_t>& data) override {
        }
    };
    struct lfo_automation_src_random_t : public automated_param_t {
        LFORateMinMaxAutomation* rateMinMax = nullptr;
        LFOSyncParameters* sync = nullptr;
        float modulateValue(double tick, float fIn, const DAW::modulation_scaling_t& scale) const override {
            const auto s = sampleCurve(tick);
            dbgassert(!fp_math::isNanOrInfd(s));
            const auto valScaled = scale.min + s * (scale.max - scale.min);
            switch (scale.mode) {
                case DAW::ModulationMode::ADD:
                    fIn += valScaled;
                    break;
                case DAW::ModulationMode::MUL:
                    fIn *= valScaled;
                    break;
                case DAW::ModulationMode::REPLACE:
                    fIn = valScaled;
                    break;
                default:
                    break;
            }
            if (scale.bClamp) {
                fIn = math::clamp(fIn, 0.0f, 1.0f);
            }
            dbgassert(!fp_math::isNanOrInfd(fIn));
            return fIn;
        }
        void sampleAutomation(double dTickBegin, double dTickEnd, samplecount_t numSamples, const DAW::modulation_scaling_t& scale, float* inOut) const override {
            for (samplecount_t i = 0; i < numSamples; ++i) {
                const auto dTickOffset = dTickBegin + i * (dTickEnd - dTickBegin) / double(numSamples);
                const auto valScaled   = scale.min + sampleCurve(dTickOffset) * (scale.max - scale.min);
                switch (scale.mode) {
                    case DAW::ModulationMode::ADD:
                        *inOut += valScaled;
                        break;
                    case DAW::ModulationMode::MUL:
                        *inOut *= valScaled;
                        break;
                    case DAW::ModulationMode::REPLACE:
                        *inOut = valScaled;
                        break;
                    default:
                        break;
                }
                if (scale.bClamp) {
                    *inOut = math::clamp(*inOut, 0.0f, 1.0f);
                }
                ++inOut;
            }
        }
        void setRange(tick_t tickBegin, tick_t tickEnd, std::vector<automation_point_t>& data) override {
        }
        void copyRange(tick_t tickBegin, tick_t tickEnd, std::vector<automation_point_t>& data) const override {
        }
        bool isActive() const override { //??
            return true;
        }
        bool isAutomated() const override { //??
            return true; 
        }
        float getValueAt(tick_t tick) const override {
            return sampleCurve(tick);
        }
        float getValueAtExact(double dTick) const override {
            return sampleCurve(dTick);
        }
        String getName() const override {
            return StringFormat("LFO %d", paramIdx+1);
        }
        void deleteTickRange(tick_t tickBegin, tick_t tickEnd) override {
        }
        void insertTickRange(tick_t tickBegin, tick_t tickEnd, const std::vector<automation_point_t>& data) override {
        }
        virtual float getPhase(double dTick) const {
            const auto [fRate, fPhase, fPhaseDuration] = rateMinMax->getRatePhase(dTick);
            double fPhaseOffset = 0.0f;
            if (sync->syncRatios.empty()) {
                fPhaseOffset = dTick / GetScaledRate(fRate);
            } else {
                auto index = math::clamp<int32_t>(math::floorfS32(fRate * CtrSize(sync->syncRatios)), 0, CtrSize(sync->syncRatios) - 1);
                auto ratio = sync->syncRatios[index];
                double barPos = dTick / double(TICKS_BAR);
                fPhaseOffset = double((barPos * ratio.denominator) / ratio.numerator);
            }
            auto phase = fPhaseOffset + fPhase;
            return phase;
        }
        virtual float sampleCurve(double dTick) const = 0;
        virtual int32_t getModeId() const = 0;

        float scaleMinMax(double dTick, float f) const {
            const auto [valMin, valMax] = rateMinMax->getMinMax(dTick);
            return f * (valMax - valMin) + valMin;
        }
        std::pair<tick_t, tick_t> getPrevNextTick(double dTick) const {
            auto prevTick = math::floordS64(dTick);
            auto nextTick = math::ceildS64(dTick);
            if (prevTick == nextTick) {
                nextTick += 1;
            }
            return { prevTick, nextTick };
        }
    };
    struct lfo_automation_src_random_smooth_t final : public lfo_automation_src_random_t {
        float sampleCurve(double dTick) const override {
            float phase = getPhase(dTick);
            seq_rand r;
            auto [prevTick, nextTick] = getPrevNextTick(phase);
            r.rng_seed(prevTick);
            auto v0 = r.rng_double();
            r.rng_seed(nextTick);
            auto v1 = r.rng_double();
            float _unused = 0.0f;
            float v = std::modf(phase, &_unused);
            // ensure phase is positive
            v = v < 0.0f ? 1.0f + v : v;
            v = v * v * (3.0f - 2.0f * v);
            v = v0 + (v1 - v0) * v;
            return scaleMinMax(dTick, v);
        }
        int32_t getModeId() const override {
            return 0;
        }
    };
    struct lfo_automation_src_random_linear_t final : public lfo_automation_src_random_t {
        float sampleCurve(double dTick) const override {
            float phase = getPhase(dTick);
            seq_rand r;
            auto [prevTick, nextTick] = getPrevNextTick(phase);
            r.rng_seed(prevTick);
            auto v0 = r.rng_double();
            r.rng_seed(nextTick);
            auto v1 = r.rng_double();
            float _unused = 0.0f;
            float v = std::modf(phase, &_unused);
            // ensure phase is positive
            v = v < 0.0f ? 1.0f + v : v;
            // v = v * v * (3.0f - 2.0f * v);
            v = v0 + (v1 - v0) * v;
            return scaleMinMax(dTick, v);
        }
        int32_t getModeId() const override {
            return 1;
        }
    };
    struct lfo_automation_src_random_exp_t final : public lfo_automation_src_random_t {
        float sampleCurve(double dTick) const override {
            float phase = getPhase(dTick);
            seq_rand r;
            auto [prevTick, nextTick] = getPrevNextTick(phase);
            r.rng_seed(prevTick);
            auto v0 = r.rng_double();
            auto shape0 = r.rng_double();
            r.rng_seed(nextTick);
            auto v1 = r.rng_double();
            float _unused = 0.0f;
            float v = std::modf(phase, &_unused);
            // ensure phase is positive
            v = v < 0.0f ? 1.0f + v : v;
            float shapeBi  = 1.0f - shape0 * 2.0f;
            float shapeExp = 0.0f;
            float scale2   = 0.2f + v * 0.8f;
            if (shapeBi < 0.0f) {
                shapeExp = 1.0f + scale2 * std::fabs(shapeBi) * 16.f;
            } else {
                shapeExp = 1.0f / (1.0f + scale2 * std::fabs(shapeBi) * 16.f);
            }
            v = ::powf(v, shapeExp);
            v = v0 + (v1 - v0) * v;
            return scaleMinMax(dTick, v);
        }
        int32_t getModeId() const override {
            return 2;
        }
    };
    struct lfo_automation_src_random_sample_and_hold_t final : public lfo_automation_src_random_t {
        float sampleCurve(double dTick) const override {
            float phase = getPhase(dTick);
            seq_rand r;
            auto [prevTick, nextTick] = getPrevNextTick(phase);
            r.rng_seed(prevTick);
            auto v0 = r.rng_double();
            auto v = v0;
            return scaleMinMax(dTick, v);
        }
        int32_t getModeId() const override {
            return 3;
        }
    };
}
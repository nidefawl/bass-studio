#include "lfo-plugin.h"
#include "assert_dbg.h"
#include "host/automation/automation.h"
#include "event.h"
#include "file/shapefile.h"
#include "gui/automation/modulation.h"
#include "gui/container/container.h"
#include "gui/controls/button.h"
#include "gui/controls/inputfield.h"
#include "gui/controls/knob.h"
#include "gui/controls/knobpluginparam.h"
#include "gui/gui.h"
#include "gui/plugin/plugin.h"
#include "gui/shape/shapeeditor.h"
#include "gui/tooltip/tooltip.h"
#include "gui/views/controls.h"
#include "guicolors.h"
#include "guiconstant.h"
#include "host/host_pluginmanager.h"
#include "host/daw/mainctrl.h"
#include "logging.h"
#include "math/seq_math.h"
#include "rand.h"
#include "renderresources.h"
#include "seq_time.h"
#include "seq_util.h"
#include "host/plugin/plugin-lockable.h"
#include "host/shape/shape.h"
#include "str_util.h"
#include "byte-buffer.h"
#include "types.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <nanovg.h>
#include <utility>
#include <vector>

namespace PluginLFO {
    constexpr int32_t NUM_CHANNELS = 1;
    constexpr int32_t BINARY_SNAPSHOT_VERSION = 4;
    constexpr int32_t PARAM_LFO_RATE = 16;
    constexpr int32_t PARAM_LFO_PHASE = 17;
    constexpr int32_t PARAM_LFO_MINIMUM = 18;
    constexpr int32_t PARAM_LFO_MAXIMUM = 19;

    const double RATE_MIN = 1;
    const double RATE_MAX = TICKS_BAR*4;

    double GetScaledRate(float paramValue) {
        return math::clamp(paramValue * (RATE_MAX - RATE_MIN) + RATE_MIN, RATE_MIN, RATE_MAX);
    }

    float RateToParam(float rate) {
        return float((rate - RATE_MIN) / (RATE_MAX - RATE_MIN));
    }

    struct SyncRatio {
        int32_t numerator;
        int32_t denominator;
        String text;
    };

    enum NoteRatio : uint8_t {
        STRAIGHT = 1,
        DOTTED = 2,
        TRIPLET = 4,
    };

    std::vector<SyncRatio> GetSyncRatios(int syncFlags = (STRAIGHT | DOTTED | TRIPLET)) {
        std::vector<SyncRatio> syncRatios;
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
        std::sort(syncRatios.begin(), syncRatios.end(), [](const SyncRatio& a, const SyncRatio& b) {
            return a.numerator * b.denominator < b.numerator * a.denominator;
        });
        return syncRatios;
    }

    std::vector<String> GetSyncRatioLabels(int syncFlags = (STRAIGHT | DOTTED | TRIPLET)) {
        auto syncs = GetSyncRatios(syncFlags);
        std::vector<String> syncRatios;
        syncRatios.reserve(syncs.size());
        for (auto& sync : syncs) {
            syncRatios.push_back(sync.text);
        }
        return syncRatios;
    }

    float GetSyncRate(const std::vector<SyncRatio>& syncRatios, bool bIsSync, float paramValue) {
        if (!bIsSync || syncRatios.empty()) {
            return GetScaledRate(paramValue);
        }

        int32_t index = math::clamp<int32_t>(math::floorfS32(paramValue * syncRatios.size()), 0, CtrSize(syncRatios) - 1);
        const SyncRatio& syncRatio = syncRatios[index];
        return (TICKS_BAR * syncRatio.numerator) / syncRatio.denominator;
    }

    String FormatSyncRate(const std::vector<SyncRatio>& syncRatios, int32_t syncFlags, float paramValue) {
        if (!syncFlags) {
            return StringFormat("%.2f", GetScaledRate(paramValue));
        }
        int32_t index = math::clamp<int32_t>(math::floorfS32(paramValue * syncRatios.size()), 0, CtrSize(syncRatios) - 1);
        return syncRatios[index].text;
    }

    struct impl_channel_snapshot_t {
        DAW::Shape::shape_snapshot_t shape;
        int32_t syncFlags = false;
        bool modeIsShape = true;
        int32_t modeRandom = -1;
    };

    struct module_lfo::lfo_impl_t final : public PluginLockable {
        struct lfo_sync_settings_t {
            int32_t syncFlags;
            std::vector<SyncRatio> syncRatios;
        };
        struct lfo_automation_src_synced_t final : public automated_param_t {
            module_lfo* module = nullptr;
            lfo_sync_settings_t* sync = nullptr;
            DAW::Shape::shape_t shape;
            float getPhase(double dTick) const {
                const auto fRate = module->getParamValue(PARAM_LFO_RATE);
                const auto fPhase = module->getParamValue(PARAM_LFO_PHASE);
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
                float moduloPhase = modf(phase, &phase);
                return moduloPhase;
            }
            float sampleCurve(double dTick) const {
                float moduloPhase = getPhase(dTick);
                auto valMin = module->getParamValue(PARAM_LFO_MINIMUM) * 2.0f - 1.0f;
                auto valMax = module->getParamValue(PARAM_LFO_MAXIMUM) * 2.0f - 1.0f;
                auto value = shape.sampleCurve(moduloPhase, false);
                return value * (valMax - valMin) + valMin;
            }
            float modulateValue(tick_t tick, float fIn, const DAW::modulation_scaling_t& scale) const override {
                const auto valScaled = scale.min + sampleCurve(tick) * (scale.max - scale.min);
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
            module_lfo* module = nullptr;
            lfo_sync_settings_t* sync = nullptr;

            float modulateValue(tick_t tick, float fIn, const DAW::modulation_scaling_t& scale) const override {
                const auto valScaled = scale.min + sampleCurve(tick) * (scale.max - scale.min);
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
                const auto fRate = module->getParamValue(PARAM_LFO_RATE);
                const auto fPhase = module->getParamValue(PARAM_LFO_PHASE);
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

            float scaleMinMax(float f) const {
                const auto valMin = module->getParamValue(PARAM_LFO_MINIMUM) * 2.0f - 1.0f;
                const auto valMax = module->getParamValue(PARAM_LFO_MAXIMUM) * 2.0f - 1.0f;
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
                const auto valMin = module->getParamValue(PARAM_LFO_MINIMUM) * 2.0f - 1.0f;
                const auto valMax = module->getParamValue(PARAM_LFO_MAXIMUM) * 2.0f - 1.0f;
                float phase = getPhase(dTick);
                seq_rand r;
                auto [prevTick, nextTick] = getPrevNextTick(phase);
                r.rng_seed(prevTick);
                auto v0 = r.rng_double();
                r.rng_seed(nextTick);
                auto v1 = r.rng_double();
                float v = modf(phase, &phase);
                v = v * v * (3.0f - 2.0f * v);
                v = v0 + (v1 - v0) * v;
                v = v * (valMax - valMin) + valMin;
                return v;
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
                float v = modf(phase, &phase);
                // v = v * v * (3.0f - 2.0f * v);
                v = v0 + (v1 - v0) * v;
                return scaleMinMax(v);
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
                float v = modf(phase, &phase);
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
                return scaleMinMax(v);
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
                return scaleMinMax(v);
            }
            int32_t getModeId() const override {
                return 3;
            }
        };
        struct lfo_channel_t : public lfo_sync_settings_t {
            bool modeIsShape = true;
            lfo_automation_src_synced_t srcSync;
            std::shared_ptr<lfo_automation_src_random_t> srcRand;
        };

        module_lfo* const module;
        std::array<lfo_channel_t, NUM_CHANNELS> channels;
        explicit lfo_impl_t(DawInstance* _daw, module_lfo* _module, const DAW::Shape::shape_t& initShape) 
            : PluginLockable(_daw),
            module(_module)
        {
            for (auto& channel : channels) {
                channel.syncFlags = STRAIGHT | DOTTED | TRIPLET;
                channel.syncRatios = GetSyncRatios(channel.syncFlags);
                channel.srcSync.module = module;
                channel.srcSync.sync = &channel;
                channel.srcSync.shape = initShape;
                channel.modeIsShape = true;
            }
            for (int32_t idx = 0; idx < CtrSize(channels); ++idx) {
                setRandomMode(idx, 0);
                channels[idx].modeIsShape = true;
            }
        }
        void setRandomMode(int32_t chIdx, int32_t mode) {
            auto& channel = channels[chIdx];
            channel.modeIsShape = false;
            switch (mode) {
                case -1:
                    if (channel.srcRand) {
                        break;
                    }
                    [[fallthrough]];
                default:
                case 0:
                    channel.srcRand = std::make_shared<lfo_automation_src_random_smooth_t>();
                    break;
                case 1:
                    channel.srcRand = std::make_shared<lfo_automation_src_random_linear_t>();
                    break;
                case 2:
                    channel.srcRand = std::make_shared<lfo_automation_src_random_exp_t>();
                    break;
                case 3:
                    channel.srcRand = std::make_shared<lfo_automation_src_random_sample_and_hold_t>();
                    break;
            }
            channel.srcRand->module = module;
            channel.srcRand->sync = &channel;
        }
        int32_t getRandomMode(int32_t chIdx) const {
            return channels[chIdx].srcRand->getModeId();
        }
        const automated_param_t* getModulationOutputData(const DAW::modulation_channel_ref& channel) {
            auto chIdx = channel.refSrc.paramIdx;
            if (!assert_expr(chIdx >= 0 && chIdx < NUM_CHANNELS))
                return nullptr;
            if (!assert_expr(chIdx < CtrSize(channels)))
                return nullptr;
            auto& ch = channels[chIdx];
            automated_param_t* src = &ch.srcSync;
            if (!ch.modeIsShape && ch.srcRand) {
                src = ch.srcRand.get();
            }
            return src;
        }
        DAW::Shape::shape_t& getShape(int32_t chIdx) {
            static DAW::Shape::shape_t shapeDummy{};
            if (!assert_expr(chIdx >= 0 && chIdx < NUM_CHANNELS))
                return shapeDummy;
            if (!assert_expr(chIdx < CtrSize(channels)))
                return shapeDummy;
            return channels[chIdx].srcSync.shape;
        }
        int32_t getSyncFlags(int32_t chIdx) const {
            dbgassert(chIdx >= 0 && chIdx < NUM_CHANNELS);
            return channels[chIdx].syncFlags;
        }
        void setSyncFlags(int32_t chIdx, int32_t flags) {
            dbgassert(chIdx >= 0 && chIdx < NUM_CHANNELS);
            channels[chIdx].syncFlags = flags;
            channels[chIdx].syncRatios = GetSyncRatios(flags);
        }
        bool getSnapshot(snapshot_t& snapshot) {
            snapshot.version = BINARY_SNAPSHOT_VERSION;
            for (int32_t i = 0; i < NUM_CHANNELS; ++i) {
                auto shapeSnapshot = DAW::Shape::shape_snapshot_t{ i, DAW::Shape::shape_preset_t{2, channels[i].srcSync.shape} };
                impl_channel_snapshot_t channelSnapshot{ std::move(shapeSnapshot), channels[i].syncFlags, channels[i].modeIsShape, channels[i].srcRand ? channels[i].srcRand->getModeId() : -1 };
                snapshot.channels.push_back(std::move(channelSnapshot));
            }
            return true;
        }

        bool setSnapshot(const snapshot_t& snapshot) {
            for (int32_t i = 0; i < NUM_CHANNELS && i < CtrSize(snapshot.channels); ++i) {
                auto& channelSnapshot = snapshot.channels[i];
                channels[i].srcSync.shape.pts = channelSnapshot.shape.shape.curve.pts;
                if (snapshot.version > 2) {
                    setSyncFlags(i, channelSnapshot.syncFlags);
                }
                if (snapshot.version > 3) {
                    setRandomMode(i, channelSnapshot.modeRandom);
                    if (channelSnapshot.modeIsShape) {
                        channels[i].modeIsShape = true;
                    }
                }
            }
            return true;
        }
        void process(const DAW::Host::Host* const host, AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) {
            for (int32_t i = 0; i < NUM_CHANNELS; ++i) {
                auto& channel = channels[i];
                float phase = channel.srcSync.getPhase(tick);
                channel.srcSync.shape.renderPhase = phase;
            }
        }
    };

    void module_lfo::initModChannels() {
        outputModChannelsDesc.clear();
        auto reg = registerParam(PARAM_LFO_RATE);
        reg->setInitial(0.5f);
        reg->name  = "Rate";
        reg->shortLabel  = "Rate";
        reg->unit  = "Ticks";
        reg = registerParam(PARAM_LFO_PHASE);
        reg->setInitial(0.0f);
        reg->name  = "Phase";
        reg->shortLabel  = "Phase";
        reg->unit  = "°";
        reg = registerParam(PARAM_LFO_MINIMUM);
        reg->setInitial(0.5f);
        reg->name  = "Minimum";
        reg->shortLabel  = "Min";
        reg->unit  = "";
        reg->isBiPolar = true;
        reg = registerParam(PARAM_LFO_MAXIMUM);
        reg->setInitial(1.0f);
        reg->name  = "Maximum";
        reg->shortLabel  = "Max";
        reg->unit  = "";
        reg->isBiPolar = true;
        impl->setSyncFlags(0, TRIPLET|DOTTED|STRAIGHT);
        outputModChannelsDesc.push_back({0, "LFO 0"});
    }

    void module_lfo::process(const DAW::Host::Host* const host, AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) {
        impl->process(host, in, out, tick, samplePos, numSamples, state);
        internalplugin::process(host, in, out, tick, samplePos, numSamples, state);
    }

    module_lfo::module_lfo(int32_t _projectGlobalId, IHostCallback* _hostCallback)
        : internal_modulator("LFO", _projectGlobalId, _hostCallback),
        impl(new lfo_impl_t{ DawInstance::getOptional(), this, DAW::Shape::GetShapeSaw(DAW::Shape::SHAPE_SHAPED|DAW::Shape::SHAPE_CYCLIC) })
    {
        initModChannels();
    }

    module_lfo::~module_lfo() {
        delete impl;
    }

    DAW::Shape::shape_t& module_lfo::getShape(int idx) {
        return this->impl->getShape(idx);
    }

    const automated_param_t* module_lfo::getModulationOutputData(const DAW::modulation_channel_ref& channel) {
        return impl->getModulationOutputData(channel);
    }

    std::shared_ptr<std::vector<std::byte>> serializeSnapshot(const snapshot_t& snapshot) {
        dbgassert(snapshot.version == BINARY_SNAPSHOT_VERSION);
        auto shrdHeapVec = std::make_shared<std::vector<std::byte>>();
        shrdHeapVec->resize(256);
        DAW::ByteBuffer::stream_write<std::vector<std::byte>> out{*shrdHeapVec, 0};
        out.write(size_t(0));
        out.write(snapshot.version);
        out.write(size_t{snapshot.uiLayout.size()});
        out.write(size_t{snapshot.channels.size()});
        for (const auto& channel : snapshot.channels) {
            DAW::Shape::writeShape(out, channel.shape);
            out.write(channel.syncFlags);
            out.write(channel.modeIsShape);
            out.write(channel.modeRandom);
        }
        for (const auto& modulation : snapshot.uiLayout) {
            out.write(modulation.uiId);
            out.write(modulation.numActive);
        }
        out.setPos(0);
        out.write(size_t(shrdHeapVec->size()));
        return shrdHeapVec;
    }

    bool deserializeSnapshot(const std::shared_ptr<std::vector<std::byte>>& data, snapshot_t& snapshotOut) {
        if (!data)
            return false;
        DAW::ByteBuffer::stream_read in(*data);
        snapshot_t snapshot;
        size_t dataSize = data->size();
        size_t dataSizeHdr = 0;
        if (!in.read(dataSizeHdr))
            return false;
        if (dataSizeHdr > dataSize)
            return false;
        in.read(snapshot.version);
        // if (snapshot.version < MINIMUM_VERSION)
        //     return false;
        if (snapshot.version > BINARY_SNAPSHOT_VERSION)
            return false;
        size_t numUiLayouts = 0;
        if (!in.read(numUiLayouts) || numUiLayouts > 1000)
            return false;
        size_t numChannels = 0;
        if (snapshot.version > 1) {
            if (!in.read(numChannels) || numChannels > 1000)
                return false;
        }
        else {
            numChannels = 1;
        }
        snapshot.uiLayout.resize(numUiLayouts);
        snapshot.channels.resize(numChannels);
        if (snapshot.version > 1) {
            for (auto& channel : snapshot.channels) {
                if (!DAW::Shape::readShape(in, channel.shape))
                    return false;
                if (snapshot.version > 2) {
                    if (!in.read(channel.syncFlags))
                        return false;
                    if (snapshot.version > 3) {
                        if (!in.read(channel.modeIsShape))
                            return false;
                        if (!in.read(channel.modeRandom))
                            return false;
                    }
                } else {
                    bool dummy;
                    if (!in.read(dummy))
                        return false;
                    channel.syncFlags = TRIPLET | DOTTED | STRAIGHT;
                }
            }
        }
        else {
            // snapshot.channels.resize(1);
            snapshot.channels[0].shape = {0, {2, DAW::Shape::GetShapeSaw(DAW::Shape::SHAPE_SHAPED|DAW::Shape::SHAPE_CYCLIC)}};
            snapshot.channels[0].syncFlags = TRIPLET | DOTTED | STRAIGHT;
        }
        for (auto& modulation : snapshot.uiLayout) {
            if (!in.read(modulation.uiId))
                return false;
            if (!in.read(modulation.numActive))
                return false;
        }
        snapshotOut = std::move(snapshot);
        return true;
    }

    std::shared_ptr<std::vector<std::byte>> module_lfo::storePresetData() {
        snapshot_t snapshot;
        impl->getSnapshot(snapshot);
        getUiSnapshot(snapshot);
        return serializeSnapshot(snapshot);
    }

    bool module_lfo::loadPresetData(const std::shared_ptr<std::vector<std::byte>>& buf) {
        if (buf->size() > 0) {
            snapshot_t snapshotLoaded;
            if (deserializeSnapshot(buf, snapshotLoaded)) {
                impl->setSnapshot(snapshotLoaded);
                setUiSnapshot(snapshotLoaded);
                return true;
            }
        }
        return false;
    }

    class guictr_module_lfo final : public guictr_base {
        module_lfo* const module;
        std::vector<guiknob_pluginparam*> guiParams;
        std::vector<gui_slider_textfield*> guiParamsTextfields;
        guictr_vert_layout firstCtr;
        i_ctr_shape_editor* const shapeEditor;
        guictr_base* ctrShapeEditor;
        DAW::Shape::guictr_curve_shape* ctrShapeScope;
        DAW::Shape::shape_t shapeScope;
        gui_textfield editfield;
        void init() {
            setLayoutMode(LAYOUT_HORIZONTAL);
            editfield.setFlag(FLG_NO_LAYOUT, true);
            setBackgroundRendered(true);
            setCanMouseHit(true);
            padding = 4;
            margin  = 2;
            editfield.setVisible(false);
            editfield.setAlignment(gui_textfield::Alignment::Center);
            editfield.setReturnCommits(true);
        }
    public:
        explicit guictr_module_lfo(module_lfo* module) : guictr_base(),
            module(module),
            shapeEditor(makeShapeEditor()),
            ctrShapeScope(DAW::Shape::makeShapeCurveView())
        {
            init();
            std::vector<automatable_param_t*> paramsSorted;
            module->getSortedParams(paramsSorted);
            {
                auto p = module->getParam(PARAM_LFO_RATE);
                auto gui = new guiknob_pluginparam(p->idx, p->idx, guiknob::knobtype::SLIDER_LABELED);
                gui->setAutomationRef(module, p->idx);
                firstCtr.addElement({0.5f, gui});
                guiParams.push_back(gui);
            }
            {
                auto p = module->getParam(PARAM_LFO_PHASE);
                auto gui = new guiknob_pluginparam(p->idx, p->idx, guiknob::knobtype::KNOB_LABELED);
                gui->setAutomationRef(module, p->idx);
                firstCtr.addElement({0.2f, gui});
                guiParams.push_back(gui);
            }
            auto minMaxCtr = new guictr_vert_layout();
            {
                auto p = module->getParam(PARAM_LFO_MINIMUM);
                auto gui = new gui_slider_textfield();
                gui->setLabel(p->shortLabel);
                gui->setFlag(FLG_RENDER_LABEL, true);
                gui->setAutomationRef(module, p->idx);
                minMaxCtr->addElement({0.5f, gui});
                guiParamsTextfields.push_back(gui);
            }
            {
                auto p = module->getParam(PARAM_LFO_MAXIMUM);
                auto gui = new gui_slider_textfield();
                gui->setLabel(p->shortLabel);
                gui->setFlag(FLG_RENDER_LABEL, true);
                gui->setAutomationRef(module, p->idx);
                minMaxCtr->addElement({0.5f, gui});
                guiParamsTextfields.push_back(gui);
            }
            firstCtr.addElement({0.15f, minMaxCtr});
            firstCtr.addElement({0.15f, new DAW::UI::Modulation::guibutton_modulate(module->getModulationChannel(0))});
            add(&firstCtr);
            shapeEditor->setShapeEditorShapeRef(&module->getShape(0));
            shapeEditor->setShapeEditorCallback([module=this->module](const DAW::Shape::shape_t& shape, bool bIsDragMove) -> void {
                auto lock = module->impl->lock();
                auto& synthShape = module->getShape(0);
                synthShape.pts = shape.pts;
                synthShape.eraseDuplicates();
            });
            ctrShapeEditor = shapeEditor->getGuiContainer();
            ctrShapeEditor->setFlag(FLG_NO_LAYOUT, true);
            ctrShapeEditor->setBackgroundRendered(false);
            ctrShapeEditor->setBackgroundRenderedInset(false);
            ctrShapeEditor->setCanMouseHit(false);
            ctrShapeEditor->id = 2;
            ctrShapeEditor->margin = 0;
            ctrShapeEditor->padding = 2;
            ctrShapeScope->setFlag(FLG_NO_LAYOUT, true);
            ctrShapeScope->setBackgroundRendered(false);
            ctrShapeScope->setBackgroundRenderedInset(false);
            ctrShapeScope->id = 3;
            ctrShapeScope->margin = 0;
            ctrShapeScope->padding = 2;
            add(ctrShapeEditor);
            add(&editfield);
        }

        ~guictr_module_lfo() override {
            remove(&editfield);
            remove(&firstCtr);
            remove(ctrShapeEditor);
            remove(ctrShapeScope);
            destroyGuis();
            delete ctrShapeEditor;
            delete ctrShapeScope;
        }

        void layout() override {
            auto cs = getSizeContent() - ivec2(padding, 0);
            auto leftSize = math::clamp<int32_t>(math::min(math::roundfS32(cs.y * 0.2f), math::roundfS32(cs.y * 0.2f)), 16, 128);
            firstCtr.size        = { leftSize, cs.y };
            ctrShapeEditor->size = { cs.x - leftSize, cs.y };
            ctrShapeEditor->pos  = { cs.x - ctrShapeEditor->size.x, 0 };
            this->ctrShapeScope->pos = this->ctrShapeEditor->pos;
            this->ctrShapeScope->size = this->ctrShapeEditor->size;
            for (auto gui : guis) {
                gui->layout();
            }
        }

        void setMode(bool bIsShape) {
            bool bChanged = false;
            if (bIsShape) {
                if (ctrShapeScope->parent) {
                    remove(ctrShapeScope);
                    bChanged = true;
                }
                if (!ctrShapeEditor->parent) {
                    add(ctrShapeEditor);
                    bChanged = true;
                }
            } else {
                if (ctrShapeEditor->parent) {
                    remove(ctrShapeEditor);
                    bChanged = true;
                }
                if (!ctrShapeScope->parent) {
                    add(ctrShapeScope);
                    bChanged = true;
                }
            }
            if (bChanged) {
                onChildLayoutChanged(this);
            }
        }
        class ctxtmenu_lfo_base : public ctxtmenu_entry {
        protected:
            module_lfo* const module;
            int32_t channel;
            int32_t perRowEntries;

            struct _entry {
                int id;
                int x;
                int y;
                int w;
                String name;
            };
            std::vector<_entry> entries;

        public:
            const int pad   = 10;
            const int inset = 5;
        public:
            ctxtmenu_lfo_base(module_lfo* _module, int32_t _channel, String _title, int _id)
                : ctxtmenu_entry(std::move(_title), _id),
                module(_module), channel(_channel)
            {
            }

            void layout(ivec2 size, float _fontSize, determine_string_width& strw) override {
                width = size.x;
                this->fontSize = _fontSize;
                const int h    = math::roundfS32(_fontSize);
                layoutE(width, h, perRowEntries);
            }

            void layoutE(int tw, int h, int perRow) {
                int iX      = inset;
                int iY      = h + 2;
                int elW     = (tw - inset * 2) / perRow;
                for (auto& e : entries) {
                    this->height = iY + h;
                    e.x = iX;
                    e.y = iY;
                    e.w = elW;
                    iX += e.w;
                    if (iX >= tw - inset * 2) {
                        iX = inset;
                        iY += h;
                    }
                }
            }

            bool contains(ivec2& ctxtSize, ivec2& mouse) const override {
                return mouse.y >= y && mouse.y < y + height && mouse.x >= 0 && mouse.x < ctxtSize.x;
            }

            int getClicked(ivec2& ctxtSize, ivec2& mouse) override {
                if (contains(ctxtSize, mouse)) {
                    const auto h = this->fontSize;
                    for (auto& e : entries) {
                        if (mouse.y >= y + e.y && mouse.y < y + e.y + h && mouse.x >= 0 && mouse.x < e.x + e.w) {
                            return this->id + e.id;
                        }
                    }
                }
                return -1;
            }
        };

        class ctxtmenu_lfo_sync final : public ctxtmenu_lfo_base {
        public:
            ctxtmenu_lfo_sync(module_lfo* _module, int32_t _channel, String _title, int _id)
                : ctxtmenu_lfo_base(_module, _channel, _title, _id)
            {
                this->id = 100;
                entries.push_back({ NoteRatio::STRAIGHT, 0, 0, 0, "Straight" });
                entries.push_back({ NoteRatio::TRIPLET, 0, 0, 0, "Triplet" });
                entries.push_back({ NoteRatio::DOTTED, 0, 0, 0, "Dotted" });
                entries.push_back({ 0, 0, 0, 0, "Off" });
                perRowEntries = 3;
            }


            void render(ivec2, NVGcontext* vg, int, ivec2 mouse) override {
                auto h = fontSize * 1.1f;

                int32_t sync = module->getSyncRatio(channel);
                for (auto& e : entries) {
                    if (mouse.y >= y + e.y && mouse.y < y + e.y + h && mouse.x >= e.x && mouse.x < e.x + e.w) {
                        nvgBeginPath(vg);
                        nvgRect(vg, e.x, y + e.y + 2, e.w, h - 4);
                        nvgFillColor(vg, theme->getColor(GuiColor::COL_CTXTMNU_HILIGHT));
                        nvgFill(vg);
                    }
                    if ((e.id&sync) || (e.id == 0 && sync == 0)) {
                        nvgBeginPath(vg);
                        nvgCircle(vg, e.x + 10, y + e.y + h / 2, 4);
                        nvgFillColor(vg, theme->getColor(GuiColor::COL_TEXT));
                        nvgFill(vg);
                    }
                }

                renderTextLabel(vg,
                                vec2(leftOffset(), y + h * 0.5f),
                                vec2(width, h),
                                title,
                                theme,
                                fontSize,
                                theme->getColor(GuiColor::COL_TEXT),
                                NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

                for (auto& e : entries) {
                    renderTextLabel(vg,
                                    vec2(e.x + 20.0f, y + e.y + h * 0.5f),
                                    vec2(width, h),
                                    e.name,
                                    theme,
                                    fontSize * 0.9f,
                                    theme->getColor(GuiColor::COL_TEXT),
                                    NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
                }
            }
        };

        class ctxtmenu_lfo_mode final : public ctxtmenu_lfo_base {
        public:
            ctxtmenu_lfo_mode(module_lfo* _module, int32_t _channel, String _title, int _id)
                : ctxtmenu_lfo_base(_module, _channel, _title, _id)
            {
                this->id = 200;
                entries.push_back({ 0, 0, 0, 0, "Shape" });
                entries.push_back({ 1, 0, 0, 0, "Random" });
                perRowEntries = 2;
            }

            void render(ivec2, NVGcontext* vg, int, ivec2 mouse) override {
                auto h = fontSize * 1.1f;

                int32_t isShapeMode = module->isShapeMode(channel);
                for (auto& e : entries) {
                    if (mouse.y >= y + e.y && mouse.y < y + e.y + h && mouse.x >= e.x && mouse.x < e.x + e.w) {
                        nvgBeginPath(vg);
                        nvgRect(vg, e.x, y + e.y + 2, e.w, h - 4);
                        nvgFillColor(vg, theme->getColor(GuiColor::COL_CTXTMNU_HILIGHT));
                        nvgFill(vg);
                    }
                    if ((e.id == 0) == isShapeMode) {
                        nvgBeginPath(vg);
                        nvgCircle(vg, e.x + 10, y + e.y + h / 2, 4);
                        nvgFillColor(vg, theme->getColor(GuiColor::COL_TEXT));
                        nvgFill(vg);
                    }
                }

                renderTextLabel(vg,
                                vec2(leftOffset(), y + h * 0.5f),
                                vec2(width, h),
                                title,
                                theme,
                                fontSize,
                                theme->getColor(GuiColor::COL_TEXT),
                                NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

                for (auto& e : entries) {
                    renderTextLabel(vg,
                                    vec2(e.x + 20.0f, y + e.y + h * 0.5f),
                                    vec2(width, h),
                                    e.name,
                                    theme,
                                    fontSize * 0.9f,
                                    theme->getColor(GuiColor::COL_TEXT),
                                    NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
                }
            }
        };

        class ctxtmenu_lfo_random_mode final : public ctxtmenu_lfo_base {
        public:
            ctxtmenu_lfo_random_mode(module_lfo* _module, int32_t _channel, String _title, int _id)
                : ctxtmenu_lfo_base(_module, _channel, _title, _id)
            {
                this->id = 300;
                entries.push_back({ 0, 0, 0, 0, "Smooth" });
                entries.push_back({ 1, 0, 0, 0, "Linear" });
                entries.push_back({ 2, 0, 0, 0, "Exponential" });
                entries.push_back({ 3, 0, 0, 0, "Sample & Hold" });
                perRowEntries = 2;
            }

            void render(ivec2, NVGcontext* vg, int, ivec2 mouse) override {
                auto h = fontSize * 1.1f;

                int32_t randomMode = module->getRandomMode(channel);
                for (auto& e : entries) {
                    if (mouse.y >= y + e.y && mouse.y < y + e.y + h && mouse.x >= e.x && mouse.x < e.x + e.w) {
                        nvgBeginPath(vg);
                        nvgRect(vg, e.x, y + e.y + 2, e.w, h - 4);
                        nvgFillColor(vg, theme->getColor(GuiColor::COL_CTXTMNU_HILIGHT));
                        nvgFill(vg);
                    }
                    if (e.id == randomMode) {
                        nvgBeginPath(vg);
                        nvgCircle(vg, e.x + 10, y + e.y + h / 2, 4);
                        nvgFillColor(vg, theme->getColor(GuiColor::COL_TEXT));
                        nvgFill(vg);
                    }
                }

                renderTextLabel(vg,
                                vec2(leftOffset(), y + h * 0.5f),
                                vec2(width, h),
                                title,
                                theme,
                                fontSize,
                                theme->getColor(GuiColor::COL_TEXT),
                                NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

                for (auto& e : entries) {
                    renderTextLabel(vg,
                                    vec2(e.x + 20.0f, y + e.y + h * 0.5f),
                                    vec2(width, h),
                                    e.name,
                                    theme,
                                    fontSize * 0.9f,
                                    theme->getColor(GuiColor::COL_TEXT),
                                    NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
                }
            }
        };

        class ctxtmenu_lfo_shape_select final : public ctxtmenu_entry {
            struct _shape_sel_entry {
                DAW::Shape::ShapeWaveform shape;
                int x;
                int y;
                int w;
                String name;
            };
            std::vector<_shape_sel_entry> entries;

        public:
            const int pad   = 10;
            const int inset = 5;
        public:
            ctxtmenu_lfo_shape_select(String _title, int _id)
                : ctxtmenu_entry(std::move(_title), _id)
            {
                this->id = 400;
                using DAW::Shape::ShapeWaveform;
                entries.push_back({ ShapeWaveform::SHAPE_SINE, 0, 0, 0, "Sine" });
                entries.push_back({ ShapeWaveform::SHAPE_TRIANGLE, 0, 0, 0, "Triangle" });
                entries.push_back({ ShapeWaveform::SHAPE_SAW, 0, 0, 0, "Saw" });
                entries.push_back({ ShapeWaveform::SHAPE_SQUARE, 0, 0, 0, "Square" });
                entries.push_back({ ShapeWaveform::SHAPE_SINE_INV, 0, 0, 0, "Sine Inv" });
                entries.push_back({ ShapeWaveform::SHAPE_TRIANGLE_INV, 0, 0, 0, "Triangle Inv" });
                entries.push_back({ ShapeWaveform::SHAPE_SAW_INV, 0, 0, 0, "Saw Inv" });
                entries.push_back({ ShapeWaveform::SHAPE_SQUARE_INV, 0, 0, 0, "Square Inv" });
            }

            void layout(ivec2 size, float _fontSize, determine_string_width& strw) override {
                width = size.x;
                this->fontSize = _fontSize;
                const int h    = math::roundfS32(_fontSize);
                layoutE(width, h, 4);
            }

            void layoutE(int tw, int h, int perRow) {
                int iX      = inset;
                int iY      = h + 2;
                int elW     = (tw - inset * 2) / perRow;
                for (_shape_sel_entry& e : entries) {
                    this->height = iY + h;
                    e.x = iX;
                    e.y = iY;
                    e.w = elW;
                    iX += e.w;
                    if (iX >= tw - inset * 2) {
                        iX = inset;
                        iY += h;
                    }
                }
            }

            void render(ivec2, NVGcontext* vg, int, ivec2 mouse) override {
                auto h = fontSize * 1.1f;

                renderTextLabel(vg,
                                vec2(leftOffset(), y + h * 0.5f),
                                vec2(width, h),
                                title,
                                theme,
                                fontSize,
                                theme->getColor(GuiColor::COL_TEXT),
                                NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
                for (_shape_sel_entry& e : entries) {
                    if (mouse.y >= y + e.y && mouse.y < y + e.y + h && mouse.x >= e.x && mouse.x < e.x + e.w) {
                        nvgBeginPath(vg);
                        nvgRect(vg, e.x, y + e.y + 2, e.w, h - 4);
                        nvgFillColor(vg, theme->getColor(GuiColor::COL_CTXTMNU_HILIGHT));
                        nvgFill(vg);
                    }
                    int inset = 4;
                    vec2 waveformPos = vec2(e.x, y + e.y) + vec2(inset, inset);
                    vec2 waveformSize = vec2(e.w, h) - vec2(inset * 2, inset * 2);
                    drawWaveform(vg, waveformPos, waveformSize, e.shape, theme->getColor(GuiColor::COL_TEXT));
                }
            }

            bool contains(ivec2& ctxtSize, ivec2& mouse) const override {
                return mouse.y >= y && mouse.y < y + height && mouse.x >= 0 && mouse.x < ctxtSize.x;
            }

            int getClicked(ivec2& ctxtSize, ivec2& mouse) override {
                if (contains(ctxtSize, mouse)) {
                    const auto h = this->fontSize;
                    for (_shape_sel_entry& e : entries) {
                        if (mouse.y >= y + e.y && mouse.y < y + e.y + h && mouse.x >= 0 && mouse.x < e.x + e.w) {
                            return this->id + e.shape;
                        }
                    }
                }
                return -1;
            }
        };
        class guictr_module_lfo_context_menu final : public guictxtmenu {
            module_lfo* const module;
            int32_t channel;
        public:
            explicit guictr_module_lfo_context_menu(module_lfo* _module, int32_t _channel)
                : guictxtmenu(), module(_module), channel(_channel) 
            {
                this->size.x   = 220;
                maxHeight = 0;
                this->fontSize = FONT_SIZE_CTXT_SMALL;
                this->paddingV = 0;
                addEntry(new ctxtmenu_lfo_sync(module, channel, "Sync", 0));
                addEntry(new ctxtmenu_lfo_mode(module, channel, "Mode", 1));
                addEntry(new ctxtmenu_lfo_shape_select("Shape", 2));
                addEntry(new ctxtmenu_lfo_random_mode(module, channel, "Random", 3));
            }
            bool clickedElement(ctxtmenu_entry* e, int _id) override {
                if (_id >= 400) {
                    using DAW::Shape::ShapeWaveform;
                    auto shapeIdx = _id - 400;
                    if (shapeIdx < 0 || shapeIdx > ShapeWaveform::SHAPE_SQUARE_INV) {
                        return false;
                    }
                    auto waveform = static_cast<ShapeWaveform>(shapeIdx);
                    auto lock = module->impl->lock();
                    auto& shape = module->getShape(channel);
                    shape.pts = GetShape(waveform);
                    module->setShapeMode(channel);
                } else if (_id >= 300) {
                    auto randomIdx = _id - 300;
                    auto lock = module->impl->lock();
                    module->setRandomMode(channel, randomIdx);
                } else if (_id >= 200) {
                    auto lock = module->impl->lock();
                    if (_id == 200) {
                        module->setShapeMode(channel);
                    } else {
                        module->setRandomMode(channel);
                    }
                } else if (_id >= 100) {
                    auto lock = module->impl->lock();
                    int flags = module->getSyncRatio(channel);
                    int clicked = _id - 100;
                    if (clicked == 0) {
                        flags = 0;
                    } else {
                        if (flags & clicked) {
                            flags &= ~clicked;
                        } else {
                            flags |= clicked;
                        }
                    }
                    module->setSyncRatio(0, flags);
                    return true;
                }
                closeContextMenu();
                return true;
            }
        };

        void rightClicked(MouseEvent& evt, guibase* what) override {
            parentCtrl->openContextMenu(new guictr_module_lfo_context_menu(module, 0), evt.mousepos);
        }

        void getSizeScale(int& w, int& h) const {
            w = 350;
            h = 300;
        }

        void layoutEntries(ivec2 pos, ivec2 cs, ivec2 dir) override {
            auto shapeWidth = cs.x > cs.y ? cs.y : cs.x*2/3;
            this->ctrShapeEditor->pos = {cs.x-shapeWidth, 0};
            this->ctrShapeEditor->size = {shapeWidth, cs.y};
            this->ctrShapeScope->pos = this->ctrShapeEditor->pos;
            this->ctrShapeScope->size = this->ctrShapeEditor->size;
            guictr_base::layoutEntries({}, {cs.x-shapeWidth, cs.y}, dir);
        }

        void buttonClicked(guibase* button) override {
            auto param = dynamic_cast<guiknob_pluginparam*>(button);
            if (param && module) {
                auto paramIdx = param->getParamIdx();
                auto paramValue = module->getParamValueDisplay(paramIdx);
                editfield.mCallbackEnd = [this, param, paramValue, paramIdx](const std::string& str) {
                    auto paramConverted = module->convertParamValueDisplay(param->getParamIdx(), param_unit_t{str, paramValue.unit});
                    if (paramConverted.success) {
                        module->setParamValue(paramIdx, paramConverted.floatVal, FLG_PAR_UPDATE_USER | FLG_PAR_UPDATE_FINISH);
                        if (param->fnValueEditChanged)
                            param->fnValueEditChanged(param->getValue(), paramConverted.floatVal);
                    }
                    editfield.setVisible(false);
                    return true;
                };
                auto layout = param->getLayout();
                editfield.pos = layout.pValue;
                editfield.size = layout.sValue;
                editfield.setVisible(true);
                editfield.layout();
                editfield.setValue(paramValue.value);
                editfield.setSelectionRange(-1, -1);
                editfield.setFontSize(layout.valueHeight*layout.fontScaleValue);
                parentCtrl->focusGui(&editfield);
                return;
            }
            guictr_base::buttonClicked(button);
        }

        void onGuiOpen() {
            for (auto knob : guiParams) {
                knob->setEffectInstance(module);
            }
        }

        void onGuiClose() {
            for (auto knob : guiParams) {
                knob->setEffectInstance(nullptr);
            }
        }

        void onSetParameter(int32_t index, float value) {
        }

        void setUiLayout(const ui_layout_t& layout) {
        }

        bool getUiLayout(ui_layout_t& layout) const {
            return true;
        }

        void prerender(NVGcontext* vg) override {
            guictr_base::prerender(vg);
            if (!module->isShapeMode(0)) {
                auto& shape = ctrShapeScope->getShape();
                auto numPoints = samplecount_t(math::clamp(size.x, 16, 1024));
                shape.pts.resize(numPoints);
                shape.flags |= DAW::Shape::ShapeFlags::SHAPE_LOCK_POINTS;
                shape.flags |= DAW::Shape::ShapeFlags::SHAPE_UNCLAMPPED;
                auto daw = dawCtrl->getDaw();
                
                auto tick = !daw->isPlaying() ? daw->getIdleTickPos() : daw->getPlaybackPos();
                tick_t range = TICKS_BAR;
                auto begin = tick - range;
                for (samplecount_t j = 0; j < samplecount_t(numPoints); ++j) {
                    auto t = begin + (j * range) / numPoints;
                    auto v = module->impl->channels[0].srcRand->sampleCurve(t);
                    auto normalizedT = (t - begin) / float(range);
                    auto& pt = shape.pts[j];
                    pt.pos.x = normalizedT;
                    pt.pos.y = v;
                }
            }
        }
    };

    using ViewCtrType = PluginViewContainerBasic<guictr_module_lfo, module_lfo>;
    std::shared_ptr<PluginViewContainer> module_lfo::createViewCtrInternal() {
        auto ctr = std::make_shared<ViewCtrType>(this, 100, 150);
        ctr->getPluginUI().setMode(isShapeMode(0));
        return ctr;
    }

    void module_lfo::getUiSnapshot(snapshot_t& snapshot) {
        for (auto& view : views) {
            auto implCtrType = dynamic_cast<ViewCtrType*>(view.get());
            ui_layout_t layout{};
            if (implCtrType && implCtrType->getPluginUI().getUiLayout(layout)) {
                layout.uiId = view->getUiId();
                // see if snapshot is already there
                auto it = std::find_if(snapshot.uiLayout.begin(), snapshot.uiLayout.end(), [&](const ui_layout_t& layout) {
                    return layout.uiId == view->getUiId();
                });
                if (it != snapshot.uiLayout.end()) {
                    *it = layout;
                } else {
                    snapshot.uiLayout.push_back(layout);
                }
            }
        }
    }

    void module_lfo::setUiSnapshot(snapshot_t& snapshot) {
        for (auto& uis : snapshot.uiLayout) {
            std::vector<std::shared_ptr<PluginViewContainer>> views;
            getAllViewCtrs(uis.uiId, views);
            for (auto& view : views) {
                auto implCtrType = dynamic_cast<ViewCtrType*>(view.get());
                if (implCtrType) {
                    implCtrType->getPluginUI().setUiLayout(uis);
                }
            }
        }
    }

    param_converted_t module_lfo::convertParamValueDisplay(int32_t idx, const param_unit_t& displayValue) {
        //TODO: use std::from_chars when floating point version arrives in libc++
        auto fTextFieldVal = static_cast<float>(atof(StringAsCStr(displayValue.value)));
        switch (idx) {
            case PARAM_LFO_RATE: {
                auto& first = impl->channels[0];
                if (first.syncFlags) {
                    auto numSyncRatios = CtrSize(first.syncRatios);
                    for (int32_t i = 0; i < numSyncRatios; ++i) {
                        if (first.syncRatios[i].text == displayValue.value) {
                            return {((i)/float(numSyncRatios-1)), true};
                        }
                        if (first.syncRatios[i].text == displayValue.value + "/1") {
                            return {((i)/float(numSyncRatios-1)), true};
                        }
                    }
                } else {
                    return {math::clamp(RateToParam(fTextFieldVal), 0.0f, 1.0f), true};
                }
                // float syncTicks = GetSyncRate(impl->getIsSync(), getParamValue(idx));
                // return {math::clamp(fPow, 0.0f, 1.0f), true};
                break;
            }
            case PARAM_LFO_MINIMUM:
            case PARAM_LFO_MAXIMUM: {
                return {math::clamp(fTextFieldVal * 0.5f + 0.5f, 0.0f, 1.0f), true};
            }
            case PARAM_LFO_PHASE: {
                return {math::clamp(fTextFieldVal / 360.0f, 0.0f, 1.0f), true};
            }
            default:
                break;
        }
        return internalplugin::convertParamValueDisplay(idx, displayValue);
    }

    param_unit_t module_lfo::convertParamValueToDisplay(int32_t idx, float value) {
        auto param = getParam(idx);
        dbgassert(param);
        if (param->idx == PARAM_LFO_RATE) {
            auto& firstInstance = impl->channels[0];
            auto lfoRateStr = FormatSyncRate(firstInstance.syncRatios, firstInstance.syncFlags, value);
            return {lfoRateStr, impl->getSyncFlags(0) ? "" : param->unit};
        }
        if (param->idx == PARAM_LFO_PHASE) {
            return {StringFormat("%.2f", value*360.0f), param->unit};
        }
        if (param->idx == PARAM_LFO_MINIMUM || param->idx == PARAM_LFO_MAXIMUM) {
            return {StringFormat("%.2f", value*2.0f-1.0f), param->unit};
        }
        return internalplugin::convertParamValueToDisplay(idx, value);
    }
    int32_t module_lfo::getSyncRatio(int32_t chIdx) const {
        return impl->getSyncFlags(chIdx); 
    }
    bool module_lfo::isShapeMode(int32_t chIdx) const {
        return impl->channels[chIdx].modeIsShape;
    }
    void module_lfo::setSyncRatio(int32_t chIdx, int32_t ratio) {
        impl->setSyncFlags(chIdx, ratio);
    }
    void module_lfo::setShapeMode(int32_t chIdx) {
        impl->channels[chIdx].modeIsShape = true;
        for (auto& view : views) {
            auto implCtrType = dynamic_cast<ViewCtrType*>(view.get());
            if (implCtrType) {
                implCtrType->ctr_main.setMode(true);
            }
        }
    }
    void module_lfo::setRandomMode(int32_t chIdx, int32_t mode) {
        impl->setRandomMode(chIdx, mode);
        for (auto& view : views) {
            auto implCtrType = dynamic_cast<ViewCtrType*>(view.get());
            if (implCtrType) {
                implCtrType->ctr_main.setMode(false);
            }
        }
    }
    int32_t module_lfo::getRandomMode(int32_t chIdx) const {
        return impl->getRandomMode(chIdx);
    }
}// namespace PluginLFO

template<>
effectbase* makeInstance<PluginLFO::module_lfo>(int32_t _projectGlobalId, IHostCallback* _hostCallback) {
    return new PluginLFO::module_lfo(_projectGlobalId, _hostCallback);
}

#include "eq-plugin.h"
#include "assert_dbg.h"
#include "host/automation/automation.h"
#include "dsp_util.h"
#include "event.h"
#include "plugins/plugin-ui.h"
#include "plugins/plugincontrol.h"
#include "seq_time.h"
#include "str_util.h"
#include "gui/container/container.h"
#include "gui/controls/knoblabeled.h"
#include "gui/controls/knobpluginparam.h"
#include "gui/plugin/plugin.h"
#include "gui/plugin/pluginctr.h"
#include "gui/plugin/pluginviewcontainers.h"
#include "host/plugin/modules.h"
#include "host/daw/mainctrl.h"
#include "host/plugin/internal/internal-plugin.h"
#include "host/track/track.h"
#include "host/track/track_impl.h"
#include "host/audiobuffer/audioblock.h"
#include "host/meter/meter.h"
#include "snapshot/snapshot.h"
#include "types.h"
#include "window.h"
#include "dsp_util.h"
#include <algorithm>
#include <memory>
#include <vector>

namespace PluginEQ {

    enum BandType {
        BandTypePeak,
        BandTypeLowShelf,
        BandTypeHighShelf,
        BandTypeLowPass,
        BandTypeHighPass,
        BandTypeBandPass,
        BandTypeNotch,
        BandTypeAllPass,
        NumBandTypes
    };

    struct band_t {
        float freq    = 1000.0;
        float gain    = 1.0;
        float q       = 1.0;
        float bw      = 1.0;
        BandType type = BandTypePeak;
    };
    constexpr static std::array<band_t, 10> defaultBands = {{
        { 32.0, 0.0, 0.707, 1.0, BandTypeLowShelf },
        { 64.0, 0.0, 0.707, 1.0, BandTypePeak },
        { 125.0, 0.0, 0.707, 1.0, BandTypePeak },
        { 250.0, 0.0, 0.707, 1.0, BandTypePeak },
        { 500.0, 0.0, 0.707, 1.0, BandTypePeak },
        { 1000.0, 0.0, 0.707, 1.0, BandTypePeak },
        { 2000.0, 0.0, 0.707, 1.0, BandTypePeak },
        { 4000.0, 0.0, 0.707, 1.0, BandTypePeak },
        { 8000.0, 0.0, 0.707, 1.0, BandTypePeak },
        { 16000.0, 0.0, 0.707, 1.0, BandTypeHighShelf },
    }};
    class EQFilter;
    struct impl_data_t {
        DAW::Host::process_scratch_buf_t buf;
        std::array<band_t, defaultBands.size()> bands = defaultBands;
        std::array<std::vector<std::shared_ptr<EQFilter>>, defaultBands.size()> filters;
        AudioBlock tmpBlock;
    };
    float freqToParam(float f) {
        return (f - 10.0f) / 40000.0f;
    }
    float paramToFreq(float p) {
        return p * 40000.0f + 10.0f;
    }
    module_eq::module_eq(int32_t _projectGlobalId, IHostCallback* _hostCallback)
        : internalplugin("EQ", getModuleType(), _projectGlobalId, _hostCallback),
        impl(new impl_data_t)
    {
        struct effectgain_param_entry {
            int32_t id;
            String name;
            String unit;
            float val;
        };
        const std::array<effectgain_param_entry, 2> parameterTypes{ {
                { PARAM_GAIN, "Gain", "dB", dsp_util::gainToLinScaleWithRange(1.0f, MTR_CEIL, DBFS_MUTE_POS) },
                { PARAM_PAN,  "Pan",  "", 0.5f }
        } };
        for (const auto& paramEntry : parameterTypes) {
            registerParam(paramEntry.id)->initValue(paramEntry);
        }
        getParam(PARAM_TRACK_PAN)->isBiPolar = true;
        const int PARAMID_FIRST_BAND = 16;
        const int PER_BAND_PARAMS = 16;
        for (size_t i = 0; i < impl->bands.size(); ++i) {
            const auto& band = impl->bands[i];
            const int paramId = PARAMID_FIRST_BAND + i * PER_BAND_PARAMS;
            auto paramGain = registerParam(paramId);
            paramGain->initValue(effectgain_param_entry{
                paramId,
                String("Band ") + std::to_string(i + 1),
                "dB",
                dsp_util::gainToLinScaleWithRange(band.gain, MTR_CEIL, DBFS_MUTE_POS)
            });
            paramGain->isBiPolar = true;
            auto paramFreq = registerParam(paramId + 1);
            paramFreq->initValue(effectgain_param_entry{
                paramId + 1,
                String("Band ") + std::to_string(i + 1) + " Freq",
                "Hz",
                freqToParam(band.freq)
            });
            auto paramQ = registerParam(paramId + 2);
            paramQ->initValue(effectgain_param_entry{
                paramId + 2,
                String("Band ") + std::to_string(i + 1) + " Q",
                "%",
                band.q
            });
            auto paramBW = registerParam(paramId + 3);
            paramBW->initValue(effectgain_param_entry{
                paramId + 3,
                String("Band ") + std::to_string(i + 1) + " BW",
                "%",
                band.bw
            });
            auto paramType = registerParam(paramId + 4);
            paramType->initValue(effectgain_param_entry{
                paramId + 4,
                String("Band ") + std::to_string(i + 1) + " Type",
                "",
                static_cast<float>(band.type)
            });
        }
    }
    module_eq::~module_eq() {
        delete impl;
    }
    class EQFilter {
        using FPType = float;
        std::array<double,16> state{};
    public:
        void eq(BandType type, float* buf, samplecount_t len, double freq, double gain, double q, double bw, double sampleRate) {
            switch (type) {
                case BandTypePeak:
                    peak(buf, len, freq, gain, q, sampleRate);
                    break;
                case BandTypeLowShelf:
                    lowShelf(buf, len, freq, gain, q, sampleRate);
                    break;
                case BandTypeHighShelf:
                    highShelf(buf, len, freq, gain, q, sampleRate);
                    break;
                case BandTypeLowPass:
                    lowPass(buf, len, freq, gain, bw, sampleRate);
                    break;
                case BandTypeHighPass:
                    highPass(buf, len, freq, gain, bw, sampleRate);
                    break;
                case BandTypeBandPass:
                    bandPass(buf, len, freq, gain, bw, sampleRate);
                    break;
                case BandTypeNotch:
                    notch(buf, len, freq, gain, bw, sampleRate);
                    break;
                case BandTypeAllPass:
                    allPass(buf, len, freq, gain, bw, sampleRate);
                    break;
                default:
                    break;
            }
        }
        void peak(float* buf, samplecount_t len, double freq, double gain, double q, double sampleRate) {
            const double w0 = 2.0 * M_PI * freq / sampleRate;
            const double alpha = std::sin(w0) / (2.0 * q);
            const double A = std::pow(10.0, gain / 40.0);
            const double a0 = 1.0 + alpha / A;
            const double a1 = -2.0 * std::cos(w0);
            const double a2 = 1.0 - alpha / A;
            const double b0 = 1.0 + alpha * A;
            const double b1 = -2.0 * std::cos(w0);
            const double b2 = 1.0 - alpha * A;
            for (samplecount_t i = 0; i < len; ++i) {
                const double x = buf[i];
                const double y = (b0 / a0) * x + (b1 / a0) * state[0] + (b2 / a0) * state[1] - (a1 / a0) * state[2] - (a2 / a0) * state[3];
                state[1] = state[0];
                state[0] = x;
                state[3] = state[2];
                state[2] = y;
                buf[i] = y;
            }
        }
        void lowShelf(float* buf, samplecount_t len, double freq, double gain, double q, double sampleRate) {
            const double w0 = 2.0 * M_PI * freq / sampleRate;
            const double A = std::pow(10.0, gain / 40.0);
            const double alpha = std::sin(w0) / (2.0 * q);
            const double a0 = 1.0 + alpha / A;
            const double a1 = -2.0 * std::cos(w0);
            const double a2 = 1.0 - alpha / A;
            const double b0 = A * ((A + 1.0) - (A - 1.0) * std::cos(w0) + 2.0 * std::sqrt(A) * alpha);
            const double b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * std::cos(w0));
            const double b2 = A * ((A + 1.0) - (A - 1.0) * std::cos(w0) - 2.0 * std::sqrt(A) * alpha);
            for (samplecount_t i = 0; i < len; ++i) {
                const double x = buf[i];
                const double y = (b0 / a0) * x + (b1 / a0) * state[0] + (b2 / a0) * state[1] - (a1 / a0) * state[2] - (a2 / a0) * state[3];
                state[1] = state[0];
                state[0] = x;
                state[3] = state[2];
                state[2] = y;
                buf[i] = y;
            }
        }
        void highShelf(float* buf, samplecount_t len, double freq, double gain, double q, double sampleRate) {
            const double w0 = 2.0 * M_PI * freq / sampleRate;
            const double A = std::pow(10.0, gain / 40.0);
            const double alpha = std::sin(w0) / (2.0 * q);
            const double a0 = 1.0 + alpha / A;
            const double a1 = -2.0 * std::cos(w0);
            const double a2 = 1.0 - alpha / A;
            const double b0 = A * ((A + 1.0) + (A - 1.0) * std::cos(w0) + 2.0 * std::sqrt(A) * alpha);
            const double b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * std::cos(w0));
            const double b2 = A * ((A + 1.0) + (A - 1.0) * std::cos(w0) - 2.0 * std::sqrt(A) * alpha);
            for (samplecount_t i = 0; i < len; ++i) {
                const double x = buf[i];
                const double y = (b0 / a0) * x + (b1 / a0) * state[0] + (b2 / a0) * state[1] - (a1 / a0) * state[2] - (a2 / a0) * state[3];
                state[1] = state[0];
                state[0] = x;
                state[3] = state[2];
                state[2] = y;
                buf[i] = y;
            }
        }
        void lowPass(float* buf, samplecount_t len, double freq, double gain, double bw, double sampleRate) {
            const double w0 = 2.0 * M_PI * freq / sampleRate;
            const double alpha = std::sin(w0) * std::sinh(std::log(2.0) / 2.0 * bw * w0 / std::sin(w0));
            const double a0 = 1.0 + alpha;
            const double a1 = -2.0 * std::cos(w0);
            const double a2 = 1.0 - alpha;
            const double b0 = (1.0 - std::cos(w0)) / 2.0;
            const double b1 = 1.0 - std::cos(w0);
            const double b2 = (1.0 - std::cos(w0)) / 2.0;
            for (samplecount_t i = 0; i < len; ++i) {
                const double x = buf[i];
                const double y = (b0 / a0) * x + (b1 / a0) * state[0] + (b2 / a0) * state[1] - (a1 / a0) * state[2] - (a2 / a0) * state[3];
                state[1] = state[0];
                state[0] = x;
                state[3] = state[2];
                state[2] = y;
                buf[i] = y;
            }
        }
        void highPass(float* buf, samplecount_t len, double freq, double gain, double bw, double sampleRate) {
            const double w0 = 2.0 * M_PI * freq / sampleRate;
            const double alpha = std::sin(w0) * std::sinh(std::log(2.0) / 2.0 * bw * w0 / std::sin(w0));
            const double a0 = 1.0 + alpha;
            const double a1 = -2.0 * std::cos(w0);
            const double a2 = 1.0 - alpha;
            const double b0 = (1.0 + std::cos(w0)) / 2.0;
            const double b1 = -(1.0 + std::cos(w0));
            const double b2 = (1.0 + std::cos(w0)) / 2.0;
            for (samplecount_t i = 0; i < len; ++i) {
                const double x = buf[i];
                const double y = (b0 / a0) * x + (b1 / a0) * state[0] + (b2 / a0) * state[1] - (a1 / a0) * state[2] - (a2 / a0) * state[3];
                state[1] = state[0];
                state[0] = x;
                state[3] = state[2];
                state[2] = y;
                buf[i] = y;
            }
        }
        void bandPass(float* buf, samplecount_t len, double freq, double gain, double bw, double sampleRate) {
            const double w0 = 2.0 * M_PI * freq / sampleRate;
            const double alpha = std::sin(w0) * std::sinh(std::log(2.0) / 2.0 * bw * w0 / std::sin(w0));
            const double a0 = 1.0 + alpha;
            const double a1 = -2.0 * std::cos(w0);
            const double a2 = 1.0 - alpha;
            const double b0 = alpha;
            const double b1 = 0.0;
            const double b2 = -alpha;
            for (samplecount_t i = 0; i < len; ++i) {
                const double x = buf[i];
                const double y = (b0 / a0) * x + (b1 / a0) * state[0] + (b2 / a0) * state[1] - (a1 / a0) * state[2] - (a2 / a0) * state[3];
                state[1] = state[0];
                state[0] = x;
                state[3] = state[2];
                state[2] = y;
                buf[i] = y;
            }
        }
        void notch(float* buf, samplecount_t len, double freq, double gain, double bw, double sampleRate) {
            const double w0 = 2.0 * M_PI * freq / sampleRate;
            const double alpha = std::sin(w0) * std::sinh(std::log(2.0) / 2.0 * bw * w0 / std::sin(w0));
            const double a0 = 1.0 + alpha;
            const double a1 = -2.0 * std::cos(w0);
            const double a2 = 1.0 - alpha;
            const double b0 = 1.0;
            const double b1 = -2.0 * std::cos(w0);
            const double b2 = 1.0;
            for (samplecount_t i = 0; i < len; ++i) {
                const double x = buf[i];
                const double y = (b0 / a0) * x + (b1 / a0) * state[0] + (b2 / a0) * state[1] - (a1 / a0) * state[2] - (a2 / a0) * state[3];
                state[1] = state[0];
                state[0] = x;
                state[3] = state[2];
                state[2] = y;
                buf[i] = y;
            }
        }
        void allPass(float* buf, samplecount_t len, double freq, double gain, double bw, double sampleRate) {
            const double w0 = 2.0 * M_PI * freq / sampleRate;
            const double alpha = std::sin(w0) * std::sinh(std::log(2.0) / 2.0 * bw * w0 / std::sin(w0));
            const double a0 = 1.0 + alpha;
            const double a1 = -2.0 * std::cos(w0);
            const double a2 = 1.0 - alpha;
            const double b0 = 1.0 - alpha;
            const double b1 = -2.0 * std::cos(w0);
            const double b2 = 1.0 + alpha;
            for (samplecount_t i = 0; i < len; ++i) {
                const double x = buf[i];
                const double y = (b0 / a0) * x + (b1 / a0) * state[0] + (b2 / a0) * state[1] - (a1 / a0) * state[2] - (a2 / a0) * state[3];
                state[1] = state[0];
                state[0] = x;
                state[3] = state[2];
                state[2] = y;
                buf[i] = y;
            }
        }
        /* void peakingEQ(float* buf, samplecount_t len, double freq, double gain, double bw, double sampleRate) {
            const double w0 = 2.0 * M_PI * freq / sampleRate;
            const double alpha = std::sin(w0) * std::sinh(std::log(2.0) / 2.0 * bw * w0 / std::sin(w0));
            const double a0 = 1.0 + alpha;
            const double a1 = -2.0 * std::cos(w0);
            const double a2 = 1.0 - alpha;
            const double b0 = 1.0 + alpha * gain;
            const double b1 = -2.0 * std::cos(w0);
            const double b2 = 1.0 - alpha * gain;
            for (samplecount_t i = 0; i < len; ++i) {
                const double x = buf[i];
                const double y = (b0 / a0) * x + (b1 / a0) * state[0] + (b2 / a0) * state[1] - (a1 / a0) * state[2] - (a2 / a0) * state[3];
                state[1] = state[0];
                state[0] = x;
                state[3] = state[2];
                state[2] = y;
                buf[i] = y;
            }
        } */
    };
    void module_eq::process(const DAW::Host::Host* const host, AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) {
        dbgassert(in->samples == format.blockSize
                && out->samples == format.blockSize
                && format.blockSize > 0
                && format.sampleRate > 0);
        if (impl->tmpBlock.samples != format.blockSize || impl->tmpBlock.channels != out->channels) {
            impl->tmpBlock = AudioBlock(out->channels, format.blockSize);
        }

        auto* bufEqd = &impl->tmpBlock;
        bufEqd->copyFrom(in);
        auto channelCount = bufEqd->channels;
        for (size_t bandIdx = 0; bandIdx < impl->bands.size(); ++bandIdx) {
            const auto& band = impl->bands[bandIdx];
            auto filters = impl->filters[bandIdx];
            while (filters.size() < out->channels) {
                filters.emplace_back(std::make_shared<EQFilter>());
            }
            for (channelnum_t ch = 0; ch < channelCount; ++ch) {
                auto& filter = filters[ch];
                auto* buf = bufEqd->buf[ch];
                filter->eq(band.type, buf, bufEqd->samples, band.freq, band.gain, band.q, band.bw, format.sampleRate);
            }
        }
        const auto autParGain = DAW::GetParameterModulationFromRouting(pluginMgr, DAW::GetRoutingFromDestinationParam(this, PARAM_GAIN));
        const auto autParPan = DAW::GetParameterModulationFromRouting(pluginMgr, DAW::GetRoutingFromDestinationParam(this, PARAM_PAN));
        out->clear();
        /* fast path: no sample accurate automation */
        if (autParGain.type <= DAW::automation_routing_type::ROUTING_CONSTANT 
            && (autParPan.type <= DAW::automation_routing_type::ROUTING_NONE 
                ||  (autParPan.type <= DAW::automation_routing_type::ROUTING_CONSTANT && autParPan.atl->getParamValue(autParPan.paramIdx) == 0.5f))) {
            float fGain = 1.0f;
            bool bIsNotMuted = true;
            if (autParGain.type != DAW::automation_routing_type::ROUTING_NONE) {
                bIsNotMuted = dsp_util::getGainLvlWithRange(autParGain.atl->getParamValue(autParGain.paramIdx), MTR_CEIL, DBFS_MUTE_POS, fGain);
            }
            if (bIsNotMuted) {
                float fPanTrack = 0.5f;
                if (autParPan.type != DAW::automation_routing_type::ROUTING_NONE) {
                    fPanTrack = autParPan.atl->getParamValue(autParPan.paramIdx);
                }
                /* fast path: center pan */
                if (math::abs(fPanTrack - 0.5f) < 0.005f) {
                    out->addFromOp(bufEqd, AudioBlock::mix_op::ADD, fGain);
                } else {
                    DAW::Panning::MultiplyConstant(bufEqd, out, fGain * (1.0f/DAW::Panning::GetCenterGain()), fPanTrack);
                }
            } else {
                /* fast path: fully muted */
            }
            return;
        }
        const auto tickBegin = tick;
        const auto tickEnd = tickBegin + host->getAudioStreamProperties().ticksPerBlock;
        DAW::Host::MixWithGainAndPanAutomation(host, impl->buf, bufEqd, out, autParGain, autParPan, tickBegin, tickEnd, state, MTR_CEIL, DBFS_MUTE_POS);
    }

    param_converted_t module_eq::convertParamValueDisplay(int32_t idx, const param_unit_t& displayValue) {
        //TODO: use std::from_chars when floating point version arrives in libc++
        auto fTextFieldVal = static_cast<float>(atof(StringAsCStr(displayValue.value)));
        if (idx == PARAM_GAIN) {
            if (fTextFieldVal <= DBFS_MUTE_POS + 1.0f)
                fTextFieldVal = 0.0f;
            if (fTextFieldVal > MTR_CEIL)
                fTextFieldVal = MTR_CEIL;
            float f_gain = pow(10.0f, fTextFieldVal / 20.0f);
            float f_linear = dsp_util::gainToLinScaleWithRange(f_gain, MTR_CEIL, DBFS_MUTE_POS);
            return {f_linear, true};
        }
        return internalplugin::convertParamValueDisplay(idx, displayValue);
    }
    param_unit_t module_eq::convertParamValueToDisplay(int32_t idx, float value) {
        auto param = getParam(idx);
        dbgassert(param);
        if (param->unit == "dB") {
            float fGain = 1.0f;
            if (dsp_util::getGainLvlWithRange(value, MTR_CEIL, DBFS_MUTE_POS, fGain)) {
                return {StringFormat("%.3f", dsp_util::dBFS(fGain)), param->unit};
            }
            return {"-INF", param->unit};
        }
        return internalplugin::convertParamValueToDisplay(idx, value);
    }

    void module_eq::loadSnapshot(const plugin_snapshot_t& snapshot) {
        internalplugin::loadSnapshot(snapshot);
        if (snapshot.vendorVersion == 0) {
            /*  Convert old snapshot format */
            auto& params = snapshot.params;
            auto it = std::find_if(params.begin(), params.end(), [](const auto& p) {
                return p.idx == PARAM_GAIN;
            });
            if (it != params.end()) {
                float val = it->val;
                val = dsp_util::linScaleToGainWithRange(val, 6.0f, -81.0f);
                val = dsp_util::gainToLinScaleWithRange(val, MTR_CEIL, DBFS_MUTE_POS);
                setParamValue(PARAM_GAIN, val, FLG_PAR_UPDATE_INIT | FLG_PAR_UPDATE_NOSTORE);
            }
        }
    }

    void module_eq::makeSnapshot(plugin_snapshot_t& snapshot, const tracksnapshot_store_opts_t& opts) {
        internalplugin::makeSnapshot(snapshot, opts);
        snapshot.vendorVersion = 1;
    }
} // namespace PluginEQ

namespace PluginEQ {
    class guicontainer_plugin_eq_header final : public guictr_base {
        module_eq* const module;
    public:
        explicit guicontainer_plugin_eq_header(module_eq* _synth)
            : guictr_base(), module(_synth) {
            padding = 0;
            margin  = 0;
        }
        ~guicontainer_plugin_eq_header() override {
            removeGuis();
        }
    };
    class guicontainer_plugin_eq_editor final : public guictr_base {
        module_eq* const module;
    public:
        explicit guicontainer_plugin_eq_editor(module_eq* _synth)
            : guictr_base(), module(_synth) {
            padding = 0;
            margin  = 0;
        }
        ~guicontainer_plugin_eq_editor() override {
            removeGuis();
        }

        void onSetParameter(int32_t index, float value) {
        }

        void onGuiOpen() {
        }

        void onGuiClose() {
        }
    };

    class guicontainer_plugin_eq final : public guictr_base {
        guicontainer_plugin_eq_editor editor;
        guicontainer_plugin_eq_header header;
    public:
        explicit guicontainer_plugin_eq(module_eq* _module) : guictr_base(), editor(_module), header(_module) {
            padding = 0;
            margin  = 0;
            // add(&header);
            add(&editor);
            setLayoutMode(autolayout_mode::LAYOUT_VERTICAL);
        }
        ~guicontainer_plugin_eq() override {
            removeGuis();
        }

        void onTick(AppCtrl* ctrl) override {
            guictr_base::onTick(ctrl);
        }

        void onSetParameter(int32_t index, float value) {
            editor.onSetParameter(index, value);
        }

        void getSizeScale(int& w, int& h) const {
            w = 1280*1.25;
            h = 720;
        }

        void onGuiOpen() {
            editor.onGuiOpen();
        }

        void onGuiClose() {
            editor.onGuiClose();
        }
    };
    class PluginViewContainerEQ final : public PluginViewContainers {
    public:
        guicontainer_plugin_eq ctr_main;
        explicit PluginViewContainerEQ(module_eq* eff)
            : ctr_main(eff) {
        }
        ~PluginViewContainerEQ() override = default;
        guicontainer_plugin_eq& getPluginUI() {
            return ctr_main;
        }
        const guicontainer_plugin_eq& getPluginUI() const {
            return ctr_main;
        }
        void layout(int32_t winW, int32_t winH) override {
            ctr_main.pos  = { 0, 0 };
            ctr_main.size = { winW, winH };
        }
        void addTo(std::vector<guictr_base*>& v) override {
            v.push_back(&ctr_main);
        }
        void onGuiOpen() override {
            ctr_main.onGuiOpen();
        }
        void onGuiClose() override {
            ctr_main.onGuiClose();
        }
        void onSetParameter(int32_t index, float value) override {
            ctr_main.onSetParameter(index, value);
        }
        void getFixedSize(int32_t* w, int32_t* h) override {
            ctr_main.getSizeScale(*w, *h);
        }
        bool isViewSupported(int32_t uiId) const override {
            return uiId != UID_VIEW_CTR_NODES;
        }
    };
    std::shared_ptr<PluginViewContainers> module_eq::createViewCtrInternal() {
        return std::make_shared<PluginViewContainerEQ>(this);
    }
} // namespace PluginEQ

template<>
effectbase* makeInstance<PluginEQ::module_eq>(int32_t _projectGlobalId, IHostCallback* _hostCallback) {
    return new PluginEQ::module_eq(_projectGlobalId, _hostCallback);
}

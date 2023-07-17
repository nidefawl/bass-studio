#include "color_util.h"
#include "guicolors.h"
#include <glm/gtx/color_space.hpp>
#include <math.h>
#include "eq-plugin.h"
#include "assert_dbg.h"
#include "guiglobals.h"
#include "host/automation/automation.h"
#include "dsp_util.h"
#include "event.h"
#include "logging.h"
#include "math/seq_math.h"
#include "plugins/eq/filter-coeffs.h"
#include "plugins/plugin-ui.h"
#include "plugins/plugincontrol.h"
#include "samplerate.h"
#include "seq_time.h"
#include "seq_util.h"
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
#include <nanovg.h>
#include <pybind11/gil.h>
#include <vector>
#include "filter-coeffs.h"

namespace PluginEQ {

    constexpr float F_MIN = 10;
    constexpr float F_MAX = 20000;
    const     float F_SCALE_EXPO = math::calcExponentForScale(0.5f, 500.0f, F_MIN, F_MAX);

    double GetScaledCutoffFrequency(float paramValue) {
        auto valueMapped = math::calcMappedValueForScale(paramValue, F_SCALE_EXPO, F_MIN, F_MAX);
        return math::clamp(valueMapped, F_MIN, F_MAX);
    }

    double GetParamValueForCutoffFrequency(double freq) {
        auto valueMapped = float(freq - F_MIN) / (F_MAX - F_MIN);
        return math::clamp<float>(math::powf(valueMapped, 1.0f/F_SCALE_EXPO), 0.0f, 1.0f);
    }

    constexpr float Q_MIN = 0.1;
    constexpr float Q_MAX = 18;
    const     float Q_SCALE_EXPO = math::calcExponentForScale(0.5f, 1.3f, Q_MIN, Q_MAX);

    double GetScaledQ(float paramValue) {
        auto valueMapped = math::calcMappedValueForScale(paramValue, Q_SCALE_EXPO, Q_MIN, Q_MAX);
        return math::clamp(valueMapped, Q_MIN, Q_MAX);
    }

    double GetParamValueForQ(double q) {
        auto valueMapped = float(q - Q_MIN) / (Q_MAX - Q_MIN);
        return math::clamp<float>(math::powf(valueMapped, 1.0f/Q_SCALE_EXPO), 0.0f, 1.0f);
    }

    enum BandType {
        BandTypeLowPass,
        BandTypeHighPass,
        BandTypeBandPass,
        BandTypePeak,
        BandTypeLowShelf,
        BandTypeHighShelf,
        BandTypeNotch,
        NumBandTypes
    };

    BandType GetScaledBandType(float paramValue) {
        auto iNumBandTypes = static_cast<int>(NumBandTypes);
        auto idxF          = iNumBandTypes * paramValue;
        return static_cast<BandType>(math::clamp(math::floorfS32(idxF), 0, iNumBandTypes - 1));
    }

    float GetParamValueForBandType(BandType type) {
        auto iNumBandTypes = static_cast<int>(NumBandTypes);
        return float(type) / iNumBandTypes;
    }

    std::array<String, 7> FILTER_TYPE_NAMES = {
        "Lowpass 12dB",
        "Highpass 12dB",
        "Bandpass",
        "Peak",
        "Lowshelf",
        "Highshelf",
        "Notch",
    };

    struct band_t {
        int32_t state = 0;
        float freq    = 1000.0;
        float gain    = 1.0;
        float q       = 1.0;
        BandType type = BandTypeLowPass;
    };

    constexpr static std::array<band_t, 10> defaultBands = {{
        { 1, 10.0, 1.0, 0.707, BandTypeHighPass },
        { 0, 64.0, 1.0, 0.707, BandTypePeak },
        { 0, 125.0, 1.0, 0.707, BandTypePeak },
        { 0, 250.0, 1.0, 0.707, BandTypePeak },
        { 0, 500.0, 1.0, 0.707, BandTypePeak },
        { 0, 1000.0, 1.0, 0.707, BandTypePeak },
        { 0, 2000.0, 1.0, 0.707, BandTypePeak },
        { 0, 4000.0, 1.0, 0.707, BandTypePeak },
        { 0, 8000.0, 1.0, 0.707, BandTypePeak },
        { 1, 16000.0, 1.0, 0.707, BandTypeLowPass },
    }};

    constexpr static int PARAMID_FIRST_BAND = 16;
    constexpr static int PER_BAND_PARAMS = 16;
    constexpr static int PARAM_OFFSET_ENABLE = 0;
    constexpr static int PARAM_OFFSET_TYPE = 1;
    constexpr static int PARAM_OFFSET_GAIN = 2;
    constexpr static int PARAM_OFFSET_FREQ = 3;
    constexpr static int PARAM_OFFSET_Q    = 4;

    const double PLOT_DB_MAX = 24;
    const double PLOT_DB_MIN = -48;
    const double PLOT_DB_GRID_STEP = 6;
    const double PLOT_DB_RANGE = PLOT_DB_MAX - PLOT_DB_MIN;
    const double PLOT_HZ_MIN = 10;
    const double PLOT_HZ_MAX = 22050;

    struct impl_data_t {
        DAW::Host::process_scratch_buf_t buf;
        std::array<band_t, defaultBands.size()> bands = defaultBands;
        std::array<std::vector<std::shared_ptr<DAW::Filter>>, defaultBands.size()> filters;
        std::array<DAW::FilterCoeffs, defaultBands.size()> filterCoeffs;
        AudioBlock tmpBlock;
    };

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
        for (int32_t i = 0; i < CtrSize(impl->bands); ++i) {
            const auto& band = impl->bands[i];
            const int32_t paramId = PARAMID_FIRST_BAND + i * PER_BAND_PARAMS;
            auto paramEnable = registerParam(paramId + PARAM_OFFSET_ENABLE);
            paramEnable->initValue(effectgain_param_entry{
                paramId + PARAM_OFFSET_TYPE,
                String("Band ") + std::to_string(i + 1) + " Enabled",
                "",
                float(band.state)
            });
            paramEnable->quantizationSteps = 1;
            auto paramType = registerParam(paramId + PARAM_OFFSET_TYPE);
            paramType->initValue(effectgain_param_entry{
                paramId + PARAM_OFFSET_TYPE,
                String("Band ") + std::to_string(i + 1) + " Type",
                "",
                GetParamValueForBandType(band.type)
            });
            paramType->quantizationSteps = NumBandTypes - 1;
            auto paramGain = registerParam(paramId + PARAM_OFFSET_GAIN);
            paramGain->initValue(effectgain_param_entry{
                paramId + PARAM_OFFSET_GAIN,
                String("Band ") + std::to_string(i + 1) + " Gain",
                "dB",
                dsp_util::gainToLinScaleWithRange(band.gain, MTR_CEIL, DBFS_MUTE_POS)
            });
            auto paramFreq = registerParam(paramId + PARAM_OFFSET_FREQ);
            paramFreq->initValue(effectgain_param_entry{
                paramId + PARAM_OFFSET_FREQ,
                String("Band ") + std::to_string(i + 1) + " Freq",
                "Hz",
                float(GetParamValueForCutoffFrequency(band.freq))
            });
            auto paramQ = registerParam(paramId + PARAM_OFFSET_Q);
            paramQ->initValue(effectgain_param_entry{
                paramId + PARAM_OFFSET_Q,
                String("Band ") + std::to_string(i + 1) + " Q",
                "",
                float(GetParamValueForQ(band.q))
            });
        }
    }

    module_eq::~module_eq() {
        delete impl;
    }

    DAW::FilterCoeffs module_eq::getFilterCoeffs(int32_t bandIdx) {
        const auto bandParamBase = PARAMID_FIRST_BAND + bandIdx * PER_BAND_PARAMS;
        double Q  = GetScaledQ(getParamValue(bandParamBase + PARAM_OFFSET_Q));
        double Fc = GetScaledCutoffFrequency(getParamValue(bandParamBase + PARAM_OFFSET_FREQ));
        int bandType = GetScaledBandType(getParamValue(bandParamBase + PARAM_OFFSET_TYPE));
        float fGain = 1.0f;
        if (dsp_util::getGainLvlWithRange(getParamValue(bandParamBase + PARAM_OFFSET_GAIN), MTR_CEIL, DBFS_MUTE_POS, fGain)) {
            fGain = dsp_util::dBFS(fGain);
        }
        switch (bandType) {
            default:
            case BandTypeLowPass:
                return DAW::FilterCoeffs::CalculateLowPass(format.sampleRate, Fc, Q);
            case BandTypeHighPass:
                return DAW::FilterCoeffs::CalculateHighPass(format.sampleRate, Fc, Q);
            case BandTypeBandPass:
                return DAW::FilterCoeffs::CalculateBandPass(format.sampleRate, Fc, Q);
            case BandTypePeak:
                return DAW::FilterCoeffs::CalculatePeak(format.sampleRate, Fc, Q, fGain);
            case BandTypeLowShelf:
                return DAW::FilterCoeffs::CalculateLowShelf(format.sampleRate, Fc, Q, fGain);
            case BandTypeHighShelf:
                return DAW::FilterCoeffs::CalculateHighShelf(format.sampleRate, Fc, Q, fGain);
            case BandTypeNotch:
                return DAW::FilterCoeffs::CalculateNotch(format.sampleRate, Fc, Q);
        }
    }

    bool module_eq::isBandEnabled(int32_t bandIdx) {
        return getParamValue(PARAMID_FIRST_BAND + bandIdx * PER_BAND_PARAMS + PARAM_OFFSET_ENABLE) > 0.5f;
    }

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
        const auto channelCount = bufEqd->channels;
        for (int bandIdx = 0; bandIdx < int32_t(impl->bands.size()); ++bandIdx) {
            if (!isBandEnabled(bandIdx)) {
                continue;
            }
            auto& filters = impl->filters[bandIdx];
            while (filters.size() < out->channels) {
                filters.emplace_back(std::make_shared<DAW::Filter>());
            }
            auto coefficients = getFilterCoeffs(bandIdx);
            for (channelnum_t ch = 0; ch < channelCount; ++ch) {
                auto bufChannel = bufEqd->SubChannelsBlock(ch, 1);
                filters[ch]->process(coefficients, bufChannel, bufChannel);
            }
        }

        // verify that the output is sane
        /* for (channelnum_t ch = 0; ch < channelCount; ++ch) {
            for (samplecount_t s = 0; s < numSamples; ++s) {
                auto sample = bufEqd->buf[ch][s];
                dbgassert(!fp_math::isNanOrInfd(sample));
                dbgassert(sample > -2.0f && sample < 2.0f);
            }
        } */

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
        if ((idx - PARAMID_FIRST_BAND) % PER_BAND_PARAMS == PARAM_OFFSET_FREQ) {
            return {StringFormat("%.3f", GetScaledCutoffFrequency(value)), param->unit};
        }
        if ((idx - PARAMID_FIRST_BAND) % PER_BAND_PARAMS == PARAM_OFFSET_Q) {
            return {StringFormat("%.3f", GetScaledQ(value)), param->unit};
        }
        if ((idx - PARAMID_FIRST_BAND) % PER_BAND_PARAMS == PARAM_OFFSET_TYPE) {
            auto idx = math::clamp(math::floorfS32(FILTER_TYPE_NAMES.size() * value), 0, CtrSize(FILTER_TYPE_NAMES) - 1);
            return {FILTER_TYPE_NAMES[idx], param->unit};
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
        module_eq* const moduleEq;
    public:
        explicit guicontainer_plugin_eq_header(module_eq* _synth)
            : guictr_base(), moduleEq(_synth) {
            (void)moduleEq;
            padding = 0;
            margin  = 0;
        }
        ~guicontainer_plugin_eq_header() override {
            removeGuis();
        }
    };
    class guicontainer_plugin_eq_editor final : public guictr_base {
        module_eq* const moduleEq;
    public:
        explicit guicontainer_plugin_eq_editor(module_eq* _synth)
            : guictr_base(), moduleEq(_synth) {
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

        void plotBand(NVGcontext* vg,
                        vec2 graphPos,
                        vec2 graphSize,
                        const std::vector<double>& magnitudes,
                        const uint32_t graphColor,
                        const float lineWidth) {
            const double pixelToDB = graphSize.y / PLOT_DB_RANGE;
            const auto len = CtrSize(magnitudes);

            nvgBeginPath(vg);
            for (int step = 0; step < len; step++) {
                float fStep = step / (float) (len - 1);

                float mag = float(magnitudes[step]);
                double magDb = mag <= 0 ? PLOT_DB_MIN : 20.0 * log10(mag);
                double posY = (magDb - PLOT_DB_MIN) * pixelToDB;

                vec2 pos = graphPos + vec2( fStep * graphSize.x, graphSize.y - posY);
                if (step == 0) {
                    nvgMoveTo(vg, pos.x, pos.y);
                } else {
                    nvgLineTo(vg, pos.x, pos.y);
                }
            }
            nvgStrokeColor(vg, rgbaToNvg(graphColor));
            nvgStrokeWidth(vg, lineWidth);
            nvgStroke(vg);
        }

        void render(NVGcontext* vg) override {
            guictr_base::render(vg);
            vec2 inset(this->padding, this->padding);
            vec2 graphPos(inset);
            vec2 graphSize = getSizeContent() ;
            uint32_t BandColors[defaultBands.size()]{};
            int32_t i = 0;
            for (auto& col : BandColors) {
                float hue = (float) i / float(defaultBands.size());
                col = vec3ToRgbU32(glm::rgbColor(vec3(hue * 360.0f, 0.8f, 0.75f))) | 0xFF000000;
                ++i;
            }

            drawGrid(vg, theme, graphPos, graphSize);

            auto len = math::floorfS32(graphSize.x) / 2;
            std::vector<double> magFrequencies(len);

            // use log10 scale
            for (int step = 0; step < len; ++step) {
                double freq = PLOT_HZ_MIN * pow(10.0, step / (len - 1.0) * log10(PLOT_HZ_MAX / PLOT_HZ_MIN));
                magFrequencies[step] = freq;
            }

            const auto format = moduleEq->getSampleFormat();
            const auto& bands = moduleEq->getImpl()->bands;
            std::vector<double> bandMagnitudes(len);
            std::vector<double> eqMagnitudes(len);
            std::fill(eqMagnitudes.begin(), eqMagnitudes.end(), 1.0);
            for (int bandIdx = 0; bandIdx < CtrSize(bands); ++bandIdx) {
                if (!moduleEq->isBandEnabled(bandIdx)) {
                    continue;
                }
                auto coefficients = moduleEq->getFilterCoeffs(bandIdx);
                coefficients.calculateMagnitudes(magFrequencies, bandMagnitudes, format.sampleRate);
                plotBand(vg, graphPos, graphSize, bandMagnitudes, BandColors[bandIdx], 1.5);
                std::transform( bandMagnitudes.begin(), bandMagnitudes.end(),
                                eqMagnitudes.begin(), eqMagnitudes.begin(),
                                std::multiplies() );

            }
            plotBand(vg, graphPos, graphSize, eqMagnitudes, 0xBBFFFFFF, 3.0);
        }
        void drawGrid(NVGcontext* vg, const guitheme_t* theme, vec2 pos, vec2 size) {
            const double pixelToDB = size.y / PLOT_DB_RANGE;
            // 6db grid step
            const float   gridStepY = float(PLOT_DB_GRID_STEP * size.y / PLOT_DB_RANGE);
            const int32_t numStepsY = math::ceildS32(PLOT_DB_RANGE / PLOT_DB_GRID_STEP);

            NVGpaint paint{};
            paint.image = -1;

            nvgGlobalAlpha(vg, 0.5f);
            nvgBeginPath(vg);
            nvgRect(vg, -2, 0, size.x + 2, size.y);
            nvgFillColor(vg, theme->getColor(GuiColor::COL_GRID_BRT));
            nvgFill(vg);
            /* draw dark grid areas */
            int32_t nRendered = 0;

            std::vector<float> stopPoints;
            // calculate stop points on x axis in log10 scale for a graphic eq plot
            // starting at PLOT_HZ_MIN (10Hz)
            // and ending at PLOT_HZ_MAX (22000Hz)
            // rendering 10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 200, 300, ... 10000, 20000
            for (int32_t i = 0; i < 4; ++i) {
                int32_t maxSteps = i < 3 ? 10 : 3;
                for (int32_t step = 1; step < maxSteps; ++step) {
                    float freq = math::powf(10.0f, i + 1) * math::powf(10.0f, log10f(step));
                    float toPx = (log10(freq) - log10(PLOT_HZ_MIN)) / (log10(PLOT_HZ_MAX) - log10(PLOT_HZ_MIN)) * size.x;
                    stopPoints.push_back(pos.x + toPx);
                }
            }
            if (!stopPoints.empty() && stopPoints.back() < pos.x + size.x) {
                stopPoints.push_back(pos.x + size.x);
            }

            for (int32_t i = 0; i < CtrSize(stopPoints) - 1; ++i) {
                if (i % 2 == 0) {
                    nvgBatchedRect(vg, stopPoints[i], pos.y, stopPoints[i + 1] - stopPoints[i], size.y);
                    nRendered++;
                }
            }

            if (nRendered) {
                paint.innerColor = theme->getColor(GuiColor::COL_GRID_DRK);
                paint.customPar  = 1;
                nvgFillPaint(vg, paint);
                nvgBatchedRender(vg);
            }

            nvgGlobalAlpha(vg, 1.0f);
            nRendered = 0;
            paint.customPar = 2;
            const float lineThickness = 4.0f;
            for (int32_t i = 0; i < CtrSize(stopPoints) - 1; ++i) {
                nvgBatchedRect(vg, stopPoints[i] - lineThickness * 0.5f, pos.y, lineThickness, size.y);
                paint.feather = 2.5f - (i%10==0?0:1) * 0.75f;
                nRendered++;
            }
            if (nRendered) {
                paint.innerColor = theme->getColor(GuiColor::COL_LINE_QRT);
                nvgFillPaint(vg, paint);
                nvgBatchedRender(vg);
            }
            nRendered = 0;
            paint.customPar = 3;
            for (int32_t i = 0; i < numStepsY; ++i) {
                nvgBatchedRect(vg, pos.x, pos.y + gridStepY * i - lineThickness * 0.5f, size.x, lineThickness);
                paint.feather = 2.5f - 1 * 0.75f;
                nRendered++;
            }
            if (nRendered) {
                paint.innerColor = theme->getColor(GuiColor::COL_LINE_QRT);
                nvgFillPaint(vg, paint);
                nvgBatchedRender(vg);
            }
            nvgBatchedRect(vg, pos.x, pos.y + float(pixelToDB * PLOT_DB_MAX) - lineThickness * 0.5f, size.x, lineThickness);
            paint.feather = 2.5f;
            paint.innerColor = theme->getColor(GuiColor::COL_LINE_BAR);
            nvgFillPaint(vg, paint);
            nvgBatchedRender(vg);
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
}// namespace PluginEQ

template<>
effectbase* makeInstance<PluginEQ::module_eq>(int32_t _projectGlobalId, IHostCallback* _hostCallback) {
    return new PluginEQ::module_eq(_projectGlobalId, _hostCallback);
}

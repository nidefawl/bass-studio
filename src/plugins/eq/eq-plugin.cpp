#include "color_util.h"
#include "guicolors.h"
#include <cstdint>
#include <glm/gtx/color_space.hpp>
#include <math.h>
#include "eq-plugin.h"
#include "assert_dbg.h"
#include "guiglobals.h"
#include "host/audio_analyzer.h"
#include "host/automation/automation.h"
#include "dsp_util.h"
#include "event.h"
#include "logging.h"
#include "math/seq_math.h"
#include "plugins/eq/eq-plugin.h"
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
#include <vector>
#include "filter-coeffs.h"
#include <dsp/rates.h>

namespace DAW {
    extern bool gClapUseSampleAccurateModulation;
}
namespace PluginEQ {

    constexpr float F_MIN = 10;
    constexpr float F_MAX = 20000;
    const     float F_SCALE_EXPO = math::calcExponentForScale(0.5f, 500.0f, F_MIN, F_MAX);

    double GetScaledCutoffFrequency(float paramValue) {
        auto valueMapped = math::calcMappedValueForScale(paramValue, F_SCALE_EXPO, F_MIN, F_MAX);
        dbgassert(!fp_math::isNanOrInfd(valueMapped));
        return math::clamp(valueMapped, F_MIN, F_MAX);
    }

    float GetParamValueForCutoffFrequency(double freq) {
        auto valueMapped = float(freq - F_MIN) / (F_MAX - F_MIN);
        if (valueMapped <= 0.0f)
            return 0.0f;
        return math::clamp<float>(math::powf(valueMapped, 1.0f/F_SCALE_EXPO), 0.0f, 1.0f);
    }

    constexpr float Q_MIN = 0.1;
    constexpr float Q_MAX = 18;
    const     float Q_SCALE_EXPO = math::calcExponentForScale(0.5f, 1.3f, Q_MIN, Q_MAX);

    double GetScaledQ(float paramValue) {
        auto valueMapped = math::calcMappedValueForScale(paramValue, Q_SCALE_EXPO, Q_MIN, Q_MAX);
        return math::clamp(valueMapped, Q_MIN, Q_MAX);
    }

    float GetParamValueForQ(double q) {
        auto valueMapped = float(q - Q_MIN) / (Q_MAX - Q_MIN);
        if (valueMapped <= 0.0f)
            return 0.0f;
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
        int32_t state = 0;      // 0 = disabled, 1 = enabled
        float freq    = 1000.0; // Hz
        float gainDb    = 0.0;    // dB
        float q       = 1.0;    // Q
        BandType type = BandTypeLowPass;
    };

    constexpr static std::array<band_t, 10> defaultBands = {{
        { 0, 120.0, 0.0, 0.707, BandTypeLowShelf },
        { 0, 200.0, 0.0, 0.707, BandTypePeak },
        { 0, 350.0, 0.0, 0.707, BandTypePeak },
        { 0, 500.0, 0.0, 0.707, BandTypePeak },
        { 0, 800.0, 0.0, 0.707, BandTypePeak },
        { 0, 1000.0, 0.0, 0.707, BandTypePeak },
        { 0, 2000.0, 0.0, 0.707, BandTypePeak },
        { 0, 4000.0, 0.0, 0.707, BandTypePeak },
        { 0, 8000.0, 0.0, 0.707, BandTypePeak },
        { 1, 16000.0, 0.0, 0.707, BandTypeHighShelf },
    }};

    constexpr static int PARAMID_FIRST_BAND = 16;
    constexpr static int PER_BAND_PARAMS = 16;
    constexpr static int PARAM_OFFSET_ENABLE = 0;
    constexpr static int PARAM_OFFSET_TYPE = 1;
    constexpr static int PARAM_OFFSET_GAIN = 2;
    constexpr static int PARAM_OFFSET_FREQ = 3;
    constexpr static int PARAM_OFFSET_Q    = 4;
    constexpr static int PARAMID_OVERSAMPLING = PARAMID_FIRST_BAND + 32 * PER_BAND_PARAMS;

    const float DBFS_MUTE_POS = -101.0f;
    const float MTR_CEIL      = 24.0f;
    const float PLOT_DB_MAX = 24;
    const float PLOT_DB_MIN = -48*2;
    const float PLOT_DB_GRID_STEP = 6;
    const float PLOT_DB_RANGE = PLOT_DB_MAX - PLOT_DB_MIN;
    const float PLOT_HZ_MIN = 10;
    const float PLOT_HZ_MAX = 22050;

    
    using FreqPlotStopPoints = std::array<float, 29>;
    FreqPlotStopPoints GetFreqPlotStopPoints() {
        FreqPlotStopPoints freqStopPoints;
        // calculate stop points on x axis in log10 scale for a graphic eq plot
        // starting at PLOT_HZ_MIN (10Hz)
        // and ending at PLOT_HZ_MAX (22000Hz)
        // rendering 10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 200, 300, ... 10000, 20000
        auto itBegin = freqStopPoints.begin();
        for (int32_t i = 0; i < 4; ++i) {
            int32_t maxSteps = i < 3 ? 10 : 3;
            for (int32_t step = 1; step < maxSteps; ++step) {
                *itBegin++ = math::powf(10.0f, i + 1) * step;
            }
        }
        return freqStopPoints;
    }
    const static FreqPlotStopPoints PlotFrequencies = GetFreqPlotStopPoints();

    struct impl_data_t {
        DAW::Host::process_scratch_buf_t buf;
        std::array<std::vector<std::shared_ptr<DAW::Filter>>, defaultBands.size()> filters;
        std::array<DAW::FilterCoeffs, defaultBands.size()> filterCoeffs;
        AudioBlock tmpBlock;
        audioanaylzer audioAnalyzer;
        audio_spectrum spectrum;
        std::vector<float> freq{};
        signalsmith::rates::Oversampler2xFIR<float> oversampler;
        AudioBlock oversampledBlock;
    };

    band_t GetBandParams(module_eq* moduleEq, int32_t bandIdx) {
        const auto bandParamBase = PARAMID_FIRST_BAND + bandIdx * PER_BAND_PARAMS;
        auto bEnabled = moduleEq->getParamValue(bandParamBase + PARAM_OFFSET_ENABLE) > 0.5f;
        auto Q  = GetScaledQ(moduleEq->getParamValue(bandParamBase + PARAM_OFFSET_Q));
        auto Fc = GetScaledCutoffFrequency(moduleEq->getParamValue(bandParamBase + PARAM_OFFSET_FREQ));
        auto bandType = GetScaledBandType(moduleEq->getParamValue(bandParamBase + PARAM_OFFSET_TYPE));
        auto fGain = 0.0f;
        if (dsp_util::getGainLvlWithRange(moduleEq->getParamValue(bandParamBase + PARAM_OFFSET_GAIN), MTR_CEIL, DBFS_MUTE_POS, fGain)) {
            fGain = dsp_util::dBFS(fGain);
        }
        return {
            .state = bEnabled,
            .freq = float(Fc),
            .gainDb = fGain,
            .q = float(Q),
            .type = bandType,
        };
    }

    void SetBandFreqAndGain(module_eq* moduleEq, int32_t bandIdx, const band_t& band, int32_t flags = FLG_PAR_UPDATE_USER | FLG_PAR_UPDATE_FINISH) {
        const auto bandParamBase = PARAMID_FIRST_BAND + bandIdx * PER_BAND_PARAMS;
        // moduleEq->setParamValue(bandParamBase + PARAM_OFFSET_ENABLE, band.state, flags);
        // moduleEq->setParamValue(bandParamBase + PARAM_OFFSET_TYPE, GetParamValueForBandType(band.type), flags);
        moduleEq->setParamValue(bandParamBase + PARAM_OFFSET_GAIN, dsp_util::dbfsToLinScaleWithRange(band.gainDb, MTR_CEIL, DBFS_MUTE_POS), flags);
        moduleEq->setParamValue(bandParamBase + PARAM_OFFSET_FREQ, GetParamValueForCutoffFrequency(band.freq), flags);
        // moduleEq->setParamValue(bandParamBase + PARAM_OFFSET_Q, GetParamValueForQ(band.q), flags);
    }

    void SetBandQ(module_eq* moduleEq, int32_t bandIdx, const band_t& band, int32_t flags = FLG_PAR_UPDATE_USER | FLG_PAR_UPDATE_FINISH) {
        const auto bandParamBase = PARAMID_FIRST_BAND + bandIdx * PER_BAND_PARAMS;
        moduleEq->setParamValue(bandParamBase + PARAM_OFFSET_Q, GetParamValueForQ(band.q), flags);
    }

    void ToggleBandEnabled(module_eq* moduleEq, int32_t bandIdx) {
        const auto bandParamBase = PARAMID_FIRST_BAND + bandIdx * PER_BAND_PARAMS;
        auto bEnabled = moduleEq->getParamValue(bandParamBase + PARAM_OFFSET_ENABLE) > 0.5f;
        moduleEq->setParamValue(bandParamBase + PARAM_OFFSET_ENABLE, bEnabled ? 0.0f : 1.0f, FLG_PAR_UPDATE_USER | FLG_PAR_UPDATE_FINISH);
    }

    DAW::FilterCoeffs GetFilterCoeffs(band_t bandParams, samplerate_t sampleRate) {
        switch (bandParams.type) {
            default:
            case BandTypeLowPass:
                return DAW::FilterCoeffs::CalculateLowPass(sampleRate, bandParams.freq, bandParams.q);
            case BandTypeHighPass:
                return DAW::FilterCoeffs::CalculateHighPass(sampleRate, bandParams.freq, bandParams.q);
            case BandTypeBandPass:
                return DAW::FilterCoeffs::CalculateBandPass(sampleRate, bandParams.freq, bandParams.q);
            case BandTypePeak:
                return DAW::FilterCoeffs::CalculatePeak(sampleRate, bandParams.freq, bandParams.q, bandParams.gainDb);
            case BandTypeLowShelf:
                return DAW::FilterCoeffs::CalculateLowShelf(sampleRate, bandParams.freq, bandParams.q, bandParams.gainDb);
            case BandTypeHighShelf:
                return DAW::FilterCoeffs::CalculateHighShelf(sampleRate, bandParams.freq, bandParams.q, bandParams.gainDb);
            case BandTypeNotch:
                return DAW::FilterCoeffs::CalculateNotch(sampleRate, bandParams.freq, bandParams.q);
        }
    }

    module_eq::module_eq(int32_t _projectGlobalId, IHostCallback* _hostCallback)
        : internalplugin("EQ", _projectGlobalId, _hostCallback),
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
        for (int32_t i = 0; i < CtrSize(defaultBands); ++i) {
            const auto& band = defaultBands[i];
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
                dsp_util::dbfsToLinScaleWithRange(band.gainDb, MTR_CEIL, DBFS_MUTE_POS)
            });
            auto paramFreq = registerParam(paramId + PARAM_OFFSET_FREQ);
            paramFreq->initValue(effectgain_param_entry{
                paramId + PARAM_OFFSET_FREQ,
                String("Band ") + std::to_string(i + 1) + " Freq",
                "Hz",
                GetParamValueForCutoffFrequency(band.freq)
            });
            auto paramQ = registerParam(paramId + PARAM_OFFSET_Q);
            paramQ->initValue(effectgain_param_entry{
                paramId + PARAM_OFFSET_Q,
                String("Band ") + std::to_string(i + 1) + " Q",
                "",
                GetParamValueForQ(band.q)
            });
        }
        auto paramOversampling = registerParam(PARAMID_OVERSAMPLING);
        paramOversampling->initValue(effectgain_param_entry{
            PARAMID_OVERSAMPLING,
            "Oversampling",
            "",
            float(1.0f)
        });
        paramOversampling->isAutomatable = false;
        paramOversampling->quantizationSteps = 1;
    }

    module_eq::~module_eq() {
        delete impl;
    }

    bool module_eq::isBandEnabled(int32_t bandIdx) {
        return getParamValue(PARAMID_FIRST_BAND + bandIdx * PER_BAND_PARAMS + PARAM_OFFSET_ENABLE) > 0.5f;
    }

    void module_eq::setSampleFormat(sampleformat_t sampleFormat) {
        impl->audioAnalyzer.init(sampleFormat.blockSize, sampleFormat.sampleRate);
        auto nBands = 1024;
        impl->audioAnalyzer.analyzerHf->setNumBands(nBands);
        impl->audioAnalyzer.analyzerLf->setNumBands(nBands);
        impl->audioAnalyzer.analyzerHf->setFreqRange(F_MIN, F_MAX);
        impl->audioAnalyzer.analyzerLf->setFreqRange(F_MIN, F_MAX);
        impl->audioAnalyzer.analyzerHf->updateBands();
        impl->audioAnalyzer.analyzerLf->updateBands();
        impl->spectrum = *impl->audioAnalyzer.analyzerLf;
        impl->freq = impl->audioAnalyzer.analyzerLf->freq;
        dbgassert(impl->audioAnalyzer.analyzerHf->freq == impl->audioAnalyzer.analyzerLf->freq);
        internalplugin::setSampleFormat(sampleFormat);
    }

    void module_eq::initBuffers() {
        internalplugin::initBuffers();
        auto numChannels = blockInputs->channels;
        this->impl->oversampler.resize(numChannels, format.blockSize);
        std::vector<float*> channelsOversampler(numChannels);
        for (int i = 0; i < numChannels; ++i) {
            channelsOversampler[i] = this->impl->oversampler[i];
        }
        impl->oversampledBlock = AudioBlock(channelsOversampler, format.blockSize * 2);
    }

    samplecount_t module_eq::getPluginLatency() {
        return getParamValue(PARAMID_OVERSAMPLING) >= 0.5f ? format.blockSize : 0;
    }

    void module_eq::onTick(double since) {
        internalplugin::onTick(since);
        impl->audioAnalyzer.onTick();
    }

    void MixFFTSpectrumBands(const audio_spectrum* lf, const audio_spectrum* hf, audio_spectrum& out) {
        constexpr float fstep = 0.22f;
        for (channelnum_t i = 0; i < audio_spectrum::NUM_CHANNELS; i++) {
            const auto& bandsA = lf->bands[i];
            const auto& bandsB = hf->bands[i];
            dbgassert(bandsA.size() == bandsB.size());
            auto& bandsM = out.bands[i];
            bandsM.resize(bandsA.size());
            const float f = 1.0f / (lf->numBands - 1);
            for (int j = 0; j < lf->numBands; ++j) {
                float fInterp = smoothstep(fstep, 1.0f - fstep, f * j);
                float mixedBand = lerpf32(bandsA[j], bandsB[j], fInterp);
                float dbfs = dsp_util::dBFS(mixedBand);
                bandsM[j] = (dbfs - PLOT_DB_MIN) / PLOT_DB_RANGE;
            }
        }
                                
        dbgassert(static_cast<size_t>(out.fftlen) == out.mags[0].size());
    }
    void ReadAutomation(const DAW::Host::Host* const host, module_eq* eq, double tick, playback_state state, samplecount_t samplePos, samplecount_t sampleCount, int nOversample) {
        auto bpm100 = host->prjGlobals.tempo100;
        auto tickPosOffset = tick + sampleToTickConvert<double, roundmode::none>(samplePos, bpm100, host->m_sampleFormatInternal.sampleRate * nOversample);
        eq->updateAutomatedParameters(host, math::floordS32(tickPosOffset), state);
    }
    auto InterpolateFilterCoeffs(const DAW::FilterCoeffs& oldCoeffs, const DAW::FilterCoeffs& newCoeffs, samplecount_t numSamples) -> DAW::FilterCoeffs {
        DAW::FilterCoeffs coeffs;
        for (size_t i = 0; i < coeffs.coefficients.size(); ++i) {
            coeffs.coefficients[i] = lerpf32(oldCoeffs.coefficients[i], newCoeffs.coefficients[i], 1.0f / numSamples);
        }
        return coeffs;
    };
    void module_eq::process(const DAW::Host::Host* const host, AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) {
        dbgassert(in->samples == format.blockSize
                && out->samples == format.blockSize
                && format.blockSize > 0
                && format.sampleRate > 0);

        const bool bOversampling = getParamValue(PARAMID_OVERSAMPLING) >= 0.5f;

        auto format = this->format;
        if (bOversampling) {
            impl->oversampler.up(in->buf, numSamples);
            format.sampleRate *= 2;
            format.blockSize *= 2;
            numSamples *= 2;
        }

        if (impl->tmpBlock.samples != format.blockSize || impl->tmpBlock.channels != out->channels) {
            impl->tmpBlock = AudioBlock(out->channels, format.blockSize);
        }

        auto* bufEqd = &impl->tmpBlock;
        auto* blockOut = out;
        if (bOversampling) {
            bufEqd->copyFrom(&impl->oversampledBlock);
            blockOut = &impl->oversampledBlock;
        } else {
            bufEqd->copyFrom(in);
        }
        const auto channelCount = bufEqd->channels;
        bool bUseSampleAccurateModulation = DAW::gClapUseSampleAccurateModulation;
        if (getAutomationLanes().empty() && getModulationsMap().empty()) {
            bUseSampleAccurateModulation = false;
        }
        if (bUseSampleAccurateModulation) {
            auto stepSizeSamples = samplecount_t(state == playback_state::status_render ? 1 : 8);
            for (samplecount_t i = 0; i < numSamples; i += stepSizeSamples) {
                ReadAutomation(host, this, tick, state, i, numSamples, bOversampling ? 2 : 1);
                for (int32_t bandIdx = 0; bandIdx < int32_t(defaultBands.size()); ++bandIdx) {
                    if (!isBandEnabled(bandIdx)) {
                        continue;
                    }
                    auto& filters = impl->filters[bandIdx];
                    while (filters.size() < out->channels) {
                        filters.emplace_back(std::make_shared<DAW::Filter>());
                    }
                    auto bandParams = GetBandParams(this, bandIdx);
                    auto coefficients = GetFilterCoeffs(bandParams, format.sampleRate);
                    impl->filterCoeffs[bandIdx] = InterpolateFilterCoeffs(impl->filterCoeffs[bandIdx], coefficients, state == playback_state::status_render ? 1 : 3);
                    for (channelnum_t ch = 0; ch < channelCount; ++ch) {
                        auto bufChannel = bufEqd->SubChannelsSamplesBlock(ch, 1, i, stepSizeSamples);
                        filters[ch]->process(impl->filterCoeffs[bandIdx], bufChannel, bufChannel);
                    }
                }
            }
        } else {
            for (int32_t bandIdx = 0; bandIdx < int32_t(defaultBands.size()); ++bandIdx) {
                if (!isBandEnabled(bandIdx)) {
                    continue;
                }
                auto& filters = impl->filters[bandIdx];
                while (filters.size() < out->channels) {
                    filters.emplace_back(std::make_shared<DAW::Filter>());
                }
                auto bandParams = GetBandParams(this, bandIdx);
                auto coefficients = GetFilterCoeffs(bandParams, format.sampleRate);
                impl->filterCoeffs[bandIdx] = InterpolateFilterCoeffs(impl->filterCoeffs[bandIdx], coefficients, 3);
                for (channelnum_t ch = 0; ch < channelCount; ++ch) {
                    auto bufChannel = bufEqd->SubChannelsBlock(ch, 1);
                    filters[ch]->process(impl->filterCoeffs[bandIdx], bufChannel, bufChannel);
                }
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
        blockOut->clear();
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
                    blockOut->addFromOp(bufEqd, AudioBlock::mix_op::ADD, fGain);
                } else {
                    DAW::Panning::MultiplyConstant(bufEqd, blockOut, fGain * (1.0f/DAW::Panning::GetCenterGain()), fPanTrack);
                }
            } else {
                /* fast path: fully muted */
            }
        } else {
            const auto tickBegin = tick;
            const auto tickEnd = tickBegin + host->getAudioStreamProperties().ticksPerBlock;
            DAW::Host::MixWithGainAndPanAutomation(host, impl->buf, bufEqd, blockOut, autParGain, autParPan, tickBegin, tickEnd, state, MTR_CEIL, DBFS_MUTE_POS);
        }
        if (bOversampling) {
            numSamples /= 2;
            impl->oversampler.down(out->buf, numSamples);
        }
        impl->audioAnalyzer.processBlock(out, 1.0f);
        MixFFTSpectrumBands(impl->audioAnalyzer.analyzerLf.get(), impl->audioAnalyzer.analyzerHf.get(), impl->spectrum);
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
        if (snapshot.vendorVersion == 1) {
            // enable all bands
            for (int32_t i = 0; i < int32_t(defaultBands.size()); ++i) {
                const auto bandParamBase = PARAMID_FIRST_BAND + i * PER_BAND_PARAMS;
                this->setParamValue(bandParamBase + PARAM_OFFSET_ENABLE, 1.0f, FLG_PAR_UPDATE_USER | FLG_PAR_UPDATE_FINISH);
            }
        }
        internalplugin::loadSnapshot(snapshot);
    }

    void module_eq::makeSnapshot(plugin_snapshot_t& snapshot, const tracksnapshot_store_opts_t& opts) {
        internalplugin::makeSnapshot(snapshot, opts);
        snapshot.vendorVersion = 2;
    }
} // namespace PluginEQ

namespace PluginEQ {
    class guicontainer_plugin_eq_params final : public guictr_base {
        module_eq* const moduleEq;
        guiknob_pluginparam knobEnabled;
        guiknob_pluginparam knobFrequency;
        guiknob_pluginparam knobGain;
        guiknob_pluginparam knobQ;
        guiknob_pluginparam knobType;
        int32_t bandIdx = -1;
    public:
        explicit guicontainer_plugin_eq_params(module_eq* _synth)
            : guictr_base(),
            moduleEq(_synth),
            knobEnabled(guiknob::knobtype::KNOB_LABELED),
            knobFrequency(guiknob::knobtype::KNOB_LABELED),
            knobGain(guiknob::knobtype::KNOB_LABELED),
            knobQ(guiknob::knobtype::KNOB_LABELED),
            knobType(guiknob::knobtype::KNOB_LABELED)
        {
            (void)moduleEq;
            padding = 0;
            margin  = 0;
            add(&knobEnabled);
            add(&knobFrequency);
            add(&knobGain);
            add(&knobQ);
            add(&knobType);
            setLayoutMode(autolayout_mode::LAYOUT_HORIZONTAL);
            knobEnabled.setBackgroundRendered(false);
            knobFrequency.setBackgroundRendered(false);
            knobGain.setBackgroundRendered(false);
            knobQ.setBackgroundRendered(false);
            knobType.setBackgroundRendered(false);
            setBackgroundRendered(true);
            setCanMouseHit(true);
        }
        void setBandIdx(int32_t band) {
            if (bandIdx == band) {
                return;
            }
            bandIdx = band;
            if (bandIdx < 0 || bandIdx >= int32_t(defaultBands.size())) {
                setVisible(false);
                return;
            }
            knobEnabled.setParamIdx(PARAMID_FIRST_BAND + band * PER_BAND_PARAMS + PARAM_OFFSET_ENABLE);
            knobFrequency.setParamIdx(PARAMID_FIRST_BAND + band * PER_BAND_PARAMS + PARAM_OFFSET_FREQ);
            knobGain.setParamIdx(PARAMID_FIRST_BAND + band * PER_BAND_PARAMS + PARAM_OFFSET_GAIN);
            knobQ.setParamIdx(PARAMID_FIRST_BAND + band * PER_BAND_PARAMS + PARAM_OFFSET_Q);
            knobType.setParamIdx(PARAMID_FIRST_BAND + band * PER_BAND_PARAMS + PARAM_OFFSET_TYPE);
            knobEnabled.setEffectInstance(moduleEq);
            knobFrequency.setEffectInstance(moduleEq);
            knobGain.setEffectInstance(moduleEq);
            knobQ.setEffectInstance(moduleEq);
            knobType.setEffectInstance(moduleEq);
            setVisible(true);
        }

        ~guicontainer_plugin_eq_params() override {
            removeGuis();
        }
    };
    class guicontainer_plugin_eq_editor final : public guictr_base {
        enum class hittype {
            HIT_NONE,
            HIT_BAND,
        };
        struct hit_result {
            hittype type = hittype::HIT_NONE;
            int32_t idx = -1;
            float dist = 0.0f;
        };

        module_eq* const moduleEq;
        guicontainer_plugin_eq_params params;
        hit_result dragged{};
        vec2 graphPos{};
        vec2 graphSize{};
        float radiusHandle = 2.0f;
    public:
        explicit guicontainer_plugin_eq_editor(module_eq* _module)
            : guictr_base(), moduleEq(_module), params(_module) {
            padding = 0;
            margin  = 0;
            setCanMouseHit(true);
            add(&params);
            params.setVisible(false);
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

        void layout() override {
            vec2 inset(this->padding, this->padding);
            graphPos = inset;
            // graphPos += vec2(14, 23);
            graphSize = vec2(size) - inset * 2.0f;
            // graphSize.y -= 20;
            radiusHandle = math::clamp(math::floorfS32(graphSize.y / 30.0f) * 0.5f, 2.0f, 7.0f);

            const float   gridStepY = float(PLOT_DB_GRID_STEP * size.y / PLOT_DB_RANGE);
            float heightLegendBottom = gridStepY * 0.5f;
            params.size = vec2(size.x * 0.5f, size.y * 0.2f);
            params.pos = vec2(size.x * 0.5f - params.size.x * 0.5f, size.y - params.size.y - heightLegendBottom * 2.0f);
            guictr_base::layout();
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
            if (!isVisible()) {
                log_printf("warning, skip rendering container with state !isVisible()\n");
                return;
            }
            if (isBackgroundRendered()) {
                renderBackground(vg);
            }
            if (!setScissorTransform(vg)) {
                return;
            }
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
            std::vector<double> bandMagnitudes(len);
            std::vector<double> eqMagnitudes(len);
            std::fill(eqMagnitudes.begin(), eqMagnitudes.end(), 1.0);

            std::array<band_t, defaultBands.size()> bandParams{};
            for (int32_t bandIdx = 0; bandIdx < int32_t(bandParams.size()); ++bandIdx) {
                bandParams[bandIdx] = GetBandParams(moduleEq, bandIdx);
            }

            for (int32_t bandIdx = 0; bandIdx < int32_t(bandParams.size()); ++bandIdx) {
                if (!moduleEq->isBandEnabled(bandIdx)) {
                    continue;
                }
                auto coefficients = GetFilterCoeffs(bandParams[bandIdx], format.sampleRate);
                coefficients.calculateMagnitudes(magFrequencies, bandMagnitudes, format.sampleRate);
                plotBand(vg, graphPos, graphSize, bandMagnitudes, BandColors[bandIdx], 1.5);
                std::transform( bandMagnitudes.begin(), bandMagnitudes.end(),
                                eqMagnitudes.begin(), eqMagnitudes.begin(),
                                std::multiplies() );
            }

            // apply global gain to eqMagnitudes
            auto fGain = 0.0f;
            dsp_util::getGainLvlWithRange(moduleEq->getParamValue(PARAM_GAIN), MTR_CEIL, DBFS_MUTE_POS, fGain);
            std::transform( eqMagnitudes.begin(), eqMagnitudes.end(),
                            eqMagnitudes.begin(),
                            [fGain](double mag) {
                                return mag * fGain;
                            } );

            plotBand(vg, graphPos, graphSize, eqMagnitudes, 0xBBFFFFFF, 3.0);


            ivec2 localMouse = toControlsObjectSpace(parentCtrl->m_mousePos, this);
            localMouse -= graphPos;
            auto hit = getMouseHit(localMouse);
            // Draw the handles
            const float pixelToDB = graphSize.y / PLOT_DB_RANGE;
            for (int32_t bandIdx = 0; bandIdx < int32_t(bandParams.size()); ++bandIdx) {

                // Draw the handle
                float posX = (log10(bandParams[bandIdx].freq) - log10(PLOT_HZ_MIN)) / (log10(PLOT_HZ_MAX) - log10(PLOT_HZ_MIN)) * graphSize.x;
                float magDb = math::max(bandParams[bandIdx].gainDb, PLOT_DB_MIN);
                float posY = (magDb - PLOT_DB_MIN) * pixelToDB;
                auto handleColor = theme->getColor(GuiColor::COL_GUI_HANDLE);
                if (!moduleEq->isBandEnabled(bandIdx)) {
                    handleColor = theme->getColor(GuiColor::COL_LABEL_INACTIVE);
                }
                if (hit.type == hittype::HIT_BAND && hit.idx == bandIdx) {
                    handleColor = theme->getColor(GuiColor::COL_GUI_HANDLE_FOCUSED);
                }
                if (dragged.type == hittype::HIT_BAND && dragged.idx == bandIdx) {
                    handleColor = theme->getColor(GuiColor::COL_GUI_HANDLE_FOCUSED);
                }
                nvgBeginPath(vg);
                nvgCircleFastNDivs(vg, graphPos.x + posX, graphPos.y + graphSize.y - posY, radiusHandle, 16);
                nvgFillColor(vg, handleColor);
                nvgFillCustomPar(vg, -2);
                nvgFill(vg);
            }

            auto impl = moduleEq->getImpl();
            auto& spectrum = impl->spectrum;
            auto& spectrumBands = spectrum.bands[0];
            nvgSave(vg);
            nvgIntersectScissor(vg, graphPos.x, graphPos.y, graphSize.x, graphSize.y);
            nvgBeginPath(vg);
            auto outsetPos = 16;
            nvgMoveTo(vg, graphPos.x - outsetPos, graphPos.y + graphSize.y + outsetPos);
            for (int32_t bandIdx = 0; bandIdx < spectrum.numBands; ++bandIdx) {
                auto bandValue = spectrumBands[bandIdx];
                auto bandFreq = impl->freq[bandIdx];
                float posX = (log10(bandFreq) - log10(PLOT_HZ_MIN)) / (log10(PLOT_HZ_MAX) - log10(PLOT_HZ_MIN)) * graphSize.x;
                float posY = (bandValue) * graphSize.y;
                nvgLineTo(vg, graphPos.x + posX, graphPos.y + graphSize.y - posY);
            }
            nvgLineTo(vg, graphPos.x + graphSize.x + outsetPos, graphPos.y + graphSize.y + outsetPos);
            nvgClosePath(vg);
            nvgStrokeColor(vg, rgbaToNvg(0xBBFFFFFF));
            nvgStrokeWidth(vg, 2.0f);
            nvgStroke(vg);
            nvgFillColor(vg, rgbaToNvg(0x3F7f7f7f));
            nvgFillCustomPar(vg, -1);
            nvgFill(vg);
            nvgSetShapeExtents(vg, graphPos.x - outsetPos, graphPos.y - outsetPos, graphSize.x + outsetPos * 2, graphSize.y + outsetPos * 2);
            nvgRestore(vg);
            for (auto c : guis) {
                if (!c->isVisible()) {
                    //log_printf("warning, skip rendering child container with state !isVisible()\n");
                    continue;
                }
                if (c->size.x <= 0 || c->size.y <= 0) {
                    // log_printf("warning, skip rendering child container %s with size <= 0 0\n", StringAsCStr(c->getClassName()));
                    continue;
                }
                {
                    nvgSave(vg);
                    c->render(vg);
                    nvgRestore(vg);
                }
            }
        }

        hit_result getMouseHit(vec2 localPos) const {
            hit_result res;
            res.type = hittype::HIT_NONE;
            res.idx = -1;
            res.dist = 0.0f;
            const float MIN_DIST = 7.0f;
            const float pixelToDB = graphSize.y / PLOT_DB_RANGE;
            std::array<band_t, defaultBands.size()> bandParams{};
            for (int32_t bandIdx = 0; bandIdx < int32_t(bandParams.size()); ++bandIdx) {
                bandParams[bandIdx] = GetBandParams(moduleEq, bandIdx);
            }
            for (int32_t bandIdx = 0; bandIdx < int32_t(bandParams.size()); ++bandIdx) {
                // if (!moduleEq->isBandEnabled(bandIdx)) {
                //     continue;
                // }
                // Calculate handle pos
                float posX = (log10(bandParams[bandIdx].freq) - log10(PLOT_HZ_MIN)) / (log10(PLOT_HZ_MAX) - log10(PLOT_HZ_MIN)) * graphSize.x;
                float magDb = math::max(bandParams[bandIdx].gainDb, PLOT_DB_MIN);
                float posY = (magDb - PLOT_DB_MIN) * pixelToDB;
                vec2 handlePos = vec2(posX, graphSize.y - posY);
                float dist = glm::distance(handlePos, localPos);
                // Check if we hit the handle
                if (dist < MIN_DIST && (res.type == hittype::HIT_NONE || dist < res.dist)) {
                    res.type = hittype::HIT_BAND;
                    res.idx = bandIdx;
                    res.dist = dist;
                }
            }
            return res;
        }

        void handleDraggedBegin(MouseEvent& evt) override {
            guictr_base::handleDraggedBegin(evt);
            dragged = getMouseHit(vec2(evt.relMousepos) - graphPos);
            if (evt.type == MouseEventType::M_EVT_DOUBLECLICK && dragged.type == hittype::HIT_BAND) {
                ToggleBandEnabled(moduleEq, dragged.idx);
            }
        }
        void handleUserMouseInput(MouseEvent& evt, bool isFinal) {
            auto mouseGraph = vec2(evt.relMousepos) - graphPos;
            if (dragged.type == hittype::HIT_BAND) {
                // calculate new Hz and dB
                float newHz = PLOT_HZ_MIN * powf(10.0f, mouseGraph.x / graphSize.x * log10(PLOT_HZ_MAX / PLOT_HZ_MIN));
                float newDb = PLOT_DB_MIN + (graphSize.y - mouseGraph.y) / graphSize.y * PLOT_DB_RANGE;
                auto bandParams = GetBandParams(moduleEq, dragged.idx);
                bandParams.freq = newHz;
                bandParams.gainDb = newDb;
                int32_t flags = FLG_PAR_UPDATE_USER;
                if (isFinal) {
                    flags |= FLG_PAR_UPDATE_FINISH;
                }
                SetBandFreqAndGain(moduleEq, dragged.idx, bandParams, flags);
                params.setBandIdx(dragged.idx);
            } else {
                params.setBandIdx(-1);
            }
        }
        void handleDraggedMove(MouseEvent& evt) override {
            guictr_base::handleDraggedMove(evt);
            handleUserMouseInput(evt, false);
        }
        void handleDraggedRelease(MouseEvent& evt) override {
            guictr_base::handleDraggedRelease(evt);
            handleUserMouseInput(evt, true);
        }

        bool handleMouseScroll(MouseEvent& evt, double xoffset, double yoffset) override {
            guictr_base::handleMouseScroll(evt, xoffset, yoffset);
            auto hit = getMouseHit(vec2(evt.relMousepos) - graphPos);
            if (hit.type == hittype::HIT_BAND) {
                auto bandParams = GetBandParams(moduleEq, hit.idx);
                float newQ = bandParams.q + float(yoffset) * 0.1f;
                bandParams.q = newQ;
                SetBandQ(moduleEq, hit.idx, bandParams, FLG_PAR_UPDATE_USER | FLG_PAR_UPDATE_FINISH);
            }
            return true;
        }

        void drawGrid(NVGcontext* vg, const guitheme_t* theme, vec2 pos, vec2 size) {
            const double pixelToDB = graphSize.y / PLOT_DB_RANGE;
            // 6db grid step
            const float   gridStepY = float(PLOT_DB_GRID_STEP * size.y / PLOT_DB_RANGE);
            const int32_t numStepsY = math::ceildS32(PLOT_DB_RANGE / PLOT_DB_GRID_STEP);

            NVGpaint paint{};
            paint.image = -1;
            int32_t extend = 2;

            nvgGlobalAlpha(vg, 0.5f);
            nvgBeginPath(vg);
            nvgRect(vg, pos.x - extend, pos.y - extend, size.x + extend * 2, size.y + extend * 2);
            nvgFillColor(vg, theme->getColor(GuiColor::COL_GRID_BRT));
            nvgFill(vg);

            auto bgImage = theme->getBackgroundImage(GuiBackgroundImage::BG_EQUALIZER_1);
            if (bgImage) {
                bgImage->render(this, vg);
            }

            /* draw dark grid areas */
            int32_t nRendered = 0;

            std::array<float, 30> stopPoints{};
            int32_t numStopPoints = 0;
            for (auto& freq : PlotFrequencies) {
                float toPx = (log10(freq) - log10(PLOT_HZ_MIN)) / (log10(PLOT_HZ_MAX) - log10(PLOT_HZ_MIN)) * size.x;
                stopPoints[numStopPoints++] = pos.x + toPx;
            }
            if (!stopPoints.empty() && stopPoints.back() < pos.x + size.x) {
                stopPoints[numStopPoints++] = pos.x + size.x;
            }

            for (int32_t i = 0; i < numStopPoints - 1; ++i) {
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
            nvgSave(vg);
            // scissor away half of the first segment so that labels fit
            float widthLegendLeft = (stopPoints[1] - stopPoints[0]) * 0.33f;
            // scissor away half of the bottom segment so that labels fit
            float heightLegendBottom = gridStepY * 0.5f;
            nvgIntersectScissor(vg, pos.x + widthLegendLeft, pos.y, size.x - widthLegendLeft, size.y - heightLegendBottom);
            nRendered = 0;
            paint.customPar = 2;
            const float lineThickness = 4.0f;
            for (int32_t i = 0; i < numStopPoints - 1; ++i) {
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
            nvgRestore(vg);
            // draw labels
            float htt = 20 * size.y / 450.0f;
            // draw y axis labels
            for (int32_t i = 1; i < numStepsY; ++i) {
                float y = pos.y + gridStepY * i;
                int32_t db = math::roundfS32(PLOT_DB_MAX - i * PLOT_DB_GRID_STEP);
                String str = StringFormat("%d", int32_t(db));
                renderTextLabel(vg,
                                vec2(pos) + vec2(widthLegendLeft - htt * 0.2f, y),
                                vec2(size.x*0.1f, math::min(htt, size.y)),
                                str,
                                theme,
                                htt,
                                theme->getColor(GuiColor::COL_LABEL_AUTOMATION_TRACK),
                                NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE
                                );
            }
            // draw x axis labels
            static const std::array<std::tuple<int32_t, String>, 9> yLabels = {
                    std::make_tuple(20,    "20 Hz"),
                    std::make_tuple(50,    "50 Hz"),
                    std::make_tuple(100,   "100 Hz"),
                    std::make_tuple(200,   "200 Hz"),
                    std::make_tuple(500,   "500 Hz"),
                    std::make_tuple(1000,  "1000 Hz"),
                    std::make_tuple(2000,  "2000 Hz"),
                    std::make_tuple(5000,  "5000 Hz"),
                    std::make_tuple(10000, "10000 Hz"),
            };
            for (const auto& [freq, str] : yLabels) {
                float x = (log10(freq) - log10(PLOT_HZ_MIN)) / (log10(PLOT_HZ_MAX) - log10(PLOT_HZ_MIN)) * size.x;
                renderTextLabel(vg,
                                vec2(x - heightLegendBottom * 0.2f, pos.y + size.y - 0),
                                vec2(size.x*0.15f, heightLegendBottom),
                                str,
                                theme,
                                htt,
                                theme->getColor(GuiColor::COL_LABEL_AUTOMATION_TRACK),
                                NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM
                                );
            }

        }
    };

    class guicontainer_plugin_eq final : public guictr_base {
        guicontainer_plugin_eq_editor editor;
    public:
        explicit guicontainer_plugin_eq(module_eq* _module) : guictr_base(), editor(_module) {
            padding = 0;
            margin  = 0;
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
    class PluginViewContainerEQ final : public PluginViewContainer {
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
    std::shared_ptr<PluginViewContainer> module_eq::createViewCtrInternal() {
        return std::make_shared<PluginViewContainerEQ>(this);
    }
}// namespace PluginEQ

template<>
effectbase* makeInstance<PluginEQ::module_eq>(int32_t _projectGlobalId, IHostCallback* _hostCallback) {
    return new PluginEQ::module_eq(_projectGlobalId, _hostCallback);
}

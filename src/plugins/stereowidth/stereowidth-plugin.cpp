#include "stereowidth-plugin.h"
#include "host/automation/automation.h"
#include "dsp_util.h"
#include "event.h"
#include "plugins/plugin-ui.h"
#include "plugins/plugincontrol.h"
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
#include "window.h"
#include <algorithm>

namespace PluginStereoWidth {
    static constexpr int32_t PARAM_WIDTH = 2;

    const float DBFS_MUTE_POS = -101.0f;
    const float MTR_CEIL      = 24.0f;

    template<typename T>
    inline void updateParam(T& cur, const T& next, const T filterCoeff) {
        T delta = next - cur;
        if (math::abs(delta) < math::F_MIN) {
            cur = next;
        } else {
            cur += filterCoeff * delta;
        }
    }

    static void processStereo(float** inputs, float** outputs, VstInt32 sampleFrames, const float filterCoeff, ProgramParameters& params, const ProgramParameters nextParams) {
        float* out1 = outputs[0];
        float* out2 = outputs[1];
        float* in1  = inputs[0];
        float* in2  = inputs[1];
        for (int a = 0; a < sampleFrames; a++) {
            updateParam(params.gain, nextParams.gain, filterCoeff);
            updateParam(params.width, nextParams.width, filterCoeff);
            float fGain = 1.0f;
            dsp_util::getGainLvlWithRange(params.gain, MTR_CEIL, DBFS_MUTE_POS, fGain);
            float width       = params.width;
            float scaleMono   = 1.0f - math::max(0.0f, (width - 0.5f) * 2.0f);
            float scaleStereo = math::min(1.0f, width * 2.0f);
            float channelL    = (*in1++);
            float channelR    = (*in2++);
            float stereo      = (channelL - channelR) / 2.0f;
            float mono        = (channelL + channelR) / 2.0f;
            stereo *= scaleStereo;
            mono *= scaleMono;
            float outL = mono + stereo;
            float outR = mono - stereo;
            (*out1++)  = outL * fGain;
            (*out2++)  = outR * fGain;
        }
    }

    module_stereowidth::module_stereowidth(int32_t _projectGlobalId, IHostCallback* _hostCallback)
        : internalplugin("Stereo Width", getModuleType(), _projectGlobalId, _hostCallback)
    {
        struct effectgain_param_entry {
            int32_t id;
            String name;
            String unit;
            float val;
        };
        const std::array<effectgain_param_entry, 2> parameterTypes{ {
                { PARAM_GAIN, "Gain", "dB", dsp_util::gainToLinScaleWithRange(1.0f, MTR_CEIL, DBFS_MUTE_POS) },
                { PARAM_WIDTH,  "Width",  "%", 0.5f }
        } };
        for (const auto& paramEntry : parameterTypes) {
            registerParam(paramEntry.id)->initValue(paramEntry);
        }
    }

    void module_stereowidth::onEnable() {
        paramsTarget.gain = getParamValue(PARAM_GAIN);
        paramsTarget.width = getParamValue(PARAM_WIDTH);
        paramsSmoothed = paramsTarget;
    }

    void module_stereowidth::process(const DAW::Host::Host* const host, AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) {
        dbgassert( in->samples == format.blockSize
                && out->samples == format.blockSize
                && format.blockSize > 0
                && format.sampleRate > 0);
        out->clear();
        paramsTarget.gain = getParamValue(PARAM_GAIN);
        paramsTarget.width = getParamValue(PARAM_WIDTH);
        dbgassert(in->channels >= 2 && out->channels >= 2);
        float fBlockFreq  = (format.sampleRate / float(format.blockSize)) * 0.45f;
        float filterCoeff = 1.0f - expf(-2.0f * M_PI * (fBlockFreq / format.sampleRate));
        processStereo(in->buf, out->buf, numSamples, filterCoeff, paramsSmoothed, paramsTarget);
    }

    param_converted_t module_stereowidth::convertParamValueDisplay(int32_t idx, const param_unit_t& displayValue) {
        //TODO: use std::from_chars when floating point version arrives in libc++
        auto fTextFieldVal = static_cast<float>(atof(StringAsCStr(displayValue.value)));
        switch (idx) {
            case PARAM_GAIN: {

                if (fTextFieldVal <= DBFS_MUTE_POS + 1.0f)
                    fTextFieldVal = 0.0f;
                if (fTextFieldVal > MTR_CEIL)
                    fTextFieldVal = MTR_CEIL;
                float f_gain = pow(10.0f, fTextFieldVal / 20.0f);
                float f_linear = dsp_util::gainToLinScaleWithRange(f_gain, MTR_CEIL, DBFS_MUTE_POS);
                return {f_linear, true};
            }
            case PARAM_WIDTH:
            {
                return {math::clamp(fTextFieldVal/200.0f, 0.0f, 1.0f), true};
            }
            default:
                break;
        }
        return internalplugin::convertParamValueDisplay(idx, displayValue);
    }
    param_unit_t module_stereowidth::convertParamValueToDisplay(int32_t idx, float value) {
        auto param = getParam(idx);
        dbgassert(param);
        if (param->unit == "dB") {
            float fGain = 1.0f;
            if (dsp_util::getGainLvlWithRange(value, MTR_CEIL, DBFS_MUTE_POS, fGain)) {
                return {StringFormat("%.3f", dsp_util::dBFS(fGain)), param->unit};
            }
            return {"-INF", param->unit};
        }
        if (param->unit == "%") {
            return {StringFormat("%.3f", value * 200.0f), param->unit};
        }
        return internalplugin::convertParamValueToDisplay(idx, value);
    }

    std::shared_ptr<PluginViewContainer> module_stereowidth::createViewCtrInternal() {
        return std::make_shared<PluginViewContainerBasic<guictr_vst2_simple, module_stereowidth>>(this, 100, 150);
    }
}// namespace PluginStereoWidth

template<>
effectbase* makeInstance<PluginStereoWidth::module_stereowidth>(int32_t _projectGlobalId, IHostCallback* _hostCallback) {
    return new PluginStereoWidth::module_stereowidth(_projectGlobalId, _hostCallback);
}

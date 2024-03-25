#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include "assert_dbg.h"
#include "host/audiobuffer/audioblock.h"
#include "config.h"
#include "dsp_util.h"
#include "dsp_util.h"
#include "event.h"
#include "gui/container/container.h"
#include "gui/controls/knoblabeled.h"
#include "gui/controls/knobpluginparam.h"
#include "gui/plugin/plugin.h"
#include "gui/plugin/pluginctr.h"
#include "gui/plugin/pluginviewcontainers.h"
#include "host/daw/mainctrl.h"
#include "host/plugin/internal/internal-plugin.h"
#include "logging.h"
#include "math/seq_math.h"
#include "host/meter/meter.h"
#include "host/plugin/modules.h"
#include "platform.h"
#include "plugins/plugin-base.h"
#include "plugins/plugin-ui.h"
#include "plugins/plugin-ui.h"
#include "plugins/plugin-window.h"
#include "plugins/plugin.h"
#include "plugins/plugincontrol.h"
#include "sampledelay-plugin.h"
#include "snapshot/snapshot.h"
#include "str_util.h"
#include "str_util.h"
#include "host/track/track_impl.h"
#include "host/track/track.h"
#include "window.h"

namespace PluginSampleDelay {
    samplecount_t convertToSamples(float value) {
        return math::clamp<samplecount_t>(math::floorfS32(MIN_DELAY + value * (MAX_DELAY - MIN_DELAY)), MIN_DELAY, MAX_DELAY);
    }

    template<typename T>
    inline void updateParam(T& cur, const T& next, const T filterCoeff) {
        T delta = next - cur;
        if (math::abs(delta) < math::F_MIN) {
            cur = next;
        } else {
            cur += filterCoeff * delta;
        }
    }

    void processStereo(AudioBlock* inputBlock, AudioBlock* outputBlock, DelayLine* delayLine, VstInt32 sampleFrames, const float filterCoeff, plugin_params_t& params, const plugin_params_t nextParams) {
        float* out1 = outputBlock->buf[0];
        float* out2 = outputBlock->buf[1];
        constexpr auto DELAYLINE_SIZE = (MAX_DELAY - MIN_DELAY);
        delayLine->write(inputBlock, DELAYLINE_SIZE * 2); // this probably shouldn't have to be twice the size
        auto& delayBlock = delayLine->getBlock();
        dbgassert(DELAYLINE_SIZE <= delayBlock.samples);
        const auto writeOffset = delayLine->getWriteOffset();
        for (samplecount_t smpPos = 0; smpPos < sampleFrames; smpPos++) {
            updateParam(params.delay, nextParams.delay, filterCoeff);
            auto delay = math::clamp<samplecount_t>(math::floorfS32(params.delay * DELAYLINE_SIZE), 0, DELAYLINE_SIZE-1);
            dbgassert(delay >= 0 && delay < DELAYLINE_SIZE);
            auto readPos = writeOffset - delay;
            readPos += smpPos;
            if (readPos < 0) {
                readPos += delayBlock.samples;
            }
            if (readPos >= delayBlock.samples) {
                readPos -= delayBlock.samples;
            }
            dbgassert(readPos >= 0);
            dbgassert(readPos + 1 <= delayBlock.samples);
            (*out1++)  = *(delayBlock.buf[0] + readPos);
            (*out2++)  = *(delayBlock.buf[1] + readPos);
        }
    }

    static constexpr int32_t PARAM_DELAY = 1;

    module_sampledelay::module_sampledelay(int32_t _projectGlobalId, IHostCallback* _hostCallback)
        : internalplugin("Sample Delay", _projectGlobalId, _hostCallback)
    {
        struct effectgain_param_entry {
            int32_t id;
            String name;
            String unit;
            float val;
        };
        const std::array<effectgain_param_entry, 1> parameterTypes{ {
            { PARAM_DELAY, "Delay", "samples",  0.5f }
        } };
        for (const auto& paramEntry : parameterTypes) {
            registerParam(paramEntry.id)->initValue(paramEntry);
        }
    }

    void module_sampledelay::postSetParameter(int32_t idx, float preVal, float val, int flags) {
        internalplugin::postSetParameter(idx, preVal, val, flags);
    }

    void module_sampledelay::process(const DAW::Host::Host* const host, AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) {
        dbgassert( in->samples == format.blockSize
                && out->samples == format.blockSize
                && format.blockSize > 0
                && format.sampleRate > 0);
        out->clear();
        this->paramsTarget.delay = getParamValue(PARAM_DELAY);
        dbgassert(in->channels >= 2 && out->channels >= 2);
        float fBlockFreq  = (format.sampleRate / float(format.blockSize)) * 0.15f;
        float filterCoeff = 1.0f - expf(-2.0f * static_cast<float>(M_PI) * (fBlockFreq / format.sampleRate));
        processStereo(in, out, delayLine.get(), numSamples, filterCoeff, paramsSmoothed, paramsTarget);
    }

    param_converted_t module_sampledelay::convertParamValueDisplay(int32_t idx, const param_unit_t& displayValue) {
        //TODO: use std::from_chars when floating point version arrives in libc++
        auto fTextFieldVal = static_cast<float>(atof(StringAsCStr(displayValue.value)));
        switch (idx) {
            case PARAM_DELAY: {
                return {math::clamp(math::clamp(math::roundfS64(fTextFieldVal), MIN_DELAY, MAX_DELAY)/static_cast<float>(MAX_DELAY-MIN_DELAY) + 0.5f, 0.0f, 1.0f), true};
            }
            default:
                break;
        }
        return internalplugin::convertParamValueDisplay(idx, displayValue);
    }

    param_unit_t module_sampledelay::convertParamValueToDisplay(int32_t idx, float value) {
        auto param = getParam(idx);
        dbgassert(param);
        if (param->idx == PARAM_DELAY) {
            auto delaySamples = convertToSamples(value);
            return {StringFormat("%zd", delaySamples), param->unit};
        }
        return internalplugin::convertParamValueToDisplay(idx, value);
    }

    std::shared_ptr<PluginViewContainer> module_sampledelay::createViewCtrInternal() {
        return std::make_shared<PluginViewContainerBasic<guictr_plugin_basic, module_sampledelay>>(this, 100, 150);
    }

    samplecount_t module_sampledelay::getPluginLatency() {
        return -MIN_DELAY;
    }

    void module_sampledelay::onEnable() {
        this->delayLine = std::make_unique<DelayLine>();
        paramsSmoothed.delay = paramsTarget.delay = getParamValue(PARAM_DELAY);
    }

} // namespace PluginSampleDelay

template<>
effectbase* makeInstance<PluginSampleDelay::module_sampledelay>(int32_t _projectGlobalId, IHostCallback* _hostCallback) {
    return new PluginSampleDelay::module_sampledelay(_projectGlobalId, _hostCallback);
}

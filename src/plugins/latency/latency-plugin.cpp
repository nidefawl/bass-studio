#include "latency-plugin.hpp"
#include "host/automation/automation.hpp"
#include "dsp_util.hpp"
#include "event.hpp"
#include "plugins/plugin-ui.hpp"
#include "plugins/plugincontrol.hpp"
#include "str_util.hpp"
#include "gui/container/container.hpp"
#include "gui/controls/knoblabeled.hpp"
#include "gui/controls/knobpluginparam.hpp"
#include "gui/plugin/plugin.hpp"
#include "gui/plugin/pluginctr.hpp"
#include "gui/plugin/pluginviewcontainers.hpp"
#include "host/plugin/modules.hpp"
#include "host/daw/mainctrl.hpp"
#include "host/plugin/internal/internal-plugin.hpp"
#include "host/track/track.hpp"
#include "host/track/track_impl.hpp"
#include "host/audiobuffer/audioblock.hpp"
#include "host/meter/meter.hpp"
#include "snapshot/snapshot.hpp"
#include "window.hpp"

namespace PluginLatency {

    static constexpr int32_t PARAM_LATENCY = 1;
    static constexpr int32_t MAX_LATENCY = 16384;

    module_latency::module_latency(int32_t _projectGlobalId, IHostCallback* _hostCallback)
        : internalplugin("Latency", _projectGlobalId, _hostCallback)
    {
        struct effectgain_param_entry {
            int32_t id;
            String name;
            String unit;
            float val;
        };
        const std::array<effectgain_param_entry, 1> parameterTypes{ {
            { PARAM_LATENCY, "Latency", "samples",  0.5f }
        } };
        for (const auto& paramEntry : parameterTypes) {
            registerParam(paramEntry.id)->initValue(paramEntry);
        }
    }

    void module_latency::postSetParameter(int32_t idx, float preVal, float val, int flags) {
        switch (idx) {
            case PARAM_LATENCY:
                if (!(flags & FLG_PAR_UPDATE_AUTOMATED)) {
                    setNewLatency(math::clamp(math::roundfS32(getParam(idx)->getValue() * MAX_LATENCY), 0, MAX_LATENCY));
                }
                break;
        }
        internalplugin::postSetParameter(idx, preVal, val, flags);
    }

    void module_latency::process(const DAW::Host::Host* const host, AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) {
        dbgassert( in->samples == format.blockSize
                && out->samples == format.blockSize
                && format.blockSize > 0
                && format.sampleRate > 0);
        if (this->latencyChanged) {
            this->latencyChanged  = false;
            this->curLatency      = this->newLatency;
            hostCallback->onLatencyChanged(this);
        }
        out->clear();
        delayAudio(this->delayLine.get(), in, out, this->curLatency);
    }

    param_converted_t module_latency::convertParamValueDisplay(int32_t idx, const param_unit_t& displayValue) {
        //TODO: use std::from_chars when floating point version arrives in libc++
        auto fTextFieldVal = static_cast<float>(atof(StringAsCStr(displayValue.value)));
        switch (idx) {
            case PARAM_LATENCY: {
                return {math::clamp(math::roundfS64(fTextFieldVal)/static_cast<float>(MAX_LATENCY), 0.0f, 1.0f), true};
            }
            default:
                break;
        }
        return internalplugin::convertParamValueDisplay(idx, displayValue);
    }

    param_unit_t module_latency::convertParamValueToDisplay(int32_t idx, float value) {
        auto param = getParam(idx);
        dbgassert(param);
        if (param->idx == PARAM_LATENCY) {
            return {StringFormat("%d", math::max(0, math::min(MAX_LATENCY, math::roundfS32(value * MAX_LATENCY)))), param->unit};
        }
        return internalplugin::convertParamValueToDisplay(idx, value);
    }

    std::shared_ptr<PluginViewContainer> module_latency::createViewCtrInternal() {
        return std::make_shared<PluginViewContainerBasic<guictr_plugin_basic, module_latency>>(this, 100, 150);
    }

    void module_latency::setNewLatency(int32_t nSamplesLatency) {
        this->newLatency     = nSamplesLatency;
        this->latencyChanged = true;
    }

    samplecount_t module_latency::getPluginLatency() {
        return this->curLatency;
    }

    void module_latency::onEnable() {
        this->delayLine = std::make_unique<DelayLine>();
        setNewLatency(math::clamp(math::roundfS32(getParam(PARAM_LATENCY)->getValue() * MAX_LATENCY), 0, MAX_LATENCY));
    }
} // namespace PluginLatency

template<>
effectbase* makeInstance<PluginLatency::module_latency>(int32_t _projectGlobalId, IHostCallback* _hostCallback) {
    return new PluginLatency::module_latency(_projectGlobalId, _hostCallback);
}


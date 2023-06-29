#include "visualizer-plugin.h"
#include "assert_dbg.h"
#include "gui/gui.h"
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

namespace lineplot {
    void enqueueAudioFromPlugin(guictr_base* ctr, AudioBlock* out);
}// namespace lineplot

namespace PluginVisualizer {

    static constexpr int32_t PARAM_LATENCY = 1;

    module_visualizer::module_visualizer(int32_t _projectGlobalId, IHostCallback* _hostCallback)
        : internalplugin("Visualizer", getModuleType(), _projectGlobalId, _hostCallback)
    {
        struct effectgain_param_entry {
            int32_t id;
            String name;
            String unit;
            float val;
        };
        const std::array<effectgain_param_entry, 1> parameterTypes{ {
            { PARAM_LATENCY, "Visualizer", "samples",  0.5f }
        } };
        for (const auto& paramEntry : parameterTypes) {
            registerParam(paramEntry.id)->initValue(paramEntry);
        }
        allocRingBuffer(ringbuffer, 2);
    }

    void module_visualizer::postSetParameter(int32_t idx, float preVal, float val, int flags) {
        // switch (idx) {
        //     case PARAM_LATENCY:
        //         if (!(flags & FLG_PAR_UPDATE_AUTOMATED)) {
        //             // set(math::clamp(math::roundfS32(getParam(idx)->getValue() * MAX_LATENCY), 0, MAX_LATENCY));
        //         }
        //         break;
        // }
        internalplugin::postSetParameter(idx, preVal, val, flags);
    }

    void module_visualizer::process(const DAW::Host::Host* const host, AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) {
        dbgassert( in->samples == format.blockSize
                && out->samples == format.blockSize
                && format.blockSize > 0
                && format.sampleRate > 0);
        out->copyFrom(in);
        enqueue(out);
    }

    void module_visualizer::enqueue(AudioBlock* buf) {
        auto& writePos = ringbuffer.writePos;
        AudioBuffer** buffers = ringbuffer.buffers;
        AudioBuffer* const qBuf = buffers[writePos%RING_BUF_SIZE];
        // dbgassert(!qBuf->inUse);
        qBuf->output->realloc(buf->samples);
        qBuf->output->copyFrom(buf);
        qBuf->inUse = true;
        this->audioQueue.enqueue(qBuf);
    }

    bool module_visualizer::try_dequeue(AudioBuffer*& buf) {
        auto success = this->audioQueue.try_dequeue(buf);
        if (success) {
            buf->time.samplePosOutput = audioQueueSamplePos;
            audioQueueSamplePos += buf->output->samples;
        }
        return success;
    }

    param_converted_t module_visualizer::convertParamValueDisplay(int32_t idx, const param_unit_t& displayValue) {
        // //TODO: use std::from_chars when floating point version arrives in libc++
        // auto fTextFieldVal = static_cast<float>(atof(StringAsCStr(displayValue.value)));
        // switch (idx) {
        //     case PARAM_LATENCY: {
        //         return {math::clamp(math::roundfS64(fTextFieldVal)/static_cast<float>(MAX_LATENCY), 0.0f, 1.0f), true};
        //     }
        //     default:
        //         break;
        // }
        return internalplugin::convertParamValueDisplay(idx, displayValue);
    }

    param_unit_t module_visualizer::convertParamValueToDisplay(int32_t idx, float value) {
    //     auto param = getParam(idx);
    //     dbgassert(param);
    //     if (param->idx == PARAM_LATENCY) {
    //         return {StringFormat("%d", math::max(0, math::min(MAX_LATENCY, math::roundfS32(value * MAX_LATENCY)))), param->unit};
    //     }
        return internalplugin::convertParamValueToDisplay(idx, value);
    }

    std::shared_ptr<PluginViewContainers> module_visualizer::createViewCtrInternal() {
        return std::make_shared<SinglePluginViewContainers<guictr_vst2_simple, module_visualizer>>(this, 100, 150);
    }
} // namespace PluginVisualizer

template<>
effectbase* makeInstance<PluginVisualizer::module_visualizer>(int32_t _projectGlobalId, IHostCallback* _hostCallback) {
    return new PluginVisualizer::module_visualizer(_projectGlobalId, _hostCallback);
}


#include "visualizer-plugin.h"
#include "assert_dbg.h"
#include "gui/gui.h"
#include "host/automation/automation.h"
#include "dsp_util.h"
#include "event.h"
#include "logging.h"
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

namespace PluginVisualizer {

    module_visualizer::module_visualizer(int32_t _projectGlobalId, IHostCallback* _hostCallback)
        : internalplugin("Visualizer", getModuleType(), _projectGlobalId, _hostCallback)
    {
        allocRingBuffer(ringbuffer, 2);
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
        writePos = (writePos+1) & RING_BUF_MASK;
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

    std::shared_ptr<PluginViewContainer> module_visualizer::createViewCtrInternal() {
        return std::make_shared<PluginViewContainerBasic<guictr_plugin_basic, module_visualizer>>(this, 100, 150);
    }
} // namespace PluginVisualizer

template<>
effectbase* makeInstance<PluginVisualizer::module_visualizer>(int32_t _projectGlobalId, IHostCallback* _hostCallback) {
    return new PluginVisualizer::module_visualizer(_projectGlobalId, _hostCallback);
}


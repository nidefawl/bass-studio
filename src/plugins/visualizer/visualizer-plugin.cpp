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
#include "tls.h"
#include "window.h"

namespace DAW::UI {
    guictr_base* MakeAudioVisualizer(::PluginVisualizer::module_visualizer* const eff);
} // namespace DAW::UI

namespace PluginVisualizer {

    module_visualizer::module_visualizer(int32_t _projectGlobalId, IHostCallback* _hostCallback)
        : internalplugin("Visualizer", _projectGlobalId, _hostCallback)
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


    class PluginViewContainerVisualizer final : public PluginViewContainer {
    public:
        module_visualizer* const eff;
        guictr_base* ctr_main = nullptr;
        explicit PluginViewContainerVisualizer(module_visualizer* eff)
            : eff(eff) {
        }
        ~PluginViewContainerVisualizer() override = default;
        void layout(int32_t winW, int32_t winH) override {
            ctr_main->pos  = { 0, 0 };
            ctr_main->size = { winW, winH };
        }
        void addTo(std::vector<guictr_base*>& v) override {
            v.push_back(ctr_main);
        }
        void setFree() override {
            if (ctr_main) {
                dbgassert(!ctr_main->getControl());
                delete ctr_main;
                ctr_main = nullptr;
            }
            PluginViewContainer::setFree();
        }
        void setUsed() override {
            if (!ctr_main)
                ctr_main = DAW::UI::MakeAudioVisualizer(eff);
            PluginViewContainer::setUsed();
        }
        void onGuiOpen() override {
        }
        void onGuiClose() override {
        }
        void onSetParameter(int32_t index, float value) override {
        }
        void getFixedSize(int32_t* w, int32_t* h) override {
            *w = 1920*0.8;
            *h = 1080*0.8;
        }
        bool isViewSupported(int32_t uiId) const override {
            return uiId == UID_VIEW_CTR_WINDOW;
        }
    };

    std::shared_ptr<PluginViewContainer> module_visualizer::createViewCtrInternal() {
        return std::make_shared<PluginViewContainerVisualizer>(this);
    }
} // namespace PluginVisualizer

template<>
effectbase* makeInstance<PluginVisualizer::module_visualizer>(int32_t _projectGlobalId, IHostCallback* _hostCallback) {
    return new PluginVisualizer::module_visualizer(_projectGlobalId, _hostCallback);
}


#pragma once
#include "str_util.hpp"
#include "host/plugin/modules.hpp"
#include "host/plugin/internal/internal-plugin.hpp"
#include "host/audiobuffer/audiobuffer.hpp"
#include <readerwriterqueue/readerwritercircularbuffer.hpp>

namespace PluginVisualizer {
class module_visualizer final : public internalplugin {
public:
    explicit module_visualizer(int32_t _projectGlobalId, IHostCallback* _hostCallback);

    PluginType getPluginType() override { return PLUGIN_TYPE_VISUALIZER; };
    void process(const DAW::Host::Host* const host, AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) override;
    std::shared_ptr<PluginViewContainer> createViewCtrInternal() override;
    void enqueue(AudioBlock* buf);
    bool try_dequeue(AudioBuffer*& buf);
    int32_t getOutputQueueSize() const {
        return static_cast<int32_t>(audioQueue.size_approx());
    }
private:
    audiothread_ringbuffer_t ringbuffer;
    moodycamel::BlockingReaderWriterCircularBuffer<AudioBuffer*> audioQueue;
    samplecount_t audioQueueSamplePos = 0;
};
}

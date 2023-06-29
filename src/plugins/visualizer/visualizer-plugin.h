#pragma once
#include "str_util.h"
#include "host/plugin/modules.h"
#include "host/plugin/internal/internal-plugin.h"
#include "host/audiobuffer/audiobuffer.h"
#include <readerwriterqueue/readerwriterqueue.hpp>

namespace PluginVisualizer {
class module_visualizer final : public internalplugin {
public:
    explicit module_visualizer(int32_t _projectGlobalId, IHostCallback* _hostCallback);

    void postSetParameter(int32_t idx, float preVal, float val, int flags) override;
    int getModuleType() override { return PLUGIN_TYPE_VISUALIZER; };
    void process(const DAW::Host::Host* const host, AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) override;
    param_converted_t convertParamValueDisplay(int32_t idx, const param_unit_t& displayValue) override;
    param_unit_t convertParamValueToDisplay(int32_t idx, float value) override;
    std::shared_ptr<PluginViewContainers> createViewCtrInternal() override;
    void enqueue(AudioBlock* buf);
    bool try_dequeue(AudioBuffer*& buf);
    int32_t getOutputQueueSize() const {
        return static_cast<int32_t>(audioQueue.size_approx());
    }
private:
    audiothread_ringbuffer_t ringbuffer;
    moodycamel::ReaderWriterQueue<AudioBuffer*> audioQueue;
    samplecount_t audioQueueSamplePos = 0;
};
}

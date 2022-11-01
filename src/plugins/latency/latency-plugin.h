#pragma once
#include "str_util.h"
#include "host/plugin/modules.h"
#include "host/plugin/internal/internal-plugin.h"

namespace PluginLatency {
class module_latency : public internalplugin {
public:
    const float DBFS_MUTE_POS = -101.0f;
    const float MTR_CEIL      = 24.0f;
    explicit module_latency(int32_t _projectGlobalId, IHostCallback* _hostCallback);

    void postSetParameter(int32_t idx, float preVal, float val, int flags) override;
    int getModuleType() override { return PLUGIN_TYPE_LATENCY; };
    samplecount_t getPluginLatency() override;
    void process(const DAW::Host::Host* const host, AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) override;
    param_converted_t convertParamValueDisplay(int32_t idx, const param_unit_t& displayValue) override;
    param_unit_t convertParamValueToDisplay(int32_t idx, float value) override;
    std::shared_ptr<PluginViewContainers> createViewCtrInternal() override;
    void onEnable() override;
private:
    void setNewLatency(int32_t nSamplesLatency);
    std::unique_ptr<DelayLine> delayLine = nullptr;
    int32_t curLatency   = 0;
    int32_t newLatency   = 0;
    std::atomic<bool> latencyChanged{ false };
};
}

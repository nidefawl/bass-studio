#pragma once
#include "host/plugin/modules.h"
#include "host/plugin/internal/internal-plugin.h"

namespace PluginDelay {
class EffectImplDelay;
class module_delay final : public internalplugin {
public:
    EffectImplDelay* impl;
    explicit module_delay(int32_t _projectGlobalId, IHostCallback* _hostCallback);
    ~module_delay() override;

    PluginType getPluginType() override { return PLUGIN_TYPE_TAPE_DELAY; };
    void process(const DAW::Host::Host* const host, AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) override;
    std::shared_ptr<PluginViewContainer> createViewCtrInternal() override;
    param_converted_t convertParamValueDisplay(int32_t idx, const param_unit_t& displayValue) override;
    param_unit_t convertParamValueToDisplay(int32_t idx, float value) override;
};

} // namespace PluginDelay

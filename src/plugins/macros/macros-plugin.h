#pragma once
#include "str_util.h"
#include "modules.h"
#include "host/plugin/internal_plugin.h"
#include "plugins/plugin-ui.h"

namespace PluginMacros {
class module_macros : public internal_automator {
    struct internal_handles_t {
    };
public:
    explicit module_macros(int32_t _projectGlobalId, IHostCallback* _hostCallback);
    ~module_macros() override;

    int getModuleType() override { return PLUGIN_TYPE_MACROS; };
    bool hasAutomationModulationOutput() const override {
        return true;
    }
    void process(AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) override;
    param_converted_t convertParamValueDisplay(int32_t idx, const param_unit_t& displayValue) override {
        return internal_automator::convertParamValueDisplay(idx, displayValue);
    };
    param_unit_t convertParamValueToDisplay(int32_t idx, float value) override {
        return internal_automator::convertParamValueToDisplay(idx, value);
    }
    std::shared_ptr<PluginViewContainers> createViewCtrInternal() override;
};
}

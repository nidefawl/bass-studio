#pragma once
#include "str_util.hpp"
#include "host/plugin/modules.hpp"
#include "host/plugin/internal/internal-plugin.hpp"

namespace PluginStereoWidth {
class ProgramParameters {
public:
    float width;
    float gain;
};
class module_stereowidth final : public internalplugin {
public:
    explicit module_stereowidth(int32_t _projectGlobalId, IHostCallback* _hostCallback);

    PluginType getPluginType() override { return PLUGIN_TYPE_STEREO_WIDTH; };
    void process(const DAW::Host::Host* const host, AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) override;
    param_converted_t convertParamValueDisplay(int32_t idx, const param_unit_t& displayValue) override;
    param_unit_t convertParamValueToDisplay(int32_t idx, float value) override;
    std::shared_ptr<PluginViewContainer> createViewCtrInternal() override;
    void onEnable() override;
private:
    ProgramParameters paramsTarget{};
    ProgramParameters paramsSmoothed{};
};
}

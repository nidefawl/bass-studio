#pragma once
#include "str_util.hpp"
#include "host/plugin/modules.hpp"
#include "host/plugin/internal/internal-plugin.hpp"

namespace PluginGain {
struct impl_data_t;
class module_gain final : public internalplugin {
    impl_data_t* impl;
public:
    const float DBFS_MUTE_POS = -101.0f;
    const float MTR_CEIL      = 24.0f;
    explicit module_gain(int32_t _projectGlobalId, IHostCallback* _hostCallback);
    ~module_gain() override;

    PluginType getPluginType() override { return PLUGIN_TYPE_GAIN; };
    void process(const DAW::Host::Host* const host, AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) override;
    param_converted_t convertParamValueDisplay(int32_t idx, const param_unit_t& displayValue) override;
    param_unit_t convertParamValueToDisplay(int32_t idx, float value) override;
    void makeSnapshot(plugin_snapshot_t& ps, const tracksnapshot_store_opts_t& opts) override;
    void loadSnapshot(const plugin_snapshot_t& snapshot) override;
    std::shared_ptr<PluginViewContainer> createViewCtrInternal() override;
};

} // namespace PluginGain

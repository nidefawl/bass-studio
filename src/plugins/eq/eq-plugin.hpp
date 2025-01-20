#pragma once
#include "str_util.hpp"
#include "host/plugin/modules.hpp"
#include "host/plugin/internal/internal-plugin.hpp"
#include "plugins/eq/filter-coeffs.hpp"

namespace PluginEQ {
struct impl_data_t;
class module_eq final : public internalplugin {
    impl_data_t* impl;
public:
    explicit module_eq(int32_t _projectGlobalId, IHostCallback* _hostCallback);
    ~module_eq() override;

    PluginType getPluginType() override { return PLUGIN_TYPE_EQ; };
    void process(const DAW::Host::Host* const host, AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) override;
    param_converted_t convertParamValueDisplay(int32_t idx, const param_unit_t& displayValue) override;
    param_unit_t convertParamValueToDisplay(int32_t idx, float value) override;
    void makeSnapshot(plugin_snapshot_t& ps, const tracksnapshot_store_opts_t& opts) override;
    void loadSnapshot(const plugin_snapshot_t& snapshot) override;
    std::shared_ptr<PluginViewContainer> createViewCtrInternal() override;
    impl_data_t* getImpl() const { return impl; }
    bool isBandEnabled(int32_t bandIdx);
    void setSampleFormat(sampleformat_t sampleFormat) override;
    void initBuffers() override;
    void onTick(double since) override;
    samplecount_t getPluginLatency() override;
};

} // namespace PluginEQ

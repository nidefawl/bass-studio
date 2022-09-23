#pragma once
#include <vector>
#include <cmath>
#include "../plugin-base.h"
#include "host/plugin/internal_plugin.h"


namespace PluginSampleCrush {

    static constexpr int32_t BITCRUSH_BITS_MIN = 0;
    static constexpr int32_t BITCRUSH_BITS_MAX = 4;

    class module_samplecrush : public internalplugin {

    public:
        explicit module_samplecrush(int32_t _projectGlobalId, i_host_callback* _hostCallback);
        ~module_samplecrush() override;

        void postSetParameter(int32_t idx, float preVal, float val, int flags) override;
        int getModuleType() override { return PLUGIN_TYPE_SAMPLE_CRUSH; };
        samplecount_t getPluginLatency() override;
        void process(AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) override;
        param_converted_t convertParamValueDisplay(int32_t idx, const param_unit_t& displayValue) override;
        param_unit_t getParamValueDisplay(int32_t idx) override;
        void makeSnapshot(plugin_snapshot_t& ps, const tracksnapshot_store_opts_t& opts) override;
        void loadSnapshot(const plugin_snapshot_t& snapshot) override;
        void postProcess(AudioBlock* out, int32_t samples, bool hasProcessed) override;
        std::shared_ptr<PluginViewContainers> createInternalView() override;
        void onEnable() override;
    };
}// namespace PluginBitcrush

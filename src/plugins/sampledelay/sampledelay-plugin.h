#pragma once
#include <vector>
#include <cmath>
#include "../plugin-base.h"
#include "host/plugin/internal_plugin.h"


namespace PluginSampleDelay {
    static constexpr samplecount_t MIN_DELAY = -16*1024;
    static constexpr samplecount_t MAX_DELAY = 16*1024;
        
    struct plugin_params_t {
            float delay;
    };

    class module_sampledelay : public internalplugin {

    public:
        explicit module_sampledelay(int32_t _projectGlobalId, i_host_callback* _hostCallback);

        void postSetParameter(int32_t idx, float preVal, float val, int flags) override;
        int getModuleType() override { return PLUGIN_TYPE_SAMPLE_DELAY; };
        samplecount_t getPluginLatency() override;
        void process(AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) override;
        param_converted_t convertParamValueDisplay(int32_t idx, const param_unit_t& displayValue) override;
        param_unit_t getParamValueDisplay(int32_t idx) override;
        std::shared_ptr<PluginViewContainers> createViewCtrInternal() override;
        void onEnable() override;
    private:
        plugin_params_t paramsSmoothed{};
        plugin_params_t paramsTarget{};
        std::unique_ptr<DelayLine> delayLine = nullptr;
    };
}// namespace PluginSampleDelay

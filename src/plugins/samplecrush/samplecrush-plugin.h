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
        explicit module_samplecrush(int32_t _projectGlobalId, IHostCallback* _hostCallback);

        int getModuleType() override { return PLUGIN_TYPE_SAMPLE_CRUSH; };
        void process(const DAW::Host::Host* const host, AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) override;
        param_converted_t convertParamValueDisplay(int32_t idx, const param_unit_t& displayValue) override;
        param_unit_t convertParamValueToDisplay(int32_t idx, float value) override;
        std::shared_ptr<PluginViewContainers> createViewCtrInternal() override;
    };
}// namespace PluginBitcrush

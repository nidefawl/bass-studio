#pragma once
#include "airwindows-types.hpp"
#include "host/plugin/internal/internal-plugin.h"
#include "host/plugin/modules.h"

namespace PluginAirWindows {

    class IEffectImpl;

    class module_airwindows final : public internalplugin {
        IEffectImpl* impl;
    public:
        explicit module_airwindows(PluginType type, int32_t _projectGlobalId, IHostCallback* _hostCallback);
        ~module_airwindows() override;
        PluginType getPluginType() override;
        void process(const DAW::Host::Host* const host, AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) override;
        std::shared_ptr<PluginViewContainer> createViewCtrInternal() override;
    };
    
} // namespace PluginAirWindows
    
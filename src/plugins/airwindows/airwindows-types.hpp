#pragma once
#include "host/plugin/modules.hpp"
#include "types.hpp"
#include "str_util.hpp"
#include "host/plugin/internal/internal-plugin.hpp"
#include <vector>

namespace PluginAirWindows {
    struct paramentry {
        int32_t id;
        String name;
        String unit;
        float val;
    };
    class IEffectImpl {
    public:
        virtual ~IEffectImpl() = default;
        virtual String getName() = 0;
        virtual void registerParams(std::vector<paramentry>& parameterTypes) = 0;
        virtual void processReplacing(float** inputs, float** outputs, samplecount_t sampleFrames, samplerate_t sampleRate) = 0;
        virtual void setParameters(internalplugin* plugin) = 0;
        virtual PluginType getPluginType() = 0;
    };
    
    effectbase* makeModuleAirWindowsInstance(PluginType type, int32_t globalid, IHostCallback* hostcallback);
} // namespace PluginAirWindows
    
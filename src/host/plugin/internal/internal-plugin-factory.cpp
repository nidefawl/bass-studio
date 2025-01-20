#include "host/plugin/base/base-plugin.hpp"
#include "host/plugin/empty/empty.hpp"
#include "host/plugin/group/group.hpp"
#include "plugins/eq/eq-plugin.hpp"
#include "plugins/gain/gain-plugin.hpp"
#include "plugins/latency/latency-plugin.hpp"
#include "plugins/eq/eq-plugin.hpp"
#include "plugins/lfo/lfo-plugin.hpp"
#include "plugins/macros/macros-plugin.hpp"
#include "plugins/samplecrush/samplecrush-plugin.hpp"
#include "plugins/sampledelay/sampledelay-plugin.hpp"
#include "plugins/stereowidth/stereowidth-plugin.hpp"
#include "host/host_pluginmanager.hpp"
#include "host/plugin/modules.hpp"
#include "plugins/visualizer/visualizer-plugin.hpp"
#include "plugins/tapedelay/tapedelay-plugin.hpp"

extern template effectbase* makeInstance<module_empty>(int32_t _projectGlobalId, IHostCallback* _hostCallback);
extern template effectbase* makeInstance<module_group>(int32_t _projectGlobalId, IHostCallback* _hostCallback);
extern template effectbase* makeInstance<PluginGain::module_gain>(int32_t _projectGlobalId, IHostCallback* _hostCallback);
extern template effectbase* makeInstance<PluginLatency::module_latency>(int32_t _projectGlobalId, IHostCallback* _hostCallback);
extern template effectbase* makeInstance<PluginSampleDelay::module_sampledelay>(int32_t _projectGlobalId, IHostCallback* _hostCallback);
extern template effectbase* makeInstance<PluginSampleCrush::module_samplecrush>(int32_t _projectGlobalId, IHostCallback* _hostCallback);
extern template effectbase* makeInstance<PluginStereoWidth::module_stereowidth>(int32_t _projectGlobalId, IHostCallback* _hostCallback);
extern template effectbase* makeInstance<PluginMacros::module_macros>(int32_t _projectGlobalId, IHostCallback* _hostCallback);
extern template effectbase* makeInstance<PluginLFO::module_lfo>(int32_t _projectGlobalId, IHostCallback* _hostCallback);
extern template effectbase* makeInstance<PluginEQ::module_eq>(int32_t _projectGlobalId, IHostCallback* _hostCallback);
extern template effectbase* makeInstance<PluginDelay::module_delay>(int32_t _projectGlobalId, IHostCallback* _hostCallback);

namespace PluginSynth {
class module_synth_unison;
namespace Mono {
class module_synth_mono;
}
namespace Shaper {
class module_synth_shaper;
}
namespace GPU {
class module_synth_gpu;
}
namespace KickXP {
class module_synth_kickxp;
}
}
namespace PluginAirWindows {
class module_airwindows;
effectbase* makeModuleAirWindowsInstance(PluginType type, int32_t globalid, IHostCallback* hostcallback);
}
extern template effectbase* makeInstance<PluginSynth::module_synth_unison>(int32_t _projectGlobalId, IHostCallback* _hostCallback);
extern template effectbase* makeInstance<PluginSynth::Shaper::module_synth_shaper>(int32_t _projectGlobalId, IHostCallback* _hostCallback);
extern template effectbase* makeInstance<PluginSynth::Mono::module_synth_mono>(int32_t _projectGlobalId, IHostCallback* _hostCallback);
extern template effectbase* makeInstance<PluginSynth::GPU::module_synth_gpu>(int32_t _projectGlobalId, IHostCallback* _hostCallback);
extern template effectbase* makeInstance<PluginSynth::KickXP::module_synth_kickxp>(int32_t _projectGlobalId, IHostCallback* _hostCallback);
namespace DAW::Host {

effectbase* PluginManager::makeModuleInstance(int32_t moduleType, int32_t moduleId, int32_t globalid) {
    effectbase* effect = nullptr;
    IHostCallback* hostcallback = getHostCallback();
    if (moduleType == MODULE_TYPE_INTERNAL_EFFECT) {
        switch (moduleId) {
            case PLUGIN_TYPE_EMPTY:
                effect = makeInstance<module_empty>(getNextGlobalModuleId(globalid), hostcallback);
                break;
            case PLUGIN_TYPE_GROUP:
                effect = makeInstance<module_group>(getNextGlobalModuleId(globalid), hostcallback);
                break;
            case PLUGIN_TYPE_GAIN:
                effect = makeInstance<PluginGain::module_gain>(getNextGlobalModuleId(globalid), hostcallback);
                break;
            case PLUGIN_TYPE_LATENCY:
                effect = makeInstance<PluginLatency::module_latency>(getNextGlobalModuleId(globalid), hostcallback);
                break;
            case PLUGIN_TYPE_SAMPLE_DELAY:
                effect = makeInstance<PluginSampleDelay::module_sampledelay>(getNextGlobalModuleId(globalid), hostcallback);
                break;
            case PLUGIN_TYPE_SAMPLE_CRUSH:
                effect = makeInstance<PluginSampleCrush::module_samplecrush>(getNextGlobalModuleId(globalid), hostcallback);
                break;
            case PLUGIN_TYPE_STEREO_WIDTH:
                effect = makeInstance<PluginStereoWidth::module_stereowidth>(getNextGlobalModuleId(globalid), hostcallback);
                break;
            case PLUGIN_TYPE_SYNTH:
                effect = makeInstance<PluginSynth::module_synth_unison>(getNextGlobalModuleId(globalid), hostcallback);
                break;
            case PLUGIN_TYPE_MACROS:
                effect = makeInstance<PluginMacros::module_macros>(getNextGlobalModuleId(globalid), hostcallback);
                break;
            case PLUGIN_TYPE_LFO:
                effect = makeInstance<PluginLFO::module_lfo>(getNextGlobalModuleId(globalid), hostcallback);
                break;
            case PLUGIN_TYPE_EQ:
                effect = makeInstance<PluginEQ::module_eq>(getNextGlobalModuleId(globalid), hostcallback);
                break;
            case PLUGIN_TYPE_VISUALIZER:
                effect = makeInstance<PluginVisualizer::module_visualizer>(getNextGlobalModuleId(globalid), hostcallback);
                break;
            case PLUGIN_TYPE_SYNTH_MONO:
                effect = makeInstance<PluginSynth::Mono::module_synth_mono>(getNextGlobalModuleId(globalid), hostcallback);
                break;
            case PLUGIN_TYPE_SYNTH_SHAPER:
                effect = makeInstance<PluginSynth::Shaper::module_synth_shaper>(getNextGlobalModuleId(globalid), hostcallback);
                break;
            case PLUGIN_TYPE_SYNTH_GPU:
                effect = makeInstance<PluginSynth::GPU::module_synth_gpu>(getNextGlobalModuleId(globalid), hostcallback);
                break;
            case PLUGIN_TYPE_SYNTH_KICKXP:
                effect = makeInstance<PluginSynth::KickXP::module_synth_kickxp>(getNextGlobalModuleId(globalid), hostcallback);
                break;
            case PLUGIN_TYPE_TAPE_DELAY:
                effect = makeInstance<PluginDelay::module_delay>(getNextGlobalModuleId(globalid), hostcallback);
                break;
            default:
                break;
        }
        if (moduleId > PLUGIN_TYPE_AIRWINDOWS_NONE && moduleId < PLUGIN_TYPE_AIRWINDOWS_MAX) {
            effect = PluginAirWindows::makeModuleAirWindowsInstance(static_cast<PluginType>(moduleId), getNextGlobalModuleId(globalid), hostcallback);
        }
        if (effect) {
            effect->load(this);
            pluginInstancesInternal.push_back(effect);
            pluginInstances.push_back(effect);
        }
    }
    return effect;
}

} // namespace DAW::Host

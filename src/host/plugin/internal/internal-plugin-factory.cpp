#include "host/plugin/base/base-plugin.h"
#include "host/plugin/empty/empty.h"
#include "host/plugin/group/group.h"
#include "plugins/eq/eq-plugin.h"
#include "plugins/gain/gain-plugin.h"
#include "plugins/latency/latency-plugin.h"
#include "plugins/eq/eq-plugin.h"
#include "plugins/lfo/lfo-plugin.h"
#include "plugins/macros/macros-plugin.h"
#include "plugins/samplecrush/samplecrush-plugin.h"
#include "plugins/sampledelay/sampledelay-plugin.h"
#include "plugins/stereowidth/stereowidth-plugin.h"
#include "host/host_pluginmanager.h"
#include "host/plugin/vst/vstplugin.h"
#include "host/plugin/modules.h"
#include "plugins/visualizer/visualizer-plugin.h"

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
}
extern template effectbase* makeInstance<PluginSynth::module_synth_unison>(int32_t _projectGlobalId, IHostCallback* _hostCallback);
extern template effectbase* makeInstance<PluginSynth::Shaper::module_synth_shaper>(int32_t _projectGlobalId, IHostCallback* _hostCallback);
extern template effectbase* makeInstance<PluginSynth::Mono::module_synth_mono>(int32_t _projectGlobalId, IHostCallback* _hostCallback);
extern template effectbase* makeInstance<PluginSynth::GPU::module_synth_gpu>(int32_t _projectGlobalId, IHostCallback* _hostCallback);
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
            default:
                break;
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

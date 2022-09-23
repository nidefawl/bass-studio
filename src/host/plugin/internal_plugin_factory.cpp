#include "base_plugin.h"
#include "empty.h"
#include "group.h"
#include "plugins/gain/gain-plugin.h"
#include "plugins/latency/latency-plugin.h"
#include "plugins/samplecrush/samplecrush-plugin.h"
#include "plugins/sampledelay/sampledelay-plugin.h"
#include "plugins/stereowidth/stereowidth-plugin.h"
#include "host/vst_host.h"
#include "host/plugin/vst_plugin.h"
#include "modules.h"

extern template effectbase* makeInstance<module_empty>(int32_t _projectGlobalId, i_host_callback* _hostCallback);
extern template effectbase* makeInstance<module_group>(int32_t _projectGlobalId, i_host_callback* _hostCallback);
extern template effectbase* makeInstance<module_gain>(int32_t _projectGlobalId, i_host_callback* _hostCallback);
extern template effectbase* makeInstance<PluginLatency::module_latency>(int32_t _projectGlobalId, i_host_callback* _hostCallback);
extern template effectbase* makeInstance<PluginSampleDelay::module_sampledelay>(int32_t _projectGlobalId, i_host_callback* _hostCallback);
extern template effectbase* makeInstance<PluginSampleCrush::module_samplecrush>(int32_t _projectGlobalId, i_host_callback* _hostCallback);
extern template effectbase* makeInstance<PluginStereoWidth::module_stereowidth>(int32_t _projectGlobalId, i_host_callback* _hostCallback);

effectbase* vsthost::makeModuleInstance(int32_t moduleType, int32_t moduleId, int32_t globalid) {
    effectbase* effect = nullptr;
    i_host_callback* hostcallback = getHostCallback();
    switch (moduleType) {
        case PLUGIN_TYPE_EMPTY:
            effect = makeInstance<module_empty>(getNextGlobalModuleId(globalid), hostcallback);
            break;
        case PLUGIN_TYPE_GROUP:
            effect = makeInstance<module_group>(getNextGlobalModuleId(globalid), hostcallback);
            break;
        case PLUGIN_TYPE_GAIN:
            effect = makeInstance<module_gain>(getNextGlobalModuleId(globalid), hostcallback);
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
        case PLUGIN_TYPE_INTERNAL_EFFECT: {
            vstpluginloadres res = loadInternalPlugin(moduleId, globalid);
            if (res.result == 0 && res.plugin) {
                effect = res.plugin;
            }
            break;
        } 
        default:
            break;
    }
    switch (moduleType) {
        case PLUGIN_TYPE_EMPTY:
        case PLUGIN_TYPE_GROUP:
        case PLUGIN_TYPE_GAIN:
        case PLUGIN_TYPE_LATENCY:
        case PLUGIN_TYPE_SAMPLE_DELAY:
        case PLUGIN_TYPE_SAMPLE_CRUSH:
        case PLUGIN_TYPE_STEREO_WIDTH:
            if (effect) {
                effect->load(this);
                pluginInstancesInternal.push_back(effect);
                pluginInstances.push_back(effect);
            }
            break;
        default:
            break;
    }
    return effect;
}

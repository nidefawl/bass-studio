#include "math/seq_math.h"
#include "str_util.h"
#include "vst_host.h"
#include "plugin/base_plugin.h"
#include "plugin/vst_plugin.h"
#include "plugin/vst_plugin_handles.h"

#include "plugins/advanced/adv-plugin.h"
#include "plugins/stereowidth/stereowidth-plugin.h"
#include "plugins/empty/empty-plugin.h"
#include "plugins/latency/latency-plugin.h"
#include "plugins/info/info-plugin.h"
#include "plugins/synth/synth-plugin.h"
#include "plugins/bitcrush/bitcrush-plugin.h"

typedef AudioEffectX* (*FnCreateModule)(audioMasterCallback);

void vsthost::registerPlugins() {
    int moduleId = 1000;
    builtinModules.clear();
    builtinModules.push_back({ moduleId++, false, PluginStereoWidth::getName(), PluginStereoWidth::createPlugin });
    builtinModules.push_back({ moduleId++, false, PluginTestAdv::getName(), PluginTestAdv::createPlugin });
    builtinModules.push_back({ moduleId++, false, PluginEmptyVST2::getName(), PluginEmptyVST2::createPlugin });
    builtinModules.push_back({ moduleId++, false, PluginLatency::getName(), PluginLatency::createPlugin });
    builtinModules.push_back({ moduleId++, false, PluginHostInfo::getName(), PluginHostInfo::createPlugin });
    builtinModules.push_back({ moduleId++, true, PluginSynth::getName(), PluginSynth::createPlugin });
    builtinModules.push_back({ moduleId++, false, PluginBitcrush::getName(), PluginBitcrush::createPlugin });
}

vstpluginloadres vsthost::loadInternalPlugin(int32_t moduleId, int32_t globalId) {
    auto it = std::find_if(builtinModules.begin(), builtinModules.end(), [moduleId](auto& reg) {
        return reg.id == moduleId;
    });

    if (it == builtinModules.end())
        return {-2, nullptr};

    builtin_module_reg_t& reg = *it;

    AudioEffectX* axeffect = reg.fnNewInstance(masterCallBackSlot);
    if (!axeffect) {
        return {-1, nullptr};
    }

    globalId = getNextGlobalModuleId(globalId);
    AEffect* aeffect  = axeffect->getAeffectHandle();
    auto* plugin = new vstplugin(new handles_t(axeffect, aeffect, nullptr), globalId, "", reg.name, moduleId);
    pluginInstancesVST2.push_back(plugin);
    pluginInstances.push_back(plugin);
    plugin->load(this);
    return {0, plugin};
}

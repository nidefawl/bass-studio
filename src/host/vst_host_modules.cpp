#include "math/seq_math.h"
#include "str_util.h"
#include "vst_host.h"
#include "modules.h"
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
#include "plugins/sampledelay/sampledelay-plugin.h"

typedef AudioEffectX* (*FnCreateModule)(audioMasterCallback);

void vsthost::registerPlugins() {
    builtinModules.clear();
    builtinModules.push_back({ PLUG_INT_STEREOWIDTH, false, PluginStereoWidth::getName(), PluginStereoWidth::createPlugin });
    builtinModules.push_back({ PLUG_INT_TEST, false, PluginTestAdv::getName(), PluginTestAdv::createPlugin });
    builtinModules.push_back({ PLUG_INT_CRASHVST, false, PluginEmptyVST2::getName(), PluginEmptyVST2::createPlugin });
    builtinModules.push_back({ PLUG_INT_HOSTINFO, false, PluginHostInfo::getName(), PluginHostInfo::createPlugin });
    builtinModules.push_back({ PLUG_INT_SYNTH, true, PluginSynth::getName(), PluginSynth::createPlugin });
    builtinModules.push_back({ PLUG_INT_BITCRUSH, false, PluginBitcrush::getName(), PluginBitcrush::createPlugin });
    builtinModules.push_back({ PLUG_INT_SAMPLE_DELAY, false, PluginSampleDelay::getName(), PluginSampleDelay::createPlugin });
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
    auto* baseVst2 = dynamic_cast<BasePluginVST2*>(axeffect);
    auto* plugin = new vstplugin(new handles_t(baseVst2, aeffect, nullptr), globalId, getHostCallback(), "", reg.name, moduleId, 0);
    if (baseVst2) {
        baseVst2->setHostSideHandle(plugin);
    }
    aeffect->user = plugin;
    pluginInstancesVST2.push_back(plugin);
    pluginInstances.push_back(plugin);
    plugin->load(this);
    return {0, plugin};
}

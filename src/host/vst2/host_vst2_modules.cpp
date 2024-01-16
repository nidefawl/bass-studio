#include "math/seq_math.h"
#include "str_util.h"
#include "host/host_pluginmanager.h"
#include "host/plugin/modules.h"
#include "host/plugin/base/base-plugin.h"
#include "host/plugin/vst/vstplugin.h"
#include "host/plugin/vst/vstplugin-handles.h"

#include "plugins/info/info-plugin.h"
#include "plugins/synth/synth-plugin.h"

typedef AudioEffectX* (*FnCreateModule)(audioMasterCallback);

namespace DAW::Host {
void PluginManager::registerModules() {
    builtinModules.clear();
    builtinModules.push_back({ PLUGIN_TYPE_HOSTINFO, false, PluginHostInfo::getName(), PluginHostInfo::createPlugin });
    builtinModules.push_back({ PLUGIN_TYPE_SYNTH, true, PluginSynth::getName(), PluginSynth::createPlugin });
}
LoadResultPlugin PluginManager::loadInternalPlugin(int32_t moduleId, int32_t globalId) {
    auto it = std::find_if(builtinModules.begin(), builtinModules.end(), [moduleId](auto& reg) {
        return reg.id == moduleId;
    });

    if (it == builtinModules.end()) {
        return LoadResultPlugin{LoadResultSharedLibrary::FromError(SharedLibState::FILE_NOT_FOUND, "Module not found")};
    }

    builtin_module_reg_t& reg = *it;

    AudioEffectX* axeffect = reg.fnNewInstance(masterCallBackSlot);
    if (!axeffect) {
        return LoadResultPlugin{LoadResultSharedLibrary::FromError(SharedLibState::DL_UNKNOWN_FORMAT, "Failed to create plugin")};
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
    return LoadResultPlugin{LoadResultSharedLibrary::FromSuccess(SharedLibPluginType::VST2, nullptr, nullptr), plugin};
}

} // namespace DAW::Host

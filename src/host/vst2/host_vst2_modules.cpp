#include "host/host_plugin_loadresult.hpp"
#include "math/seq_math.hpp"
#include "str_util.hpp"
#include "host/host_pluginmanager.hpp"
#include "host/plugin/modules.hpp"
#include "host/plugin/base/base-plugin.hpp"
#include "host/plugin/vst/vstplugin.hpp"
#include "host/plugin/vst/vstplugin-handles.hpp"

#include "plugins/info/info-plugin.hpp"
#include "plugins/synth/synth-plugin.hpp"

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
        return LoadResultPluginImpl{LoadResultSharedLibrary::FromError(SharedLibState::FILE_NOT_FOUND, "Module not found")};
    }

    builtin_module_reg_t& reg = *it;

    AudioEffectX* axeffect = reg.fnNewInstance(masterCallBackSlot);
    if (!axeffect) {
        return LoadResultPluginImpl{LoadResultSharedLibrary::FromError(SharedLibState::DL_UNKNOWN_FORMAT, "Failed to create plugin")};
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
    return LoadResultPluginImpl{LoadResultSharedLibrary::FromSuccess(SharedLibPluginType::VST2, nullptr, nullptr), plugin};
}

} // namespace DAW::Host

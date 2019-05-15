#include "math/seq_math.h"
#include "str_util.h"
#include "vst_host.h"
#include "plugin/base_plugin.h"
#include "plugin/vst_plugin.h"
#include "plugin/vst_plugin_handles.h"

#include "vstsdk-host-2.4/aeffectx.h"
#include "vstsdk-plugin-2.4/audioeffect.h"
#include "vstsdk-plugin-2.4/audioeffectx.h"

#include "plugins/advanced/adv-plugin.h"
#include "plugins/stereowidth/stereowidth-plugin.h"
#include "plugins/empty/empty-plugin.h"
#include "plugins/latency/latency-plugin.h"


vstpluginloadres vsthost::loadInternalPlugin(int32_t moduleId, int32_t globalId) {
	AudioEffectX* axeffect = NULL;
	String name = "";
	switch (moduleId) {
	case PLUG_INT_STEREOWIDTH:
		axeffect = PluginStereoWidth::createPlugin(masterCallBackSlot);
		name = PluginStereoWidth::getName();
		break;
	case PLUG_INT_TEST:
		axeffect = PluginTestAdv::createPlugin(masterCallBackSlot);
		name = PluginTestAdv::getName();
		break;
	case PLUG_INT_CRASHVST:
		axeffect = PluginEmptyVST2::createPlugin(masterCallBackSlot);
		name = PluginEmptyVST2::getName();
		break;
	case PLUG_INT_LATENCY:
		axeffect = PluginLatency::createPlugin(masterCallBackSlot);
		name = PluginLatency::getName();
		break;
	default:
		dbgassert(0);
		break;
	}
	if (!axeffect) {
		return vstpluginloadres(-1, NULL);
	}
	void* moduleHandle = nullptr;
	globalId = getNextGlobalModuleId(globalId);

	AEffect* aeffect = axeffect->getAeffectHandle();
	vstplugin* plugin = new vstplugin(new handles_t(axeffect, aeffect, moduleHandle), globalId, "", name, moduleId);
	pluginInstancesVST2.push_back(plugin);
	pluginInstances.push_back(plugin);
	plugin->load(this);
	return vstpluginloadres(0, plugin);
}

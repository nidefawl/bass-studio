#include "base_plugin.h"
#include "internal_plugin.h"
#include "empty.h"
#include "group.h"

#include "host/vst_host.h"
#include "host/plugin/vst_plugin.h"
#include "modules.h"
#include "leak_detect.h"

effectbase* vsthost::makeModuleInstance(int32_t moduleType, int32_t moduleId, int32_t globalid) {

//	vsthost* host = vsthost::getInstance();
	effectbase* effect = nullptr;
	switch (moduleType) {
		case PLUGIN_TYPE_EMPTY:
			effect = new module_empty(getNextGlobalModuleId(globalid));
			break;
		case PLUGIN_TYPE_GROUP:
			effect = new module_group(getNextGlobalModuleId(globalid));
			break;
		case PLUGIN_TYPE_INTERNAL_EFFECT:
			{
				vstpluginloadres res = loadInternalPlugin(moduleId, globalid);
				if (res.result == 0 && res.plugin) {
					effect = res.plugin;
					break;
				}
				assert(0);
			}
			break;
		default:
//			assert(0);
			break;
	}
	switch (moduleType) {
		case PLUGIN_TYPE_EMPTY:
		case PLUGIN_TYPE_GROUP:
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

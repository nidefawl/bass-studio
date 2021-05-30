#include "base_plugin.h"
#include "internal_plugin.h"
#include "empty.h"
#include "group.h"
#include "plugin_impl_gain.h"

#include "host/vst_host.h"
#include "host/plugin/vst_plugin.h"
#include "modules.h"


extern template effectbase* makeInstance<module_empty>(int32_t _projectGlobalId);
extern template effectbase* makeInstance<module_group>(int32_t _projectGlobalId);
extern template effectbase* makeInstance<module_gain>(int32_t _projectGlobalId);

effectbase* vsthost::makeModuleInstance(int32_t moduleType, int32_t moduleId, int32_t globalid) {

	effectbase* effect = nullptr;
	switch (moduleType) {
		case PLUGIN_TYPE_EMPTY:
			effect = makeInstance<module_empty>(getNextGlobalModuleId(globalid));
			break;
		case PLUGIN_TYPE_GROUP:
			effect = makeInstance<module_group>(getNextGlobalModuleId(globalid));
			break;
		case PLUGIN_TYPE_GAIN:
			effect = makeInstance<module_gain>(getNextGlobalModuleId(globalid));
			break;
		case PLUGIN_TYPE_INTERNAL_EFFECT:
			{
				vstpluginloadres res = loadInternalPlugin(moduleId, globalid);
				if (res.result == 0 && res.plugin) {
					effect = res.plugin;
					break;
				}
				dbgassert(0);
			}
			break;
		default:
//			dbgassert(0);
			break;
	}
        switch (moduleType) {
        case PLUGIN_TYPE_EMPTY:
        case PLUGIN_TYPE_GROUP:
        case PLUGIN_TYPE_GAIN:
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

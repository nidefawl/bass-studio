#include "base_plugin.h"
#include "internal_plugin.h"
#include "empty.h"
#include "group.h"

#include "host/vst_host.h"
#include "host/plugin/vst_plugin.h"
#include "modules.h"
#include "leak_detect.h"

effectbase* makeModuleInstance(int32_t moduleType, int32_t moduleId, int32_t globalid = -1) {

	vsthost* host = vsthost::getInstance();
	effectbase* effect = nullptr;
	switch (moduleType) {
		case PLUGIN_TYPE_EMPTY:
			effect = new module_empty(host->getNextGlobalModuleId(globalid));
			if (effect)
				effect->load(host);
			break;
		case PLUGIN_TYPE_GROUP:
			effect = new module_group(host->getNextGlobalModuleId(globalid));
			if (effect)
				effect->load(host);
			break;
		case PLUGIN_TYPE_INTERNAL_EFFECT:
			{
				vstpluginloadres res = host->loadInternalPlugin(moduleId, globalid);
				if (res.result == 0 && res.plugin) {
					effect = res.plugin;
					break;
				}
				assert(0);
			}
			break;
		default:
			assert(0);
			break;
	}
	return effect;
}

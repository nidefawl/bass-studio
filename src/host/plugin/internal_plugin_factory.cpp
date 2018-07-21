#include "base_plugin.h"
#include "internal_plugin.h"
#include "empty.h"
#include "group.h"

#include "../vst_host.h"
#include "modules.h"
#include "leak_detect.h"

effectbase* makeModuleInstance(int32_t uid, int32_t globalId = -1) {

	vsthost* host = vsthost::getInstance();
	int32_t id = host->getNextGlobalModuleId(globalId);
	effectbase* effect = nullptr;
	switch (uid) {
	case PLUGIN_TYPE_EMPTY:
		effect = new module_empty(id);
		break;
	case PLUGIN_TYPE_GROUP:
		effect = new module_group(id);
		break;
		default:
		break;
	}
	if (effect)
		effect->load(vsthost::getInstance());
	return effect;
}

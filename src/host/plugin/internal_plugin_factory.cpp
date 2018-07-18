#include "base_plugin.h"
#include "internal_plugin.h"
#include "empty.h"
#include "group.h"

#include "../vst_host.h"
#include "modules.h"
#include "leak_detect.h"

effectbase* makeModuleInstance(int32_t uid) {

	vsthost* host = vsthost::getInstance();
	int32_t id = host->getNextGlobalModuleId();
	switch (uid) {
	case 0:
		return new module_empty(id);
	case 1:
		return new module_group(id);
		default:
		break;
	}
	return nullptr;
}

#pragma once
#include <stdint.h>
#include "seq_time.h"

class vstplugin;
class plugin_reference_t {
public:
	virtual ~plugin_reference_t() {};
	virtual void breakReference() = 0;
	virtual void setReference(vstplugin* plugin) = 0;
};
struct param_automation_src_t {
	virtual ~param_automation_src_t() {};
	virtual bool isActive() = 0;
	virtual float getValueAt(tick_t tick) = 0;
};
struct automated_param_t {
	int32_t paramIdx = -1;
	vstplugin* plugin = NULL;
	param_automation_src_t* src = NULL;
	plugin_reference_t* ref = NULL;
};

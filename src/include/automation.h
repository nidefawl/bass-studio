#pragma once
#include "seq_time.h"
#include <memory>
struct automatable_t {
	virtual ~automatable_t() {};
	virtual String getAutomatableName() = 0;
	virtual int32_t getNumParameters() = 0;
	virtual String getParamName(int32_t paramIdx) = 0;
	virtual float getParamValue(int32_t idx) = 0;
	virtual void setParamValue(int32_t idx, float val) = 0;
	virtual void updateAutomatedParameters(tick_t pos) = 0;
};
class vstplugin;
struct automation_point_t {
	tick_t time;
	float val;
};
struct plugin_param_autiomation_src_t {
	int32_t pluginSlot;
	int32_t trackIdx;
	int32_t paramIdx;
};
class plugin_reference_t {
public:
	virtual ~plugin_reference_t() {};
	virtual void onDstDelete() = 0;
	virtual void onSrcDelete() = 0;
	virtual void setDst(vstplugin* plugin, int32_t paramIdx) = 0;
	virtual plugin_param_autiomation_src_t serialize() = 0;
};
struct automation_src_t {
	virtual ~automation_src_t() {};
	virtual bool isActive() = 0;
	virtual float getValueAt(tick_t tick) = 0;
};
struct automated_param_t {
	int32_t paramIdx = -1;
	automation_src_t* src = NULL;
	std::shared_ptr<plugin_reference_t> ref;
};


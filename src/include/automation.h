#pragma once
#include "str_util.h"
#include "seq_time.h"
#include <memory>
#include <assert.h>
#include <vector>

struct automation_point_t {
	tick_t time;
	float val;
};
int32_t indexOfTick(std::vector<automation_point_t>& dataPoints, tick_t tick);
int32_t addPointAt(std::vector<automation_point_t>& dataPoints, tick_t tick);
void simplifyData(std::vector<automation_point_t>& data);
struct automation_t {
	bool active = true;
	std::vector<automation_point_t> points;
	virtual ~automation_t() {};
	virtual float getDstValue() = 0;
	virtual void setDstValue(float f) = 0;
	virtual bool isActive() {
		return active && points.size() > 0;
	}
	virtual bool isAutomated() {
		return points.size() > 0;
	}
	virtual float getValueAt(tick_t tick);
	void copyRange(tick_t tickBegin, tick_t tickEnd, std::vector<automation_point_t>& data);
	void setRange(tick_t tickBegin, tick_t tickEnd, std::vector<automation_point_t>& data);
};
struct automation_clipboard_t {
	tick_t start;
	tick_t len;
	std::vector<automation_point_t> dataPoints;
};
struct automation_view_t: public automation_t {

	int32_t targetParam = -1;
	float dummy = 0.5f;
	float getDstValue() override {
		return dummy;
	}
	void setDstValue(float f) override {
		dummy = f;
		active = false;
	}
};
class vstplugin;
struct vstparam_automation_t: public automation_t {
	int32_t paramIdx = -1;
	vstplugin* plugin = NULL;

	float dummy = 0.5f;

	vstparam_automation_t()
	{
	}
	~vstparam_automation_t() {
		//notify vstplugin
	}
	void setTarget(vstplugin* _plugin, int32_t _paramIdx) {
		plugin = _plugin;
		paramIdx = _paramIdx;
	}
	vstplugin* getTargetPlugin() {
		return plugin;
	}
	float getDstValue() override;
	void setDstValue(float f) override;
};
struct automationlane_snapshot_t {
	int32_t type = -1;
	int32_t refId = -1;
	int32_t paramIdx = -1;
	int32_t height = 4;
};
class plugin_reference_t {
public:
	virtual ~plugin_reference_t() {};
	virtual void onDstDelete() = 0;
	virtual void onSrcDelete() = 0;
	virtual void setDst(vstplugin* plugin, int32_t paramIdx) = 0;
	virtual automationlane_snapshot_t serialize() = 0;
};
struct automated_param_t {
	int32_t paramIdx = -1;
	automation_t* src = NULL;
};
struct automatable_t {
	virtual ~automatable_t() {};
	virtual String getAutomatableName() = 0;
	virtual int32_t getNumParameters() = 0;
	virtual String getParamName(int32_t paramIdx) = 0;
	virtual float getParamValue(int32_t idx) = 0;
	virtual void setParamValue(int32_t idx, float val) = 0;
	virtual void updateAutomatedParameters(tick_t pos) = 0;
	virtual automation_t* getAutomation(int32_t idx) = 0;
	virtual void getAutomated(std::vector<int32_t>& targets) = 0;
	virtual automationlane_snapshot_t toRef() = 0;
};


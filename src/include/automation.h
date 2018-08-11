#pragma once
#include "str_util.h"
#include "seq_time.h"
#include <memory>
#include <assert.h>
#include <vector>

#define PARAM_ENABLE 0
#define AUTOMATABLE_MIXER 0
#define AUTOMATABLE_ARP 1
#define AUTOMATABLE_EFFECT 2

struct automation_point_t {
	tick_t time;
	float val;
};
int32_t indexOfTick(std::vector<automation_point_t>& dataPoints, tick_t tick);
int32_t addPointAt(std::vector<automation_point_t>& dataPoints, tick_t tick, int32_t quantizationSteps);
void simplifyData(std::vector<automation_point_t>& data);
inline float quantizeFloat(float f, int32_t steps) {
	if (!steps)
		return f;
	int32_t val = steps * f;
	return val / (float) steps;
}
struct automation_t {
	int32_t quantizationSteps = 0;
	bool active = true;
	std::vector<automation_point_t> points;
	virtual ~automation_t() {};
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
struct automation_view_t: public automation_t {
	int32_t targetParam = -1;
};
struct automation_clipboard_t {
	tick_t start;
	tick_t len;
	std::vector<automation_point_t> dataPoints;
};
class vstplugin;
struct vstparam_automation_t: public automation_t {
	int32_t paramIdx = -1;

	vstparam_automation_t()
	{
	}
	~vstparam_automation_t() {
		//notify vstplugin
	}
	void setTarget(int32_t _paramIdx) {
		paramIdx = _paramIdx;
	}
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
	automation_t src;
	automated_param_t(int32_t _paramIdx) : paramIdx(_paramIdx) { }
};
union param_step_fi_u {
	float valFloat;
	int32_t valInt;
};
struct automatable_param_t {
	int32_t idx;
	float value;
	int32_t flags;

	param_step_fi_u min;
	param_step_fi_u max;
	param_step_fi_u stepSmall;
	param_step_fi_u step;
	param_step_fi_u stepLarge;

	String shortLabel;//8
	String label;//64

	//if kVstParameterSupportsDisplayIndex
	int16_t displayIndex;		///< index where this parameter should be displayed (starting with 0)

	//if kVstParameterSupportsDisplayCategory
	int16_t category;			///< 0: no category, else group index + 1
	int32_t internalIdx;
};
struct automatable_t {
	std::vector<automatable_param_t> params;
	std::vector<automated_param_t> automatedParams;
	virtual ~automatable_t() {};
	virtual String getAutomatableName() = 0;
	virtual float getParamValue(int32_t idx) = 0;
	virtual void setParamValue(int32_t idx, float val, int flags) = 0;
	virtual automationlane_snapshot_t toRef() = 0;

	virtual void flipParamValue(int32_t idx) {
		setParamValue(idx, 1.0f-getParamValue(idx), 2);
	}
	int32_t getQuantizationSteps(int32_t idx) {
		automation_t* at = getAutomation(idx);
		assert(at);
		return at->quantizationSteps;
	}
	float quantizeVal(int32_t idx, float f) {
		automation_t* at = getAutomation(idx);
		assert(at);
		f = quantizeFloat(f, at->quantizationSteps);
		return f;
	}

	int32_t getNumParameters() const {
		return params.size();
	}
	String getParamName(int32_t idx) {
		if (idx >= 0 && idx < (int32_t)params.size()) {
			return params[idx].label;
		}
		return "";
	}
	void getAutomated(std::vector<int32_t>& targets) {
		for (automated_param_t t : automatedParams) {
			if (t.src.isAutomated())
				targets.push_back(t.paramIdx);
		}
	}
	void updateAutomatedParameters(tick_t pos) {
		for (automated_param_t& param : automatedParams) {
			if (param.src.isActive()) {
				float val = param.src.getValueAt(pos);
				setParamValue(param.paramIdx, val, 1);
			}
		}
	}
	automation_t* getAutomation(int32_t paramIdx) {
		if (!hasParam(paramIdx)) {
			return NULL;
		}
		for (automated_param_t& param : automatedParams) {
			if (paramIdx == param.paramIdx) {
				return &param.src;
			}
		}
		automatedParams.emplace_back(paramIdx);
		return &automatedParams.back().src;
	}
	void deactivateAutomation(int32_t paramIdx) {
		for (automated_param_t& param : automatedParams) {
			if (paramIdx == param.paramIdx) {
				param.src.active = false;
				return;
			}
		}
	}
	bool hasParam(int32_t idx) {
		if (idx >= 0 && idx < (int32_t)params.size()) {
			return true;
		}
		return false;
	}
	automated_param_t* getRegisteredAutomation(int32_t idx) {
		auto it = std::find_if(automatedParams.begin(), automatedParams.end(), [idx](automated_param_t& ap) {
			return ap.paramIdx == idx;
		});
		if (it != automatedParams.end()) {
			automated_param_t* ap = &(*it);
			if (ap->src.isAutomated())
				return ap;
		}
		return NULL;
	}
	virtual void postSetParameter(int32_t idx, float preVal, float val, int flags) {
	}
};


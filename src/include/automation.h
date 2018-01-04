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
inline int32_t indexOfTick(std::vector<automation_point_t>& dataPoints, tick_t tick) {
	int32_t idx;
	for (idx = 0; idx < dataPoints.size(); idx++) {
		automation_point_t& pt = dataPoints[idx];
		if (pt.time > tick) {
			break;
		}
	}
	return idx;
}
inline int32_t addPointAt(std::vector<automation_point_t>& dataPoints, tick_t tick) {
	int32_t idx;
	for (idx = 0; idx < dataPoints.size(); idx++) {
		automation_point_t& pt = dataPoints[idx];
		if (pt.time > tick) {
			break;
		}
	}
	if (!dataPoints.empty()) {
		float v;
		if (idx == dataPoints.size()) {
			v = dataPoints[idx-1].val;
		} else if (idx == 0) {
			v = dataPoints[0].val;
		} else {
			automation_point_t& pt2 = dataPoints[idx];
			automation_point_t& pt1 = dataPoints[idx - 1];
			assert(tick >= pt1.time && tick <= pt2.time);
			tick_t tickDist = pt2.time - pt1.time;
			float pr = (tick - pt1.time) / (float) tickDist;
			v = pt1.val + pr * (pt2.val - pt1.val);
		}
		dataPoints.insert(dataPoints.begin() + idx, { tick, v });
		return idx;
	} else {
		dataPoints.insert(dataPoints.begin(), { tick, 0 });
	}
	return 0;
}
struct automation_t {
	std::vector<automation_point_t> points;
	virtual ~automation_t() {};
	virtual float getDstValue() = 0;
	virtual void setDstValue(float f) = 0;
	virtual bool isActive() {
		return true;
	}
	virtual float getValueAt(tick_t tick) {
		if (points.size()) {
			int32_t idx = indexOfTick(points, tick);
			assert(idx <= points.size());
			if (idx == points.size())
				return points.back().val;
			if (idx > 0) {
				automation_point_t& pt1 = points[idx-1];
				automation_point_t& pt2 = points[idx];
				assert(tick>=pt1.time && tick <= pt2.time);
				tick_t tickDist = pt2.time-pt1.time;
				float pr = (tick-pt1.time)/(float)tickDist;
				return pt1.val+pr*(pt2.val-pt1.val);
			}
			return points.front().val;
		}
		return getDstValue();
	}
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
	bool isActive() override {
		return true;
	}
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
};


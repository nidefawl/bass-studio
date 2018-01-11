#include "automation.h"
#include "vst_plugin.h"
#include "../gui/automatable.h"

int32_t indexOfTick(std::vector<automation_point_t>& dataPoints, tick_t tick) {
	int32_t idx;
	for (idx = 0; idx < dataPoints.size(); idx++) {
		automation_point_t& pt = dataPoints[idx];
		if (pt.time > tick) {
			break;
		}
	}
	return idx;
}
int32_t addPointAt(std::vector<automation_point_t>& dataPoints, tick_t tick) {
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

void simplifyData(std::vector<automation_point_t>& data) {
	//remove multiple points on same time
	{

		auto first = data.begin();
		auto last = data.end();
	    if (first != last) {
	        for(auto i = first; i != last; ++i) {
	        	tick_t firstTime = (*i).time;
	            *first++ = std::move(*i);
				if (i + 1 != last) {
					auto j = i + 2;
					for (; j < last; ++j) {
						if (firstTime != (*j).time) {
							break;
						}
					}
					i = j - 2;
				}
	        }
			if (first != last)
	        data.erase(first, last);
	    }
	}
    {

        //remove multiple consecutive points with same value
		auto first = data.begin();
    	auto last = data.end();
        if (first != last) {
            for(auto i = first; i != last; ++i) {
            	float firstVal = (*i).val;
                *first++ = std::move(*i);

				if (i + 1 != last) {
					auto j = i + 2;
					for (; j < last; ++j) {
						if (firstVal != (*j).val || firstVal != (*(j - 1)).val) {
							break;
						}
					}
					i = j - 2;
				}
            }
			if (first != last)
            data.erase(first, last);
        }
    }
}

float automation_t::getValueAt(tick_t tick) {
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
	return 0.5f;
}
void automation_t::copyRange(tick_t tickBegin, tick_t tickEnd, std::vector<automation_point_t>& data) {
	if (points.size()) {
		int32_t idxStart = indexOfTick(points, tickBegin);
		int32_t idxEnd = indexOfTick(points, tickEnd) + 1;
		if (idxStart >= points.size()) {
			idxStart = points.size()-1;
		}
		if (idxEnd >= points.size()) {
			idxEnd = points.size()-1;
		}
		automation_point_t ptStart{0, getValueAt(tickBegin)};
		automation_point_t ptEnd{tickEnd - tickBegin, getValueAt(tickEnd)};
		int32_t loopLen = std::max(0, (idxEnd)-(idxStart));
		data.reserve(2+loopLen);
		data.push_back(std::move(ptStart));
		for (int j = idxStart; j <= idxEnd; j++) {
			auto pt = points[j];
			if (pt.time >= tickBegin && pt.time <= tickEnd) {
				pt.time -= tickBegin;
				data.push_back(std::move(pt));
			}
		}
		data.push_back(std::move(ptEnd));
	}
}
void automation_t::setRange(tick_t tickBegin, tick_t tickEnd, std::vector<automation_point_t>& data) {
	std::vector<automation_point_t> pointsTmp;
	pointsTmp.reserve(data.size()+points.size());
	int32_t idx1 = 0;
	for (; idx1 < points.size(); idx1++) {
		auto& pt = points[idx1];
		if (pt.time <= tickBegin) {
			pointsTmp.push_back(pt);
		} else {
			break;
		}
	}
	if (!pointsTmp.empty() && !data.empty() && pointsTmp.back().time != tickBegin) {
		automation_point_t ptStart{tickBegin, getValueAt(tickBegin)};
		pointsTmp.push_back(std::move(ptStart));
	}
	for (int32_t idx = 0; idx < data.size(); idx++) {
		auto pt = data[idx];
		pt.time += tickBegin;
		pointsTmp.push_back(std::move(pt));
	}
	if (!pointsTmp.empty() && !data.empty()) {
		automation_point_t ptEnd{tickEnd, getValueAt(tickEnd)};
		pointsTmp.push_back(std::move(ptEnd));
	}
	for (; idx1 < points.size(); idx1++) {
		auto& pt = points[idx1];
		if (pt.time >= tickEnd) {
			pointsTmp.push_back(pt);
		}
	}
	simplifyData(pointsTmp);
	points = std::move(pointsTmp);

}

float vstparam_automation_t::getDstValue() {
	if (plugin) {
		return plugin->getParamValue(paramIdx);
	}
	return dummy;
}
void vstparam_automation_t::setDstValue(float f) {
	active = false;
	dummy = f;
	if (plugin) {
		return plugin->setParamValue(paramIdx, f);
	}
}


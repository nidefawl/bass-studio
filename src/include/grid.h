#pragma once
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <stdint.h>
#include <vector>
#include "math.h"
#include "seq_time.h"
#include "guicolors.h"
#include "logging.h"
#include "layout.h"
#include "grid_constants.h"

using glm::ivec2;



struct grid_div {
	tick_t time;
	beatbar16th_t pos;
	float screenpos;
	int8_t color;
	float thickness;
	float width;
};
struct grid_density {
	bool enabled = true;
	bool isfixed = false;
	int8_t fixedBars = 2;
	int8_t dynamicDensity = 3;
	template<class Archive>
	void serialize(Archive & ar) {
		ar(enabled, isfixed, fixedBars, dynamicDensity);
	}
	int getSnap() {
		if (!enabled) {
			return GRID_OFF;
		}
		if (isfixed)
			return fixedBars;
		return dynamicDensity;
	}
};
class scaled_grid;
class grid_changed_cb {
public:
	virtual ~grid_changed_cb() {
	}
	virtual void gridChanged(scaled_grid& grid) = 0;
};

class scaled_grid : public layout_grid_t {
protected:

	std::vector<grid_changed_cb*> callbacks;
	int lastOffset = 0;
	float lastZoom = DEFAULT_ZOOM;
	int lastW = 200;
	bool dirty = false;
public:
	grid_density grid_dens;
	std::vector<grid_div> gridList;

	float bars = 0;
	float bar_size = 0;
	float incr_bg = 0;
public:
	scaled_grid()
	{
	}
	void addCallback(grid_changed_cb* cb){
		this->callbacks.push_back(cb);
	}
	void setLayout(layout_grid_t& layout) {
		setZoom(layout.zoom);
		setOffset(layout.offset);
		notifyChange();
	}
	void update(ivec2 contentsize) {
//		if (!dirty &&lastOffset == offset
//			&& lastZoom == zoom
//			&& lastW == contentsize.x) {
//			return;
//		}
		dirty = false;
		calcLen(offset, zoom, contentsize.x);
		lastOffset = offset;
		lastZoom = zoom;
		lastW = contentsize.x;
	}
	tick_t getTickLength();
	void notifyChange();
	void setZoom(float zoom);
	void setOffset(float offset);
	double toObjSpace(double screenx) {
		return toObjSpace(screenx, this->zoom, this->offset);
	}
	double toObjSpace(double screenx, double _zoom, double _offset) {
		double relx = screenx + _offset;
		const double stepSize = _zoom * 128;
		const double scale = 1024.0f;
		double barSize = scale / stepSize;
		double objx = relx / barSize;
		return objx;
	}
	double toScreenSpace(double objx) {
		return toScreenSpace(objx, this->zoom);
	}
	double toScreenSpace(double objx, double _zoom) {
		const double stepSize = _zoom * 128;
		const double scale = 1024.0f;
		double barSize = scale / stepSize;
		double screenx = objx * barSize;
		return screenx;
	}
	double calcOffset(double screenx, double objx) {
		double screenpos = toScreenSpace(objx);
		return screenpos - screenx;
	}
	tick_t screenToTickSnap(int32_t x, int snap);

	tick_t screenToTick(int32_t x) {
		double d = toObjSpace(x);
		double dTick = TICKS_BAR * d;
		return (int32_t) round(dTick);
	}
	double tickToScreenD(tick_t x) {
		double bar = x / (double)TICKS_BAR;
		return toScreenSpace(bar) - offset;
	}
	double tickLenToScreen(tick_t x) {
		double bar = x / (double)TICKS_BAR;
		return toScreenSpace(bar);
	}
	int32_t pixelsToTicks(int32_t pixels) {
		double x = toObjSpace(pixels, this->zoom, 0) * TICKS_BAR;
		return std::max(1, (int32_t)ceil(x));
	}
	void showRange(tick_t start, tick_t end);
	void calcLen(int scrollOffsetX, float zoom, int contentWidth);
	void makeTickVisible(tick_t tickTime);
	tick_t prev(tick_t tickTime);
	tick_t next(tick_t tickTime);
	tick_t distPrev(tick_t tickTime);
	tick_t distNext(tick_t tickTime);
};


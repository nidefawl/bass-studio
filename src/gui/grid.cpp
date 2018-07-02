#include "seq_time.h"
#include "grid.h"
#include "mainctrl.h"
#include "leak_detect.h"

using glm::ivec2;

void scaled_grid::makeTickVisible(tick_t tickTime) {
	double tickBars = tickTime / (double)TICKS_BAR;
	if (lastW > 0) {
		double screenx = toScreenSpace(tickBars);
		if (screenx < this->offset) {
			setOffset((int)screenx);
		}
		if (screenx > this->offset+lastW) {
			setOffset((int)(screenx-(lastW)));
		}
	}
	notifyChange();
}
void scaled_grid::showRange(tick_t start, tick_t end) {
	double rangeBars = (end-start) / (double)TICKS_BAR;
	if (lastW > 0) {
		this->zoom=(float)(8.0f / (lastW / rangeBars));
		double screenpos = toScreenSpace(start / (double) TICKS_BAR);
		this->offset=(float)(screenpos);
		notifyChange();
	}
}
void scaled_grid::setZoom(double fNewZoom) {
	double newZoom = fNewZoom < MIN_ZOOM ? MIN_ZOOM : fNewZoom > 100 ? 100 : fNewZoom;
	double length = toObjSpace(lastW, fNewZoom, 0);
	double projectWorkingArea = MainCtrl::get()->getProjectWorkingArea();
	if (length > projectWorkingArea) {
		newZoom = 8.0 / (lastW / projectWorkingArea);
	}
	this->zoom = (float) (newZoom < MIN_ZOOM ? MIN_ZOOM : newZoom > 100 ? 100 : newZoom);
}
void scaled_grid::notifyChange() {
	for (grid_changed_cb* cb : this->callbacks) {
		cb->gridChanged(*this);
	}
}

tick_t scaled_grid::getTickLength() {
	if (this->gridList.size() < 2) {
		return TICKS_BAR;
	}

	//TODO: way too hacky
	auto first = this->gridList[0];
	auto second = this->gridList[1];
	return second.time-first.time;
}
void scaled_grid::setOffset(int newOffset) {
	newOffset = newOffset < 0 ? 0 : newOffset;
//	double rightMost = toObjSpace(lastW, this->zoom, newOffset);
//	double projectWorkingArea = MainCtrl::get()->getProjectWorkingArea();
//	if (rightMost > projectWorkingArea) {
//		double n = toScreenSpace(projectWorkingArea);
//		newOffset =  n - (double)lastW;
//		if (newOffset < 0.) {
//			newOffset = 0.;
//		}
//	}
	this->offset = (int) newOffset;
}
tick_t scaled_grid::next(tick_t tick) {
	tick_t len = getTickLength();
	tick_t pos = floor(tick / (double)len);
	pos++;
	return pos * len;
}
tick_t scaled_grid::prev(tick_t tick) {
	tick_t len = getTickLength();
	tick_t pos = floor(tick / (double)len);
	if (pos*len==tick){
		pos--;
	}
	return pos * len;
}
tick_t scaled_grid::distNext(tick_t tick) {
	tick_t len = getTickLength();
	tick_t pos = floor(tick / (double)len);
	pos++;
	return (pos * len) - tick;
}
tick_t scaled_grid::distPrev(tick_t tick) {
	tick_t len = getTickLength();
	tick_t pos = floor(tick / (double)len);
	if (pos*len==tick){
		pos--;
	}
	return (pos * len) - tick;
}
tick_t scaled_grid::screenToTickSnap(int32_t x, int snap) {
	tick_t tick = screenToTick(x);
	if (snap != SNAP_OFF && this->grid_dens.getSnap() != GRID_OFF) {
		grid_div* min = NULL;
		tick_t minDist = 0;
		for (grid_div& d : gridList) {
			if (snap == SNAP_LEAST) {
				if (d.time > tick) {
					return min == NULL ? 0 : min->time;
				}
				min = &d;
			} else {
				tick_t dist = abs(d.time-tick);
				if (min == NULL || dist < minDist) {
					minDist = dist;
					min = &d;
				}
			}
		}
		if (min != NULL) {
			tick = min->time;
		} else if (snap == SNAP_LEAST) {
			return 0;
		}
	}
	return tick;
}

void scaled_grid::calcLen(int scrollOffsetX, double fzoom, int contentWidth) {
	gridList.clear();
	gridList.reserve(100);
	const float stepSize = fzoom * 128;
	const float scale = 1024.0f;
	const float barSize = scale / stepSize;
	//bg
	{
		float steps_bg = stepSize;
		float incr = scale / steps_bg;
		float minW = 256.0f;
		float maxW = minW*2.0f;
		while (incr < minW) {
			steps_bg /= 4;
			incr = scale / steps_bg;
		}
		if (incr > maxW) {
			steps_bg *= 4;
			incr = scale / steps_bg;
		}
		this->incr_bg = incr;
	}


	//grid2
	{
		int denum_step = 4;
		int step = 1;
		int denum_substep = 0;
		float minBarSize = 24;
		float minSubSize = 24;
//		grid_dens.isfixed = false;
//		grid_dens.fixedBars = 6;
//		grid_dens.dynamicDensity = 3;
		if (grid_dens.isfixed) {
			if (grid_dens.fixedBars < 4) {
				step = (1 << (3 - grid_dens.fixedBars));
				denum_step = 0;
			} else {
				denum_step = (1 << (grid_dens.fixedBars - 3));
				while (denum_step > 4) {
					denum_step /= 2;
					if (denum_substep == 0) denum_substep = 2;
					else denum_substep *= 2;
				}
			}
		} else {
			step = 1;
			denum_substep = 32;
			minBarSize = (float)(1 << (8 - grid_dens.dynamicDensity));
			minSubSize = (float)(1 << (8 - grid_dens.dynamicDensity));
		}
		while (step < (1 << 14) && barSize*step < minBarSize) {
			step = step * 2;
			//					substep = 0;
		}
		float denom_size = denum_step > 0 ? barSize / (float)denum_step : 0;
		while (denum_step > 0 && denom_size < minSubSize) {
			denum_step = denum_step / 2;
			denom_size = barSize / denum_step;
		}
		if (denum_step == 0) {
			denum_substep = 0;
		}
		float denom_sub_size = denum_substep > 0 ? denom_size / (float)denum_substep : 0;
		while (denum_substep > 0 && denom_sub_size < minSubSize) {
			denum_substep = denum_substep / 2;
			denom_sub_size = denom_size / denum_substep;
		}
		float bar_offset = -fmod((float)offset, (barSize*step));
		int numBarsOnScreen = (int)ceil(contentWidth / barSize) + 1; //todo: maybe make this optimal
		int firstBarLeftOfScreen = (int)floor(offset / (barSize));
		firstBarLeftOfScreen = (firstBarLeftOfScreen / step)*step;
		//TODO: very important: make sure grid is never empty
		MainCtrl* main = MainCtrl::get();
		for (int bar = 0; bar < numBarsOnScreen; bar += step) {
			const float pos = bar_offset + bar * barSize;
			const tick_t timeBar = (firstBarLeftOfScreen + bar) * TICKS_BAR;
			grid_div div = {};
			div.time = timeBar;
			div.pos = main->toBeatBar16th(timeBar);
			div.screenpos = pos;
			div.width = denum_step > 0 ? (denum_substep>0 ? denom_sub_size : denom_size) : barSize*step;
			div.color = COL_LINE_BAR;
			div.thickness = 0.9f;
			gridList.push_back(div);
//			if (step < 2)
			for (int bar_denom = 0; bar_denom < denum_step; bar_denom++) {
				const tick_t timeQuarter = timeBar + (bar_denom*(4 / denum_step)) * TICKS_QUARTER;
				const float pos_denom = pos + bar_denom * denom_size;
				if (bar_denom > 0) {
					grid_div div_quarter = {};
					div_quarter.time = timeQuarter;
					div_quarter.pos = main->toBeatBar16th(timeQuarter);
					div_quarter.screenpos = pos_denom;
					div_quarter.width = denum_substep > 0 ? denom_sub_size : denom_size;
					div_quarter.color = COL_LINE_QRT;
					div_quarter.thickness = 0.75f;
					gridList.push_back(div_quarter);
				}
				for (int bar_denom_sub = 1; bar_denom_sub < denum_substep; bar_denom_sub++) {
					tick_t timeSmall = timeQuarter + (bar_denom_sub*(32 / denum_substep)) * (TICKS_16TH>>3);
					grid_div div_smaller = {};
					div_smaller.time = timeSmall;
					div_smaller.pos = main->toBeatBar16th(timeSmall);

					div_smaller.screenpos = bar_offset + bar * barSize + bar_denom * denom_size + bar_denom_sub * denom_sub_size;
					div_smaller.width = denom_sub_size;
					div_smaller.color = COL_LINE_XTH;
					div_smaller.thickness = 0.6f;
					gridList.push_back(div_smaller);
				}
			}
		}
		this->bars = contentWidth / barSize;
	}

	this->bar_size = barSize;
}

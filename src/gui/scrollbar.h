#pragma once
#include <nanovg.h>
#include "gui.h"
#include "guicolors.h"
#include <glm/glm.hpp>
#include <glm/vec2.hpp>

using glm::vec2;
using glm::ivec2;

class gui_scrollcontainer {
public:
	gui_scrollcontainer() {}
	virtual ~gui_scrollcontainer() {}
	virtual ivec2 getScrollTotalSize() = 0;
	virtual ivec2 getScrollViewSize() = 0;
	virtual void scrollOffsetChanged(int dir, float offset) = 0;
	virtual bool handleMouseScroll(MouseEvent& evt, double xoffset, double yoffset) = 0;
};
class gui_scrollbar : public guibase {
	int dir;
	gui_scrollcontainer& ctr;
public:
	float scrollOffset;
	static const int defaultW = 20;
	static const int smallW = 10;
	gui_scrollbar(int _dir, float _offset, gui_scrollcontainer& _ctr);
	virtual void render(NVGcontext* vg);
	virtual bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
		if (this->contains(mpos)) {
			evt.requestFocus(this);
			return true;
		}
		return false;
	}
	float startOffset = 0;
	virtual void handleDraggedBegin(MouseEvent& evt) {
		startOffset = scrollOffset;
	}
	void setScrollOffset(float f) {
		if (getScrollRange() <= 0)
			f = 0;
		float _newOffset = f < 0 ? 0 : f > 1 ? 1 : f;
		scrollOffset = _newOffset;
		ctr.scrollOffsetChanged(dir, scrollOffset);
	}
	float getScrollRange() {
		ivec2 vcS = ctr.getScrollTotalSize();
		ivec2 vs = ctr.getScrollViewSize();
		vec2 barOff(0);
		vec2 barS = size;
		if (vcS[dir] > 0) {
			barS[dir] = min((float) size[dir], (vs[dir] / (float) vcS[dir]) * size[dir]);
			barOff[dir] = (size[dir] - barS[dir]) * scrollOffset;
		}
		return size[dir] - barS[dir];
	}
	double toPixels() {
		ivec2 vcS = ctr.getScrollTotalSize();
		ivec2 vs = ctr.getScrollViewSize();
		int32_t dist = vcS[dir]-vs[dir];
		return max(0.0, (double)scrollOffset*dist);
	}
	void scrollVisible(int32_t y, int32_t size) {
		ivec2 vcS = ctr.getScrollTotalSize();
		ivec2 vs = ctr.getScrollViewSize();
		int32_t dist = vcS[dir]-vs[dir];
		if (dist > 0) {
			double pxOffset = toPixels();
			if (y < pxOffset) {
				double offset = y / (double)dist;
				setScrollOffset((float) offset);
			} else if (y+size > pxOffset+vs[dir]) {
				double offset = (y+size-vs[dir]) / (double)dist;
				setScrollOffset((float) offset);
			}
		}
	}
	void scrollTo(double pixels) {
		ivec2 vcS = ctr.getScrollTotalSize();
		ivec2 vs = ctr.getScrollViewSize();
		int32_t dist = vcS[dir]-vs[dir];
		if (dist > 0) {
			double offset = pixels / (double)dist;
			setScrollOffset((float) offset);
		}
	}
	virtual void handleDraggedMove(MouseEvent& evt) {
		float scrollRange = getScrollRange();
		if (scrollRange>0) {
			int32_t dragPixels = (evt.mousepos-evt.dragStart)[dir];
			setScrollOffset(startOffset + dragPixels/(float)scrollRange);
		}

	}
	virtual bool handleMouseScroll(MouseEvent& evt, double xoffset, double yoffset) {
		if (yoffset) {
			ivec2 vcS = ctr.getScrollTotalSize();
			int32_t cS = vcS[dir];
			float scrollRange = cS;
			if (scrollRange>0) {
				setScrollOffset(scrollOffset - (yoffset*100)/(float)scrollRange);
			}
		}
		return true;
	}
	virtual void handleDraggedRelease(MouseEvent& evt) {
	}
};


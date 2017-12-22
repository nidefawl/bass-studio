#pragma once
#include <nanovg.h>
#include "gui.h"
#include "guicolors.h"

using glm::vec2;
using glm::ivec2;
using glm::vec4;
using glm::ivec4;
class gui_scrollcontainer {
public:
	gui_scrollcontainer() {}
	virtual ~gui_scrollcontainer() {}
	virtual int32_t getContentHeight() = 0;
	virtual int32_t getContentWidth() = 0;
	virtual void scrollOffsetChanged(int dir, float offset) = 0;
	virtual bool handleMouseScroll(MouseEvent& evt, double xoffset, double yoffset) = 0;
};
class gui_scrollbar : public guibase {
	int dir;
	gui_scrollcontainer& ctr;
public:
	float scrollOffset;
	gui_scrollbar(int _dir, float _offset, gui_scrollcontainer& _ctr) : guibase(), dir(_dir), ctr(_ctr), scrollOffset(_offset) {
	}
	virtual void render(NVGcontext* vg) {
		nvgBeginPath(vg);
		nvgRoundedRect(vg, pos.x, pos.y, size.x, size.y, G_RND);
		NVGcolor bg = g_guiColors[COL_BG_DRK];
		nvgFillColor(vg, bg);
		nvgFill(vg);
		int32_t s = 0;
		int32_t cS = 0;
		if (dir == 1) {
			s = size.y;
			cS = ctr.getContentHeight();
		} else {
			s = size.x;
			cS = ctr.getContentWidth();
		}
		float barLen = 0;
		if (cS > 0) {
			barLen = min((float) s, (s / (float) cS) * s);
		}
		float scrollRange = (s-barLen);
		float barOffset = scrollOffset*scrollRange;
		if (cS > 0) {
			float barW = size.x;
			float barH = size.y;
			float barOffsetX = 0;
			float barOffsetY = 0;
			if (dir == 1) {
				barH = barLen;
				barOffsetY = barOffset;
			} else {
				barW = barLen;
				barOffsetX = barOffset;
			}
			int32_t inset = 1;
			nvgBeginPath(vg);
			nvgRoundedRect(vg, pos.x+barOffsetX+inset, pos.y+barOffsetY+inset, barW-inset*2, barH-inset*2, G_RND);


			bool focused = MainCtrl::get()->guiCtrFocused == this->parent || (MainCtrl::get()->guiDragged==NULL&&MainCtrl::get()->guiOver == this);
			if (focused) {
//				nvgStrokeWidth(vg, 1.0f);
//				nvgStrokeColor(vg, g_guiColors[COL_BG_DRK_FOCUSED]);
//				nvgStroke(vg);
				nvgFillColor(vg, g_guiColors[COL_BG_DRK_FOCUSED]);
			} else {
				nvgFillColor(vg, g_guiColors[COL_BG_DRKER]);
			}
			nvgFill(vg);

		}
	}
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
		float _newOffset = f < 0 ? 0 : f > 1 ? 1 : f;
		scrollOffset = _newOffset;
		ctr.scrollOffsetChanged(dir, scrollOffset);
	}
	float getScrollRange() {

		int32_t s = 0;
		int32_t cS = 0;
		if (dir == 1) {
			s = size.y;
			cS = ctr.getContentHeight();
		} else {
			s = size.x;
			cS = ctr.getContentWidth();
		}
		float barLen = 0;
		if (cS > 0) {
			barLen = min((float) s, (s / (float) cS) * s);
		}
		return (s-barLen);
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
			int32_t cS = 0;
			if (dir == 1) {
				cS = ctr.getContentHeight();
			} else {
				cS = ctr.getContentWidth();
			}
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

#pragma once
#include "math/vec.h"
#include "math/seq_math.h"
#include "gui.h"
#include "exceptions.h"
#include "mouse.h"
#include "event.h"
#include "basectrl.h"

class Splitter;
class splitter_cb {
public:
	splitter_cb() {}
	virtual ~splitter_cb() {}
	virtual void handleSplitterChanged(Splitter& splitter, float scale, int clampedAt) = 0;
};
class Splitter : public guictr_base {
public:
	static constexpr int SPLITTER_LAYOUT_THICKNESS = 10;
	int type;
	float scale;
	float scaleMin, scaleMax;
	splitter_cb* notifyCtrl = nullptr;
	Splitter(int _type, float _scale)
	: guictr_base(),
	  type(_type),
	  scale(_scale)
	{
		scaleMin = 0;
		scaleMax = 1;
		padding = 0;
	}
	void setMinMax(float _min, float _max) {
		this->scaleMin = _min;
		this->scaleMax = _max;
	}
	virtual bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
		if (this->contains(mpos) && evt.type <= MouseHitType::MOUSE_RIGHT) {
			evt.requestFocus(this);
			evt.requestCursor(type == 0 ? CURSOR_RESIZE_V : CURSOR_RESIZE_H);
			return true;
		}
		return false;
	}
	int32_t leftOrTop(int32_t wh) {
		return round(wh*scale);
	}
	int32_t rightOrBottom(int32_t wh) {
		return wh-leftOrTop(wh);
	}
	virtual void handleDraggedBegin(MouseEvent& evt) {
	}
	virtual void handleDraggedMove(MouseEvent& evt) {
		ivec2 windowSize = parentCtrl->m_size;
		float sc = type == 0  ? (evt.mousepos.y/(float)windowSize.y) : (evt.mousepos.x/(float)windowSize.x);
		int clampedAt = 0;
		if (sc < scaleMin) {
			clampedAt = -1;
		}
		if (sc > scaleMax) {
			clampedAt = 1;
		}
		this->scale = (sc < scaleMin ? scaleMin : sc > scaleMax ? scaleMax : sc);
		if (notifyCtrl) {
			notifyCtrl->handleSplitterChanged(*this, this->scale, clampedAt);
		} else{
			parentCtrl->relayout(); //TODO: this sucks, triggers a complete relayout
		}

	}
	virtual void handleDraggedRelease(MouseEvent& evt) {
	}
	virtual bool isStaticContainer() {
		return true;
	}
	float getScale() const {
		return this->scale;
	}
	void setScale(float f) {
		this->scale = f;
	}
	float getMin() {
		return scaleMin;
	}
	float getMax() {
		return scaleMax;
	}
};

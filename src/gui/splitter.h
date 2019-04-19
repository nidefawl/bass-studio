#pragma once
#include "math/vec.h"
#include "math/seq_math.h"
#include "gui.h"
#include "exceptions.h"
#include "mouse.h"
#include "event.h"
#include "basectrl.h"

class Splitter : public guictr_base {
public:
	static constexpr int SPLITTER_LAYOUT_THICKNESS = 10;
	int type;
	float scale;
	float min, max;
	Splitter(int _type, float _scale)
	: guictr_base(),
	  type(_type),
	  scale(_scale)
	{
		min = 0;
		max = 1;
		padding = 0;
	}
	void setMinMax(float _min, float _max) {
		this->min = _min;
		this->max = _max;
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
		this->scale = (sc < min ? min : sc > max ? max : sc);
		parentCtrl->relayout();
	}
	virtual void handleDraggedRelease(MouseEvent& evt) {
	}
	virtual bool isStaticContainer() {
		return true;
	}
};

#pragma once
#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <nanovg.h>
#include <functional>
#include "color_util.h"
#include "str_util.h"
#include "seq_util.h"
#include "seq_math.h"
#include "gui.h"
#include "guicolors.h"
#include "event.h"
using glm::vec2;
using glm::ivec2;

class guiknob : public guibase {
	const float angleOpen = 90;
	const float range = (360 - angleOpen) * M_PI / 180.0f;
	const float start = -FLOAT_PI * 1.5f + (angleOpen / 2.0f) * M_PI / 180.0f;
	bool* enabledPtr = NULL;
	float* valuePtr = NULL;
	float value = 0.0f;
	const bool renderBackground;
public:
    std::function<float()> fnGetValue;
    std::function<void(float)> fnSetValue;
    std::function<void(bool)> fnFocus;
	NVGcolor valColor = G_BLUE;
	NVGcolor indColor = G_WHITE;
	guiknob(const bool _renderBackground = true) : guibase(), renderBackground(_renderBackground) {
	}
	virtual bool hovered() {
		return this == AppCtrl::get()->guiOver;
	}
	virtual bool pressed() {
		return this == AppCtrl::get()->guiDragged;
	}
	virtual bool focused() {
		return this == AppCtrl::get()->guiFocused;
	}
	virtual void handleDraggedBegin(MouseEvent& evt) {
		if (evt.guiDragged == this) {
			AppCtrl::get()->captureMouse(this);
		}
	}
	virtual void handleDraggedMove(MouseEvent& evt) {
		if (evt.guiDragged == this && evt.type == M_EVT_CAPTURED_MOVE) {
			int disty = (int)evt.dragDistance->y;
			if (abs(disty) < 1)
				return;
			float value = getValue();
			float scale = isCtrl(evt.kbmods) ? 2000.0f : 200.0f;
			float delta = disty/scale;
			if (abs(delta) > 1e-2f) {
				value -= delta;
				setValue(value);
				evt.dragDistance->y = 0;
			}
		}
	}
    virtual bool focusEvent(bool focused) override {
    	if (fnFocus) fnFocus(focused);
    	return true;
    }
	virtual bool handleMouseScroll(MouseEvent& evt, double xoffset, double yoffset) {
		float value = getValue();
		float scale = isCtrl(evt.kbmods) ? 200.0f : 20.0f;
		value += yoffset/scale;
		setValue(value);
		return true;
	}
	virtual void handleDraggedRelease(MouseEvent& evt) {
	}
	void handleRightClick(MouseEvent& evt) override {
		if (parent)
			parent->rightClicked(evt, this);
	}
	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
		if (contains(mpos)) {
			evt.requestFocus(this);
			return true;
		}
		return false;
	}
	void render(NVGcontext* vg) {
		nvgLineCap(vg, NVGlineCap::NVG_ROUND);
		ivec2 insetP = pos+ivec2(0);
		ivec2 insetS = size-ivec2(0);
		if (renderBackground) {
			renderWidgetBorder(vg);
	//		nvgBeginPath(vg);
	//		nvgRect(vg, insetP.x, insetP.y, insetS.x, insetS.y);
	//		nvgFillColor(vg, c);
	//		nvgFill(vg);
		}
	    float cx = insetP.x+insetS.x/2.0f;
	    float cy = insetP.y+insetS.y/1.8f;
	    	    vec2 center(cx, cy);
	    float minSize = min(insetS.x, insetS.y);
//	    float r = (minSize*0.66f)/2.0f;
	    float r = (minSize*0.8f)/2.0f;
	    float lineThickness = max(1.0f, roundf((minSize / 8.0f)*2.0f)/2.0f);

		NVGcolor c2 = g_guiColors[COL_BG_BRT];
		if (hovered())
			c2 = g_guiColors[COL_BG_DRKER];
		if (focused())
			c2 = g_guiColors[COL_BG_DRKER2];
		if ((hovered() || focused())) {
//		    nvgBeginPath(vg);
//		    nvgCircle(vg, cx, cy, r*1.5f);
//		    nvgFillColor(vg, c2);
//			nvgFill(vg);
		}
//	    nvgBeginPath(vg);
//	    vec2 pts[3] = {
//				vec2(cosf(start), sinf(start)),
//				vec2(0, -1),
//				vec2(cosf(start+range), sinf(start+range)),
//	    };
//		for (vec2 v : pts) {
//			vec2 vStart = v * (r-lineThickness*0.5f) + center;
//			vec2 vEnd = v * (r+lineThickness*1.2f) + center;
//		    nvgMoveTo(vg, vStart.x, vStart.y);
//		    nvgLineTo(vg, vEnd.x, vEnd.y);
//	    }
//		nvgStrokeColor(vg, g_guiColors[COL_GRID_BRT]);
//		nvgStrokeWidth(vg, max(1.0f, round((r/16.0f)*2.0f)/2.0f));
//		nvgStroke(vg);

	    nvgBeginPath(vg);
	    nvgArc(vg, cx, cy, r, start, start+range, NVG_CW);
		nvgStrokeColor(vg, G_WHITE);
		nvgStrokeWidth(vg, lineThickness);
		nvgStroke(vg);
		float val = getValue();
		float end = start + val * range;
	    if (val > 1E-8F) {
		    nvgBeginPath(vg);
		    nvgArc(vg, cx, cy, r, start, end, NVG_CW);
			nvgStrokeColor(vg, valColor);
			nvgStrokeWidth(vg, lineThickness+1.0f);
			nvgStroke(vg);
	    }

	    nvgBeginPath(vg);
	    nvgCircleFast(vg, cx, cy, r*0.7f);
	    nvgFillColor(vg, g_guiColors[COL_BG_DRKER2]);
		nvgFill(vg);
	    nvgBeginPath(vg);
	    nvgCircleFast(vg, cx, cy, r*0.7f-1.5f);
	    nvgFillColor(vg, c2);
		nvgFill(vg);
		vec2 pos(cosf(end), sinf(end));
		vec2 posStart = pos*1.5f+center;
		vec2 posEnd = pos*r*0.7f+center;
		nvgBeginPath(vg);
		nvgMoveTo(vg, posStart.x, posStart.y);
		nvgLineTo(vg, posEnd.x, posEnd.y);
		nvgStrokeColor(vg, indColor);
		nvgStrokeWidth(vg, max(1.0f, roundf((r/8.0f)*2.0f)/2.0f));
		nvgStroke(vg);
		nvgLineCap(vg, NVGlineCap::NVG_BUTT);


	}
	void setValue(float newValue) {
		newValue = CLAMP_I(newValue, 0.0f, 1.0f);
		if (fnSetValue) {
			fnSetValue(newValue);
		} else if (valuePtr) {
			*valuePtr = newValue;
		} else {
			value = newValue;
		}
	}
	virtual float getValue() {
		if (fnGetValue) {
			return fnGetValue();
		} else if (valuePtr) {
			return *valuePtr;
		} else {
	//	    float time = fmod(getTimeMillis()/1000.0f, 2.0f);
	//	    float val = CLAMP_I(0.9f*sinf(time * FLOAT_PI) + 0.5f, 0.0f, 1.0f);
			return value;
		}
	}
	void setEnabledRef(bool* _enabledPtr) {
		enabledPtr = _enabledPtr;
	}
	void setValueRef(float* _valuePtr) {
		valuePtr = _valuePtr;
	}
	virtual bool enabled() {
		if (enabledPtr)
			return *enabledPtr;
		return true;
	}
};

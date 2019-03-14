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
struct automatable_t;
class guiknob : public guibase {
protected:
	const float angleOpen = 90;
	const float range = (360 - angleOpen) * M_PI / 180.0f;
	const float start = -FLOAT_PI * 1.5f + (angleOpen / 2.0f) * M_PI / 180.0f;
	bool* enabledPtr = NULL;
	float* valuePtr = NULL;
	float value = 0.0f;
	const bool renderBackground;
	bool changedValue = false;
	float initialValue = 0.0f;
	float lastVal = 0.0f;
#ifdef BUILD_BUILTIN_EFFECT
	automatable_t* paramAutomatable = nullptr;
	int32_t paramIdx = -1;
#endif
public:
    std::function<float()> fnGetValue;
    std::function<void(float,int)> fnSetValue;
    std::function<void(float,float)> fnValueEditChanged;
    std::function<void(float,float)> fnValueEditFinish;
    std::function<void(MouseHitEvt&, bool)> fnFocus;
	NVGcolor valColor = G_BLUE;
	NVGcolor indColor = G_WHITE;
	guiknob(const bool _renderBackground = true) : guibase(), renderBackground(_renderBackground) {
	}
#ifdef BUILD_BUILTIN_EFFECT
	void setAutomationRef(automatable_t* _paramAutomatable, int32_t _paramIdx) {
		this->paramAutomatable = _paramAutomatable;
		this->paramIdx = _paramIdx;
	}
	void setAutomationHandlers();
#endif
	virtual bool hovered() {
		return this == parentCtrl->guiOver;
	}
	virtual bool pressed() {
		return this == parentCtrl->guiDragged;
	}
	virtual bool focused() {
		return this == parentCtrl->guiFocused;
	}
	virtual void handleDraggedBegin(MouseEvent& evt) {
		if (evt.guiDragged == this) {
			parentCtrl->captureMouse(this);
		}
		initialValue = getValue();
		changedValue = false;
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
				setValue(value, 0);
				evt.dragDistance->y = 0;
				lastVal = value;
				changedValue = true;
			}
		}
	}
	virtual void handleDraggedRelease(MouseEvent& evt) {
		if (changedValue) {
			onValueEditFinish(initialValue, lastVal);
		}
		changedValue = false;
	}
    virtual bool focusEvent(MouseHitEvt& evt, bool focused) override {
    	if (fnFocus) fnFocus(evt, focused);
    	return true;
    }
	virtual bool handleMouseScroll(MouseEvent& evt, double xoffset, double yoffset) {
		float value = getValue();
		float scale = isCtrl(evt.kbmods) ? 200.0f : 20.0f;
		value += yoffset/scale;
		setValue(value, 2);
		return true;
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
	void renderButtonAt(NVGcontext* vg, ivec2 insetP, ivec2 insetS);
	virtual void updateAutomationState(NVGcontext* vg);
	virtual void render(NVGcontext* vg);
	float getValueInternal() {
		return value;
	}
	void setValueInit(float newValue) {
		value = newValue;
		if (valuePtr) {
			*valuePtr = newValue;
		}
	}
	void setValue(float newValue, int flags) {
		float curval = getValue();
		newValue = CLAMP_I(newValue, 0.0f, 1.0f);
		value = newValue;
		if (fnSetValue) {
			fnSetValue(newValue, flags);
		} else if (valuePtr) {
			*valuePtr = newValue;
		}
		if (fnValueEditChanged) {
			fnValueEditChanged(curval, getValue());
		}
	}
	virtual void onValueEditFinish(float from, float to) {
		if (fnValueEditFinish) {
			fnValueEditFinish(from, to);
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
	guictxtmenu_base* getTooltip(AppCtrl* appctrl) override;
};

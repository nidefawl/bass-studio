#pragma once
#include <glm/vec2.hpp>
#include <nanovg.h>
#include <functional>
#include "color_util.h"
#include "str_util.h"
#include "seq_util.h"
#include "seq_math.h"
#include "gui.h"
#include "guicolors.h"
#include "basectrl.h"
#include "event.h"
using glm::vec2;
using glm::ivec2;

class guibuttonbase : public guibase {
public:
	guibuttonbase() : guibase() {
	}
	guibuttonbase(ivec2 _pos, ivec2 _size) : guibase(_pos, _size) {
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
	virtual bool enabled() {
		return true;
	}
	virtual void handleDraggedMove(MouseEvent& evt) {
	}
	virtual void handleDraggedRelease(MouseEvent& evt) {
		if (parent)
			parent->buttonClicked(this);
	}
	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
		if (contains(mpos)) {
			evt.requestFocus(this);
			return true;
		}
		return false;
	}
	virtual int32_t getStateFlags() {
		int32_t flgs = 0;
		if (pressed()) {
			flgs |= FLG_DRG;
		}
		if (hovered()) {
			flgs |= FLG_HVRD;
		}
		if (focused()) {
			flgs |= FLG_FOC;
		}
		if (enabled()) {
			flgs |= FLG_ENBL;
		}
		return flgs;
	}
};
class guibutton : public guibuttonbase {
	bool* enabledPtr = NULL;
	bool* activePtr = NULL;
public:
	guibutton() : guibuttonbase() {
	}
	virtual bool enabled() {
		if (enabledPtr)
			return *enabledPtr;
		return true;
	}
	virtual int active() {
		if (activePtr)
			return (*activePtr) ? 1 : 0;
		return -1;
	}
	void setEnabledRef(bool* _enabledPtr) {
		enabledPtr = _enabledPtr;
	}
	void setActiveRef(bool* _activePtr) {
		activePtr = _activePtr;
	}
	void (*drawFn)(NVGcontext*,ivec2&, ivec2&, const NVGcolor&, int drawParm, int drawParm2) = NULL;
	int drawParm = 0;
	void render(NVGcontext* vg) {
		renderWidgetBorder(vg, getStateFlags());
		if (drawFn) {
			drawFn(vg, pos, size, theme->getBgColor(getStateFlags()), drawParm, active());
		}
	}
};
class guibuttontoggle : public guibuttonbase {
	int _getIcon() {
		return getIcon?getIcon():icon;
	}
public:
	float radius = 0;
	bool* state = NULL;
	int icon = -1;
    std::function<int()> getIcon;
	guibuttontoggle() : guibuttonbase() {
	}
	guibuttontoggle(float _radius) : guibuttonbase(ivec2(0), ivec2((int)(_radius * 2))) {
		this->radius = _radius;
	}
	bool enabled() override {
		if (state)
			return *state;
		return true;
	}
	void render(NVGcontext* vg) {
		vec2 cen = vec2(radius);
		cen.x += pos.x;
		cen.y += pos.y;
		int32_t state = getStateFlags();
		nvgBeginPath(vg);
		nvgCircleFast(vg, cen.x, cen.y, radius);
		nvgFillColor(vg, theme->getBgColor(state));
		nvgFill(vg);
		nvgStrokeColor(vg, theme->getBgStrokeColor(state));
		nvgStrokeWidth(vg, theme->getBgStrokeWidth(state));
		nvgStroke(vg);
		int icon = _getIcon();
		if (icon >= 0) {


			int32_t extImg = 2;
			int32_t iconW = (int32_t)ceil(radius*2)+extImg*2;
			RenderResources::NvgImageTexture& image = RenderResources::imgIcons[icon];
			NVGpaint paintIcon = nvgImagePattern(vg, -extImg, -extImg, iconW, iconW, 0, image.id, 1.0f);
			nvgTranslate(vg, pos.x, pos.y);
			nvgBeginPath(vg);
			nvgRect(vg, -extImg, -extImg, iconW, iconW);
			nvgFillPaint(vg, paintIcon);
			nvgFill(vg);
			nvgTranslate(vg, -pos.x, -pos.y);
		}

		/*nvgBeginPath(vg);
		nvgRect(vg, pos.x, pos.y, size.x, size.y);
		nvgFillColor(vg, c);
		nvgFill(vg);*/
	}
};

#pragma once
#include <glm/vec2.hpp>
#include <nanovg.h>
#include "color_util.h"
#include "str_util.h"
#include "seq_util.h"
#include "seq_math.h"
#include "gui.h"
#include "guicolors.h"
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
		return this == MainCtrl::get()->guiOver;
	}
	virtual bool pressed() {
		return this == MainCtrl::get()->guiDragged;
	}
	virtual bool focused() {
		return this == MainCtrl::get()->guiFocused;
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
	virtual bool active() {
		if (activePtr)
			return *activePtr;
		return true;
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
			drawFn(vg, pos, size, theme->getBgColor(getStateFlags()), drawParm, activePtr ? active() : -1);
		}
	}
};

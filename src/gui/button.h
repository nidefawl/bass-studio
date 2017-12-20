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
	NVGcolor color;
	NVGcolor colorStroke;
	NVGcolor colorHover;
	NVGcolor colorPressed;
	guibuttonbase() : guibase() {
		uint32_t rgb = nvgToRGB(g_guiColors[COL_BG_DRK]);
		setColor(rgb);
	}
	guibuttonbase(ivec2 _pos, ivec2 _size) : guibase(_pos, _size) {
		uint32_t rgb = nvgToRGB(g_guiColors[COL_BG_DRK]);
		setColor(rgb);
	}
	void setColor(uint32_t hex) {
		vec4 hsl = hexToHSL(hex);
		color = nvgHSL(hsl.x, hsl.y, hsl.z);
		colorStroke = nvgHSL(hsl.x, CLAMP_F(hsl.y*1.3f), 0.4f);
		colorHover = nvgHSL(hsl.x, CLAMP_F(hsl.y*0.7f), CLAMP_F(hsl.z + 0.3f));
		colorPressed = nvgHSL(hsl.x, CLAMP_F(hsl.y*0.7f), CLAMP_F(hsl.z + 0.3f));
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
};
class guibutton : public guibuttonbase {
	bool* enabledPtr = NULL;
	bool* activePtr = NULL;
public:
	guibutton() : guibuttonbase() {
		setColor(nvgToRGB(g_guiColors[COL_BG_DRK]));
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
	void (*drawFn)(NVGcontext*,ivec2&, ivec2&, NVGcolor&, int drawParm, int drawParm2) = NULL;
	int drawParm = 0;
	void render(NVGcontext* vg) {
		NVGcolor c;
		if (!enabled()) {
			c = G_BUTTON_DISABLED;
		}
		else if (pressed()) {
			c = colorPressed;
		}
		else if (hovered()) {
			c = colorHover;
		}
		else {
			c = color;
		}
		ivec2 insetP = pos+ivec2(1);
		ivec2 insetS = size-ivec2(2);
		renderWidgetBorder(vg);
		nvgBeginPath(vg);
		nvgRect(vg, insetP.x, insetP.y, insetS.x, insetS.y);
		nvgFillColor(vg, c);
		nvgFill(vg);
		if (hovered() || focused()) {
			NVGcolor c2 = colorStroke;
			if (hovered())
				c2 = G_WHITE;
			nvgBeginPath(vg);
			nvgRect(vg, pos.x, pos.y, size.x, size.y);
			nvgStrokeColor(vg, c2);
			nvgStrokeWidth(vg, 1);
			nvgStroke(vg);
		}
		if (drawFn) {
			drawFn(vg, pos, size, c, drawParm, activePtr ? active() : -1);
		}
	}
};

#pragma once
#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <nanovg.h>
#include <functional>
#include "guicolors.h"
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
namespace GuiColor {
extern constant_t COL_BTN_BG_DEFAULT_INACTIVE;
extern constant_t COL_BTN_BG_DEFAULT_ACTIVE;
extern constant_t COL_BTN_BG_BYPASS_ACTIVE;
extern constant_t COL_BTN_BG_SHOW_ACTIVE;
}

class guibuttonbase : public guibase {
protected:
	String str = "";
	int fontSize = 0;
public:
	guibuttonbase() : guibase() {
	}
	guibuttonbase(ivec2 _pos, ivec2 _size) : guibase(_pos, _size) {
	}
	virtual void handleDraggedMove(MouseEvent& evt) {
	}
	virtual void handleDraggedRelease(MouseEvent& evt) {
		if (parent)
			parent->buttonClicked(this);
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
	void setText(String _str) {
		str = _str;
	}
	void setFontSize(int fs) {
		fontSize = fs;
	}
	int getFontSize() {
		return fontSize;
	}
};
class guibutton : public guibuttonbase {
	bool* enabledPtr = NULL;
//	bool* activePtr = NULL;
public:
	guibutton() : guibuttonbase() {
	}
	virtual bool isEnabled() override {
		if (enabledPtr)
			return *enabledPtr;
		return guibuttonbase::isEnabled();
	}
//	virtual int active() {
//		if (activePtr)
//			return (*activePtr) ? 1 : 0;
//		return -1;
//	}
	void setEnabledRef(bool* _enabledPtr) {
		enabledPtr = _enabledPtr;
	}
//	void setActiveRef(bool* _activePtr) {
//		activePtr = _activePtr;
//	}
	virtual int32_t getStateFlags() {
		int32_t state = guibuttonbase::getStateFlags();
//		if (active()) {
//			state |= FLG_ACT;
//		}
		return state;
	}
	void (*drawFn)(NVGcontext*,ivec2&, ivec2&, const NVGcolor&, int drawParm, int drawParm2) = NULL;
	int drawParm = 0;
	void render(NVGcontext* vg) {
		int32_t fl = getStateFlags();
		renderWidgetBorder(vg, fl);
		if (str.length() > 0) {
			int fontScale = this->fontSize > 0 ? this->fontSize : G_FONT_SCALE(size.y);
			GuiColor::constant_t c = (fl&FLG_ENBL) ? GuiColor::COL_LABEL_ACTIVE : GuiColor::COL_LABEL_INACTIVE;
			NVGcolor color = theme->getColor(c);
			setFont(vg, fontScale, color, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
			nvgText(vg, pos.x + size.x / 2.0f, pos.y + G_FONT_MIDDLE_OFFSET(size.y), StringAsCStr(str), NULL);
		}

		if (drawFn) {
			drawFn(vg, pos, size, theme->getBgColor(getStateFlags()), drawParm, isEnabled());
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
    std::function<bool()> getState;
	GuiColor::constant_t colorActive = GuiColor::COL_BTN_BG_DEFAULT_ACTIVE;
	guibuttontoggle() : guibuttonbase() {
	}
	guibuttontoggle(float _radius) : guibuttonbase(ivec2(0), ivec2((int)(_radius * 2))) {
		this->radius = _radius;
	}
	void setRadius(float fRadius) {
		this->radius = fRadius;
//		this->size = ivec2((int)(fRadius * 2));
	}
	bool isEnabled() override {
		if (state)
			return *state;
		if (getState)
			return getState();
		return true;
	}
	void render(NVGcontext* vg);
};

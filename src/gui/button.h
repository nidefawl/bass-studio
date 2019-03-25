#pragma once
#include <nanovg.h>
#include <functional>
#include "math/vec.h"
#include "math/seq_math.h"
#include "guicolors.h"
#include "color_util.h"
#include "str_util.h"
#include "seq_util.h"
#include "gui.h"
#include "guicolors.h"
#include "basectrl.h"
#include "event.h"

namespace GuiColor {
extern constant_t COL_BTN_BG_DEFAULT_INACTIVE;
extern constant_t COL_BTN_BG_DEFAULT_ACTIVE;
extern constant_t COL_BTN_BG_BYPASS_ACTIVE;
extern constant_t COL_BTN_BG_SHOW_ACTIVE;
}

class guibuttonbase : public guibase {
protected:
	GuiColor::constant_t buttonColor;
	String str = "";
	int fontSize = 0;
public:
	void (*drawFn)(NVGcontext*, ivec2&, ivec2&, const NVGcolor&, int drawParm, int drawParm2) = NULL;
	int drawParm = 0;
public:
	guibuttonbase() : guibase() {
	}
	void setButtonColor(GuiColor::constant_t color) {
		buttonColor = color;
		setFlagInternal(FLG_HAS_COLOR_BG);
	}
	virtual NVGcolor getBackgroundColor(int stateflags) const override;
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
	void renderButtonLabel(NVGcontext* vg, int32_t stateFlags) {
		if (drawFn||str.length()) {
			nvgSave(vg);
			setScissorTransform(vg);
			ivec2 renderPos(0);
			if (str.length() > 0) {
	//			nvgDawText(vg, this, pos, size, str, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
				int fontScale = this->fontSize > 0 ? this->fontSize : G_FONT_SCALE(size.y);
				GuiColor::constant_t c = (stateFlags&FLG_ENBL) ? GuiColor::COL_LABEL_ACTIVE : GuiColor::COL_LABEL_INACTIVE;
				NVGcolor color = theme->getColor(c);
				setFont(vg, fontScale, color, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
				float strWidth = nvgText(vg, renderPos.x + size.x / 2.0f, renderPos.y + G_FONT_MIDDLE_OFFSET(size.y), StringAsCStr(str), NULL);
				if (strWidth*2 > size.x) {
				}
			}

			if (drawFn) {
				drawFn(vg, renderPos, size, getBackgroundColor(getStateFlags()), drawParm, isEnabled());
			}
			nvgRestore(vg);
		}
	}
};
class guibutton : public guibuttonbase {
	bool* enabledPtr = NULL;
//	bool* activePtr = NULL;
public:
	guibutton() : guibuttonbase() {
	}
	virtual bool isEnabled() const override {
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
	virtual int32_t getStateFlags() const {
		int32_t state = guibuttonbase::getStateFlags();
//		if (active()) {
//			state |= FLG_ACT;
//		}
		return state;
	}
	void render(NVGcontext* vg) {
		int32_t fl = getStateFlags();
		renderWidgetBorder(vg, fl);
		renderButtonLabel(vg, fl);
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
	void setRadius(float fRadius) {
		this->radius = fRadius;
		if (size.x == 0 && size.y == 0) {
			size = ivec2((int32_t)std::round(this->radius*2.0f));
		}
	}
	bool isEnabled() const override {
		if (state)
			return *state;
		if (getState)
			return getState();
		return true;
	}
	void render(NVGcontext* vg);
};

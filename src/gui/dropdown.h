#pragma once
#include <nanovg.h>
#include "color_util.h"
#include "str_util.h"
#include "seq_util.h"
#include "math/seq_math.h"
#include "gui.h"
#include "guicolors.h"
#include "event.h"
#include "button.h"


class guidropdownbase : public guibuttonbase {
public:
	enum dropdown_field_selectitem {
		SELECT_IDX,
		SELECT_NEXT,
		SELECT_PREVIOUS,
		SELECT_FIRST,
		SELECT_LAST,
	};
	guidropdownbase() : guibuttonbase() {
	}
	void render(NVGcontext* vg) override {
//		nvgIntersectScissor(vg, pos.x, pos.y, size.x, size.y);
		renderWidgetBorder(vg, getStateFlags());
		int fontScale = math::round((this->fontSize > 0 ? this->fontSize : size.y) * fFontScale);
		setFont(vg, fontScale, G_WHITE, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgText(vg, pos.x + size.x / 2.0f, pos.y + G_FONT_MIDDLE_OFFSET(size.y), StringAsCStr(getString()), NULL);
	}
	void handleDraggedRelease(MouseEvent& evt) override {
		if (parent)
			parent->buttonClicked(this);
	}
	virtual bool handleMouseScroll(MouseEvent& evt, double xoffset, double yoffset);
	virtual void handleRightClick(MouseEvent& evt) {
	}
	virtual bool handleKeyInput(KeyEvent& kevt);
	virtual bool handleCharInput(unsigned int codepoint) {
		return false;
	}
	virtual void select(dropdown_field_selectitem req, uint32_t idxOffset);
	virtual uint32_t getSelectIndex() {
		return 0xFFFFFFFF;
	}
	virtual uint32_t getLastIndex() {
		return 0;
	}
	virtual void setSelectedIndex(uint32_t) {

	}
	virtual String getString() = 0;
};


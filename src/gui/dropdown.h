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
	virtual String getString() = 0;
};

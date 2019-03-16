#pragma once
#include <nanovg.h>
#include "color_util.h"
#include "str_util.h"
#include "seq_util.h"
#include "seq_math.h"
#include "gui.h"
#include "guicolors.h"
#include "event.h"
#include "button.h"


class guidropdownbase : public guibuttonbase {
public:
	guidropdownbase() : guibuttonbase() {
	}
	void render(NVGcontext* vg) {
		renderWidgetBorder(vg, getStateFlags());
		setFont(vg, G_FONT_SCALE(size.y), G_WHITE, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgText(vg, pos.x + size.x / 2.0f, pos.y + G_FONT_MIDDLE_OFFSET(size.y), StringAsCStr(getString()), NULL);
	}
	virtual void handleDraggedRelease(MouseEvent& evt) {
		if (parent)
			parent->buttonClicked(this);
	}
	virtual String getString() = 0;
};

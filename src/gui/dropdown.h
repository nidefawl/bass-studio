#pragma once
#include <glm/vec2.hpp>
#include <nanovg.h>
#include <vector>
#include "color_util.h"
#include "str_util.h"
#include "seq_util.h"
#include "seq_math.h"
#include "gui.h"
#include "guicolors.h"
#include "event.h"
#include "button.h"

using glm::vec2;
using glm::ivec2;
class guidropdown_entry {

	void render(NVGcontext* vg) {

	}
};
class guidropdown : public guibuttonbase {
	String cur;
public:
	guidropdown() : guibuttonbase() {
	}
	virtual bool enabled() {
		return true;
	}
	void render(NVGcontext* vg) {
		renderWidgetBorder(vg, getStateFlags());
		setFont(vg, G_FONT_SCALE(size.y), G_WHITE, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgText(vg, pos.x + size.x / 2.0f, pos.y + G_FONT_MIDDLE_OFFSET(size.y), StringAsCStr(cur), NULL);
	}
	virtual void handleDraggedRelease(MouseEvent& evt) {
		if (parent)
			parent->buttonClicked(this);
	}
	virtual void setCur(String _cur) {
		cur = _cur;
	}
};

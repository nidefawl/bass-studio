#include "dropdown.h"
#include "str_util.h"

bool guidropdownbase::handleMouseScroll(MouseEvent& evt, double xoffset, double yoffset) {
	if (0xFFFFFFFF != getSelectIndex()) {
		bool isUp = yoffset > 0;
		if (isUp) {
			select(dropdown_field_selectitem::SELECT_PREVIOUS, 1);
		} else {
			select(dropdown_field_selectitem::SELECT_NEXT, 1);
		}
		return true;
	}
	return false;
}

bool guidropdownbase::handleKeyInput(KeyEvent& kevt) {
    if (kevt.type == KeyEventType::K_PRESS || kevt.type == KeyEventType::K_REPEAT) {
    	if (isArrowKey(kevt.keyCode)) {
    		ivec2 dir;
    		arrowKeyToXY(kevt.keyCode, dir.x, dir.y);
    		if (dir.y) {
    			if (dir.y > 0) {
    				select(dropdown_field_selectitem::SELECT_PREVIOUS, 1);
    			} else {
    				select(dropdown_field_selectitem::SELECT_NEXT, 1);
    			}
    		}
    	}
    }
	return false;
}

void guidropdownbase::select(dropdown_field_selectitem req, uint32_t idxOffset) {
	uint32_t index = getSelectIndex();
	if (index == 0xFFFFFFFF)
		return;

	switch (req) {
	case SELECT_IDX:
		setSelectedIndex(idxOffset);
		break;
	case SELECT_NEXT:
		setSelectedIndex(math::min<uint32_t>(getLastIndex(), index + idxOffset));
		break;
	case SELECT_PREVIOUS:
		setSelectedIndex(math::max<uint32_t>(0, index - idxOffset));
		break;
	case SELECT_FIRST:
		setSelectedIndex(0);
		break;
	case SELECT_LAST:
		setSelectedIndex(getLastIndex());
		break;
	}
}

void guidropdownbase::render(NVGcontext* vg) {
	//		nvgIntersectScissor(vg, pos.x, pos.y, size.x, size.y);
	renderWidgetBorder(vg, getStateFlags());
	if (this->label.length()) {
		setFont(vg, G_FONT_SCALE(size.y), G_WHITE, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
		String str = getString();
		float pX = nvgText(vg, pos.x + size.x - 3, pos.y + G_FONT_MIDDLE_OFFSET(size.y), StringAsCStr(str), NULL);
		NVGcolor mDisabledTextColor = GUI_COLORA(255, 80);
		setFont(vg, G_FONT_SCALE(size.y), mDisabledTextColor, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
		float bounds[4] { 0 };
		nvgTextBounds(vg, pos.x + 3.0f, pos.y + G_FONT_MIDDLE_OFFSET(size.y), StringAsCStr(this->label), nullptr, bounds);
		if (pX - 3 > bounds[2]) {
			nvgText(vg, pos.x + 3.0f, pos.y + G_FONT_MIDDLE_OFFSET(size.y), StringAsCStr(this->label), NULL);
		}
	} else {
		int fontScale = math::round((this->fontSize > 0 ? this->fontSize : size.y) * fFontScale);
		setFont(vg, fontScale, G_WHITE, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgText(vg, pos.x + size.x / 2.0f, pos.y + G_FONT_MIDDLE_OFFSET(size.y), StringAsCStr(getString()), NULL);
	}
}

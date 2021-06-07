#pragma once
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

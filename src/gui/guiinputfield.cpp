#include <stdbool.h>
#include <stdint.h>
#include "guiinputfield.h"

#include "math/seq_math.h"
#include "str_util.h"
#include "color_util.h"

#include "keyboard.h"
#include "gui.h"
#include "guicolors.h"
#include "guicontainer.h"
#include "button.h"
#include "knob.h"
#include "basectrl.h"
#include "textfield.h"


gui_numberinput_field::gui_numberinput_field(int32_t* _number) :
		guibuttonbase(), number(_number) {
	field.setParent(this);
}

void gui_numberinput_field::layout() {
	field.pos = pos;
	field.size = size;
	field.layout();
	field.setFontSize(math::max(4, field.size.y - 2));
}

void gui_numberinput_field::render(NVGcontext* vg) {
	int32_t fl = getStateFlags();
	renderWidgetBorder(vg, fl);
	if (isEditing) {
		this->field.render(vg);
		return;
	}
	setFont(vg, G_FONT_SCALE(size.y), G_WHITE, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
	int32_t _number = number ? *number : 0;
	String str = StringFormat("%d", _number);
	float pX = nvgText(vg, pos.x + size.x - 3, pos.y + G_FONT_MIDDLE_OFFSET(size.y), StringAsCStr(str), NULL);
	if (!isEditing && this->label.length()) {
		NVGcolor mDisabledTextColor = GUI_COLORA(255, 80);
		setFont(vg, G_FONT_SCALE(size.y), mDisabledTextColor, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
		float bounds[4]{0};
		nvgTextBounds(vg, pos.x + 3.0f, pos.y + G_FONT_MIDDLE_OFFSET(size.y), StringAsCStr(this->label), nullptr, bounds);
		if (pX-3 > bounds[2]) {
			nvgText(vg, pos.x + 3.0f, pos.y + G_FONT_MIDDLE_OFFSET(size.y), StringAsCStr(this->label), NULL);
		}

	}
}

bool gui_numberinput_field::focusEvent(MouseHitEvt& evt, bool focused) {
	if (!focused) {
		endEdit(true);
	} else {
		//			isEditing = true;
	}
	this->field.focusEvent(evt, focused);
	return true;
}

void gui_numberinput_field::endEdit(bool success) {
	if (isEditing) {
		this->field.endEdit();
		if (success && this->number) {
			const char* cstr = this->field.value().c_str();
			int newVal = atoi(cstr);
			if (fnClamp) {
				newVal = fnClamp(newVal);
			}
			*number = newVal;
			if (parent)
				parent->buttonClicked(this);

			if (fnValueEditChanged) {
				fnValueEditChanged(this, newVal);
			}
		}
	}
	isEditing = false;
}

void gui_numberinput_field::startEdit(bool keepcontent) {
	if (!isEditing) {
		//            mValueTemp = mValue;
		if (keepcontent) {
			if (this->number) {
				String s = StringFormat("%d", *this->number);
				this->field.setValue(s);
			}
		} else {
			this->field.setValue("");
		}
		//			this->field.focusEvent(true); //MainCtrl::get()->setFocused(this);
		this->field.beginEdit();
		this->field.setSelectionRange(-1, -1);
	}
	isEditing = true;
}

void gui_numberinput_field::handleDraggedBegin(MouseEvent& evt) {
	if (isEditing) {
		this->field.handleDraggedBegin(evt);
	} else {
		if (evt.type == MouseEventType::M_EVT_DOUBLECLICK) {
			startEdit(true);
		} else {
			if (evt.guiDragged == this) {
				parentCtrl->captureMouse(this);
			}
		}
	}
}

void gui_numberinput_field::handleDraggedMove(MouseEvent& evt) {
	if (isEditing) {
		this->field.handleDraggedMove(evt);
		return;
	}
	if (number && evt.guiDragged == this && evt.type == M_EVT_CAPTURED_MOVE) {
		int disty = (int) (evt.dragDistance->y) / 2;
		if (math::abs(disty) < 1)
			return;

		evt.dragDistance->y = 0;
		int absy = math::abs(disty);
		if (absy >= 4)
			absy = 64;
		else if (absy >= 2)
			absy = 4;

		int newVal = *number - ((disty < 0 ? -1 : 1) * absy);
		if (fnClamp) {
			newVal = fnClamp(newVal);
		}
		*number = newVal;
		if (fnValueEditChanged) {
			fnValueEditChanged(this, newVal);
		}
		if (parent)
			parent->buttonClicked(this);

		return;
	}
}

void gui_numberinput_field::handleDraggedRelease(MouseEvent& evt) {
	if (isEditing) {
		this->field.handleDraggedRelease(evt);
	}
}

bool gui_numberinput_field::handleKeyInput(KeyEvent& kevt) {
	if (kevt.type != K_RELEASE) {
		if (kevt.keyCode == KEY_ENTER || kevt.keyCode == KEY_KP_ENTER || kevt.keyCode == KEY_ESCAPE) {
			endEdit(kevt.keyCode == KEY_ENTER || kevt.keyCode == KEY_KP_ENTER);
			return true;
		}
	}
	if (isEditing) {
		return this->field.handleKeyInput(kevt);
	}
	bool handled = false;
	if (kevt.type != K_RELEASE) {
		if (isArrowKey(kevt.keyCode)) {
			ivec2 dir;
			arrowKeyToXY(kevt.keyCode, dir.x, dir.y);
			if (dir.y) {
				if ((kevt.mods & KB_MOD_SHIFT)) {
					dir *= 12;
				}
				*number += dir.y;
				handled = true;
			}
		}

	}
	return handled;
}

bool gui_numberinput_field::handleCharInput(unsigned int codepoint) {
	startEdit(false);
	return this->field.handleCharInput(codepoint);
}

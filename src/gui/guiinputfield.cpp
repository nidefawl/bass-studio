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

void gui_numberinput_field_base::render(NVGcontext* vg) {
	int32_t fl = getStateFlags();
	renderWidgetBorder(vg, fl);
	if (isEditing) {
		this->field.render(vg);
		return;
	}
	setFont(vg, G_FONT_SCALE(size.y), G_WHITE, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
//	int32_t _number = number ? *number : 0;
//	String str = StringFormat("%d", _number);
	String str = getAsStringLiteral();
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

bool gui_numberinput_field_base::focusEvent(MouseHitEvt& evt, bool focused) {
	if (!focused) {
		endEdit(true);
	} else {
		//			isEditing = true;
	}
	this->field.focusEvent(evt, focused);
	return true;
}

void gui_numberinput_field_base::endEdit(bool success) {
	if (isEditing) {
		this->field.endEdit();
		if (success) {
			endEditImpl();
		}
	}
	isEditing = false;
}

void gui_numberinput_field_base::startEdit(bool keepcontent) {
	if (!isEditing) {
		//            mValueTemp = mValue;
		if (keepcontent) {
			this->field.setValue(getAsStringLiteral());
		} else {
			this->field.setValue("");
		}
		//			this->field.focusEvent(true); //MainCtrl::get()->setFocused(this);
		this->field.beginEdit();
		this->field.setSelectionRange(-1, -1);
	}
	isEditing = true;
}

void gui_numberinput_field_base::handleDraggedBegin(MouseEvent& evt) {
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

void gui_numberinput_field_base::handleDraggedMove(MouseEvent& evt) {
	if (isEditing) {
		this->field.handleDraggedMove(evt);
		return;
	}
	if (evt.guiDragged == this && evt.type == M_EVT_CAPTURED_MOVE) {
		int disty = (int) (evt.dragDistance->y) / 2;
		if (math::abs(disty) < 1)
			return;

		evt.dragDistance->y = 0;
		int absy = math::abs(disty);
		if (absy >= 4)
			absy = 64;
		else if (absy >= 2)
			absy = 4;
		onMouseDragValue(disty, absy);
		if (parent)
			parent->buttonClicked(this);

		return;
	}
}

void gui_numberinput_field_base::handleDraggedRelease(MouseEvent& evt) {
	if (isEditing) {
		this->field.handleDraggedRelease(evt);
	}
}

bool gui_numberinput_field_base::handleKeyInput(KeyEvent& kevt) {
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
				onKeyInputChangeValue(dir);
				handled = true;
			}
		}

	}
	return handled;
}

bool gui_numberinput_field_base::handleCharInput(unsigned int codepoint) {
	startEdit(false);
	return this->field.handleCharInput(codepoint);
}
template<>
int32_t gui_numberinput_field_generic<int32_t>::parseLiteral(const char* szNumber) {
	return atoi(szNumber);
}
template<>
String gui_numberinput_field_generic<int32_t>::valueToStringLiteral(int32_t val) {
	return StringFormat("%d", val);
}

template<>
void gui_numberinput_field_generic<int32_t>::onMouseDragValue(int32_t disty, int32_t absy) {
	if (this->number) {
		setValue(getValue() - ((disty < 0 ? -1 : 1) * absy));
	}
}
template<>
void gui_numberinput_field_generic<int32_t>::onKeyInputChangeValue(ivec2 direction) {
	if (this->number) {
		setValue(getValue() + direction.y);
	}
}
template<>
uint32_t gui_numberinput_field_generic<uint32_t>::parseLiteral(const char* szNumber) {
	return atoi(szNumber);
}
template<>
String gui_numberinput_field_generic<uint32_t>::valueToStringLiteral(uint32_t val) {
	return StringFormat("%d", val);
}

template<>
void gui_numberinput_field_generic<uint32_t>::onMouseDragValue(int32_t disty, int32_t absy) {
	if (this->number) {
		setValue(getValue() - ((disty < 0 ? -1 : 1) * absy));
	}
}
template<>
void gui_numberinput_field_generic<uint32_t>::onKeyInputChangeValue(ivec2 direction) {
	if (this->number) {
		setValue(getValue() + direction.y);
	}
}
template<>
float gui_numberinput_field_generic<float>::parseLiteral(const char* szNumber) {
	return atof(szNumber);
}
template<>
String gui_numberinput_field_generic<float>::valueToStringLiteral(float val) {
	return StringFormat("%f", val);
}
template<>
void gui_numberinput_field_generic<float>::onMouseDragValue(int32_t disty, int32_t absy) {
	if (this->number) {
		float number_local = getValue();
		int32_t u = *reinterpret_cast<int32_t*>(&number_local) + (disty > 0 ? -1 : 1) * absy;
		setValue(*reinterpret_cast<float*>(&u));
//		setValue(*number - ((disty < 0 ? -1 : 1) * absy)*0.0001f);
	}
}
template<>
void gui_numberinput_field_generic<float>::onKeyInputChangeValue(ivec2 direction) {
	if (this->number) {
		setValue(getValue() + direction.y*0.01f);
	}
}

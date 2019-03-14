#pragma once
#include <stdbool.h>
#include <stdint.h>

#include "str_util.h"
#include "color_util.h"

#include "keyboard.h"
#include "gui.h"
#include "guicontainer.h"
#include "button.h"
#include "knob.h"
#include "settings.h"
#include "basectrl.h"
#include "textfield.h"

class gui_numberinput_field: public guibuttonbase {
	int32_t* number;
	bool drawBackground = true;
	gui_textfield field;
	bool isEditing = false;

public:
	gui_numberinput_field(int32_t* _number) :
			guibuttonbase(), number(_number) {
		this->canTextInput = true;
		setColor(nvgToRGB(theme->getColor(COL_BG_DRK)));
	}
	virtual void setControl(BaseCtrl* parentCtrl) override {
		guibase::setControl(parentCtrl);
		field.setControl(parentCtrl);
	}
	void setDrawBackground(bool state) {
		drawBackground = state;
	}
	void setRef(int32_t* number) {
		this->number = number;
	}
	void layout() {
		field.pos = pos;
		field.size = size;
		field.layout();
		field.setFontSize(max(4, field.size.y-2));
	}

	void render(NVGcontext* vg) {
		int32_t flags = getStateFlags();
		if (drawBackground || (flags & (FLG_FOC|FLG_HVRD|FLG_DRG|FLG_ACT))) {
			renderWidgetBorder(vg, flags);
		}
//		setFont(vg, G_FONT_SCALE(size.y), G_WHITE, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
//		String str2 = StringFormat("mValue '%s'", StringAsCStr(field.mValue));
//		String str3 = StringFormat("mValueTemp '%s'", StringAsCStr(field.mValueTemp));
//		String str4 = StringFormat("mCursorPos %d", field.mCursorPos);
//
//		String sel = "";
//		field.copySelectionString(sel);
//		String str5 = StringFormat("selection '%s'", StringAsCStr(sel));
//		nvgText(vg, pos.x +  3, bottom() + G_FONT_MIDDLE_OFFSET(size.y), StringAsCStr(str2), NULL);
//		nvgText(vg, pos.x +  3, bottom() + size.y + G_FONT_MIDDLE_OFFSET(size.y), StringAsCStr(str3), NULL);
//		nvgText(vg, pos.x +  3, bottom() + size.y*2 + G_FONT_MIDDLE_OFFSET(size.y), StringAsCStr(str4), NULL);
//		nvgText(vg, pos.x +  3, bottom() + size.y*3 + G_FONT_MIDDLE_OFFSET(size.y), StringAsCStr(str5), NULL);
		if (isEditing) {
			this->field.render(vg);
			return;
		}


		setFont(vg, G_FONT_SCALE(size.y), G_WHITE, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
		int32_t _number = number ? *number : 0;
		String str = StringFormat("%d", _number);
		nvgText(vg, pos.x + size.x - 3, pos.y + G_FONT_MIDDLE_OFFSET(size.y), StringAsCStr(str), NULL);
	}

	bool focusEvent(MouseHitEvt& evt, bool focused) override {
		if (!focused) {
			endEdit(true);
		} else {
//			isEditing = true;

		}
		this->field.focusEvent(evt, focused);
		return true;
	}
	void endEdit(bool success) {
		if (isEditing) {
			this->field.endEdit();
			if (success && this->number) {
				const char* cstr = this->field.value().c_str();
				int newVal = atoi(cstr);
				*number = newVal;
				if (parent)
					parent->buttonClicked(this);
			}
		}
		isEditing = false;
	}
	void startEdit(bool keepcontent) {
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
	void handleDraggedBegin(MouseEvent& evt) override {
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
	void handleDraggedMove(MouseEvent& evt) override {
		if (isEditing) {
			this->field.handleDraggedMove(evt);
			return;
		}
		if (number && evt.guiDragged == this && evt.type == M_EVT_CAPTURED_MOVE) {
			int disty = (int) evt.dragDistance->y / 2;
			if (abs(disty) < 1)
				return;
			evt.dragDistance->y = 0;
			int absy = abs(disty);
			if (absy >= 4)
				absy = 64;
			else if (absy >= 2)
				absy = 4;
			*number -= (disty < 0 ? -1 : 1) * absy;
			if (parent)
				parent->buttonClicked(this);
			return;
		}
	}
	void handleDraggedRelease(MouseEvent& evt) override {
		if (isEditing) {
			this->field.handleDraggedRelease(evt);
		}
	}
	virtual bool handleKeyInput(KeyEvent& kevt) override {
		if (kevt.type != K_RELEASE) {
			if (kevt.keyCode == KEY_ENTER||kevt.keyCode == KEY_KP_ENTER || kevt.keyCode == KEY_ESCAPE) {
				endEdit(kevt.keyCode == KEY_ENTER||kevt.keyCode == KEY_KP_ENTER);
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
	virtual bool handleCharInput(unsigned int codepoint) override {
		startEdit(false);
		return this->field.handleCharInput(codepoint);
	}

};

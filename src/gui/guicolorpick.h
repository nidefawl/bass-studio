#pragma once
#include <vector>
#include <memory>
#include <functional>
#include <stdint.h>
#include <nanovg.h>

#include "str_util.h"
#include "gui/knob.h"
#include "gui/knoblabeled.h"
#include "gui/button.h"
#include "gui/guicontainer.h"
#include "keyboard.h"

#include "textfield.h"


uint32_t nvgToRGB(NVGcolor c);
class gui_input_filtered: public guibuttonbase {
	int32_t* number;
	bool drawBackground = true;
	gui_textfield field;
	input_filter_hex32 filter;
	bool isAlignCenter = false;
	bool isEditing = false;
	int draggedByte = -1;
public:
	gui_input_filtered(int32_t* _number) :
			guibuttonbase(), number(_number) {
		setTint(nvgToRGB(theme->getColor(GuiColor::COL_BG_DRK)));
		field.setParent(this);
		field.setFilter(&filter);
		setAlignCenter(false);
	}
	void setAlignCenter(bool b) {
		isAlignCenter = b;
		if (b) {
			field.setAlignment(gui_textfield::Alignment::Center);
		} else {
			field.setAlignment(gui_textfield::Alignment::Left);
		}
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
	gui_textfield& getField() {
		return field;
	}
	void layout() {
		field.pos = pos;
		field.size = size;
		field.layout();
		field.setFontSize((int32_t)std::max(4.0, field.size.y*0.7));
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

		int align = isAlignCenter ? NVG_ALIGN_CENTER : NVG_ALIGN_RIGHT;
		setFont(vg, G_FONT_SCALE((int32_t)(size.y*0.8)), G_WHITE, align | NVG_ALIGN_MIDDLE);
		int32_t _number = number ? *number : 0;
		String str = filter.formatNumber(_number);
		nvgText(vg, pos.x + size.x - 3 + (isAlignCenter?size.x*-0.5:0), pos.y + G_FONT_MIDDLE_OFFSET(size.y), StringAsCStr(str), NULL);
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
				int newVal = filter.parseString(this->field.value());
				*number = newVal;
				if (parent)
					parent->buttonClicked(this);
			}
		}
		isEditing = false;
	}
	void startEdit(bool keepcontent) {
		if (!isEditing) {
			if (this->number) {
				String s = filter.formatNumber(*this->number);
				this->field.setValue(s);
			}
			this->field.beginEdit();
			this->field.setSelectionRange(2, 2);
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
					int32_t rel = (int32_t)evt.relMousepos.x*4.0f/size.x;
					if (rel < 0) rel = 0;
					if (rel > 3) rel = 3;
					rel = 3-rel;
					my_printf("relMousepos %d/%d %f %d \n", evt.relMousepos.x, size.x, evt.relMousepos.x*4.0f/size.x, rel);
					this->draggedByte = rel;
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

			if (this->draggedByte >= 0 && draggedByte < 4) {
//				my_printf("drag start %d %d\n", evt.dragStart.x, evt.dragStart.y);
				int disty = (int) evt.dragDistance->y / 2;
				if (abs(disty) < 1)
					return;
				evt.dragDistance->y = 0;
				int absy = abs(disty);
				if (absy >= 4)
					absy = 64;
				else if (absy >= 2)
					absy = 4;
				int current = *number;
				int byte = (current>>(draggedByte*8))&0xFF;
				byte -= (disty < 0 ? -1 : 1) * absy;
				if (byte < 0) {
					byte = 0;
				} else if (byte > 255) {
					byte = 255;
				}
				*number = (current&(~(0xFF<<(draggedByte*8)))) | byte << (draggedByte*8);
				if (parent)
					parent->buttonClicked(this);
			}
			return;
		}
	}
	void handleDraggedRelease(MouseEvent& evt) override {
		if (isEditing) {
			this->field.handleDraggedRelease(evt);
		}
		this->draggedByte = -1;
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
		bool b = this->field.handleCharInput(codepoint);
		if (b) {

			if (this->number) {
				int newVal = filter.parseString(this->field.mValueTemp);
				*number = newVal;
			}
			if (parent)
				parent->buttonClicked(this);
		}
		return b;
	}

};
class gui_color_pick : public guictr_base {
	guiknob_labeled_base knH;
	guiknob_labeled_base knS;
	guiknob_labeled_base knL;
	guiknob_labeled_base knA;
	gui_input_filtered hexInput;
	NVGcolor nvgColor{1.0f, 1.0f, 1.0f, 1.0f};
	int32_t colorInt32 = 0xFFFFFFFF;
	NVGcolor* ptrNvgColor = nullptr;
	int32_t* ptrColorInt32 = nullptr;
	void setHSL_(float h, float s, float v, float a);
public:
	std::function<void(int32_t)> fnSetValue;
public:
	gui_color_pick()
	: guictr_base(),
	  knH(false),
	  knS(false),
	  knL(false),
	  knA(false),
	  hexInput(&colorInt32) {
		padding=0;
		margin=0;
		add(&knH);
		add(&knS);
		add(&knL);
		add(&knA);
		add(&hexInput);
		init();
	}
	void setHSL(float h, float s, float v, float a);
	void setInt32(int32_t rgba);
	void init();
	~gui_color_pick() {
		remove(&knA);
		remove(&knH);
		remove(&knS);
		remove(&knL);
	}
	virtual void buttonClicked(guibase* button) override;
	void layout();
	void setRefInt32(int32_t* ptrInt32);
	void setRefNvg(NVGcolor* ptrNvg);
	void render(NVGcontext* vg) override {
		if (!setScissorTransform(vg)) {
			return;
		}
		int sizeQuad = knH.size.y;
		int inset = 0;
		if (isBackgroundRendered()) {
			inset = 4;
			nvgBeginPath(vg);
			nvgRect(vg, knH.left()-sizeQuad, 0, sizeQuad,sizeQuad);
			nvgFillColor(vg, this->theme->getBgColor(getStateFlags()));
			nvgFill(vg);
		}
		nvgBeginPath(vg);
		nvgRect(vg, knH.left()-sizeQuad+inset, inset, sizeQuad-inset*2,sizeQuad-inset*2);
		nvgFillColor(vg, this->nvgColor);
		nvgFill(vg);
		for (auto* g : guis)
			g->render(vg);
	}
	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
		if (this->contains(mpos)) {
			ivec2 localMouse = this->toContainerSpace(mpos);
			for (guibase* gui : guis) {
				if (gui->mouseHitTest(localMouse, evt)) {
					return true;
				}
			}
			evt.requestFocus(this);
			return true;
		}
		return false;
	}
	void handleRightClick(MouseEvent& evt);
	void setColor(int32_t rgba);
};

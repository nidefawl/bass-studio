#include "guicolorpick.h"
#include "glheaders.h"
#include <nanovg.h>
#include <vector>
#include <memory>
#include <stdint.h>
#include <nanovg.h>

#include "str_util.h"
#include "color_util.h"

#include "basectrl.h"
#include "gui/knob.h"
#include "gui/button.h"
#include "gui/guicontainer.h"
#include "gui/textfield.h"
#include "guicontextmenu_daw.h"
#include "guicontextmenu_color.h"

gui_color_pick::gui_color_pick() :
		guictr_base(), knH(false), knS(false), knL(false), knA(false), hexInput(&colorInt32) {
	padding = 0;
	margin = 0;
	setCanMouseHit(true);
	add(&knH);
	add(&knS);
	add(&knL);
	add(&knA);
	add(&hexInput);
	init();
}
void gui_color_pick::init() {
	this->hexInput.setAlignCenter(true);
	guiknob_labeled_base* knobs[4] = {
		&knH, &knS, &knL, &knA
	};
	const char* knoblabels[4] = {
		"Hue", "Saturation", "Brightness", "Alpha"
	};
	auto setColor = [this]() {
		float h = knH.getValueClamped();
		float s = knS.getValueClamped();
		float v = knL.getValueClamped();
		float a = knA.getValueClamped();
		setHSL_(h, s, v, a);
	};
	auto setEditColor = [setColor](float preVal, float val) {
		setColor();
	};
	auto getDisplayValue = [](float val) {
		int32_t v = CLAMP_I((int32_t)(100.0f*val), 0, 100);
		return StringFormat("%d%%", v);
	};
	for (int i = 0; i < 4; i++) {
		auto* knob = knobs[i];
		knob->setIsSlider(true);
		knob->setLabel(knoblabels[i]);
		knob->setBackgroundRendered(true);
		knob->fnValueEditChanged = setEditColor;
		knob->fnValueEditFinish = setEditColor;
		knob->fnGetDisplayValue = getDisplayValue;
	}
	knA.fnGetDisplayValue = [](float val) {
		int32_t alpha = CLAMP_I((int32_t)(255.0f*val), 0, 255);
		return StringFormat("%d", alpha);
	};
	setInt32(0xFF7f7f7f);
}
void gui_color_pick::layout() {
	int sizeQuad = size.y;
	float sliderW = size.y/4;
	for (auto* g : guis)
		g->size = vec2(sliderW, size.y);
	this->hexInput.size = {sizeQuad, sizeQuad/4};
	knA.pos = vec2(size.x-sliderW, 0);
	knL.pos = vec2(knA.left()-sliderW, 0);
	knS.pos = vec2(knL.left()-sliderW, 0);
	knH.pos = vec2(knS.left()-sliderW, 0);
	this->hexInput.pos = {knH.left()-sizeQuad, sizeQuad/4*3};
	for (auto* g : guis) {
		g->layout();
	}
}
void gui_color_pick::render(NVGcontext* vg) {
	if (!setScissorTransform(vg)) {
		return;
	}
	int sizeQuad = knH.size.y;
	int inset = 0;
	if (isBackgroundRendered()) {
		inset = 4;
		nvgBeginPath(vg);
		nvgRect(vg, knH.left() - sizeQuad, 0, sizeQuad, sizeQuad);
		nvgFillColor(vg, getBackgroundColor(getStateFlags()));
		nvgFill(vg);
	}
	nvgBeginPath(vg);
	nvgRect(vg, knH.left() - sizeQuad + inset, inset, sizeQuad - inset * 2, sizeQuad - inset * 2);
	nvgFillColor(vg, this->nvgColor);
	nvgFill(vg);
	for (auto* g : guis)
		g->render(vg);
}

void gui_color_pick::buttonClicked(guibase* button) {
	if (button == &hexInput) {
		setInt32(colorInt32);
	}
}
void gui_color_pick::setInt32(int32_t rgba) {
	glm::vec4 color = colorHex(rgba);
	glm::vec4 hsla = rgbToHSL(color.x, color.y, color.z);
	this->knH.setValueInit(hsla.x);
	this->knS.setValueInit(hsla.y);
	this->knL.setValueInit(hsla.z);
	this->knA.setValueInit(color.w);
	this->colorInt32 = rgba;
	this->nvgColor = rgbaToNvg(rgba);
	if (ptrColorInt32) {
		*ptrColorInt32 = colorInt32;
	}
	if (ptrNvgColor) {
		*ptrNvgColor = nvgColor;
	}
	if (fnSetValue) {
		fnSetValue(rgba);
	}
}
void gui_color_pick::setHSL(float h, float s, float l, float a) {
	this->knH.setValueInit(h);
	this->knS.setValueInit(s);
	this->knL.setValueInit(l);
	this->knA.setValueInit(a);
	auto col = nvgHSL(h, s, l);
	int32_t rgb = nvgToRGB(col) & 0xFFFFFF;
	int32_t alpha = CLAMP_I((int32_t)(255.0f*a), 0, 255)<<24;
	int32_t rgba = rgb | alpha;
	this->colorInt32 = rgba;
	this->nvgColor = rgbaToNvg(rgba);
	if (ptrColorInt32) {
		*ptrColorInt32 = colorInt32;
	}
	if (ptrNvgColor) {
		*ptrNvgColor = nvgColor;
	}
	if (fnSetValue) {
		fnSetValue(rgba);
	}
}
void gui_color_pick::setHSL_(float h, float s, float l, float a) {
	auto col = nvgHSL(h, s, l);
	int32_t rgb = nvgToRGB(col) & 0xFFFFFF;
int32_t alpha = CLAMP_I((int32_t)(255.0f*a), 0, 255)<<24;
//	setInt32(rgb | alpha);
	int32_t rgba = rgb | alpha;
	this->colorInt32 = rgba;
	this->nvgColor = rgbaToNvg(rgba);
	if (ptrColorInt32) {
		*ptrColorInt32 = colorInt32;
	}
	if (ptrNvgColor) {
		*ptrNvgColor = nvgColor;
	}
	if (fnSetValue) {
		fnSetValue(rgba);
	}
}
void gui_color_pick::setRefInt32(int32_t* ptrInt32) {
	ptrColorInt32 = ptrInt32;
}
void gui_color_pick::setRefNvg(NVGcolor* ptrNvg) {
	ptrNvgColor = ptrNvg;
}
void gui_color_pick::handleRightClick(MouseEvent& evt) {
	guictxtmenu_colorpalette* ctxt = new guictxtmenu_colorpalette();
	ctxt->callback = [this](int32_t val) {
		this->setInt32(val);
	};
	parentCtrl->openContextMenu(ctxt, evt.mousepos);
}


gui_input_filtered::gui_input_filtered(int32_t* _number) :
		guibuttonbase(), number(_number) {
	field.setParent(this);
	field.setFilter(&filter);
	setAlignCenter(false);
}

void gui_input_filtered::setAlignCenter(bool b) {
	isAlignCenter = b;
	if (b) {
		field.setAlignment(gui_textfield::Alignment::Center);
	} else {
		field.setAlignment(gui_textfield::Alignment::Left);
	}
}

void gui_input_filtered::layout() {
	field.pos = pos;
	field.size = size;
	field.layout();
	field.setFontSize((int32_t) (math::max(4.0, field.size.y * 0.7)));
}

void gui_input_filtered::render(NVGcontext* vg) {
	int32_t fl = getStateFlags();
	renderWidgetBorder(vg, fl);
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
	setFont(vg, G_FONT_SCALE((int32_t )(size.y * 0.8)), G_WHITE, align | NVG_ALIGN_MIDDLE);
	int32_t _number = number ? *number : 0;
	String str = filter.formatNumber(_number);
	nvgText(vg, pos.x + size.x - 3 + (isAlignCenter ? size.x * -0.5 : 0), pos.y + G_FONT_MIDDLE_OFFSET(size.y), StringAsCStr(str), NULL);
}

bool gui_input_filtered::focusEvent(MouseHitEvt& evt, bool focused) {
	if (!focused) {
		endEdit(true);
	} else {
		//			isEditing = true;
	}
	this->field.focusEvent(evt, focused);
	return true;
}

void gui_input_filtered::endEdit(bool success) {
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

void gui_input_filtered::startEdit(bool keepcontent) {
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

void gui_input_filtered::handleDraggedBegin(MouseEvent& evt) {
	if (isEditing) {
		this->field.handleDraggedBegin(evt);
	} else {
		if (evt.type == MouseEventType::M_EVT_DOUBLECLICK) {
			startEdit(true);
		} else {
			if (evt.guiDragged == this) {
				int32_t rel = (int32_t) evt.relMousepos.x * 4.0f / size.x;
				if (rel < 0)
					rel = 0;
				if (rel > 3)
					rel = 3;
				rel = 3 - rel;
				my_printf("relMousepos %d/%d %f %d \n", evt.relMousepos.x, size.x, evt.relMousepos.x * 4.0f / size.x, rel);
				this->draggedByte = rel;
				parentCtrl->captureMouse(this);
			}
		}
	}
}

void gui_input_filtered::handleDraggedMove(MouseEvent& evt) {
	if (isEditing) {
		this->field.handleDraggedMove(evt);
		return;
	}
	if (number && evt.guiDragged == this && evt.type == M_EVT_CAPTURED_MOVE) {
		if (this->draggedByte >= 0 && draggedByte < 4) {
			//				my_printf("drag start %d %d\n", evt.dragStart.x, evt.dragStart.y);
			int disty = (int) (evt.dragDistance->y) / 2;
			if (math::abs(disty) < 1)
				return;

			evt.dragDistance->y = 0;
			int absy = math::abs(disty);
			if (absy >= 4)
				absy = 64;
			else if (absy >= 2)
				absy = 4;

			int current = *number;
			int byte = (current >> (draggedByte * 8)) & 0xFF;
			byte -= (disty < 0 ? -1 : 1) * absy;
			if (byte < 0) {
				byte = 0;
			} else if (byte > 255) {
				byte = 255;
			}

			*number = (current & (~(0xFF << (draggedByte * 8)))) | byte << (draggedByte * 8);
			if (parent)
				parent->buttonClicked(this);
		}
		return;
	}
}

void gui_input_filtered::handleDraggedRelease(MouseEvent& evt) {
	if (isEditing) {
		this->field.handleDraggedRelease(evt);
	}
	this->draggedByte = -1;
}

bool gui_input_filtered::handleKeyInput(KeyEvent& kevt) {
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

bool gui_input_filtered::handleCharInput(unsigned int codepoint) {
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

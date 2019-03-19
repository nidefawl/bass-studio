#pragma once
#include <stdbool.h>
#include <stdint.h>

#include "str_util.h"
#include "color_util.h"

#include "keyboard.h"
#include "gui.h"
#include "guicolors.h"
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
    std::function<void(gui_numberinput_field*,int32_t)> fnValueEditChanged;
    std::function<int32_t(int32_t)> fnClamp;

	gui_numberinput_field(int32_t* _number);
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
	void layout();

	void render(NVGcontext* vg);

	bool focusEvent(MouseHitEvt& evt, bool focused) override;
	void endEdit(bool success);
	void startEdit(bool keepcontent);
	void handleDraggedBegin(MouseEvent& evt) override;
	void handleDraggedMove(MouseEvent& evt) override;
	void handleDraggedRelease(MouseEvent& evt) override;
	virtual bool handleKeyInput(KeyEvent& kevt) override;
	virtual bool handleCharInput(unsigned int codepoint) override;

};

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
	gui_input_filtered(int32_t* _number);
	void setAlignCenter(bool b);
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
	void layout();

	void render(NVGcontext* vg);

	bool focusEvent(MouseHitEvt& evt, bool focused) override;
	void endEdit(bool success);
	void startEdit(bool keepcontent);
	void handleDraggedBegin(MouseEvent& evt) override;
	void handleDraggedMove(MouseEvent& evt) override;
	void handleDraggedRelease(MouseEvent& evt) override;
	virtual bool handleKeyInput(KeyEvent& kevt) override;
	virtual bool handleCharInput(unsigned int codepoint) override;

};

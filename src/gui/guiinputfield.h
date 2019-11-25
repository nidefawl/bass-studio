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
#include "basectrl.h"
#include "textfield.h"

class gui_numberinput_field_base : public guibuttonbase {
protected:
	gui_textfield field;
	bool isEditing = false;
public:
	gui_numberinput_field_base() :
		guibuttonbase() {
		field.setParent(this);
	}

    void layout() {
    	field.pos = pos;
    	field.size = size;
    	field.layout();
    	field.setFontSize(math::max(4, field.size.y - 2));
    }
	virtual void setControl(BaseCtrl* parentCtrl) override {
		guibase::setControl(parentCtrl);
		field.setControl(parentCtrl);
	}
	gui_textfield& getField() {
		return field;
	}

	void render(NVGcontext* vg);

	bool focusEvent(MouseHitEvt& evt, bool focused) override;
	void endEdit(bool success);
	void startEdit(bool keepcontent);
	void handleDraggedBegin(MouseEvent& evt) override;
	void handleDraggedMove(MouseEvent& evt) override;
	void handleDraggedRelease(MouseEvent& evt) override;
	virtual bool handleKeyInput(KeyEvent& kevt) override;
	virtual bool handleCharInput(unsigned int codepoint) override;

	virtual String getAsStringLiteral() = 0;
	virtual void endEditImpl() = 0;
	virtual void onMouseDragValue(int32_t disty, int32_t absy) = 0;
	virtual void onKeyInputChangeValue(ivec2 direction) = 0;
};
template<typename T>
class gui_numberinput_field_generic: public gui_numberinput_field_base {
protected:
	T* number;
public:
    std::function<void(gui_numberinput_field_base*,T)> fnValueEditChanged;
    std::function<T(T)> fnClamp;

    gui_numberinput_field_generic(T* _number) :
    	gui_numberinput_field_base(), number(_number) {
    }
	void setRef(T* number) {
		this->number = number;
	}
	virtual T getValue() {
		return *number;
	}
	virtual T parseLiteral(const char* szNumber);
	virtual String valueToStringLiteral(T val);
	void onMouseDragValue(int32_t disty, int32_t absy) override;
	void onKeyInputChangeValue(ivec2 direction) override;
	String getAsStringLiteral() override {
		if (this->number) {
			return valueToStringLiteral(getValue());
		}
		return "<undef>";
	}
	void endEditImpl() override {
		if (this->number) {
			const char* cstr = this->field.value().c_str();
			setValue(parseLiteral(cstr));
			if (parent)
				parent->buttonClicked(this);
		}
	}
private:
	virtual void setValue(T newVal) {
		if (fnClamp) {
			newVal = fnClamp(newVal);
		}
		*number = newVal;
		if (fnValueEditChanged) {
			fnValueEditChanged(this, newVal);
		}
	}
};
//TODO: rename to gui_numberinput_int32
class gui_numberinput_field : public gui_numberinput_field_generic<int32_t> {
public:
	gui_numberinput_field(int32_t* _number) : gui_numberinput_field_generic<int32_t>(_number) {

	}
//	int32_t parseLiteral(const char* szNumber) override {
//		return atoi(szNumber);
//	}
//	String valueToStringLiteral(int32_t val) override {
//		return StringFormat("%d", val);
//	}
};
uint32_t nvgToRGB(NVGcolor c);


class gui_input_filtered: public guibuttonbase {
	int32_t* number;
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

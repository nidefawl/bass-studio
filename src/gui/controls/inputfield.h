#pragma once
#include "basectrl.h"
#include "gui/container/container.h"
#include "gui/controls/button.h"
#include "keyboard.h"
#include "textfield.h"

class gui_numberinput_field_base : public guibutton {
protected:
    gui_textfield field;
    bool isEditing = false;

public:
    gui_numberinput_field_base() : guibutton() {
        field.setParent(this);
        setFlag(FLG_RENDER_BACKGROUND_INSET, true);
        setFlag(FLG_BG_SHADING, true);
    }

    void layout() override {
        field.pos  = pos;
        field.size = size;
        field.layout();
        field.setFontSize(size.y * FONT_AUTOSCALE);
    }
    void setControl(BaseCtrl* parentCtrl) override {
        guibase::setControl(parentCtrl);
        field.setControl(parentCtrl);
    }
    gui_textfield& getField() {
        return field;
    }

    void render(NVGcontext* vg) override;

    bool focusEvent(MouseHitEvt& evt, bool focused) override;
    void endEdit(bool success);
    void startEdit(bool keepcontent);
    void handleDraggedBegin(MouseEvent& evt) override;
    void handleDraggedMove(MouseEvent& evt) override;
    void handleDraggedRelease(MouseEvent& evt) override;
    bool handleKeyInput(KeyEvent& kevt) override;
    bool handleCharInput(uint32_t codepoint) override;

    virtual String getAsStringLiteral()                        = 0;
    virtual void endEditImpl()                                 = 0;
    virtual void onMouseDragValue(int32_t disty, int32_t absy) = 0;
    virtual void onKeyInputChangeValue(ivec2 direction)        = 0;
};
template<typename T>
class gui_numberinput_field_generic final : public gui_numberinput_field_base {
protected:
    T* number;
    const char* strFormat = nullptr;
public:
    std::function<void(gui_numberinput_field_base*, T)> fnValueEditChanged;
    std::function<T(T)> fnClamp;

    explicit gui_numberinput_field_generic(T* _number) : gui_numberinput_field_base(), number(_number) {
    }
    void setRef(T* number) {
        this->number = number;
    }
    virtual T getValue() {
        return *number;
    }
    void setStringFormat(const char* strFormat) {
        this->strFormat = strFormat;
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

using gui_numberinput_i32 = gui_numberinput_field_generic<int32_t>;
using gui_numberinput_u32 = gui_numberinput_field_generic<uint32_t>;
using gui_numberinput_double = gui_numberinput_field_generic<double>;
using gui_numberinput_float = gui_numberinput_field_generic<float>;

class gui_input_filtered final : public guibutton {
    uint32_t* number;
    gui_textfield field;
    input_filter_hex32 filter;
    bool isAlignCenter = false;
    bool isEditing     = false;
    uint8_t draggedByte    = 0xFF;

public:
    explicit gui_input_filtered(uint32_t* _number);
    void setAlignCenter(bool b);
    void setControl(BaseCtrl* parentCtrl) override {
        guibase::setControl(parentCtrl);
        field.setControl(parentCtrl);
    }
    void setRef(uint32_t* number) {
        this->number = number;
    }
    gui_textfield& getField() {
        return field;
    }
    void layout() override;

    void render(NVGcontext* vg) override;

    bool focusEvent(MouseHitEvt& evt, bool focused) override;
    void endEdit(bool success);
    void startEdit(bool keepcontent);
    void handleDraggedBegin(MouseEvent& evt) override;
    void handleDraggedMove(MouseEvent& evt) override;
    void handleDraggedRelease(MouseEvent& evt) override;
    bool handleKeyInput(KeyEvent& kevt) override;
    bool handleCharInput(uint32_t codepoint) override;
};

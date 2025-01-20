#include "inputfield.hpp"

#include "keyboard.hpp"
#include "logging.hpp"
#include "str_util.hpp"

void gui_numberinput_field_base::render(NVGcontext* vg) {
    if (!isRenderableSizeAndContext(vg))
        return;
    const auto stateFlags = getStateFlags();
    renderWidgetBorder(vg, stateFlags);
    if (isEditing) {
        this->field.render(vg);
        return;
    }
    const auto fontSizeScaled = math::clamp(size.y, 4, 48) * FONT_AUTOSCALE;
    const String str = getAsStringLiteral();

    const auto posText = vec2(pos) + vec2(size.x - 3, size.y * 0.5f);
    float textWidth = renderTextLabel(vg,
                    posText,
                    vec2(size),
                    str,
                    theme,
                    fontSizeScaled,
                    theme->getColor(getLabelColor()),
                    NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
    if (this->label.length()) {
        renderTextLabel(vg,
                        vec2(pos) + vec2(3.0f, size.y * 0.5f),
                        vec2(size.x - textWidth - 6.0f, size.y),
                        label,
                        theme,
                        fontSizeScaled,
                        theme->getColor(GuiColor::COL_LABEL_INACTIVE),
                        NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    }
}

bool gui_numberinput_field_base::focusEvent(MouseHitEvt& evt, bool focused) {
    if (!focused) {
        endEdit(true);
    } else {
//        isEditing = true;
    }
    this->field.focusEvent(evt, focused);
    return true;
}

void gui_numberinput_field_base::endEdit(bool success) {
    if (isEditing) {
        this->field.endEdit(success);
        if (success) {
            endEditImpl();
        }
    }
    isEditing = false;
}

void gui_numberinput_field_base::startEdit(bool keepcontent) {
    if (!isEditing) {
        if (keepcontent) {
            this->field.setValue(getAsStringLiteral());
        } else {
            this->field.setValue("");
        }
        this->field.setSelectionRange(-1, -1);
        this->field.beginEdit();
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

        int absy = math::abs(disty);
        if (absy >= 4)
            absy = 64;
        else if (absy >= 2)
            absy = 4;
        if (onMouseDragValue(disty, absy)) {
            evt.dragDistance->y = 0;
            if (parent)
                parent->buttonClicked(this);
        }
    }
}

void gui_numberinput_field_base::handleDraggedRelease(MouseEvent& evt) {
    if (isEditing) {
        this->field.handleDraggedRelease(evt);
    }
}

bool gui_numberinput_field_base::handleKeyInput(KeyEvent& kevt) {
    if (kevt.type != K_RELEASE) {
        if (kevt.keyCode == KeyboardKey::DAW_KB_ENTER 
            || kevt.keyCode == KeyboardKey::DAW_KB_KP_ENTER 
            || kevt.keyCode == KeyboardKey::DAW_KB_ESCAPE) {
            endEdit(kevt.keyCode == KeyboardKey::DAW_KB_ENTER || kevt.keyCode == KeyboardKey::DAW_KB_KP_ENTER);
            return true;
        }
        if (isEditing) {
            return this->field.handleKeyInput(kevt);
        }
        if (isArrowKey(kevt.keyCode)) {
            ivec2 dir;
            arrowKeyToXY(kevt.keyCode, dir.x, dir.y);
            if (dir.y) {
                if ((kevt.mods & KB_MOD_SHIFT)) {
                    dir *= 12;
                }
                onKeyInputChangeValue(dir);
                if (parent)
                    parent->buttonClicked(this);
                return true;
            }
        }
    }
    return false;
}

bool gui_numberinput_field_base::handleCharInput(uint32_t codepoint) {
    if (!isEditing && parentCtrl->isGlobalKeybindCodepoint(codepoint)) {
        return false;
    }
    if (!this->field.canHandleCharInput(codepoint)) {
        return false;
    }
    startEdit(false);
    return this->field.handleCharInput(codepoint);
}
template<>
int32_t gui_numberinput_field_generic<int32_t>::parseLiteral(const char* szNumber) {
    return atoi(szNumber);
}
template<>
String gui_numberinput_field_generic<int32_t>::valueToStringLiteral(int32_t val) {
    return StringFormat(strFormat ? strFormat : "%d", val);
}

template<>
bool gui_numberinput_field_generic<int32_t>::onMouseDragValue(int32_t disty, int32_t absy) {
    if (this->number) {
        setValue(getValue() - ((disty < 0 ? -1 : 1) * absy));
    }
    return true;
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
    return StringFormat(strFormat ? strFormat : "%d", val);
}

template<>
bool gui_numberinput_field_generic<uint32_t>::onMouseDragValue(int32_t disty, int32_t absy) {
    if (this->number) {
        setValue(getValue() - ((disty < 0 ? -1 : 1) * absy));
    }
    return true;
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
    return StringFormat(strFormat ? strFormat : "%.3f", val);
}
template<>
bool gui_numberinput_field_generic<float>::onMouseDragValue(int32_t disty, int32_t absy) {
    if (this->number) {
        float step = stepSize != 0 ? stepSize : 0.0064f;
        float r = disty / 64.0f;
        if (stepSize != 0 && math::abs(r) < math::abs(1.0f)) {
            return false;
        }
        setValue(*number - (disty < 0 ? -1 : 1) * (step));
    }
    return true;
}
template<>
void gui_numberinput_field_generic<float>::onKeyInputChangeValue(ivec2 direction) {
    if (this->number) {
        setValue(getValue() + direction.y * 0.01f);
    }
}

template<>
String gui_numberinput_field_generic<double>::valueToStringLiteral(double val) {
    return StringFormat(strFormat ? strFormat : "%.3f", val);
}
template<>
double gui_numberinput_field_generic<double>::parseLiteral(const char* szNumber) {
    return atof(szNumber);
}
template<>
bool gui_numberinput_field_generic<double>::onMouseDragValue(int32_t disty, int32_t absy) {
    if (this->number) {
        double step = stepSize != 0 ? stepSize : 0.0064;
        double r = disty / 64.0;
        if (stepSize != 0 && math::abs(r) < math::abs(1.0)) {
            return false;
        }
        setValue(*number - (disty < 0 ? -1 : 1) * (step));
    }
    return true;
}
template<>
void gui_numberinput_field_generic<double>::onKeyInputChangeValue(ivec2 direction) {
    if (this->number) {
        setValue(getValue() + direction.y * 0.01);
    }
}

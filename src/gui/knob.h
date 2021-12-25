#pragma once
#include <nanovg.h>
#include <functional>
#include "math/vec.h"
#include "math/seq_math.h"
#include "color_util.h"
#include "str_util.h"
#include "seq_util.h"
#include "gui.h"
#include "guicolors.h"
#include "event.h"

struct automatable_t;
class guiknob : public guibase {
protected:
    const float angleOpen = 90;
    const float range     = (360 - angleOpen) * M_PI / 180.0f;
    const float start     = -FLOAT_PI * 1.5f + (angleOpen / 2.0f) * M_PI / 180.0f;
    bool* enabledPtr      = NULL;
    float* valuePtr       = NULL;
    float value           = 0.0f;
    bool isSlider;
    bool changedValue            = false;
    float fModifyBeginValue      = 0.0f;
    float fDefaultValue          = 0.5f;
    float lastVal                = 0.0f;
    bool bDoubleClickSetsDefault = true;

    automatable_t* paramAutomatable = nullptr;
    int32_t paramIdx                = -1;

public:
    std::function<float()> fnGetValue;
    std::function<void(float, int)> fnSetValue;
    std::function<void(float, float)> fnValueEditChanged;
    std::function<void(float, float)> fnValueEditFinish;
    std::function<void(MouseHitEvt&, bool)> fnFocus;
    GuiColor::constant_t valColor = GuiColor::COL_KNOB;
    GuiColor::constant_t indColor = GuiColor::COL_KNOB_IND;
    guiknob(const bool _renderBackground = true, const bool _isSlider = false) : guibase(), isSlider(_isSlider) {
        setBackgroundRendered(_renderBackground);
        setCanMouseHit(true);
    }

    void setAutomationRef(automatable_t* _paramAutomatable, int32_t _paramIdx) {
        this->paramAutomatable = _paramAutomatable;
        this->paramIdx         = _paramIdx;
    }
    void setAutomationHandlers();

    bool isAutomated();
    void setIsSlider(bool b) {
        this->isSlider = b;
    }
    virtual void handleDraggedBegin(MouseEvent& evt);
    virtual void handleDraggedMove(MouseEvent& evt);
    virtual void handleDraggedRelease(MouseEvent& evt);

    virtual bool focusEvent(MouseHitEvt& evt, bool focused) override {
        if (fnFocus) fnFocus(evt, focused);
        return true;
    }
    virtual bool handleMouseScroll(MouseEvent& evt, double xoffset, double yoffset);
    void handleRightClick(MouseEvent& evt) override;
    void renderButtonAt(NVGcontext* vg, ivec2 insetP, ivec2 insetS);
    virtual void render(NVGcontext* vg);
    float getValueInternal() {
        return value;
    }
    void setValueInit(float newValue) {
        value = newValue;
        if (valuePtr) {
            *valuePtr = newValue;
        }
        setDisplayValue(newValue);
    }
    void setValue(float newValue, int flags) {
        float curval = getValue();
        newValue     = CLAMP_I(newValue, 0.0f, 1.0f);
        value        = newValue;
        if (fnSetValue) {
            fnSetValue(newValue, flags);
        } else if (valuePtr) {
            *valuePtr = newValue;
        }
        if (fnValueEditChanged) {
            fnValueEditChanged(curval, getValue());
        }
        setDisplayValue(newValue);
    }
    virtual void setDisplayValue(float f) {
    }
    virtual void onValueEditFinish(float from, float to) {
        if (fnValueEditFinish) {
            fnValueEditFinish(from, to);
        }
    }
    float getValueClamped() {
        return CLAMP_F(getValue());
    }
    virtual float getValue() {
        if (fnGetValue) {
            return fnGetValue();
        } else if (valuePtr) {
            return *valuePtr;
        } else {
//            float time = fmod(getTimeMillis() / 1000.0f, 2.0f);
//            float val  = CLAMP_I(0.9f*sinf(time * FLOAT_PI) + 0.5f, 0.0f, 1.0f);
            return value;
        }
    }
    void setEnabledRef(bool* _enabledPtr) {
        enabledPtr = _enabledPtr;
    }
    void setValueRef(float* _valuePtr) {
        valuePtr = _valuePtr;
    }
    virtual bool enabled() {
        if (enabledPtr)
            return *enabledPtr;
        return true;
    }
    guictxtmenu_base* getTooltip(AppCtrl* appctrl) override;
    void setToDefaultValue();
};

#pragma once
#include "nanovg/nanovg.h"
#include <functional>
#include "math/vec.h"
#include "math/seq_math.h"
#include "color_util.h"
#include "str_util.h"
#include "seq_util.h"
#include "gui/gui.h"
#include "guicolors.h"
#include "event.h"

struct automatable_t;
class guiknob : public guibase {
protected:
    const double angleOpen = 90.0;
    const float range      = static_cast<float>((360.0 - angleOpen) * M_PI / 180.0);
    const float start      = static_cast<float>(-M_PI * 1.5 + (angleOpen / 2.0) * M_PI / 180.0);
    bool* enabledPtr       = nullptr;
    float* valuePtr        = nullptr;
    float value            = 0.0f;
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
    std::function<void(float, float)> fnValueEditBegin;
    std::function<void(float, float)> fnValueEditChanged;
    std::function<void(float, float)> fnValueEditFinish;
    std::function<void(MouseHitEvt&, bool)> fnFocus;
    GuiColor::constant_t valColor = GuiColor::COL_KNOB;
    GuiColor::constant_t indColor = GuiColor::COL_KNOB_IND;
    explicit guiknob(const bool _renderBackground = true, const bool _isSlider = false) : guibase(), isSlider(_isSlider) {
        setBackgroundRendered(_renderBackground);
        setCanMouseHit(true);
    }

    void setAutomationRef(automatable_t* _paramAutomatable, int32_t _paramIdx) {
        this->paramAutomatable = _paramAutomatable;
        this->paramIdx         = _paramIdx;
    }
    int32_t getParamIdx() const { return paramIdx; }
    void setKnobInternalHandlers();

    bool isAutomated();
    void setIsSlider(bool b) {
        this->isSlider = b;
    }
    void handleDraggedBegin(MouseEvent& evt) override;
    void handleDraggedMove(MouseEvent& evt) override;
    void handleDraggedRelease(MouseEvent& evt) override;

    bool focusEvent(MouseHitEvt& evt, bool focused) override {
        if (fnFocus) fnFocus(evt, focused);
        return true;
    }
    bool handleMouseScroll(MouseEvent& evt, double xoffset, double yoffset) override;
    void handleRightClick(MouseEvent& evt) override;
    void renderButtonAt(NVGcontext* vg, ivec2 insetP, ivec2 insetS, float value);
    void render(NVGcontext* vg) override;
    float getValueInternal() {
        return value;
    }
    void setValueInit(float newValue) {
        value = newValue;
        if (valuePtr) {
            *valuePtr = newValue;
        }
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

#pragma once
#include "gui/gui.h"
#include "gui/controls/textfield.h"
#include "guicolors.h"
#include "math/seq_math.h"
#include <functional>
#include <optional>

struct automatable_t;
struct automatable_param_t;
struct param_modulation_range_t;
namespace DAW::UI::Modulation {
    class gui_dragged_modulation;
}

class guiknob : public guibase {
public:
    enum class knobtype {
        KNOB_LABELED,
        KNOB_UNLABELED,
        SLIDER_LABELED,
    };
protected:
    const double angleOpen = 90.0;
    const float range      = static_cast<float>((360.0 - angleOpen) * M_PI / 180.0);
    const float start      = static_cast<float>(-M_PI * 1.5 + (angleOpen / 2.0) * M_PI / 180.0);
    const knobtype knobType;
    bool* enabledPtr       = nullptr;
    float* valuePtr        = nullptr;
    float value            = 0.0f;
    bool changedValue            = false;
    float fModifyBeginValue      = 0.0f;
    float fDefaultValue          = 0.5f;
    float lastVal                = 0.0f;
    bool bDoubleClickSetsDefault = true;
    bool bIsBipolar              = false;

    automatable_t* paramAutomatable = nullptr;
    int32_t paramIdx                = -1;

    String valueDisplay = "  ";
public:
    std::function<float()> fnGetValue;
    std::function<void(float, int)> fnSetValue;
    std::function<void(float, float)> fnValueEditBegin;
    std::function<void(float, float)> fnValueEditChanged;
    std::function<void(float, float)> fnValueEditFinish;
    std::function<void(MouseHitEvt&, bool)> fnFocus;
    explicit guiknob(knobtype knobType) : guibase(), knobType(knobType) {
        setBackgroundRendered(false);
        setCanMouseHit(true);        
    }
    virtual float getQuantizationStep() const {
        return 1e-12f;
    }
    void setIsBipolar(bool bIsBipolar) {
        this->bIsBipolar = bIsBipolar;
        if (bIsBipolar) {
            fDefaultValue = 0.0f;
        }else {
            fDefaultValue = 0.5f;
        }
    }

    bool getIsBipolar() const {
        return bIsBipolar;
    }
    void setAutomationRef(automatable_t* _paramAutomatable, int32_t _paramIdx) {
        this->paramAutomatable = _paramAutomatable;
        this->paramIdx         = _paramIdx;
    }
    int32_t getParamIdx() const { return paramIdx; }
    void setKnobInternalHandlers();
    float getParamScaled(const automatable_param_t* param, int type);

    virtual bool isAutomated();
    virtual bool isModulated();

    void handleDraggedBegin(MouseEvent& evt) override;
    void handleDraggedMove(MouseEvent& evt) override;
    void handleDraggedRelease(MouseEvent& evt) override;
    GuiColor::constant_t getBackgroundColor() const override;

    bool focusEvent(MouseHitEvt& evt, bool focused) override {
        if (fnFocus) fnFocus(evt, focused);
        return true;
    }
    bool handleMouseScroll(MouseEvent& evt, double xoffset, double yoffset) override;
    void handleRightClick(MouseEvent& evt) override;
    void renderButtonAt(NVGcontext* vg, ivec2 insetP, ivec2 insetS, float value);
    void renderRangeIndicator(NVGcontext* vg, ivec2 insetP, ivec2 insetS, float rangeValueMin, float rangeValueMax, NVGcolor color, int idx, int numRanges);
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
        newValue     = math::clamp(newValue, 0.0f, 1.0f);
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
        return math::clamp(getValue(), 0.0f, 1.0f);
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
    virtual void setToDefaultValue();
    virtual std::optional<std::vector<param_modulation_range_t>> getKnobModulationRanges();
    void modulationDragMove(DAW::UI::Modulation::gui_dragged_modulation* g, ivec2 mousepos) override;
    void modulationDragRelease(DAW::UI::Modulation::gui_dragged_modulation* g, ivec2 mousepos) override;
};

class gui_slider_textfield : public gui_textfield {
protected:
    automatable_t* paramAutomatable = nullptr;
    int32_t paramIdx  = -1;
    float fBeginValue = 0.0f;
public:
    gui_slider_textfield() : gui_textfield() {
        setCanMouseHit(true);
        setAlignment(gui_textfield::Alignment::Center);
        setReturnCommits(true);
    }
    GuiColor::constant_t getBackgroundColor() const override;
    virtual bool renderAsBipolar();
    virtual String getValueAsString(float param);
    virtual float getRenderScaledValue(float param) {
        return param;
    }
    virtual float modifyParam(float param, float amt, bool applyUserInputScaling);
    virtual float parseTextValue(const String& str);

    int32_t getParamIdx() const {
        return paramIdx;
    }
    void setAutomationRef(automatable_t* _paramAutomatable, int32_t _paramIdx) {
        this->paramAutomatable = _paramAutomatable;
        this->paramIdx         = _paramIdx;
    }
    void handleRightClick(MouseEvent& evt) override;
    virtual bool isAutomated();
    virtual bool isModulated();
    // void setColors();

    void modulationDragMove(DAW::UI::Modulation::gui_dragged_modulation* g, ivec2 mousepos) override;
    void modulationDragRelease(DAW::UI::Modulation::gui_dragged_modulation* g, ivec2 mousepos) override;
    void render(NVGcontext* vg) override;
    void layout() override {
        gui_textfield::layout();
        setFontSize(size.y);
    }
    bool focusEvent(MouseHitEvt& evt, bool focused) override {
        if (!focused) {
            gui_textfield::focusEvent(evt, focused);
        }
        return true;
    }
    bool handleCharInput(uint32_t codepoint) override;
    bool keyboardEvent(KeyboardKey key, int scancode, KeyboardState action, KeyboardMods modifiers) override;
    void onTextEndEdit() override;
    void handleDraggedBegin(MouseEvent& evt) override;
    void handleDraggedMove(MouseEvent& evt) override;
    void handleDraggedRelease(MouseEvent& evt) override;
    void updateAutomatableParam(float amt, bool applyUserInputScaling, bool isFinal);
};
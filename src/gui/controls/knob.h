#pragma once
#include "gui/gui.h"
#include "gui/controls/textfield.h"
#include "guicolors.h"
#include "math/seq_math.h"
#include "gui/automation/modulation.h"
#include <functional>
#include <nanovg_min.h>
#include <optional>

struct automatable_t;
struct automatable_param_t;
struct param_modulation_range_t;

class guiknob : public guibase, public DAW::UI::IModulateable {
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
    bool bClampToZero            = true;

    automatable_t* paramAutomatable = nullptr;
    int32_t paramIdx                = -1;

    String strValueDisplay = "N/A";
public:
    std::function<float()> fnGetValue;
    std::function<void(float, int)> fnSetValue;
    std::function<void(float, float)> fnValueEditBegin;
    std::function<void(float, float)> fnValueEditChanged;
    std::function<void(float, float)> fnValueEditFinish;
    std::function<void(MouseHitEvt&, bool)> fnFocus;
    explicit guiknob(knobtype knobType) : guibase(), knobType(knobType) {
        setGuiType(gui_type::GUI_TYPE_KNOB);
        setBackgroundRendered(false);
        setCanMouseHit(true);        
    }
    knobtype getKnobType() const {
        return knobType;
    }
    virtual float getQuantizationStep() const {
        if (paramAutomatable) {
            auto p = paramAutomatable->getParam(paramIdx);
            if (assert_expr(p)) {
                return p->quantizationSteps ? 1.0f / p->quantizationSteps : 0.0f;
            }
        }
        return 0.0f;
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

    void setClampToZero(bool bClampToZero) {
        this->bClampToZero = bClampToZero;
    }

    bool getIsClampToZero() const {
        return bClampToZero;
    }

    void setAutomationRef(automatable_t* _paramAutomatable, int32_t _paramIdx) {
        this->paramAutomatable = _paramAutomatable;
        this->paramIdx         = _paramIdx;
    }
    void getAutomationRef(automatable_t*& at, int32_t& paramIdx) const override {
        paramIdx = this->paramIdx;
        at       = this->paramAutomatable;
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
    void renderRangeIndicator(NVGcontext* vg, ivec2 center, ivec2 insetP, ivec2 insetS, float fRenderValue, float rangeValueMin, float rangeValueMax, NVGcolor color, int idx, int numRanges);
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
        newValue     = math::clamp(newValue, 0.0f - (!bClampToZero * 1.0f), 1.0f);
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
        return math::clamp(getValue(), 0.0f - (!bClampToZero * 1.0f), 1.0f);
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
    virtual std::optional<std::vector<param_modulation_range_t>*> getKnobModulationRanges();
    virtual std::optional<std::pair<float,float>> getModulationMinMax() const {
        return std::nullopt;
    }
    void storeEditModulationTransform(NVGcontext* vg);
};

class gui_slider_textfield : public gui_textfield, public DAW::UI::IModulateable {
protected:
    automatable_t* paramAutomatable = nullptr;
    int32_t paramIdx  = -1;
    float fBeginValue = 0.0f;
    float m_fontScale = 1.0f;
    textlabel_dynamic_t m_textLabelParamName;
    textlabel_dynamic_t m_textLabelParamValue;
public:
    gui_slider_textfield() : gui_textfield() {
        setGuiType(gui_type::GUI_TYPE_SLIDER_TEXTFIELD);
        setCanMouseHit(true);
        setAlignment(gui_textfield::Alignment::Center);
        setReturnCommits(true);
    }
    void setFontScale(float f) {
        m_fontScale = f;
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
    void setAutomationRef(automatable_t* _paramAutomatable, int32_t _paramIdx);
    void getAutomationRef(automatable_t*& at, int32_t& paramIdx) const override {
        paramIdx = this->paramIdx;
        at       = this->paramAutomatable;
    }
    void handleRightClick(MouseEvent& evt) override;
    virtual bool isAutomated();
    virtual bool isModulated();

    void render(NVGcontext* vg) override;
    void layout() override;
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
    void onTick(AppCtrl* appctrl) override;
    virtual std::optional<std::vector<param_modulation_range_t>*> getKnobModulationRanges();
    virtual std::optional<std::pair<float,float>> getModulationMinMax() const {
        return std::nullopt;
    }
    void renderRangeIndicator(NVGcontext* vg, ivec2 insetP, ivec2 insetS, float fRenderValue, float rangeValueMin, float rangeValueMax, NVGcolor color, int idx, int numRanges);
    bool handleMouseScroll(MouseEvent& evt, double xoffset, double yoffset) override;
    virtual float getQuantizationStep() const {
        if (paramAutomatable) {
            auto p = paramAutomatable->getParam(paramIdx);
            if (assert_expr(p)) {
                return p->quantizationSteps ? 1.0f / p->quantizationSteps : 0.0f;
            }
        }
        return 0.0f;
    }
};

class guictr_select_enum final : public guictr_base, public DAW::UI::IModulateable {
    class guibutton_select_enum : public guibutton {
    public:
        guibutton_select_enum() = default;
        void renderButtonLabel(NVGcontext* vg, int32_t stateFlags) override {
            nvgSave(vg);
            if (setScissorTransform(vg)) {
                static_cast<guictr_select_enum*>(parent)->renderButtonLabel(this, vg, stateFlags);
            }
            nvgRestore(vg);
        }
        bool getState() const override {
            return static_cast<guictr_select_enum*>(parent)->getButtonStateState(this);
        }
    };
    const int N;
    std::vector<guibutton_select_enum> buttons;
    automatable_t* paramAutomatable = nullptr;
    int32_t paramIdx                = -1;
public:
    std::function<void(guibutton*, int32_t, NVGcontext*, int32_t, ivec2)> fnRenderButtonLabel;
    explicit guictr_select_enum(size_t N) : guictr_base(), N(N), buttons(N) {
        padding = 0;
        setLayoutMode(autolayout_mode::LAYOUT_VERTICAL);
        for (size_t i = 0; i < N; ++i) {
            buttons[i].id = i;
            add(&buttons[i]);
        }
    }
    ~guictr_select_enum() override {
        removeGuis();
    }
    void setAutomationRef(automatable_t* _paramAutomatable, int32_t _paramIdx) {
        this->paramAutomatable = _paramAutomatable;
        this->paramIdx         = _paramIdx;
        setCanMouseHit(this->paramAutomatable != nullptr);
    }
    void getAutomationRef(automatable_t*& at, int32_t& paramIdx) const override {
        paramIdx = this->paramIdx;
        at       = this->paramAutomatable;
    }
    int32_t getParamIdx() const { return paramIdx; }
    void renderButtonLabel(guibutton_select_enum* button, NVGcontext* vg, int32_t stateFlags) {
        ivec2 renderFrame = button->size;
        ivec2 renderPos(0);
        if (renderFrame.y > 10 && renderFrame.x > 10) {
            if (fnRenderButtonLabel) {
                fnRenderButtonLabel(button, button->id, vg, stateFlags, renderFrame);
            } else if (!button->getText().empty()) {
                auto fontScale = math::clamp(math::min(renderFrame.y, renderFrame.x), 4, 48) * FONT_AUTOSCALE;
                renderCenteredMultilineText(vg, theme, button->getText(), fontScale, getLabelColor(), renderPos, renderFrame);
            }
        }
    }
    bool getButtonStateState(const guibutton_select_enum* button) const {
        int32_t idx = static_cast<int32_t>(button - &buttons[0]);
        if (paramAutomatable && paramIdx >= 0) {
            auto valModulated = paramAutomatable->getParamValue(paramIdx);
            auto param = paramAutomatable->getParam(paramIdx);
            dbgassert(param);
            if (param->quantizationSteps > 0) {
                return idx == math::floorfS32(valModulated * param->quantizationSteps + 0.5f);
            }
            return idx == math::roundfS32(valModulated * (N - 1));
        }
        return false;
    }
    void buttonClicked(guibase* button) override {
        if (assert_expr(button >= &buttons[0] && button < &buttons[N])) {
            auto idx = button->id;
            if (paramAutomatable && paramIdx >= 0) {
                float val = static_cast<float>(idx) / (N - 1);
                paramAutomatable->setParamEdit(paramIdx, val, param_update_flags::FLG_PAR_UPDATE_USER | param_update_flags::FLG_PAR_UPDATE_FINISH);
            }
        }
    }

    float modifyParam(float param, float amt, bool applyUserInputScaling) {
        if (applyUserInputScaling) {
            amt *= 0.01f;
        }
        return math::clamp(param - amt, 0.0f, 1.0f);
    }

    void updateAutomatableParam(float amt, bool applyUserInputScaling, bool isFinal) {
        float fNew = modifyParam(paramAutomatable->getParam(paramIdx)->getValue(), amt, applyUserInputScaling);
        int32_t flags = param_update_flags::FLG_PAR_UPDATE_USER;
        if (isFinal) {
            flags |= param_update_flags::FLG_PAR_UPDATE_FINISH;
        }
        paramAutomatable->setParamEdit(paramIdx, fNew, flags);
    }
    int32_t getNumButtons() const {
        return CtrSize(buttons);
    }
    guibutton_select_enum& getButton(int32_t idx) {
        return buttons.at(idx);
    }
    void rightClicked(MouseEvent& evt, guibase* button) override;
};
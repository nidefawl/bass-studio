#pragma once
#include "gui/container/container.h"
#include "gui/container/scrollcontainer.h"
#include "gui/controls/knobpluginparam.h"
#include "host/plugin/plugin-lockable.h"
#include "synth-modulations.hpp"


namespace PluginSynth {

class guictr_synth_title : public guictr_base {
    float titleHeight = 10.0f;
public:
    guictr_synth_title() = default;

    void renderContainerLabel(NVGcontext* vg) override {
        if (isFlag(FLG_RENDER_LABEL) && label.length() && titleHeight > 0) {
            const auto bgColor = getInnerBackgroundColorFromState(getStateFlags());
            renderTextLabel(vg,
                            vec2(getPosContent()) + vec2(padding + 2, titleHeight / 2.0),
                            vec2(getSizeContent()) - vec2(INSET_TITLE + 2, 0),
                            label,
                            theme,
                            titleHeight,
                            theme->getContrastColor(bgColor),
                            NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        }
    }

    void setTitleHeight(float height) {
        titleHeight = height;
    }

    float getTitleHeight() const {
        return label.empty() ? 0 : titleHeight;
    }

    virtual void layoutParameterGroup(ivec2& prefSize, vec2 knobSize, float titleHeight) {
    }
    void layoutEntriesGrid(ivec2 pos, ivec2 cs, int32_t maxCols) override {
        cs.y -= getTitleHeight();
        pos.y += getTitleHeight();
        guictr_base::layoutEntriesGrid(pos, cs, maxCols);
    }
    void layoutEntries(ivec2 pos, ivec2 cs, ivec2 dir) override {
        cs.y -= getTitleHeight();
        pos.y += getTitleHeight();
        guictr_base::layoutEntries(pos, cs, dir);
    }
};

class guicontainer_modulation_slot_destination final : public guictr_base {
    PluginLockable* const moduleInstance;
    ModulationController* const synth;
    const int32_t slotIndex;
    const int32_t destSlotIndex;
    guidropdown_generic<String> dropdown;
    guiknob knob;
    float rowHeight = HEIGHT_DEFAULT_INPUT;

public:
    guicontainer_modulation_slot_destination(PluginLockable* const _module, ModulationController* _synth, int32_t _slotIndex, int32_t _destSlotIndex)
        : guictr_base(),
          moduleInstance(_module),
          synth(_synth),
          slotIndex(_slotIndex),
          destSlotIndex(_destSlotIndex),
          knob(guiknob::knobtype::KNOB_UNLABELED) {
        padding      = 1;
        sortChildren = true;
        setCanMouseHit(true);
        setLabel(StringFormat("Mod %d Dst %d", _slotIndex, _destSlotIndex));
        knob.setIsBipolar(true);
        auto vecOpts = std::vector<String>();
        vecOpts.emplace_back("None");
        const auto& paramsDest = synth->getDestinations();
        for (auto param : paramsDest) {
            vecOpts.push_back(param.name);
        }
        dropdown.setZOrder(-1);
        dropdown.setOptions(vecOpts);
        dropdown.setLabel(StringFormat("Mod %d Dst %d", _slotIndex, _destSlotIndex));
        dropdown.setCallback([this](int idx, String& value) -> String {
            if (idx >= 0) {
                {
                    ThreadLock lock = moduleInstance->lock();
                    const auto& paramsDest = synth->getDestinations();
                    if (idx > 0 && idx-1 < CtrSize(paramsDest)) {
                        synth->setModulationDestination(slotIndex, destSlotIndex, paramsDest[idx-1].dstIdx, knob.getValue());
                    } else {
                        synth->setModulationDestination(slotIndex, destSlotIndex, -1, knob.getValue());
                    }
                }
                if (parent) {
                    parent->buttonClicked(this);
                }
                return value;
            }
            return StringFormat("%d", idx);
        });
        dropdown.setCurrentString("<unused>");
        knob.fnValueEditChanged = [this](float prev, float value) {
            {
                ThreadLock lock = moduleInstance->lock();
                synth->setModulationDestRange(slotIndex, destSlotIndex, knob.getValue());
            }
            if (parent) {
                parent->buttonClicked(this);
            }
        };
        knob.setClampToZero(false);
        add(&dropdown);
        add(&knob);
    }
    void setFromSynth() {
        auto modulation = synth->getModulationIfExists(slotIndex);
        if (modulation && CtrSize(modulation->destinations) > destSlotIndex) {
            auto& dest = modulation->destinations[destSlotIndex];
            if (dest.parameter < 0) {
                dropdown.setSelectedIndex(0);
            } else {
                const auto& paramsDest = synth->getDestinations();
                auto it = std::find_if(std::begin(paramsDest), std::end(paramsDest), [dest](auto& desc) { return desc.dstIdx == dest.parameter; });
                if (std::end(paramsDest) != it) {
                    dropdown.setSelectedIndex(1 + static_cast<int32_t>(it - std::begin(paramsDest)));
                    dropdown.setCurrentString(it->name);
                } else {
                    dropdown.setSelectedIndex(-1);
                }
            }
            knob.setValueInit(static_cast<float>(dest.range));
        } else {
            dropdown.setSelectedIndex(0);
        }
    }
    ~guicontainer_modulation_slot_destination() override {
        removeGuis();
    }
    void layout() override {
        auto cs       = getSizeContent();
        knob.size     = { cs.y, cs.y };
        knob.pos      = cs - knob.size;
        dropdown.pos  = { 0, 0 };
        dropdown.size = { knob.pos.x - padding, cs.y };
        for (guibase* gui : guis) {
            gui->layout();
        }
    }
    void determineSize(ivec2& prefSize) override {
        prefSize.y = math::roundfS32(rowHeight);
    }
    void setRowHeight(float rowHeight) {
        this->rowHeight = rowHeight;
    }
};

class guicontainer_modulation_slot_source final : public guictr_base {
    PluginLockable* const moduleInstance;
    ModulationController* const synth;
    const int32_t slotIndex;
    const int32_t srcSlotIndex;
    double constant = 0.0;
    guidropdown_generic<String> dropdownOperator;
    guidropdown_generic<String> dropdownSource;
    gui_numberinput_double inputConstant;
    gui_textfield textfieldFunction;
    guibutton buttonInputRange;
    std::function<bool(String)> fnValidateFunction;
    float rowHeight = HEIGHT_DEFAULT_INPUT;

public:
    guicontainer_modulation_slot_source(PluginLockable* const _module, ModulationController* _synth, int32_t _slotIndex, int32_t _srcSlotIndex)
        : guictr_base(),
          moduleInstance(_module),
          synth(_synth),
          slotIndex(_slotIndex),
          srcSlotIndex(_srcSlotIndex),
          inputConstant(&constant) {
        padding      = 1;
        sortChildren = true;
        setCanMouseHit(true);
        setLabel(StringFormat("Mod %d Input %d", _slotIndex, _srcSlotIndex));
        {
            auto vecOpts = std::vector<String>();
            for (size_t i = 0; i < ModulationOperator::NumModulationOperators; ++i) {
                vecOpts.emplace_back(stringsModOp[i]);
            }
            // place it right after the source dropdown
            dropdownOperator.setOptions(vecOpts);
            // dropdownOperator.setLabel(StringFormat("Mod %d Op %d", slotIndex, srcSlotIndex));
            dropdownOperator.setCallback([this](int idx, String& value) -> String {
                {
                    ThreadLock lock = moduleInstance->lock();
                    synth->setModulationOperator(slotIndex, srcSlotIndex, idx);
                }
                if (parent) {
                    parent->buttonClicked(this);
                }
                return dropdownOperator.optionToString(value);
            });
        }
        {
            auto vecOpts = std::vector<String>();
            auto& paramsSource = synth->getSources();
            for (auto& param : paramsSource) {
                vecOpts.emplace_back(param.name);
            }
            dropdownSource.setZOrder(1);
            dropdownSource.setOptions(vecOpts);
            dropdownSource.setLabel(StringFormat("Mod %d Src %d", slotIndex, srcSlotIndex));
            dropdownSource.setCallback([this](int idx, String& value) -> String {
                if (idx >= 0) {
                    {
                        ThreadLock lock = moduleInstance->lock();
                        auto& paramsSource = synth->getSources();
                        if (idx < CtrSize(paramsSource)) {
                            auto desc = paramsSource[idx];
                            synth->setModulationType(slotIndex, srcSlotIndex, desc.srcIdx);
                        } else {
                            synth->setModulationType(slotIndex, srcSlotIndex, -1);
                        }
                    }
                    if (parent) {
                        parent->buttonClicked(this);
                    }
                    return dropdownSource.optionToString(value);
                }
                return "";
            });
        }
        {
            inputConstant.setLabel(StringFormat("Mod %d Constant %d", slotIndex, srcSlotIndex));
            inputConstant.fnValueEditChanged = [this](gui_numberinput_field_base*, double value) {
                {
                    ThreadLock lock = moduleInstance->lock();
                    synth->setModulationConstant(slotIndex, srcSlotIndex, value);
                }
                if (parent) {
                    parent->buttonClicked(this);
                }
            };
            inputConstant.fnClamp = [](double value) -> double {
                return value;
            };
        }
        {
            buttonInputRange.setLabel("Bipolar");
        }
        {
            textfieldFunction.setLabel(StringFormat("Mod %d Function %d", slotIndex, srcSlotIndex));
            // textfieldFunction.setTextfieldColor(GuiColor::COL_TEXTBOX_TEXT);
            textfieldFunction.setInputActivates(true);
            textfieldFunction.setReturnCommits(true);
            fnValidateFunction = ([this](const String& value) {
                {
                    try {
                        MathExpr expr = MathExpr::parse(value, synth->getVarNames());
                        {
                            ThreadLock lock = moduleInstance->lock();
                            synth->setModulationFunction(slotIndex, srcSlotIndex, std::move(expr));
                        }
                        textfieldFunction.setLabel(StringFormat("Mod %d Function %d", slotIndex, srcSlotIndex));
                        textfieldFunction.setTextfieldColor(GuiColor::COL_TEXTBOX_TEXT);
                    } catch (mu::Parser::exception_type& e) {
                        log_lf(Log::L_ERROR, "Error in expression: %s\n", e.GetMsg().c_str());
                        textfieldFunction.setLabel(StringFormat("Error in expression: %s", e.GetMsg().c_str()));
                        textfieldFunction.setTextfieldColor(GuiColor::COL_INVALID_INPUT);
                        {
                            ThreadLock lock = moduleInstance->lock();
                            MathExpr expr;
                            expr.str        = value;
                            expr.parsedExpr = nullptr;
                            synth->setModulationFunction(slotIndex, srcSlotIndex, std::move(expr));
                        }
                    }
                }
                if (parent) {
                    parent->buttonClicked(this);
                }
                return true;
            });
            textfieldFunction.setChangeCallback(fnValidateFunction);
            textfieldFunction.setEndEditCallback(fnValidateFunction);
        }
        add(&dropdownOperator);
        add(&dropdownSource);
        add(&inputConstant);
        add(&textfieldFunction);
        add(&buttonInputRange);
    }

    ~guicontainer_modulation_slot_source() override {
        removeGuis();
    }

    void setFromSynth() {
        auto modulation = synth->getModulationIfExists(slotIndex);
        dropdownOperator.setVisible(srcSlotIndex > 0 && (modulation && CtrSize(modulation->inputs) > srcSlotIndex));
        if (modulation && CtrSize(modulation->inputs) > srcSlotIndex) {
            auto& src = modulation->inputs[srcSlotIndex];
            switch (src.type) {
                case ModulationType::Function:
                case ModulationType::Constant:
                    dropdownSource.setSelectedIndex(static_cast<int32_t>(src.type) + 1);
                    break;
                case ModulationType::ModulationSource: {
                    auto& paramsSource = synth->getSources();
                    auto it = std::find_if(paramsSource.cbegin(), paramsSource.cend(), [searchIdx = 2+src.src](auto& desc) { return desc.srcIdx == searchIdx; });
                    if (it != paramsSource.cend()) {
                        dropdownSource.setSelectedIndex(static_cast<int32_t>(it - paramsSource.cbegin()));
                    } else {
                        dropdownSource.setSelectedIndex(0);
                    }
                    break;
                }
                default:
                    dropdownSource.setSelectedIndex(0);
                    break;
            }
            dropdownOperator.setSelectedIndex(static_cast<int32_t>(src.op));
            textfieldFunction.setVisible(textfieldFunction.isEditing() || src.type == ModulationType::Function);
            if (!textfieldFunction.isEditing()) {
                textfieldFunction.setValue(src.function.str);
            }
            if (!src.function.str.empty() && !src.function.parsedExpr) {
                textfieldFunction.setLabel("Error in expression");
                textfieldFunction.setTextfieldColor(GuiColor::COL_INVALID_INPUT);
            } else if (src.function.parsedExpr && src.function.parsedExpr->nanInfCounter) {
                textfieldFunction.setLabel(StringFormat("%d NaN/Inf detected", src.function.parsedExpr->nanInfCounter));
                textfieldFunction.setTextfieldColor(GuiColor::COL_INVALID_INPUT);
            }
            dropdownOperator.setVisible(dropdownOperator.isVisible() && (src.type != ModulationType::Function));
            inputConstant.setVisible(src.type == ModulationType::Constant);
            buttonInputRange.setVisible(src.type != ModulationType::Constant && src.type != ModulationType::Function);
            switch (src.range) {
                case ModulationRange::Bipolar:
                    buttonInputRange.setText("+/-");
                    buttonInputRange.setLabel("Bipolar [-1.0 - 1.0]");
                    break;
                case ModulationRange::Unipolar:
                    buttonInputRange.setText("+");
                    buttonInputRange.setLabel("Unipolar [0.0 - 1.0]");
                    break;
                case ModulationRange::Triangle:
                    buttonInputRange.setText("\\/");
                    buttonInputRange.setLabel("Triangle [0.0 - 1.0]");
                    break;
                default:
                    break;
            }
            constant = src.value;
        } else {
            dropdownSource.setSelectedIndex(0);
            dropdownOperator.setSelectedIndex(0);
            textfieldFunction.setVisible(false);
            inputConstant.setVisible(false);
            buttonInputRange.setVisible(false);
            buttonInputRange.setText("+");
            buttonInputRange.setLabel("Unipolar");
            constant = 1.0;
        }
    }

    void layout() override {
        auto cs = getSizeContent();
        // dbgassert(cs.x > 0);
        auto sizeRightOperator = cs.x;
        dropdownSource.pos     = { 0, 0 };
        if (dropdownOperator.isVisible()) {
            auto partialSize      = cs.x * 1 / 4;
            dropdownOperator.pos  = {};
            dropdownOperator.size = { partialSize - padding, cs.y };
            sizeRightOperator     = cs.x - partialSize;
            dropdownSource.pos.x  = dropdownOperator.right() + padding;
        }
        auto widthButtonBipolar = math::roundfS32(size.y);
        if (buttonInputRange.isVisible()) {
            sizeRightOperator -= widthButtonBipolar;
            buttonInputRange.size = { widthButtonBipolar, cs.y };
            buttonInputRange.pos  = { cs.x - widthButtonBipolar, 0 };
        }
        dropdownSource.size = { sizeRightOperator - padding, cs.y };
        if (inputConstant.isVisible()) {
            auto partialSize2     = (sizeRightOperator) *3 / 10;
            dropdownSource.size.x = dropdownSource.size.x - partialSize2 - padding;
            inputConstant.size    = { partialSize2, cs.y };
            inputConstant.pos     = { dropdownSource.right() + padding, 0 };
        }
        if (textfieldFunction.isVisible()) {
            auto partialSize2      = (sizeRightOperator) *7 / 10;
            dropdownSource.size.x  = dropdownSource.size.x - partialSize2 - padding;
            textfieldFunction.size = { partialSize2, cs.y };
            textfieldFunction.pos  = { dropdownSource.right() + padding, 0 };
            // textfieldFunction.setFontSize(textfieldFunction.getSize.y);
        }
        for (guibase* gui : guis) {
            gui->layout();
            // dbgassert(!gui->isVisible() || (gui->size.x > 0 && gui->size.y > 0));
        }
    }

    void buttonClicked(guibase* button) override {
        if (button == &buttonInputRange) {
            {

                ThreadLock lock = moduleInstance->lock();
                auto modulation = synth->getModulationIfExists(slotIndex);
                if (modulation && CtrSize(modulation->inputs) > srcSlotIndex) {
                    auto& input = modulation->inputs[srcSlotIndex];
                    synth->setModulationInputRange(slotIndex, srcSlotIndex, static_cast<ModulationRange>((static_cast<int32_t>(input.range) + 1) % ModulationRange::NumModulationRanges));
                }
            }
            if (parent) {
                parent->buttonClicked(this);
            }
        }
        guictr_base::buttonClicked(button);
    }

    void determineSize(ivec2& prefSize) override {
        prefSize.y = math::roundfS32(rowHeight);
    }

    void setRowHeight(float rowHeight) {
        this->rowHeight = rowHeight;
    }
};


class guicontainer_modulation_slot final : public guictr_synth_title {
    PluginLockable* const moduleInstance;
    ModulationController* const synth;
    const int32_t slotIndex;
    std::vector<guicontainer_modulation_slot_source*> sources;
    std::vector<guicontainer_modulation_slot_destination*> destinations;

public:
    explicit guicontainer_modulation_slot(PluginLockable* const _module, ModulationController* _synth, int32_t slotIndex)
        : moduleInstance(_module),
          synth(_synth),
          slotIndex(slotIndex) {
        padding      = 1;
        margin       = 0;
        sortChildren = true;
        setLabel(StringFormat("Modulation %d", slotIndex + 1));
        setBackgroundRendered(true);
        setBackgroundRenderedInset(true);
        setFlag(FLG_RENDER_LABEL, true);
        setCanMouseHit(true);
    }

    ~guicontainer_modulation_slot() override {
        removeGuis();
        for (auto& d : sources) {
            delete d;
        }
        for (auto& d : destinations) {
            delete d;
        }
    }

    void setFromSynth() {
        auto modulation     = synth->getModulationIfExists(slotIndex);
        auto modSourceCount = modulation ? modulation->inputs.size() : 0;
        auto modDestCount   = modulation ? modulation->destinations.size() : 0;
        while (sources.size() > modSourceCount + 1) {
            remove(sources.back());
            delete sources.back();
            sources.pop_back();
        }
        while (sources.size() < modSourceCount + 1) {
            const auto srcIdx = CtrSize(sources);
            auto dropdown     = new guicontainer_modulation_slot_source(moduleInstance, synth, slotIndex, srcIdx);
            dropdown->setZOrder(-srcIdx * 10);
            sources.push_back(dropdown);
            add(sources.back());
        }
        while (destinations.size() > 1 && destinations.size() > modDestCount + 1) {
            remove(destinations.back());
            delete destinations.back();
            destinations.pop_back();
        }
        while (destinations.size() < modDestCount + 1) {
            const auto dstIdx = CtrSize(destinations);
            auto dstSlot      = new guicontainer_modulation_slot_destination(moduleInstance, synth, slotIndex, dstIdx);
            // place it after source and operator dropdowns
            dstSlot->setZOrder(-(1000 + dstIdx * 10 + 2));
            destinations.push_back(dstSlot);
            add(destinations.back());
        }
        for (auto& src : sources) {
            src->setFromSynth();
        }
        for (auto& dst : destinations) {
            dst->setFromSynth();
        }
    }

    void render(NVGcontext* vg) override {
        // auto halfSize = vec2(size) * 0.5f;
        // vec2 posScrolled = vec2(pos) + halfSize;
        // nvgTransformByState(vg, 2, &posScrolled.x, &posScrolled.y);
        // if (posScrolled.y < -halfSize.y) {
        //     log_printf("Skip: right, bottom %d %d, in parent space: %f %f\n", right(), bottom(), posScrolled.x, posScrolled.y);
        //     return;
        // }
        guictr_base::render(vg);
    }

    void drawBackground(NVGcontext* vg, const guitheme_t* theme, ivec2 posInset, ivec2 sizeInset, int margin, bool drawInset) override {
        if (sizeInset.y > 0 && sizeInset.x > 0) {
            nvgTranslateZ(vg, -2.0f);
            // nvgShapeAntiAlias(vg, 0);
            nvgBeginPath(vg);
            nvgRect(vg, pos.x, posInset.y, size.x, getTitleHeight());
            auto color = dbgcolorsArray[1 + (slotIndex % (dbgcolorsArraySize - 1))];
            color.a = 0.5f;
            nvgFillColor(vg, color);
            nvgFill(vg);
            nvgBeginPath(vg);
            nvgRect(vg, pos.x, posInset.y, size.x, sizeInset.y);
            nvgStrokeWidth(vg, theme->get(GuiConstant::CONST_GUI_INSET_WIDGET_BG));
            nvgStrokeColor(vg, color);
            nvgStroke(vg);
            // nvgShapeAntiAlias(vg, USE_NANOVG_AA);
            nvgTranslateZ(vg, 1.0f);
        }
    }

    void determineSize(ivec2& prefSize) override {
        vec2 sizeTotal = {0, 0};
        for (auto& src : guis) {
            src->size = prefSize;
            src->determineSize(src->size);
            sizeTotal.y += src->size.y;
        }
        prefSize.y = math::ceilfS32(getTitleHeight() + sizeTotal.y + padding * 2);
    }

    void setRowHeight(float height) {
        setTitleHeight(height);
        for (auto& src : sources) {
            src->setRowHeight(height*1.5f);
        }
        for (auto& dst : destinations) {
            dst->setRowHeight(height*1.5f);
        }
    }

    void layout() override {
        auto cs        = getSizeContent();
        vec2 pos      = {0, getTitleHeight()};
        for (auto& slot : guis) {
            slot->pos    = pos;
            slot->size.x = cs.x;
            // slot->size.y = math::roundfS32(rowHeight);
            slot->layout();
            pos.y = slot->bottom();
        }
    }
};

class guicontainer_modulation final : public guictr_synth_title {
    PluginLockable* const moduleInstance;
    ModulationController* const synth;
    guictr_scrollbar scrollContainerModulation;
    std::vector<guicontainer_modulation_slot*> slots;
    bool bGuiNeedsRefresh = true;

public:
    explicit guicontainer_modulation(PluginLockable* _module, ModulationController* _synth)
        : moduleInstance(_module), synth(_synth) {
        margin  = 0;
        padding = 0;
        setLabel("Modulation");
        setBackgroundRendered(false);
        setBackgroundRenderedInset(false);
        setFlag(FLG_RENDER_LABEL, false);
        setCanMouseHit(true);
        scrollContainerModulation.maxHeight = -1;
        add(&scrollContainerModulation);
        scrollContainerModulation.setLayoutMode(autolayout_mode::LAYOUT_VERTICAL);
    }

    ~guicontainer_modulation() override {
        scrollContainerModulation.removeGuis();
        removeGuis();
        for (auto& slot : slots) {
            delete slot;
        }
    }

    void layout() override {
        scrollContainerModulation.pos = {0, getTitleHeight()};
        scrollContainerModulation.size = size;
        scrollContainerModulation.maxHeight = size.y;
        scrollContainerModulation.determineSize(scrollContainerModulation.size);
        for (auto* gui : guis) {
            gui->layout();
        }
    }

    void setTitleHeight(float height) {
        guictr_synth_title::setTitleHeight(isFlag(FLG_RENDER_LABEL) ? height*1.5f : 0);
        for (auto& slot : slots) {
            slot->setRowHeight(height);
        }
    }

    void setFromSynth() {
        auto modulations = synth->getModulationCount();
        while (CtrSize(slots) <= modulations) {
            slots.push_back(new guicontainer_modulation_slot(moduleInstance, synth, CtrSize(slots)));
            scrollContainerModulation.add(slots.back());
        }
        for (auto& slot : slots) {
            slot->setFromSynth();
        }
        if (parent && size.x > 0 && size.y > 0) {
            layout();
        }
    }

    void buttonClicked(guibase* button) override {
        onChildLayoutChanged(this);
        bGuiNeedsRefresh = true;
        guictr_base::buttonClicked(button);
    }

    void onTick(AppCtrl* ctrl) override {
        if (bGuiNeedsRefresh) {
            bGuiNeedsRefresh = false;
            setFromSynth();
        }
        guictr_base::onTick(ctrl);
    }

    void determineSize(ivec2& prefSize) override {
        ivec2 sizeTotal = {};
        for (auto& src : guis) {
            src->size = prefSize;
            src->determineSize(src->size);
            sizeTotal.y += src->size.y;
        }
        prefSize.y = math::ceilfS32(getTitleHeight() + sizeTotal.y + padding * 2);
    }
};

class guiknob_synthparam final : public guiknob_pluginparam {
    ModulationController* const synth;
    const int32_t param;

public:
    explicit guiknob_synthparam(int32_t idx, int32_t idxExternal, ModulationController* _impl, int32_t _param, guiknob::knobtype _knobtype = guiknob::knobtype::KNOB_LABELED)
        : guiknob_pluginparam(idxExternal, idx, _knobtype),
            synth(_impl),
            param(_param) {
        m_layout.inset = 2;
    }
    std::optional<std::vector<param_modulation_range_t>> getKnobModulationRanges() override {
        if (synth) {
            if (!synth->isShowModulationRanges()) {
                return std::nullopt;
            }
            auto modIdx = synth->getModulationIdx(param);
            if (modIdx < 0) {
                return std::nullopt;
            }
            return synth->getParamModulationRanges(modIdx);
        }
        return std::nullopt;
    }
    int32_t getParam() const {
        return param;
    }
};
class guiknob_synthparam_textfield final : public gui_slider_textfield {
    ModulationController* synth;
    int32_t param;

public:
    guiknob_synthparam_textfield() = default;

    void setSynthParam(ModulationController* synth, int32_t param) {
        this->synth = synth;
        this->param = param;
    }

    std::optional<std::vector<param_modulation_range_t>> getKnobModulationRanges() override {
        if (synth) {
            if (!synth->isShowModulationRanges()) {
                return std::nullopt;
            }
            auto modIdx = synth->getModulationIdx(param);
            if (modIdx < 0) {
                return std::nullopt;
            }
            return synth->getParamModulationRanges(modIdx);
        }
        return std::nullopt;
    }
};

class guictr_synth_param_container : public guictr_synth_title {
    ModulationController* const synth;
    std::vector<guiknob_synthparam*> knobs;
    vec2 sliderSize{ 0.0f, 0.0f };
public:
    explicit guictr_synth_param_container(ModulationController* synth)
        : synth(synth) {
        margin  = 4;
        padding = 4;
        setBackgroundRendered(true);
        setBackgroundRenderedInset(true);
        setFlag(FLG_RENDER_LABEL, true);
        setCanMouseHit(true);
    }
    ~guictr_synth_param_container() override {
        destroyGuis();
    }

    void addParamKnob(guiknob_synthparam* knob) {
        knobs.push_back(knob);
        add(knob);
    }

    void render(NVGcontext* vg) override {
        if (!isVisible()) {
            log_printf("warning, skip rendering container with state !isVisible()\n");
            return;
        }
        if (isBackgroundRendered()) {
            renderBackground(vg);
        }
        if (!setScissorTransform(vg)) {
            return;
        }
        for (auto c : guis) {
            if (!c->isVisible()) {
                //log_printf("warning, skip rendering child container with state !isVisible()\n");
                continue;
            }
            if (c->size.x <= 5 || c->size.y <= 5) {
                continue;
            }
            {
                nvgSave(vg);
                c->render(vg);
                nvgRestore(vg);
            }
        }
        if (synth && synth->isShowModulationRanges()) {
            for (auto k : knobs) {
                auto modIdx = synth->getModulationIdx(k->getParam());
                if (modIdx < 0)
                    continue;
                auto valueMin = static_cast<float>(this->synth->getModulationAmountMin(modIdx));
                auto valueMax = static_cast<float>(this->synth->getModulationAmountMax(modIdx));
                auto layout = k->getLayout();
                auto col = dbgcolorsArray[0];
                col.a = 0.5;
                k->renderRangeIndicator(vg, layout.pKnob, layout.sKnob, valueMin, valueMax, col, 9, 10);
            }
        }
    }

    void buttonClicked(guibase* button) override {
        parent->buttonClicked(button);
    }

    void layoutParameterGroup(ivec2& prefSize, vec2 knobSize, float titleHeight) override {
        int newPadding = 0;
        while (newPadding < 4 && newPadding * 48 < prefSize.y) {
            newPadding++;
        }
        padding = newPadding;
        if (label.empty())
            titleHeight = 0;
        this->setTitleHeight(titleHeight);
        auto cs                = getSizeContent();
        const auto knobsPerCol = 3;
        const auto innerSize   = vec2(cs.x, cs.y - titleHeight);
        auto knobPos           = ivec2(0, titleHeight);
        auto sliderSize      = vec2(knobSize.x, innerSize.y);
        this->sliderSize     = sliderSize;
        knobSize.y           = (innerSize.y - padding * (knobsPerCol - 1)) / float(knobsPerCol);
        auto knobSizeRounded = ivec2(math::roundfS32(this->sliderSize.x), math::roundfS32(this->sliderSize.y));
        int32_t knobIdx      = 0;
        for (auto knob : guis) {
            if (knob->id == 1) {
                auto offset = knobIdx % knobsPerCol;
                knob->pos   = knobPos + ivec2(0, offset * (knobSize.y + padding));
                knob->size  = knobSize;
                knobIdx++;
                if (knobIdx % knobsPerCol == 0) {
                    knobPos.x += knobSizeRounded.x + padding;
                }
            }
        }
        if (knobIdx % knobsPerCol != 0) {
            knobPos.x += knobSizeRounded.x + padding;
        }
        for (auto knob : guis) {
            if (knob->id == 0) {
                knob->pos  = knobPos;
                knob->size = knobSizeRounded;
                knobPos.x += knobSizeRounded.x + padding;
            }
        }
        vec2 sizeFull = innerSize;
        for (auto knob : guis) {
            if (knob->id == 2) {
                knob->pos  = vec2(knobPos.x, titleHeight);
                knob->size = sizeFull;
                knobPos.x += math::floorfS32(sizeFull.x + padding);
            }
        }
        prefSize.x = math::roundfS32(knobPos.x - padding) + padding * 2;
    }
};

} // namespace PluginSynth

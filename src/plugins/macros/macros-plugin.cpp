#include "macros-plugin.h"
#include "assert_dbg.h"
#include "automation.h"
#include "gui/container/container.h"
#include "gui/controls/button.h"
#include "gui/controls/inputfield.h"
#include "gui/controls/knobpluginparam.h"
#include "gui/gui.h"
#include "gui/tooltip/tooltip.h"
#include "gui/views/controls.h"
#include "guicolors.h"
#include "guiconstant.h"
#include "host/host_pluginmanager.h"
#include "host/mainctrl.h"
#include "logging.h"
#include "math/seq_math.h"
#include "renderresources.h"
#include "seq_util.h"

#include "str_util.h"
#include "logging.h"
#include "byte-buffer.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <utility>

namespace DAW::UI {
    class guictr_edit_modulation_slot : public guictr_base {
        automatable_t* paramAutomatable = nullptr;
        int32_t paramIdx = 0;
        int32_t modulationIndex = 0;
        guibutton btnSourceName;
        gui_numberinput_float fieldMinVal;
        gui_numberinput_float fieldMaxVal;
        guibutton btnRemove;
        public:
        guictr_edit_modulation_slot()
            : guictr_base(),
            fieldMinVal(nullptr),
            fieldMaxVal(nullptr)
        {
            margin  = 0;
            padding = 2;
            this->guiType = gui_type::CTR_TYPE_EDIT_MODULATION;
            add(&btnSourceName);
            add(&fieldMaxVal);
            add(&fieldMinVal);
            add(&btnRemove);
            fieldMaxVal.setLabel("Max");
            fieldMinVal.setLabel("Min");
            btnRemove.setLabel("Remove");
            btnRemove.setText("Remove");
            btnSourceName.setText("Source");
            btnSourceName.setLabel("Source");
        }
        ~guictr_edit_modulation_slot() override {
            removeGuis();
        }
        void setAutomationRef(const Host::PluginManager* host, automatable_t* _paramAutomatable, int32_t _paramIdx, DAW::automation_channel_ref _ref, int32_t modulationIndex) {
            dbgassert(host);
            dbgassert(_paramAutomatable);
            btnRemove.id = 16 + modulationIndex;
            this->modulationIndex  = modulationIndex;
            this->paramAutomatable = _paramAutomatable;
            this->paramIdx         = _paramIdx;
            auto& modChannels = _paramAutomatable->getModulations();
            auto& modChannelRef       = modChannels[modulationIndex];
            fieldMinVal.setRef(&modChannelRef.scale.min);
            fieldMaxVal.setRef(&modChannelRef.scale.max);
            auto channel = DAW::ResolveModulationChannel(host, modChannelRef);
            String channelName;
            if (channel) {
                channelName = channel->getName();
            }
            String srcName = paramAutomatable->getAutomatableName();
            srcName += " ";
            srcName += paramAutomatable->getParamName(_paramIdx);
            if (!channelName.empty()) {
                btnRemove.setTooltipText("Remove Modulation: " + channelName);
            } else {
                btnRemove.setTooltipText("Remove Modulation");
            }
            setLabel(srcName);
            btnSourceName.setText(channelName);
        }

        void renderBackground(NVGcontext* vg) override {
            drawInsetBackground(vg, theme, getPosContent(), getSizeContent());
        }
        GuiConstant::constant_t getGuiConstantHeight() const {
            return GuiConstant::CONST_ROW_HEIGHT;
        }
        void layout() override {
            const int32_t TRACK_HEIGHT_STEP = theme->get(getGuiConstantHeight());
            auto padding = theme->get(GuiConstant::CONST_PADDING_EDITOR_CONTROLS);
            auto cs = getSizeContent();
            auto srcNameW = 0.5f * cs.x;
            auto w    = (cs.x-srcNameW) - padding * 2;
            btnSourceName.size = {srcNameW, TRACK_HEIGHT_STEP};
            btnSourceName.pos = {padding, 0};
            fieldMinVal.size = ivec2(w*0.4-padding, TRACK_HEIGHT_STEP);
            fieldMinVal.pos  = ivec2(padding+srcNameW, 0);
            fieldMaxVal.size = ivec2(w*0.4-padding, TRACK_HEIGHT_STEP);
            fieldMaxVal.pos  = ivec2(padding+srcNameW+w*0.4, fieldMinVal.top());
            btnRemove.size   = ivec2(w*0.2, TRACK_HEIGHT_STEP);
            btnRemove.pos    = ivec2(padding+srcNameW+w*0.8, fieldMaxVal.top());
            for (guibase* gui : guis) {
                gui->layout();
            }
        }
        void determineSize(ivec2& prefSize) override {
            const int32_t TRACK_HEIGHT_STEP = theme->get(getGuiConstantHeight());
            auto padding2 = paddingBR(padding) + paddingTL(padding);
            prefSize = ivec2(prefSize.x, TRACK_HEIGHT_STEP+padding2.y);
        }
        void buttonClicked(guibase* _button) override {
            if (parent) {
                parent->buttonClicked(_button);
            }
        }
    };
    class guictr_edit_modulation : public guictxtmenu_base {
        automatable_t* paramAutomatable = nullptr;
        int32_t paramIdx = 0;
        guibutton btnAddModulation;
        std::vector<guictr_edit_modulation_slot*> slots;
        const Host::PluginManager* host = nullptr;
        public:
        guictr_edit_modulation()
            : guictxtmenu_base()
        {
            this->guiType = gui_type::CTR_TYPE_EDIT_MODULATION;
            add(&btnAddModulation);
            btnAddModulation.setLabel("Add Modulation");
            btnAddModulation.setText("Add");
            padding = 2;
        }
        ~guictr_edit_modulation() override {
            removeGuis();
        }
        void setAutomationRef(const Host::PluginManager* host, automatable_t* _paramAutomatable, int32_t _paramIdx, DAW::automation_channel_ref _ref) {
            this->host = host;
            this->paramAutomatable = _paramAutomatable;
            this->paramIdx         = _paramIdx;
            updateSlots();
        }
        void updateSlots() {
            dbgassert(host);
            String text = "Modulation: ";
            text += paramAutomatable->getAutomatableName();
            text += " ";
            text += paramAutomatable->getParamName(paramIdx);
            setLabel(text);
            auto isModulated = paramAutomatable->isParamModulated(paramIdx);
            if (isModulated) {
                auto& inputs = paramAutomatable->getModulations(paramIdx);
                auto numInputs = inputs.size();
                while (slots.size() > numInputs) {
                    remove(slots.back());
                    delete slots.back();
                    slots.pop_back();
                }
                while (slots.size() < numInputs) {
                    auto slot = new guictr_edit_modulation_slot();
                    add(slot);
                    slots.push_back(slot);
                }
                for (size_t i = 0; i < numInputs; ++i) {
                    slots[i]->setAutomationRef(host, paramAutomatable, paramIdx, *inputs[i], i);
                }
            } else {
                while (slots.size() > 0) {
                    remove(slots.back());
                    delete slots.back();
                    slots.pop_back();
                }
            }
            
        }

        void renderBackground(NVGcontext* vg) override {
            drawInsetBackground(vg, theme, getPosContent(), getSizeContent());
        }
        GuiConstant::constant_t getGuiConstantHeight() const {
            return GuiConstant::CONST_ROW_HEIGHT;
        }
        GuiConstant::constant_t getGuiConstantTitlebar() const {
            return GuiConstant::CONST_ROW_HEIGHT;
        }
        void render(NVGcontext* vg) override {
            nvgIntersectScissor(vg, pos.x, pos.y, size.x, size.y);
            nvgTranslate(vg, pos.x, pos.y);
            renderFrameBase(vg);
            int flags = parentCtrl->isCtrOrChildFocused(this) ? TITLEBAR_FLG_FOCUSED : 0;
            if (isSelected()) flags |= TITLEBAR_FLG_SELECTED;
            renderTitleBar(vg, size, getLabel(), getGuiConstantTitlebar(), 0, flags, true);
            renderFrameOutline(vg);
            ivec2 posInset  = getPosContent();
            nvgTranslate(vg, posInset.x-pos.x, posInset.y-pos.y);
            nvgTranslateZ(vg, -4.0f);
            for (guibase* gui: guis) {
                nvgSave(vg);
                gui->render(vg);
                nvgRestore(vg);
            }
        }

        void layout() override {
            const int32_t TRACK_HEIGHT_STEP = theme->get(getGuiConstantHeight());
            auto padding = theme->get(GuiConstant::CONST_PADDING_EDITOR_CONTROLS);
            auto cs      = getSizeContent();
            auto w       = cs.x - padding * 2;
            auto posSlots = ivec2(0, theme->get(getGuiConstantTitlebar()));
            for (auto slot : slots) {
                slot->size = ivec2(cs.x, TRACK_HEIGHT_STEP);
                slot->pos  = posSlots;
                posSlots.y += slot->size.y;
            }
            this->btnAddModulation.size = ivec2(w, TRACK_HEIGHT_STEP);
            this->btnAddModulation.pos  = ivec2(padding, posSlots.y+padding);
            for (guibase* gui : guis) {
                gui->layout();
            }
        }
        void determineSize(ivec2& prefSize) override {
            const int32_t TRACK_HEIGHT_STEP = theme->get(getGuiConstantHeight());
            auto innerHeight = TRACK_HEIGHT_STEP;
            for (auto* slot : slots) {
                ivec2 tmpSize = prefSize;
                slot->determineSize(tmpSize);
                innerHeight += tmpSize.y;
            }
            innerHeight += TRACK_HEIGHT_STEP;
            auto padding2 = paddingBR(padding) + paddingTL(padding);
            innerHeight += padding2.y;
            prefSize = ivec2(prefSize.x, innerHeight + TRACK_HEIGHT_STEP);
        }

        void buttonClicked(guibase* _button) override {
            if (_button->id >= 16) {
                int32_t modulationIndex = _button->id - 16;
                if (paramAutomatable) {
                    paramAutomatable->removeModulation(modulationIndex);
                    updateSlots();
                    parentCtrl->relayout();
                }
                // closeContextMenu();
            }
        }
    };
    class guictr_dragged_modulation_src : public guitooltip<guictr_dragged_modulation_src> {
        DAW::automation_channel_ref ref;
    public:
        guictr_dragged_modulation_src() : guitooltip<guictr_dragged_modulation_src>(this) {
            this->guiType = gui_type::CTR_TYPE_MODULATION_DRAGGED;
            pos = { 0, 0 };
            setDragRendered(true);
        }
        void setChannelRef(const DAW::automation_channel_ref& _ref) {
            ref = _ref;
        }
        DAW::automation_channel_ref getChannelRef() const {
            return ref;
        }
        ~guictr_dragged_modulation_src() override = default;
        bool isDragMoveable() override {
            return true;
        }
        void renderDragged(NVGcontext* vg, ivec2 mousepos, ivec2 dragOffset) override;
        void handleDraggedRelease(MouseEvent& evt) override;
        void handleDraggedMove(MouseEvent& evt) override;
        void dragMoveOn(guibase* target, ivec2 mousepos) override;
        void dragReleaseOn(guibase* target, ivec2 mousepos) override;
    };

    void guictr_dragged_modulation_src::handleDraggedRelease(MouseEvent& evt) {
        dawCtrl->objectDragRelease(this, evt);
    }

    void guictr_dragged_modulation_src::handleDraggedMove(MouseEvent& evt) {
        dawCtrl->objectDragMove(this, evt);
    }

    void guictr_dragged_modulation_src::dragMoveOn(guibase* target, ivec2 mousepos) {
        target->modulationDragMove(this, toControlsObjectSpace(mousepos, target));
    }

    void guictr_dragged_modulation_src::dragReleaseOn(guibase* target, ivec2 mousepos) {
        log_printf("guictr_dragged_modulation_src drag on %s\n", StringAsCStr(target->getClassName()));
        target->modulationDragRelease(this, toControlsObjectSpace(mousepos, target));
    }

    void guictr_dragged_modulation_src::renderDragged(NVGcontext* vg, ivec2 mousepos, ivec2 dragOffset) {
        mousepos -= pos;
        mousepos += ivec2(20, 20);
        nvgTranslate(vg, mousepos.x, mousepos.y);
        auto iconS = ivec2(fontSize);
        NVGcolor color = theme->getColor(GuiColor::COL_KNOB_MODULATED);
        NVGcolor color2 = theme->getColor(GuiColor::COL_LABEL_ACTIVE);
        ivec2 bgCenter = pos + ivec2(0, size.y/2) - ivec2(iconS.x+INSET_TABLE, iconS.y/2);

        drawBackground(vg, theme, pos-ivec2(iconS.x+INSET_TABLE*2, 0), math::maxvec2(size, iconS)+ivec2(iconS.x+INSET_TABLE*2, 0), 0, false);
        drawTextureSymbol(vg, bgCenter, iconS, color, ICON_MODULATION, -1); 
        setFont(vg, fontSize, color2, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
        Table::DrawTableNVG(table, vg, theme, ivec2(INSET_TABLE), getSizeContent() - ivec2(INSET_TABLE << 1), fontSize);
        if (textField.isVisible()) {
            textField.render(vg);
        }
    }
    class guibutton_modulate : public guibutton, public IDraggedModulationSource {
        const DAW::automation_channel_ref ref;
        guictr_dragged_modulation_src dragged;
        bool hasDragged        = false;
        public:
        guibutton_modulate(DAW::automation_channel_ref ref) : guibutton(), ref(ref) {
            this->guiType = gui_type::CTR_TYPE_MODULATION_BUTTON;
            drawFn   = drawTextureSymbol;
            drawParm = ICON_MODULATION;
            dragged.setParent(this);
        }
        const DAW::automation_channel_ref& getChannelRef() const override {
            return ref;
        }
        void setControl(BaseCtrl* parentCtrl) override {
            guibase::setControl(parentCtrl);
            dragged.setControl(parentCtrl);
        }
        void handleDraggedMove(MouseEvent& evt) override {
            hasDragged = false;
            if (!hasDragged) {
                dragged.setChannelRef(ref);
                dragged.setLabel(StringFormat("Modulation Macro %d", ref.idx));
                dragged.pos = {};
                dragged.layout();
                dbgassert(dragged.isDragRendered());
                parentCtrl->setDragged(&dragged);
                hasDragged = true;
            }
            dawCtrl->objectDragMove(&dragged, evt);
        }
        void handleDraggedRelease(MouseEvent& evt) override {
                dbgassert(dragged.isDragRendered());
            if (hasDragged) {
                dawCtrl->objectDragRelease(&dragged, evt);
                return;
            }
            if (parent)
                parent->buttonClicked(this);
        }
    };
}
namespace DAW {
    void ConnectModulationInputChannel(automatable_t* dev, int32_t paramIdx, DAW::automation_channel_ref ref) {
        if (dev->isParamModulated(paramIdx)) {
            auto& inputs = dev->getModulations(paramIdx);
            for (auto input : inputs) {
                if (input->ref.refId == ref.ref.refId && input->ref.paramIdx == ref.ref.paramIdx) {
                    return;
                }
            }
        }
        auto inputRef = ref;
        inputRef.idx = paramIdx;
        inputRef.ref = ref.ref;
        dev->getModulations().push_back(inputRef);
        dev->updateModulationMap();
    }
    void DisonnectModulationForParam(automatable_t* dev, int32_t paramIdx) {
        auto& inputs = dev->getModulations();
        for (int i = 0; i < CtrSize(inputs); i++) {
            if (inputs[i].idx == paramIdx) {
                inputs.erase(inputs.begin() + i);
            }
        }
        dev->updateModulationMap();
    }
    void DisonnectModulationInputChannel(automatable_t* dev, DAW::automation_channel_ref ref) {
        auto& inputs = dev->getModulations();
        for (int i = 0; i < CtrSize(inputs); i++) {
            if (inputs[i].ref.refId == ref.ref.refId && inputs[i].ref.paramIdx == ref.ref.paramIdx) {
                inputs.erase(inputs.begin() + i);
            }
        }
    }
    void OpenModulationEditor(DawCtrl* dawCtrl, ivec2 mousePos, automatable_t* atl, int32_t paramIdx, DAW::automation_channel_ref ref) {
        dawCtrl->closeAllContextMenus();
        auto ctxtMenu = new DAW::UI::guictr_edit_modulation();
        ctxtMenu->size = {420, 420};
        ctxtMenu->pos = {0, 0};
        ctxtMenu->canTakeInputFocus = true;
        ctxtMenu->maxHeight = -1;
        ctxtMenu->setAutomationRef(dawCtrl->getDaw()->getPluginManager(), atl, paramIdx, ref);
        dawCtrl->openContextMenu(ctxtMenu, mousePos);
    }
}
template<>
void guitooltip<DAW::UI::guictr_dragged_modulation_src>::setContent() {
    table.tableWidth = 140;
    auto cell = Table::tblString{ptr->getTooltipText()};
    if (table.strW) {
        table.tableWidth = table.strW->getStringWidth(cell.str);
    }
    Table::tbl_row_t row{{std::move(cell)}};
    table.rows.push_back(std::move(row));
}

void guiknob::modulationDragMove(DAW::UI::guictr_dragged_modulation_src* g, ivec2 mousepos) {
    
}

void guiknob::modulationDragRelease(DAW::UI::guictr_dragged_modulation_src* g, ivec2 mousepos) {
    if (this->paramAutomatable) {
        DAW::ConnectModulationInputChannel(this->paramAutomatable, paramIdx, g->getChannelRef());
    }
}

GuiColor::constant_t gui_slider_textfield::getBackgroundColor() const {
    return gui_textfield::getBackgroundColor();
}
namespace DAW {
    bool IsSourceAndDest(){
        return false;
    }
}
bool gui_slider_textfield::isHighlighted() {
    if (!paramAutomatable) {
        return false;
    }
    auto dragged = dawCtrl->getDraggedModulation();
    if (dragged) {
        return true;
    }
    auto focused = dawCtrl->getFocusedModulation();
    if (focused) {
        auto ref = focused->getChannelRef();
        if (paramAutomatable->isParamConnectedTo(paramIdx, ref))
            return true;
    }
    return false;
}
bool guiknob::isHighlighted() {
    if (!paramAutomatable) {
        return false;
    }
    auto dragged = dawCtrl->getDraggedModulation();
    if (dragged) {
        return true;
    }
    auto focused = dawCtrl->getFocusedModulation();
    if (focused) {
        auto ref = focused->getChannelRef();
        if (paramAutomatable && paramAutomatable->isParamConnectedTo(paramIdx, ref))
            return true;
    }
    return false;
}


void gui_slider_textfield::modulationDragMove(DAW::UI::guictr_dragged_modulation_src* g, ivec2 mousepos) {
}

void gui_slider_textfield::modulationDragRelease(DAW::UI::guictr_dragged_modulation_src* g, ivec2 mousepos) {
    if (this->paramAutomatable) {
        DAW::ConnectModulationInputChannel(this->paramAutomatable, paramIdx, g->getChannelRef());
    }
}

namespace PluginMacros {
    constexpr int32_t NUM_MACROS = 12;
    constexpr int32_t PARAM_MACROS_FIRST = 16;
    constexpr int32_t BINARY_SNAPSHOT_VERSION = 1;

    class guictr_macro : public guictr_base {
        module_macros* const module;
        const int32_t idx;
        guiknob_pluginparam knob;
        DAW::UI::guibutton_modulate btnModulate;
    public:
        explicit guictr_macro(module_macros* module, int32_t idx, automatable_param_t* param) : guictr_base(),
            module(module),
            idx(idx),
            knob(param->idx, param->idx, guiknob::knobtype::SLIDER_LABELED),
            btnModulate(module->getModulationChannel(idx))
        {
            (void) this->module;
            (void) this->idx;
            padding = margin = 0;
            setBackgroundRendered(false);
            setCanMouseHit(false);
            add(&knob);
            add(&btnModulate);
        }
        ~guictr_macro() override {
            removeGuis();
        };
        void layout() override {
            auto cs = getSizeContent();
            float buttonHeight = 0.125f * cs.y;
            float knobHeight = cs.y - buttonHeight;
            knob.size = ivec2(cs.x, knobHeight);
            btnModulate.size = ivec2(cs.x, buttonHeight);
            btnModulate.pos = ivec2(0, knobHeight);
            guictr_base::layout();
        }
        void setEffectInstance(effectbase* _hostSidePlugin) {
            knob.setEffectInstance(_hostSidePlugin);
        }
        int32_t getParamIdx() const { return knob.getParamIdx(); }
        guiknob_pluginparam* getKnob() { return &knob; }
    };
    class guictr_module_macros : public guictr_base {
        effectbase* const module;
        std::vector<guictr_macro*> macroCtrs;
        gui_textfield editfield;
        int32_t numKnobs = 2;
        void init() {
            setLayoutMode(LAYOUT_HORIZONTAL);
            editfield.setFlag(FLG_NO_LAYOUT, true);
            setBackgroundRendered(true);
            setCanMouseHit(true);
            padding = 4;
            margin  = 2;
            editfield.setVisible(false);
            editfield.setAlignment(gui_textfield::Alignment::Center);
            editfield.setReturnCommits(true);
        }
    public:
        explicit guictr_module_macros(module_macros* module) : guictr_base(),
            module(module)
        {
            init();
            std::vector<automatable_param_t*> paramsSorted;
            module->getSortedParams(paramsSorted);
            erase_if(paramsSorted, [](const automatable_param_t* p) {
                return p->idx < PARAM_MACROS_FIRST || p->idx >= PARAM_MACROS_FIRST + NUM_MACROS;
            });
            macroCtrs.reserve(paramsSorted.size());
            for (automatable_param_t* param : paramsSorted) {
                macroCtrs.push_back(new guictr_macro(module, param->idx - PARAM_MACROS_FIRST, param));
                macroCtrs.back()->setVisible(CtrSize(macroCtrs) - 1 < numKnobs);
                add(macroCtrs.back());
            };
            add(&editfield);
        }
        bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
            return guictr_base::mouseHitTest(mpos, evt);
        }
        ~guictr_module_macros() override {
            removeGuis();
            for (auto* knob : macroCtrs) {
                delete knob;
            }
        }
        void setNumKnobs(int32_t num) {
            numKnobs = math::clamp(num, 0, CtrSize(macroCtrs));
            int32_t idx = 0;
            for (auto& knob : macroCtrs) {
                knob->setVisible(idx++ < num);
            }
        }
        void getSizeScale(int& w, int& h) const {
            w = 100*numKnobs;
            h = 300;
        }

        void layoutEntries(ivec2 dir) override {
            guictr_base::layoutEntries(dir);
        }
        void buttonClicked(guibase* button) override {
            auto param = dynamic_cast<guiknob_pluginparam*>(button);
            if (param && module) {
                auto paramIdx = param->getParamIdx();
                auto paramValue = module->getParamValueDisplay(paramIdx);
                editfield.mCallbackEnd = [this, param, paramValue, paramIdx](const std::string& str) {
                    auto paramConverted = module->convertParamValueDisplay(param->getParamIdx(), param_unit_t{str, paramValue.unit});
                    if (paramConverted.success) {
                        module->setParamValue(paramIdx, paramConverted.floatVal, FLG_PAR_UPDATE_USER | FLG_PAR_UPDATE_FINISH);
                        if (param->fnValueEditChanged)
                            param->fnValueEditChanged(param->getValue(), paramConverted.floatVal);
                    }
                    editfield.setVisible(false);
                    return true;
                };
                auto layout = param->getLayout();
                editfield.pos = layout.pValue;
                editfield.size = layout.sValue;
                editfield.setVisible(true);
                editfield.layout();
                editfield.setValue(paramValue.value);
                editfield.setSelectionRange(-1, -1);
                editfield.setFontSize(layout.valueHeight*layout.fontScaleValue);
                parentCtrl->focusGui(&editfield);
                return;
            }
            guictr_base::buttonClicked(button);
        }

        void onGuiOpen() {
            for (auto knob : macroCtrs) {
                knob->setEffectInstance(module);
            }
        }

        void onGuiClose() {
            for (auto knob : macroCtrs) {
                knob->setEffectInstance(nullptr);
            }
        }

        guiknob_pluginparam* getKnobFromParameter(int32_t index) {
            auto it = std::find_if(macroCtrs.begin(), macroCtrs.end(), [index](auto* knob) {
                return knob->getParamIdx() == index;
            });
            auto* macro = it != macroCtrs.end() ? *it : nullptr;
            return macro ? macro->getKnob() : nullptr;
        }

        void onSetParameter(int32_t index, float value) {
        }

        void setUiLayout(const ui_layout_t& layout) {
            setNumKnobs(layout.numActive);
        }

        bool getUiLayout(ui_layout_t& layout) const {
            layout.numActive = numKnobs;
            return true;
        }
    };

    struct module_macros::macro_impl_t {
        struct macro_automation_src_param_t : public automated_param_t {
            module_macros* module = nullptr;
            bool isActive() const override { //??
                return true;
            }
            bool isAutomated() const override { //??
                return true; 
            }
            float getValueAt(tick_t tick) const override {
                return module->getParamValue(PARAM_MACROS_FIRST + paramIdx);
            }
            float getValueAtExact(double dTick) const override {
                return module->getParamValue(PARAM_MACROS_FIRST + paramIdx);
            }
            String getName() const override {
                return StringFormat("Macro %d", paramIdx+1);
            }
            float modulateValue(tick_t tick, float f, const DAW::automation_scaling_t& scale) const override {
                auto f1 = getValueAt(tick);
                return scale.min + f1 * (scale.max - scale.min);
            }
            void sampleAutomation(double dTickBegin, double dTickEnd, samplecount_t numSamples, const DAW::automation_scaling_t& scale, float* inOut) const override {
                float valFixed = module->getParamValue(PARAM_MACROS_FIRST + paramIdx);
                std::fill(inOut, inOut + numSamples, scale.min + valFixed * (scale.max - scale.min));
            }
            void setRange(tick_t tickBegin, tick_t tickEnd, std::vector<automation_point_t>& data) override {
            }
            void copyRange(tick_t tickBegin, tick_t tickEnd, std::vector<automation_point_t>& data) const override {
            }
        };
        module_macros* module;
        std::array<macro_automation_src_param_t, NUM_MACROS> macroAutomationSrcParams;
        const macro_automation_src_param_t* getModulationOutputData(const DAW::automation_channel_ref& channel) {
            auto chIdx = channel.ref.paramIdx;
            if (!assert_expr(chIdx >= 0 && chIdx < NUM_MACROS))
                return nullptr;
            if (!assert_expr(chIdx < CtrSize(macroAutomationSrcParams)))
                return nullptr;
            return &macroAutomationSrcParams[chIdx];
        }
        //TODO: handle disconnect
    };
    module_macros::module_macros(int32_t _projectGlobalId, IHostCallback* _hostCallback)
        : internal_automator("Macros", getModuleType(), _projectGlobalId, _hostCallback),
        impl(new macro_impl_t{ this, { } })
    {
        for (int32_t i = 0; i < NUM_MACROS; ++i) {
            impl->macroAutomationSrcParams[i].module = this;
            impl->macroAutomationSrcParams[i].paramIdx = i;
            auto paramIdx = PARAM_MACROS_FIRST + i;
            automatable_param_t* regparam = registerParam(paramIdx);
            regparam->defaultValue = 0.0f;
            regparam->value = 0.0f;
            regparam->name  = StringFormat("Macro %d", i + 1);
            regparam->shortLabel  = StringFormat("Macro %d", i + 1);
            regparam->unit  = "%";
        }
    }
    module_macros::~module_macros() {
        delete impl;
    }
    const automated_param_t* module_macros::getModulationOutputData(const DAW::automation_channel_ref& channel) {
        return impl->getModulationOutputData(channel);
    }
    using ViewCtrType = SinglePluginViewContainers<guictr_module_macros, module_macros>;
    std::shared_ptr<PluginViewContainers> module_macros::createViewCtrInternal() {
        return std::make_shared<ViewCtrType>(this, 100, 150);
    }

    std::shared_ptr<std::vector<std::byte>> serializeSnapshot(const snapshot_t& snapshot) {
        dbgassert(snapshot.version == BINARY_SNAPSHOT_VERSION);
        auto shrdHeapVec = std::make_shared<std::vector<std::byte>>();
        shrdHeapVec->resize(256);
        DAW::ByteBuffer::stream_write<std::vector<std::byte>> out{*shrdHeapVec, 0};
        out.write(size_t(0));
        out.write(snapshot.version);
        out.write(size_t{snapshot.uiLayout.size()});
        for (const auto& modulation : snapshot.uiLayout) {
            out.write(modulation.uiId);
            out.write(modulation.numActive);
        }
        out.setPos(0);
        out.write(size_t(shrdHeapVec->size()));
        return shrdHeapVec;
    }
    bool deserializeSnapshot(const std::shared_ptr<std::vector<std::byte>>& data, snapshot_t& snapshotOut) {
        if (!data)
            return false;
        DAW::ByteBuffer::stream_read in(*data);
        snapshot_t snapshot;
        size_t dataSize = data->size();
        size_t dataSizeHdr = 0;
        if (!in.read(dataSizeHdr))
            return false;
        if (dataSizeHdr > dataSize)
            return false;
        in.read(snapshot.version);
        // if (snapshot.version < MINIMUM_VERSION)
        //     return false;
        if (snapshot.version > BINARY_SNAPSHOT_VERSION)
            return false;
        size_t numUiLayouts = 0;
        if (!in.read(numUiLayouts) || numUiLayouts > 1000)
            return false;
        snapshot.uiLayout.resize(numUiLayouts);

        for (auto& modulation : snapshot.uiLayout) {
            if (!in.read(modulation.uiId))
                return false;
            if (!in.read(modulation.numActive))
                return false;
        }
        snapshotOut = std::move(snapshot);
        return true;
    }

    std::shared_ptr<std::vector<std::byte>> module_macros::storePresetData() {
        snapshot_t snapshot;
        snapshot.version = BINARY_SNAPSHOT_VERSION;
        getUiSnapshot(snapshot);
        return serializeSnapshot(snapshot);
    }
    bool module_macros::loadPresetData(const std::shared_ptr<std::vector<std::byte>>& buf) {
        if (buf->size() > 0) {
            snapshot_t snapshotLoaded;
            if (deserializeSnapshot(buf, snapshotLoaded)) {
                setUiSnapshot(snapshotLoaded);
                return true;
            }
        }
        return false;
    }

    void module_macros::getUiSnapshot(snapshot_t& snapshot) {
        for (auto& view : views) {
            auto implCtrType = dynamic_cast<ViewCtrType*>(view.get());
            ui_layout_t layout{};
            if (implCtrType && implCtrType->getPluginUI().getUiLayout(layout)) {
                layout.uiId = view->getUiId();
                // see if snapshot is already there
                auto it = std::find_if(snapshot.uiLayout.begin(), snapshot.uiLayout.end(), [&](const ui_layout_t& layout) {
                    return layout.uiId == view->getUiId();
                });
                if (it != snapshot.uiLayout.end()) {
                    *it = layout;
                } else {
                    snapshot.uiLayout.push_back(layout);
                }
            }
        }
    }

    void module_macros::setUiSnapshot(snapshot_t& snapshot) {
        for (auto& uis : snapshot.uiLayout) {
            std::vector<std::shared_ptr<PluginViewContainers>> views;
            getAllViewCtrs(uis.uiId, views);
            for (auto& view : views) {
                auto implCtrType = dynamic_cast<ViewCtrType*>(view.get());
                if (implCtrType) {
                    implCtrType->getPluginUI().setUiLayout(uis);
                }
            }
        }
    }
}// namespace PluginMacros

template<>
effectbase* makeInstance<PluginMacros::module_macros>(int32_t _projectGlobalId, IHostCallback* _hostCallback) {
    return new PluginMacros::module_macros(_projectGlobalId, _hostCallback);
}

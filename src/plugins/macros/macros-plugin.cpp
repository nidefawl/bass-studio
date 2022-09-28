#include "macros-plugin.h"
#include "automation.h"
#include "gui/container/container.h"
#include "gui/controls/knobpluginparam.h"
#include "guiconstant.h"
#include "host/mainctrl.h"
#include "logging.h"
#include "math/seq_math.h"
#include "renderresources.h"
#include "seq_util.h"

namespace DAW::UI {
    class guictr_dragged_modulation_src : public guictr_base {
        const int HEIGHT_ENTRY = 20;
        DAW::automation_channel_ref ref;
    public:
        Table::tbl table;
        guictr_dragged_modulation_src() : guictr_base(gui_type::CTR_TYPE_PLUGINS_DRAGGED) {
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
        void layout() override {
        }
        bool isDragMoveable() override {
            return true;
        }
        void renderDragged(NVGcontext* vg, ivec2 mousepos, ivec2 dragOffset) override;
        void setStrings(std::vector<String>& list);
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
        //        mousepos += dragOffset;
        mousepos -= pos;
        mousepos.x -= size.x / 2;
        nvgTranslate(vg, mousepos.x, mousepos.y);
        drawBackground(vg, theme, pos, size, 0, false);
        ivec2 inset = { 2, 2 };
        UIFont::font_instance instance = theme->getFont(UIFont::FONT_DEFAULT);
        UIFont::bindFont(vg, instance);
        nvgFillColor(vg, THEMECOL_TEXT);
        Table::DrawTableNVG(this->table, vg, theme, pos + inset, size - inset * 2, HEIGHT_ENTRY - 4);
    }
    void guictr_dragged_modulation_src::setStrings(std::vector<String>& list) {
        table.tableWidth  = 200 - (INSET_TABLE<<1);
        table.titleHeight = HEIGHT_ENTRY;
        table.rowHeight   = HEIGHT_ENTRY;
        table.rows.clear();
        for (String& s : list) {
            Table::tbl_row_t row;
            row.cols.push_back(s);
            table.rows.push_back(row);
        }
        Table::AdjustColSizes(table);
        size = ivec2(table.tableWidth, table.rows.size() * table.rowHeight) + ivec2(INSET_TABLE << 1);
    }

    class guibutton_modulate : public guibutton {
        DAW::automation_channel_ref const ref;
        guictr_dragged_modulation_src dragged;
        bool hasDragged        = false;
        public:
        guibutton_modulate(DAW::automation_channel_ref ref) : guibutton(), ref(ref) {
            drawFn   = drawTextureSymbol;
            drawParm = ICON_MODULATION;
            dragged.setParent(this);
        }
        void setControl(BaseCtrl* parentCtrl) override {
            guibase::setControl(parentCtrl);
            dragged.setControl(parentCtrl);
        }
        void handleDraggedMove(MouseEvent& evt) override {
            hasDragged = false;
            if (!hasDragged) {
                dragged.setChannelRef(ref);
                std::vector<String> list;
                list.emplace_back("Modulation");
                dragged.setStrings(list);
                dragged.pos = {};
                parentCtrl->setDragged(&dragged);
                hasDragged = true;
            }
            dawCtrl->objectDragMove(&dragged, evt);
        }
        void handleDraggedRelease(MouseEvent& evt) override {
            if (hasDragged) {
                dawCtrl->objectDragRelease(&dragged, evt);
                return;
            }
            if (parent)
                parent->buttonClicked(this);
            // if (isSelected()) {
            //     static_cast<guictr_plugins*>(this->parent)->onSelected(evt, this);
            // }
        }
    };
}
namespace DAW {
    void ConnectModulationInputChannel(const Host::PluginManager* const host, automatable_t* dev, int32_t paramIdx, DAW::automation_channel_ref ref) {
        std::vector<DAW::automation_channel_ref>& inputs = dev->inputChannelsAutomation;
        bool bFound = false;
        for (DAW::automation_channel_ref& input : inputs) {
            if (input.idx == paramIdx) {
                input.ref = ref.ref;
                bFound    = true;
            }
        }
        if (!bFound) {
            DAW::automation_channel_ref inputRef = ref;
            inputRef.idx = paramIdx;
            inputRef.ref = ref.ref;
            inputs.push_back(inputRef);
        }
    }
}
void guiknob_pluginparam::modulationDragMove(DAW::UI::guictr_dragged_modulation_src* g, ivec2 mousepos) {
    
}
void guiknob_pluginparam::modulationDragRelease(DAW::UI::guictr_dragged_modulation_src* g, ivec2 mousepos) {
    if (hostSidePlugin) {
        DAW::ConnectModulationInputChannel(nullptr, hostSidePlugin, paramIdx, g->getChannelRef());
    }
}

namespace PluginMacros {
    static constexpr int32_t PARAM_NUM_MACROS = 32;
    static constexpr int32_t PARAM_MACROS_FIRST = 16;
    class guictr_macro : public guictr_base {
        module_macros* const module;
        guiknob_pluginparam knob;
        DAW::UI::guibutton_modulate btnModulate;
    public:
        explicit guictr_macro(module_macros* module, automatable_param_t* param) : guictr_base(),
            module(module),
            knob(param->idx, param->idx, guiknob::knobtype::SLIDER_LABELED),
            btnModulate(module->getModulationChannel(param->idx))
        {
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
            float buttonHeight = 0.25f * cs.y;
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
        int32_t numKnobs = 4;
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
                return p->idx == PARAM_ENABLE;
            });
            macroCtrs.reserve(paramsSorted.size());
            for (automatable_param_t* param : paramsSorted) {
                macroCtrs.push_back(new guictr_macro(module, param));
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
            numKnobs = num;
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
    };

    struct module_macros::macro_impl_t {
        struct macro_automation_src_param_t : public automated_param_t {
            module_macros* module;
            bool isActive() const override { //??
                return true;
            }
            bool isAutomated() const override { //??
                return true; 
            }
            float getValueAt(tick_t tick) const override {
                return module->getParamValue(paramIdx);
            }
            float getValueAtExact(double dTick) const override {
                return module->getParamValue(math::floorfS32(paramIdx));
            }
        };
        std::array<macro_automation_src_param_t, PARAM_NUM_MACROS> macroAutomationSrcParams;
        const macro_automation_src_param_t* getModulationOutputData(int32_t channel) const {
            dbgassert(channel >= 0 && channel < PARAM_NUM_MACROS);
            dbgassert(channel < CtrSize(macroAutomationSrcParams));
            return &macroAutomationSrcParams[channel % macroAutomationSrcParams.size()];
        }
    };
    module_macros::module_macros(int32_t _projectGlobalId, IHostCallback* _hostCallback)
        : internal_automator("Macros", getModuleType(), _projectGlobalId, _hostCallback),
        impl(new macro_impl_t{})
    {
        for (int32_t i = 0; i < PARAM_NUM_MACROS; ++i) {
            impl->macroAutomationSrcParams[i].module = this;
            impl->macroAutomationSrcParams[i].paramIdx = i;
            auto paramIdx = PARAM_MACROS_FIRST + i;
            automatable_param_t* regparam = registerParam(paramIdx);
            regparam->defaultValue = 0.0f;
            regparam->value = 0.0f;
            regparam->name  = StringFormat("Macro %d", i);
            regparam->shortLabel  = StringFormat("Macro %d", i);
            regparam->unit  = "%";
        }
    }
    module_macros::~module_macros() {
        delete impl;
    }
    const automated_param_t* module_macros::getModulationOutputData(int32_t channel) const {
        return impl->getModulationOutputData(channel);
    }
    std::shared_ptr<PluginViewContainers> module_macros::createViewCtrInternal() {
        return std::make_shared<SinglePluginViewContainers<guictr_module_macros, module_macros>>(this, 100, 150);
    }
}// namespace PluginMacros

template<>
effectbase* makeInstance<PluginMacros::module_macros>(int32_t _projectGlobalId, IHostCallback* _hostCallback) {
    return new PluginMacros::module_macros(_projectGlobalId, _hostCallback);
}

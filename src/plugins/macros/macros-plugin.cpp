#include "macros-plugin.h"
#include "automation.h"
#include "gui/container/container.h"
#include "gui/controls/knobpluginparam.h"
#include "gui/gui.h"
#include "gui/tooltip/tooltip.h"
#include "guicolors.h"
#include "guiconstant.h"
#include "host/mainctrl.h"
#include "logging.h"
#include "math/seq_math.h"
#include "renderresources.h"
#include "seq_util.h"

#include "str_util.h"
#include "logging.h"
#include "byte-buffer.h"
#include <array>
#include <cstdint>
#include <utility>

namespace DAW::UI {
    class guictr_dragged_modulation_src : public guitooltip<guictr_dragged_modulation_src> {
        const int HEIGHT_ENTRY = 20;
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

    class guibutton_modulate : public guibutton {
        DAW::automation_channel_ref const ref;
        guictr_dragged_modulation_src dragged;
        bool hasDragged        = false;
        public:
        guibutton_modulate(DAW::automation_channel_ref ref) : guibutton(), ref(ref) {
            this->guiType = gui_type::CTR_TYPE_MODULATION_BUTTON;
            drawFn   = drawTextureSymbol;
            drawParm = ICON_MODULATION;
            dragged.setParent(this);
        }
        DAW::automation_channel_ref getChannelRef() const {
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
        auto& inputs = dev->inputChannelsAutomation;
        bool bFound = false;
        for (auto& input : inputs) {
            if (input.idx == paramIdx) {
                input.ref = ref.ref;
                bFound    = true;
            }
        }
        if (!bFound) {
            auto inputRef = ref;
            inputRef.idx = paramIdx;
            inputRef.ref = ref.ref;
            inputs.push_back(inputRef);
        }
    }
    void DisonnectModulationInputChannel(automatable_t* dev, int32_t paramIdx) {
        auto& inputs = dev->inputChannelsAutomation;
        for (int i = 0; i < CtrSize(inputs); i++) {
            if (inputs[i].idx == paramIdx) {
                inputs.erase(inputs.begin() + i);
            }
        }
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

bool guiknob::isHighlighted() {
    using DAW::UI::guictr_dragged_modulation_src;
    if (parentCtrl->guiOver == this && paramAutomatable && parentCtrl->guiDragged && parentCtrl->guiDragged->getGuiType() == gui_type::CTR_TYPE_MODULATION_DRAGGED) {
        return true;
    }
    if (paramAutomatable && parentCtrl->guiOver && parentCtrl->guiOver->getGuiType() == gui_type::CTR_TYPE_MODULATION_BUTTON) {
        auto ref = static_cast<DAW::UI::guibutton_modulate*>(parentCtrl->guiOver)->getChannelRef();
        for (auto& input : paramAutomatable->inputChannelsAutomation) {
            if (input.idx == paramIdx) {
                if (input.ref.refId == ref.ref.refId && input.ref.paramIdx == ref.ref.paramIdx) {
                    return true;
                }
            }
        }
    }
    return false;
}
void guiknob::modulationDragMove(DAW::UI::guictr_dragged_modulation_src* g, ivec2 mousepos) {
    
}
void guiknob::modulationDragRelease(DAW::UI::guictr_dragged_modulation_src* g, ivec2 mousepos) {
    if (this->paramAutomatable) {
        DAW::ConnectModulationInputChannel(this->paramAutomatable, paramIdx, g->getChannelRef());
    }
}

namespace PluginMacros {
    constexpr int32_t PARAM_NUM_MACROS = 12;
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
                return p->idx < PARAM_MACROS_FIRST || p->idx >= PARAM_MACROS_FIRST + PARAM_NUM_MACROS;
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
            module_macros* module;
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

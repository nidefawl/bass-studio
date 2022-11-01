#include "macros-plugin.h"
#include "assert_dbg.h"
#include "host/automation/automation.h"
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
#include "host/daw/mainctrl.h"
#include "logging.h"
#include "math/seq_math.h"
#include "plugins/macros/macros-plugin.h"
#include "renderresources.h"
#include "seq_util.h"

#include "str_util.h"
#include "logging.h"
#include "byte-buffer.h"
#include <array>
#include <utility>

namespace PluginMacros {
    constexpr int32_t NUM_MACROS = 12;
    constexpr int32_t PARAM_MACROS_FIRST = 16;
    constexpr int32_t BINARY_SNAPSHOT_VERSION = 1;

    class guictr_macro : public guictr_base {
        module_macros* const module;
        const int32_t idx;
        guiknob_pluginparam knob;
        DAW::UI::Modulation::guibutton_modulate btnModulate;
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
        module_macros* const module;
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
            w = 80*numKnobs;
            h = 300;
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

        class ctxtmenu_macro_count : public ctxtmenu_entry {

            struct _time_sel_entry {
                int id;
                int x;
                int y;
                int w;
                String name;
            };
            std::vector<_time_sel_entry> entries;

        public:
            const int pad   = 10;
            const int inset = 5;
        public:
            ctxtmenu_macro_count(String _title, int _id)
                : ctxtmenu_entry(std::move(_title), _id)
            {
                entries.push_back({ 2, 0, 0, 0, "2" });
                entries.push_back({ 4, 0, 0, 0, "4" });
                entries.push_back({ 8, 0, 0, 0, "8" });
            }

            void layout(ivec2 size, float _fontSize, determine_string_width& strw) override {
                width = size.x;
                this->fontSize = _fontSize;
                const int h    = math::roundfS32(_fontSize);
                layoutE(width, h, 3);
            }

            void layoutE(int tw, int h, int perRow) {
                int iX      = inset;
                int iY      = h + 2;
                int elW     = (tw - inset * 2) / perRow;
                for (_time_sel_entry& e : entries) {
                    this->height = iY + h;
                    e.x = iX;
                    e.y = iY;
                    e.w = elW;
                    iX += e.w;
                    if (iX >= tw - inset * 2) {
                        iX = inset;
                        iY += h;
                    }
                }
            }


            void render(ivec2, NVGcontext* vg, int, ivec2 mouse) override {
                auto h = fontSize * 1.1f;

                for (_time_sel_entry& e : entries) {
                    if (mouse.y >= y + e.y && mouse.y < y + e.y + h && mouse.x >= e.x && mouse.x < e.x + e.w) {
                        nvgBeginPath(vg);
                        nvgRect(vg, e.x, y + e.y + 2, e.w, h - 4);
                        nvgFillColor(vg, theme->getColor(GuiColor::COL_CTXTMNU_HILIGHT));
                        nvgFill(vg);
                    }
                }

                renderTextLabel(vg,
                                vec2(leftOffset(), y + h * 0.5f),
                                vec2(width, h),
                                title,
                                theme,
                                fontSize,
                                theme->getColor(GuiColor::COL_TEXT),
                                NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

                for (_time_sel_entry& e : entries) {
                    renderTextLabel(vg,
                                    vec2(e.x + 20.0f, y + e.y + h * 0.5f),
                                    vec2(width, h),
                                    e.name,
                                    theme,
                                    fontSize * 0.9f,
                                    theme->getColor(GuiColor::COL_TEXT),
                                    NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
                }
            }

            bool contains(ivec2& ctxtSize, ivec2& mouse) const override {
                return mouse.y >= y && mouse.y < y + height && mouse.x >= 0 && mouse.x < ctxtSize.x;
            }

            int getClicked(ivec2& ctxtSize, ivec2& mouse) override {
                if (contains(ctxtSize, mouse)) {
                    const auto h = this->fontSize;
                    for (_time_sel_entry& e : entries) {
                        if (mouse.y >= y + e.y && mouse.y < y + e.y + h && mouse.x >= 0 && mouse.x < e.x + e.w) {
                            return 100 + e.id;
                        }
                    }
                }
                return -1;
            }
        };
        class guictr_module_macros_context_menu : public guictxtmenu {
            module_macros* const module;
            guictr_module_macros* const ctr;
        public:
            explicit guictr_module_macros_context_menu(guictr_module_macros* _ctr, module_macros* _module)
            : guictxtmenu(), module(_module), ctr(_ctr) {
                (void) module;
                this->size.x   = 220;
                maxHeight = 0;
                this->fontSize = FONT_SIZE_CTXT_SMALL;
                this->paddingV = 0;
                addEntry(new ctxtmenu_macro_count("Macros", 0));
            }
            bool clickedElement(ctxtmenu_entry* e, int _id) override {
                if (_id >= 100) {
                    ctr->setNumKnobs(_id-100);
                    ctr->parent->onChildLayoutChanged(ctr);
                }
                closeContextMenu();
                return true;
            }
        };

        void rightClicked(MouseEvent& evt, guibase* what) override {
            parentCtrl->openContextMenu(new guictr_module_macros_context_menu(this, this->module), evt.mousepos);
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
            float modulateValue(tick_t tick, float fIn, const DAW::modulation_scaling_t& scale) const override {
                const auto valScaled = scale.min + module->getParamValue(PARAM_MACROS_FIRST + paramIdx) * (scale.max - scale.min);
                switch (scale.mode) {
                    case DAW::ModulationMode::ADD:
                        fIn += valScaled;
                        break;
                    case DAW::ModulationMode::MUL:
                        fIn *= valScaled;
                        break;
                    case DAW::ModulationMode::REPLACE:
                        fIn = valScaled;
                        break;
                    default:
                        break;
                }
                if (scale.bClamp) {
                    fIn = math::clamp(fIn, 0.0f, 1.0f);
                }
                return fIn;
            }
            void sampleAutomation(double dTickBegin, double dTickEnd, samplecount_t numSamples, const DAW::modulation_scaling_t& scale, float* inOut) const override {
                const auto valScaled = scale.min + module->getParamValue(PARAM_MACROS_FIRST + paramIdx) * (scale.max - scale.min);
                if (scale.mode == DAW::ModulationMode::REPLACE) {
                    std::fill(inOut, inOut + numSamples, valScaled);
                } else {
                    for (samplecount_t i = 0; i < numSamples; ++i) {
                        switch (scale.mode) {
                            case DAW::ModulationMode::ADD:
                                inOut[i] += valScaled;
                                break;
                            case DAW::ModulationMode::MUL:
                                inOut[i] *= valScaled;
                                break;
                            case DAW::ModulationMode::REPLACE:
                                inOut[i] = valScaled;
                                break;
                            default:
                                break;
                        }
                        if (scale.bClamp) {
                            *inOut = math::clamp(*inOut, 0.0f, 1.0f);
                        }
                    }
                }
            }
            void setRange(tick_t tickBegin, tick_t tickEnd, std::vector<automation_point_t>& data) override {
            }
            void copyRange(tick_t tickBegin, tick_t tickEnd, std::vector<automation_point_t>& data) const override {
            }
        };
        module_macros* module{};
        std::array<macro_automation_src_param_t, NUM_MACROS> macroAutomationSrcParams;
        const macro_automation_src_param_t* getModulationOutputData(const DAW::modulation_channel_ref& channel) {
            auto chIdx = channel.refSrc.paramIdx;
            if (!assert_expr(chIdx >= 0 && chIdx < NUM_MACROS))
                return nullptr;
            if (!assert_expr(chIdx < CtrSize(macroAutomationSrcParams)))
                return nullptr;
            return &macroAutomationSrcParams[chIdx];
        }
        //TODO: handle disconnect
    };
    void module_macros::initModChannels() {
        outputModChannelsDesc.clear();
        outputModChannelsDesc.reserve(NUM_MACROS);
        for (channelnum_t i = 0; i < channelnum_t(NUM_MACROS); ++i) {
            impl->macroAutomationSrcParams[i].module = this;
            impl->macroAutomationSrcParams[i].paramIdx = i;
            auto paramIdx = PARAM_MACROS_FIRST + i;
            automatable_param_t* regparam = registerParam(paramIdx);
            regparam->setInitial(0.0f);
            regparam->name  = StringFormat("Macro %d", i + 1);
            regparam->shortLabel  = StringFormat("Macro %d", i + 1);
            regparam->unit  = "%";
            outputModChannelsDesc.push_back({i, regparam->name});
        }
    }
    module_macros::module_macros(int32_t _projectGlobalId, IHostCallback* _hostCallback)
        : internal_modulator("Macros", getModuleType(), _projectGlobalId, _hostCallback),
        impl(new macro_impl_t{ this, { } })
    {
        initModChannels();
    }
    module_macros::~module_macros() {
        delete impl;
    }
    const automated_param_t* module_macros::getModulationOutputData(const DAW::modulation_channel_ref& channel) {
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

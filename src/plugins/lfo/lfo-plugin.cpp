#include "lfo-types.hpp"
#include "lfo-plugin.h"
#include "lfo-snapshot.hpp"
#include "lfo-ui.hpp"
#include "assert_dbg.h"
#include "host/automation/automation.h"
#include "event.h"
#include "file/shapefile.h"
#include "gui/automation/modulation.h"
#include "gui/container/container.h"
#include "gui/controls/button.h"
#include "gui/controls/inputfield.h"
#include "gui/controls/knob.h"
#include "gui/controls/knobpluginparam.h"
#include "gui/gui.h"
#include "gui/plugin/plugin.h"
#include "gui/shape/shapeeditor.h"
#include "gui/tooltip/tooltip.h"
#include "gui/views/controls.h"
#include "guicolors.h"
#include "guiconstant.h"
#include "host/host.h"
#include "host/host_pluginmanager.h"
#include "host/daw/mainctrl.h"
#include "logging.h"
#include "math/seq_math.h"
#include "rand.h"
#include "renderresources.h"
#include "seq_time.h"
#include "seq_util.h"
#include "host/plugin/plugin-lockable.h"
#include "host/shape/shape.h"
#include "str_util.h"
#include "byte-buffer.h"
#include "types.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <nanovg.h>
#include <utility>
#include <vector>

namespace PluginLFO {
    using namespace DAW::LFO;
    constexpr int32_t NUM_CHANNELS = 1;
    constexpr int32_t BINARY_SNAPSHOT_VERSION = 4;
    constexpr int32_t PARAM_LFO_RATE = 16;
    constexpr int32_t PARAM_LFO_PHASE = 17;
    constexpr int32_t PARAM_LFO_MINIMUM = 18;
    constexpr int32_t PARAM_LFO_MAXIMUM = 19;
    constexpr int32_t PARAM_LFO_PHASE_RESET_TICKS = 20;

    DAW_CXX_CONSTINIT thread_local std::vector<const LFORateMinMaxAutomation*> stack;
    class LFOAutomation : public LFORateMinMaxAutomation {
    public:
        internal_modulator* module = nullptr;
        std::pair<float, float> getMinMax(double dTick) const override {
            auto host = module->getPluginManager();
            if (!host || stl_contains(stack, this)) {
                return {
                    module->getParamValue(PARAM_LFO_MINIMUM) * 2.0f - 1.0f,
                    module->getParamValue(PARAM_LFO_MAXIMUM) * 2.0f - 1.0f
                };
            }
            stack.push_back(this);
            auto state = module->hostCallback->m_playbackState;
            auto valMin = module->getModulatedParameterAt(host, PARAM_LFO_MINIMUM, dTick, state) * 2.0f - 1.0f;
            auto valMax = module->getModulatedParameterAt(host, PARAM_LFO_MAXIMUM, dTick, state) * 2.0f - 1.0f;
            stack.pop_back();
            return { valMin, valMax };
        }
        std::tuple<float, float, float> getRatePhase(double dTick) const override {
            auto host = module->getPluginManager();
            if (!host || stl_contains(stack, this)) {
                return { module->getParamValue(PARAM_LFO_RATE), module->getParamValue(PARAM_LFO_PHASE), module->getParamValue(PARAM_LFO_PHASE_RESET_TICKS) };
            }
            stack.push_back(this);
            auto state = module->hostCallback->m_playbackState;
            auto valRate = module->getModulatedParameterAt(host, PARAM_LFO_RATE, dTick, state);
            auto valPhase = module->getModulatedParameterAt(host, PARAM_LFO_PHASE, dTick, state);
            auto valResetTicks = module->getModulatedParameterAt(host, PARAM_LFO_PHASE_RESET_TICKS, dTick, state);
            stack.pop_back();
            return { valRate, valPhase, valResetTicks };
        }
    };

    struct module_lfo::lfo_impl_t final : public PluginLockable {
        struct lfo_channel_t : public LFOSyncParameters {
            bool modeIsShape = true;
            DAW::Shape::shape_t shape{};
            lfo_automation_src_synced_t srcSync;
            std::shared_ptr<lfo_automation_src_random_t> srcRand;
        };
        module_lfo* const module;
        std::array<lfo_channel_t, NUM_CHANNELS> channels;
        LFOAutomation rateMinMax;
        explicit lfo_impl_t(DawInstance* _daw, module_lfo* _module, const DAW::Shape::shape_t& initShape) 
            : PluginLockable(_daw),
            module(_module)
        {
            rateMinMax.module = module;
            for (auto& channel : channels) {
                channel.shape = initShape;
                channel.syncFlags = STRAIGHT | DOTTED | TRIPLET;
                channel.syncRatios = GetSyncRatios(channel.syncFlags);
                channel.srcSync.rateMinMax = &rateMinMax;
                channel.srcSync.sync = &channel;
                channel.srcSync.shape = &channel.shape;
                channel.modeIsShape = true;
            }
            for (int32_t idx = 0; idx < CtrSize(channels); ++idx) {
                setRandomMode(idx, 0);
                channels[idx].modeIsShape = true;
            }
        }
        void setRandomMode(int32_t chIdx, int32_t mode) {
            auto& channel = channels[chIdx];
            channel.modeIsShape = false;
            switch (mode) {
                case -1:
                    if (channel.srcRand) {
                        break;
                    }
                    [[fallthrough]];
                default:
                case 0:
                    channel.srcRand = std::make_shared<lfo_automation_src_random_smooth_t>();
                    break;
                case 1:
                    channel.srcRand = std::make_shared<lfo_automation_src_random_linear_t>();
                    break;
                case 2:
                    channel.srcRand = std::make_shared<lfo_automation_src_random_exp_t>();
                    break;
                case 3:
                    channel.srcRand = std::make_shared<lfo_automation_src_random_sample_and_hold_t>();
                    break;
            }
            channel.srcRand->rateMinMax = &rateMinMax;
            channel.srcRand->sync = &channel;
        }
        int32_t getRandomMode(int32_t chIdx) const {
            return channels[chIdx].srcRand->getModeId();
        }
        const automated_param_t* getModulationOutputData(const DAW::modulation_channel_ref& channel) {
            auto chIdx = channel.refSrc.paramIdx;
            if (!assert_expr(chIdx >= 0 && chIdx < NUM_CHANNELS))
                return nullptr;
            if (!assert_expr(chIdx < CtrSize(channels)))
                return nullptr;
            auto& ch = channels[chIdx];
            automated_param_t* src = &ch.srcSync;
            if (!ch.modeIsShape && ch.srcRand) {
                src = ch.srcRand.get();
            }
            return src;
        }
        DAW::Shape::shape_t& getShape(int32_t chIdx) {
            static DAW::Shape::shape_t shapeDummy{};
            if (!assert_expr(chIdx >= 0 && chIdx < NUM_CHANNELS))
                return shapeDummy;
            if (!assert_expr(chIdx < CtrSize(channels)))
                return shapeDummy;
            return channels[chIdx].shape;
        }
        int32_t getSyncFlags(int32_t chIdx) const {
            dbgassert(chIdx >= 0 && chIdx < NUM_CHANNELS);
            return channels[chIdx].syncFlags;
        }
        void setSyncFlags(int32_t chIdx, int32_t flags) {
            dbgassert(chIdx >= 0 && chIdx < NUM_CHANNELS);
            channels[chIdx].syncFlags = flags;
            channels[chIdx].syncRatios = GetSyncRatios(flags);
        }
        bool getSnapshot(snapshot_t& snapshot) {
            snapshot.version = BINARY_SNAPSHOT_VERSION;
            for (int32_t i = 0; i < NUM_CHANNELS; ++i) {
                auto shapeSnapshot = DAW::Shape::shape_snapshot_t{ i, DAW::Shape::shape_preset_t{2, channels[i].shape} };
                impl_channel_snapshot_t channelSnapshot{ std::move(shapeSnapshot), channels[i].syncFlags, channels[i].modeIsShape, channels[i].srcRand ? channels[i].srcRand->getModeId() : -1 };
                snapshot.channels.push_back(std::move(channelSnapshot));
            }
            return true;
        }

        bool setSnapshot(const snapshot_t& snapshot) {
            for (int32_t i = 0; i < NUM_CHANNELS && i < CtrSize(snapshot.channels); ++i) {
                auto& channelSnapshot = snapshot.channels[i];
                channels[i].shape.pts = channelSnapshot.shape.shape.curve.pts;
                if (snapshot.version > 2) {
                    setSyncFlags(i, channelSnapshot.syncFlags);
                }
                if (snapshot.version > 3) {
                    setRandomMode(i, channelSnapshot.modeRandom);
                    if (channelSnapshot.modeIsShape) {
                        channels[i].modeIsShape = true;
                    }
                }
            }
            return true;
        }
        void process(const DAW::Host::Host* const host, AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) {
            for (int32_t i = 0; i < NUM_CHANNELS; ++i) {
                auto& channel = channels[i];
                float phase = channel.srcSync.getPhase(tick);
                channel.shape.renderPhase = phase;
            }
        }
    };

    void module_lfo::initModChannels() {
        outputModChannelsDesc.clear();
        auto reg = registerParam(PARAM_LFO_RATE);
        reg->setInitial(0.5f);
        reg->name  = "Rate";
        reg->shortLabel  = "Rate";
        reg->unit  = "Ticks";
        reg = registerParam(PARAM_LFO_PHASE);
        reg->setInitial(0.0f);
        reg->name  = "Phase";
        reg->shortLabel  = "Phase";
        reg->unit  = "°";
        reg = registerParam(PARAM_LFO_MINIMUM);
        reg->setInitial(0.5f);
        reg->name  = "Minimum";
        reg->shortLabel  = "Min";
        reg->unit  = "";
        reg->isBiPolar = true;
        reg = registerParam(PARAM_LFO_MAXIMUM);
        reg->setInitial(1.0f);
        reg->name  = "Maximum";
        reg->shortLabel  = "Max";
        reg->unit  = "";
        reg->isBiPolar = true;
        reg = registerParam(PARAM_LFO_PHASE_RESET_TICKS);
        reg->setInitial(0.0f);
        reg->name  = "Phase Reset";
        reg->shortLabel  = "Reset";
        reg->unit  = "Ticks";
        reg->isAutomatable = false;
        reg->quantizationSteps = LFO_PHASE_RESET_STEPS.size();
        impl->setSyncFlags(0, TRIPLET|DOTTED|STRAIGHT);
        outputModChannelsDesc.push_back({0, "LFO 0"});
    }

    void module_lfo::process(const DAW::Host::Host* const host, AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) {
        impl->process(host, in, out, tick, samplePos, numSamples, state);
        internalplugin::process(host, in, out, tick, samplePos, numSamples, state);
    }

    module_lfo::module_lfo(int32_t _projectGlobalId, IHostCallback* _hostCallback)
        : internal_modulator("LFO", _projectGlobalId, _hostCallback),
        impl(new lfo_impl_t{ DawInstance::getOptional(), this, DAW::Shape::GetShapeSaw(DAW::Shape::SHAPE_SHAPED|DAW::Shape::SHAPE_CYCLIC) })
    {
        initModChannels();
    }

    module_lfo::~module_lfo() {
        delete impl;
    }

    DAW::Shape::shape_t& module_lfo::getShape(int idx) {
        return this->impl->getShape(idx);
    }

    const automated_param_t* module_lfo::getModulationOutputData(const DAW::modulation_channel_ref& channel) {
        return impl->getModulationOutputData(channel);
    }

    std::shared_ptr<std::vector<std::byte>> serializeSnapshot(const snapshot_t& snapshot) {
        dbgassert(snapshot.version == BINARY_SNAPSHOT_VERSION);
        auto shrdHeapVec = std::make_shared<std::vector<std::byte>>();
        shrdHeapVec->resize(256);
        DAW::ByteBuffer::stream_write<std::vector<std::byte>> out{*shrdHeapVec, 0};
        out.write(size_t(0));
        out.write(snapshot.version);
        out.write(size_t{snapshot.uiLayout.size()});
        out.write(size_t{snapshot.channels.size()});
        for (const auto& channel : snapshot.channels) {
            DAW::Shape::writeShape(out, channel.shape);
            out.write(channel.syncFlags);
            out.write(channel.modeIsShape);
            out.write(channel.modeRandom);
        }
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
        size_t numChannels = 0;
        if (snapshot.version > 1) {
            if (!in.read(numChannels) || numChannels > 1000)
                return false;
        }
        else {
            numChannels = 1;
        }
        snapshot.uiLayout.resize(numUiLayouts);
        snapshot.channels.resize(numChannels);
        if (snapshot.version > 1) {
            for (auto& channel : snapshot.channels) {
                if (!DAW::Shape::readShape(in, channel.shape))
                    return false;
                if (snapshot.version > 2) {
                    if (!in.read(channel.syncFlags))
                        return false;
                    if (snapshot.version > 3) {
                        if (!in.read(channel.modeIsShape))
                            return false;
                        if (!in.read(channel.modeRandom))
                            return false;
                    }
                } else {
                    bool dummy;
                    if (!in.read(dummy))
                        return false;
                    channel.syncFlags = TRIPLET | DOTTED | STRAIGHT;
                }
            }
        }
        else {
            // snapshot.channels.resize(1);
            snapshot.channels[0].shape = {0, {2, DAW::Shape::GetShapeSaw(DAW::Shape::SHAPE_SHAPED|DAW::Shape::SHAPE_CYCLIC)}};
            snapshot.channels[0].syncFlags = TRIPLET | DOTTED | STRAIGHT;
        }
        for (auto& modulation : snapshot.uiLayout) {
            if (!in.read(modulation.uiId))
                return false;
            if (!in.read(modulation.numActive))
                return false;
        }
        snapshotOut = std::move(snapshot);
        return true;
    }

    std::shared_ptr<std::vector<std::byte>> module_lfo::storePresetData() {
        snapshot_t snapshot;
        impl->getSnapshot(snapshot);
        getUiSnapshot(snapshot);
        return serializeSnapshot(snapshot);
    }

    bool module_lfo::loadPresetData(const std::shared_ptr<std::vector<std::byte>>& buf) {
        if (buf->size() > 0) {
            snapshot_t snapshotLoaded;
            if (deserializeSnapshot(buf, snapshotLoaded)) {
                impl->setSnapshot(snapshotLoaded);
                setUiSnapshot(snapshotLoaded);
                return true;
            }
        }
        return false;
    }

    class guictr_module_lfo final : public guictr_base {
        module_lfo* const module;
        std::vector<guiknob_pluginparam*> guiParams;
        std::vector<gui_slider_textfield*> guiParamsTextfields;
        guictr_vert_layout firstCtr;
        i_ctr_shape_editor* const shapeEditor;
        guictr_base* ctrShapeEditor;
        DAW::Shape::guictr_curve_shape* ctrShapeScope;
        DAW::Shape::shape_t shapeScope;
        gui_textfield editfield;
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
        explicit guictr_module_lfo(module_lfo* module) : guictr_base(),
            module(module),
            shapeEditor(makeShapeEditor()),
            ctrShapeScope(DAW::Shape::makeShapeCurveView())
        {
            init();
            std::vector<automatable_param_t*> paramsSorted;
            module->getSortedParams(paramsSorted);
            {
                auto p = module->getParam(PARAM_LFO_RATE);
                auto gui = new guiknob_pluginparam(p->idx, p->idx, guiknob::knobtype::SLIDER_LABELED);
                gui->setAutomationRef(module, p->idx);
                firstCtr.addElement({0.5f, gui});
                guiParams.push_back(gui);
            }
            {
                auto p = module->getParam(PARAM_LFO_PHASE);
                auto gui = new guiknob_pluginparam(p->idx, p->idx, guiknob::knobtype::KNOB_LABELED);
                gui->setAutomationRef(module, p->idx);
                firstCtr.addElement({0.2f, gui});
                guiParams.push_back(gui);
            }
            auto minMaxCtr = new guictr_vert_layout();
            {
                auto p = module->getParam(PARAM_LFO_MINIMUM);
                auto gui = new gui_slider_textfield();
                gui->setLabel(p->shortLabel);
                gui->setFlag(FLG_RENDER_LABEL, true);
                gui->setAutomationRef(module, p->idx);
                minMaxCtr->addElement({0.5f, gui});
                guiParamsTextfields.push_back(gui);
            }
            {
                auto p = module->getParam(PARAM_LFO_MAXIMUM);
                auto gui = new gui_slider_textfield();
                gui->setLabel(p->shortLabel);
                gui->setFlag(FLG_RENDER_LABEL, true);
                gui->setAutomationRef(module, p->idx);
                minMaxCtr->addElement({0.5f, gui});
                guiParamsTextfields.push_back(gui);
            }
            firstCtr.addElement({0.15f, minMaxCtr});
            firstCtr.addElement({0.15f, new DAW::UI::Modulation::guibutton_modulate(module->getModulationChannel(0))});
            add(&firstCtr);
            shapeEditor->setShapeEditorShapeRef(&module->getShape(0));
            shapeEditor->setShapeEditorCallback([module=this->module](const DAW::Shape::shape_t& shape, bool bIsDragMove) -> void {
                auto lock = module->impl->lock();
                auto& synthShape = module->getShape(0);
                synthShape.pts = shape.pts;
                synthShape.eraseDuplicates();
            });
            ctrShapeEditor = shapeEditor->getGuiContainer();
            ctrShapeEditor->setFlag(FLG_NO_LAYOUT, true);
            ctrShapeEditor->setBackgroundRendered(false);
            ctrShapeEditor->setBackgroundRenderedInset(false);
            ctrShapeEditor->setCanMouseHit(false);
            ctrShapeEditor->id = 2;
            ctrShapeEditor->margin = 0;
            ctrShapeEditor->padding = 2;
            ctrShapeScope->setHandleMouseDrag(false);
            ctrShapeScope->setFlag(FLG_NO_LAYOUT, true);
            ctrShapeScope->setBackgroundRendered(false);
            ctrShapeScope->setBackgroundRenderedInset(false);
            ctrShapeScope->id = 3;
            ctrShapeScope->margin = 0;
            ctrShapeScope->padding = 2;
            add(ctrShapeEditor);
            add(&editfield);
        }

        ~guictr_module_lfo() override {
            remove(&editfield);
            remove(&firstCtr);
            remove(ctrShapeEditor);
            remove(ctrShapeScope);
            destroyGuis();
            delete ctrShapeEditor;
            delete ctrShapeScope;
        }

        void layout() override {
            auto cs = getSizeContent() - ivec2(padding, 0);
            auto leftSize = math::clamp<int32_t>(math::min(math::roundfS32(cs.y * 0.2f), math::roundfS32(cs.y * 0.2f)), 16, 128);
            firstCtr.size        = { leftSize, cs.y };
            ctrShapeEditor->size = { cs.x - leftSize, cs.y };
            ctrShapeEditor->pos  = { cs.x - ctrShapeEditor->size.x, 0 };
            this->ctrShapeScope->pos = this->ctrShapeEditor->pos;
            this->ctrShapeScope->size = this->ctrShapeEditor->size;
            for (auto gui : guis) {
                gui->layout();
            }
        }

        void setMode(bool bIsShape) {
            bool bChanged = false;
            if (bIsShape) {
                if (ctrShapeScope->parent) {
                    remove(ctrShapeScope);
                    bChanged = true;
                }
                if (!ctrShapeEditor->parent) {
                    add(ctrShapeEditor);
                    bChanged = true;
                }
            } else {
                if (ctrShapeEditor->parent) {
                    remove(ctrShapeEditor);
                    bChanged = true;
                }
                if (!ctrShapeScope->parent) {
                    add(ctrShapeScope);
                    bChanged = true;
                }
            }
            if (bChanged) {
                onChildLayoutChanged(this);
            }
        }

        class guictr_module_lfo_context_menu final : public guictxtmenu {
            module_lfo* const module;
            int32_t channel;
        public:
            explicit guictr_module_lfo_context_menu(module_lfo* _module, int32_t _channel)
                : guictxtmenu(), module(_module), channel(_channel) 
            {
                this->size.x   = 220;
                maxHeight = 0;
                this->fontSize = FONT_SIZE_CTXT_SMALL;
                this->paddingV = 0;
                using namespace DAW::LFO;
                addEntry(new ctxtmenu_lfo_sync<module_lfo>(module, channel, "Sync", 100));
                addEntry(new ctxtmenu_lfo_mode<module_lfo>(module, channel, "Mode", 200));
                addEntry(new DAW::Shape::ctxtmenu_lfo_shape_select("Shape", 400));
                addEntry(new ctxtmenu_lfo_random_mode<module_lfo>(module, channel, "Random", 300));
            }
            bool clickedElement(ctxtmenu_entry* e, int _id) override {
                if (_id >= 400) {
                    using DAW::Shape::ShapeWaveform;
                    auto shapeIdx = _id - 400;
                    if (shapeIdx < 0 || shapeIdx > ShapeWaveform::SHAPE_PULSE_INV) {
                        return false;
                    }
                    auto waveform = static_cast<ShapeWaveform>(shapeIdx);
                    auto lock = module->impl->lock();
                    auto& shape = module->getShape(channel);
                    shape.pts = GetShape(waveform);
                    module->setShapeMode(channel);
                } else if (_id >= 300) {
                    auto randomIdx = _id - 300;
                    auto lock = module->impl->lock();
                    module->setRandomMode(channel, randomIdx);
                } else if (_id >= 200) {
                    auto lock = module->impl->lock();
                    if (_id == 200) {
                        module->setShapeMode(channel);
                    } else {
                        module->setRandomMode(channel);
                    }
                } else if (_id >= 100) {
                    auto lock = module->impl->lock();
                    int flags = module->getSyncRatio(channel);
                    int clicked = _id - 100;
                    if (clicked == 0) {
                        flags = 0;
                    } else {
                        if (flags & clicked) {
                            flags &= ~clicked;
                        } else {
                            flags |= clicked;
                        }
                    }
                    module->setSyncRatio(channel, flags);
                    return true;
                }
                closeContextMenu();
                return true;
            }
        };

        void rightClicked(MouseEvent& evt, guibase* what) override {
            parentCtrl->openContextMenu(new guictr_module_lfo_context_menu(module, 0), evt.mousepos);
        }

        void getSizeScale(int& w, int& h) const {
            w = 350;
            h = 300;
        }

        void layoutEntries(ivec2 pos, ivec2 cs, ivec2 dir) override {
            auto shapeWidth = cs.x > cs.y ? cs.y : cs.x*2/3;
            this->ctrShapeEditor->pos = {cs.x-shapeWidth, 0};
            this->ctrShapeEditor->size = {shapeWidth, cs.y};
            this->ctrShapeScope->pos = this->ctrShapeEditor->pos;
            this->ctrShapeScope->size = this->ctrShapeEditor->size;
            guictr_base::layoutEntries({}, {cs.x-shapeWidth, cs.y}, dir);
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
            for (auto knob : guiParams) {
                knob->setEffectInstance(module);
            }
        }

        void onGuiClose() {
            for (auto knob : guiParams) {
                knob->setEffectInstance(nullptr);
            }
        }

        void onSetParameter(int32_t index, float value) {
        }

        void setUiLayout(const ui_layout_t& layout) {
        }

        bool getUiLayout(ui_layout_t& layout) const {
            return true;
        }

        void prerender(NVGcontext* vg) override {
            guictr_base::prerender(vg);
            if (!module->isShapeMode(0)) {
                auto& shape = ctrShapeScope->getShape();
                auto numPoints = samplecount_t(math::clamp(size.x, 16, 1024));
                shape.pts.resize(numPoints);
                shape.flags |= DAW::Shape::ShapeFlags::SHAPE_LOCK_POINTS;
                // shape.flags |= DAW::Shape::ShapeFlags::SHAPE_UNCLAMPPED;
                auto daw = dawCtrl->getDaw();
                
                auto tick = !daw->isPlaying() ? daw->getIdleTickPos() : daw->getPlaybackPos();
                tick_t range = TICKS_BAR;
                auto begin = tick - range;
                for (samplecount_t j = 0; j < samplecount_t(numPoints); ++j) {
                    auto t = begin + (j * range) / numPoints;
                    auto v = module->impl->channels[0].srcRand->sampleCurve(t);
                    auto normalizedT = (t - begin) / float(range);
                    auto& pt = shape.pts[j];
                    pt.pos.x = normalizedT;
                    pt.pos.y = v;
                }
            }
        }
    };

    using ViewCtrType = PluginViewContainerBasic<guictr_module_lfo, module_lfo>;
    std::shared_ptr<PluginViewContainer> module_lfo::createViewCtrInternal() {
        auto ctr = std::make_shared<ViewCtrType>(this, 100, 150);
        ctr->getPluginUI().setMode(isShapeMode(0));
        return ctr;
    }

    void module_lfo::getUiSnapshot(snapshot_t& snapshot) {
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

    void module_lfo::setUiSnapshot(snapshot_t& snapshot) {
        for (auto& uis : snapshot.uiLayout) {
            std::vector<std::shared_ptr<PluginViewContainer>> views;
            getAllViewCtrs(uis.uiId, views);
            for (auto& view : views) {
                auto implCtrType = dynamic_cast<ViewCtrType*>(view.get());
                if (implCtrType) {
                    implCtrType->getPluginUI().setUiLayout(uis);
                }
            }
        }
    }

    param_converted_t module_lfo::convertParamValueDisplay(int32_t idx, const param_unit_t& displayValue) {
        //TODO: use std::from_chars when floating point version arrives in libc++
        auto fTextFieldVal = static_cast<float>(atof(StringAsCStr(displayValue.value)));
        switch (idx) {
            case PARAM_LFO_RATE: {
                auto& first = impl->channels[0];
                if (first.syncFlags) {
                    auto numSyncRatios = CtrSize(first.syncRatios);
                    for (int32_t i = 0; i < numSyncRatios; ++i) {
                        if (first.syncRatios[i].text == displayValue.value) {
                            return {((i)/float(numSyncRatios-1)), true};
                        }
                        if (first.syncRatios[i].text == displayValue.value + "/1") {
                            return {((i)/float(numSyncRatios-1)), true};
                        }
                    }
                } else {
                    return {math::clamp(RateToParam(fTextFieldVal), 0.0f, 1.0f), true};
                }
                // float syncTicks = GetSyncRate(impl->getIsSync(), getParamValue(idx));
                // return {math::clamp(fPow, 0.0f, 1.0f), true};
                break;
            }
            case PARAM_LFO_MINIMUM:
            case PARAM_LFO_MAXIMUM: {
                return {math::clamp(fTextFieldVal * 0.5f + 0.5f, 0.0f, 1.0f), true};
            }
            case PARAM_LFO_PHASE: {
                return {math::clamp(fTextFieldVal / 360.0f, 0.0f, 1.0f), true};
            }
            case PARAM_LFO_PHASE_RESET_TICKS: {
                float phaseTicks = ResetTicksToParam(fTextFieldVal);
                return {phaseTicks, true};
            }
            default:
                break;
        }
        return internalplugin::convertParamValueDisplay(idx, displayValue);
    }

    param_unit_t module_lfo::convertParamValueToDisplay(int32_t idx, float value) {
        auto param = getParam(idx);
        dbgassert(param);
        if (param->idx == PARAM_LFO_RATE) {
            auto& firstInstance = impl->channels[0];
            auto lfoRateStr = FormatSyncRate(firstInstance.syncRatios, firstInstance.syncFlags, value);
            return {lfoRateStr, firstInstance.syncFlags ? "" : param->unit};
        }
        if (param->idx == PARAM_LFO_PHASE) {
            return {StringFormat("%.2f", value*360.0f), param->unit};
        }
        if (param->idx == PARAM_LFO_MINIMUM || param->idx == PARAM_LFO_MAXIMUM) {
            return {StringFormat("%.2f", value*2.0f-1.0f), param->unit};
        }
        if (param->idx == PARAM_LFO_PHASE_RESET_TICKS) {
            auto resetTicksStr = FormatResetTicks(value);
            return {resetTicksStr, param->unit};
        }
        return internalplugin::convertParamValueToDisplay(idx, value);
    }
    int32_t module_lfo::getSyncRatio(int32_t chIdx) const {
        return impl->getSyncFlags(chIdx); 
    }
    bool module_lfo::isShapeMode(int32_t chIdx) const {
        return impl->channels[chIdx].modeIsShape;
    }
    void module_lfo::setSyncRatio(int32_t chIdx, int32_t ratio) {
        impl->setSyncFlags(chIdx, ratio);
    }
    void module_lfo::setShapeMode(int32_t chIdx) {
        impl->channels[chIdx].modeIsShape = true;
        for (auto& view : views) {
            auto implCtrType = dynamic_cast<ViewCtrType*>(view.get());
            if (implCtrType) {
                implCtrType->ctr_main.setMode(true);
            }
        }
    }
    void module_lfo::setRandomMode(int32_t chIdx, int32_t mode) {
        impl->setRandomMode(chIdx, mode);
        for (auto& view : views) {
            auto implCtrType = dynamic_cast<ViewCtrType*>(view.get());
            if (implCtrType) {
                implCtrType->ctr_main.setMode(false);
            }
        }
    }
    int32_t module_lfo::getRandomMode(int32_t chIdx) const {
        return impl->getRandomMode(chIdx);
    }
}// namespace PluginLFO

template<>
effectbase* makeInstance<PluginLFO::module_lfo>(int32_t _projectGlobalId, IHostCallback* _hostCallback) {
    return new PluginLFO::module_lfo(_projectGlobalId, _hostCallback);
}

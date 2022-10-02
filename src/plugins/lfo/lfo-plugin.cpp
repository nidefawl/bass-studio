#include "lfo-plugin.h"
#include "assert_dbg.h"
#include "automation.h"
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
#include "host/host_pluginmanager.h"
#include "host/mainctrl.h"
#include "logging.h"
#include "math/seq_math.h"
#include "renderresources.h"
#include "seq_time.h"
#include "seq_util.h"
#include "host/plugin/plugin-lockable.h"
#include "shape.h"
#include "str_util.h"
#include "logging.h"
#include "byte-buffer.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <utility>
#include <vector>

namespace PluginLFO {
    constexpr int32_t NUM_CHANNELS = 12;
    constexpr int32_t BINARY_SNAPSHOT_VERSION = 2;
    constexpr int32_t PARAM_LFO_RATE = 16;
    constexpr int32_t PARAM_LFO_PHASE = 17;
    constexpr int32_t PARAM_LFO_MINIMUM = 18;
    constexpr int32_t PARAM_LFO_MAXIMUM = 19;

    const double RATE_MIN = 1;
    const double RATE_MAX = TICKS_BAR*4;
    double GetScaledRate(float paramValue) {
        return math::clamp(paramValue * (RATE_MAX - RATE_MIN) + RATE_MIN, RATE_MIN, RATE_MAX);
    }
    float RateToParam(float rate) {
        return (rate - RATE_MIN) / (RATE_MAX - RATE_MIN);
    }
    struct SyncRatio {
        int32_t numerator;
        int32_t denominator;
        String text;
    };
    enum NoteRatio : uint8_t {
        NORMAL = 1,
        DOTTED = 2,
        TRIPLET = 4,
    };
    std::vector<SyncRatio> GetSyncRatios(int ratioFlags = (NORMAL | DOTTED | TRIPLET)) {
        std::vector<SyncRatio> syncRatios;
        for (int32_t i = 64; i >= 1; i /= 2) {
            if (ratioFlags & NoteRatio::TRIPLET) {
                syncRatios.push_back({ 1, i * 3, StringFormat("%d/%d", 1, i*3) });// triplet
            }
            if (ratioFlags & NoteRatio::NORMAL) {
                syncRatios.push_back({ 1, i, StringFormat("%d/%d", 1, i) });// straight
            }
            if (ratioFlags & NoteRatio::DOTTED) {
                syncRatios.push_back({ 3, i, StringFormat("%d/%d", 3, i) });// dotted
            }
        }
        for (int32_t i = 2; i < 32; i *= 2) {
            if (ratioFlags & NoteRatio::TRIPLET) {
                syncRatios.push_back({ i, 3, StringFormat("%d/%d", i*3, 1) });// triplet
            }
            if (ratioFlags & NoteRatio::NORMAL) {
                syncRatios.push_back({ i, 1, StringFormat("%d/%d", i, 1) });// straight
            }
            if (ratioFlags & NoteRatio::DOTTED) {
                syncRatios.push_back({ 3 * i, 1, StringFormat("%d/%d", 3 * i, 1) });// dotted
            }
        }
        if (ratioFlags & NoteRatio::NORMAL) {
            for (int32_t i : {32, 64, 128}) {
                syncRatios.push_back({ i, 1, StringFormat("%d/%d", i, 1) });// straight
            }
        }
        std::sort(syncRatios.begin(), syncRatios.end(), [](const SyncRatio& a, const SyncRatio& b) {
            return (1000 * a.numerator / a.denominator) < (1000 * b.numerator / b.denominator);
        });
        return syncRatios;
    }
    std::vector<String> GetSyncRatioLabels(int ratioFlags = (NORMAL | DOTTED | TRIPLET)) {
        auto syncs = GetSyncRatios(ratioFlags);
        std::vector<String> syncRatios;
        syncRatios.reserve(syncs.size());
        for (auto& sync : syncs) {
            syncRatios.push_back(sync.text);
        }
        return syncRatios;
    }

    float GetSyncRate(const std::vector<SyncRatio>& syncRatios, bool bIsSync, float paramValue) {
        if (!bIsSync || syncRatios.empty()) {
            return GetScaledRate(paramValue);
        }

        int32_t index = math::clamp<int32_t>(math::floorfS32(paramValue * syncRatios.size()), 0, syncRatios.size() - 1);
        const SyncRatio& syncRatio = syncRatios[index];
        return (TICKS_BAR * syncRatio.numerator) / syncRatio.denominator;
    }
    String FormatSyncRate(const std::vector<SyncRatio>& syncRatios, bool bIsSync, float paramValue) {
        if (!bIsSync || syncRatios.empty()) {
            return StringFormat("%.2f", GetScaledRate(paramValue));
        }
        int32_t index = math::clamp<int32_t>(math::floorfS32(paramValue * syncRatios.size()), 0, syncRatios.size() - 1);
        return syncRatios[index].text;
    }

    struct module_lfo::lfo_impl_t : public PluginLockable {
        struct lfo_automation_src_param_t : public automated_param_t {
            module_lfo* module = nullptr;
            DAW::Shape::shape_t shape;
            bool bIsSync = false;
            std::vector<SyncRatio> syncRatios;
            float getPhase(double dTick) const {
                const auto fRate = module->getParamValue(PARAM_LFO_RATE);
                const auto fPhase = module->getParamValue(PARAM_LFO_PHASE);
                double fPhaseOffset = 0.0f;
                if (!bIsSync || syncRatios.empty()) {
                    fPhaseOffset = dTick / double(GetScaledRate(fRate));
                } else {
                    auto index = math::clamp<int32_t>(math::floorfS32(fRate * CtrSize(syncRatios)), 0, syncRatios.size() - 1);
                    auto ratio = syncRatios[index];
                    double barPos = dTick / double(TICKS_BAR);
                    fPhaseOffset = double((barPos * ratio.denominator) / ratio.numerator);
                }
                auto phase = fPhaseOffset + fPhase;
                float moduloPhase = modf(phase, &phase);
                return moduloPhase;
            }
            float sampleCurve(double dTick) const {
                float moduloPhase = getPhase(dTick);
                auto valMin = module->getParamValue(PARAM_LFO_MINIMUM) * 2.0f - 1.0f;
                auto valMax = module->getParamValue(PARAM_LFO_MAXIMUM) * 2.0f - 1.0f;
                auto value = shape.sampleCurve(moduloPhase, false);
                return value * (valMax - valMin) + valMin;
            }
            float modulateValue(tick_t tick, float fIn, const DAW::modulation_scaling_t& scale) const override {
                const auto valScaled = scale.min + sampleCurve(tick) * (scale.max - scale.min);
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
                    fIn = math::clamp(fIn, scale.min, scale.max);
                }
                return fIn;
            }
            void sampleAutomation(double dTickBegin, double dTickEnd, samplecount_t numSamples, const DAW::modulation_scaling_t& scale, float* inOut) const override {
                for (samplecount_t i = 0; i < numSamples; ++i) {
                    const auto dTickOffset     = dTickBegin + i * (dTickEnd - dTickBegin) / double(numSamples);
                    const auto valScaled = scale.min + sampleCurve(dTickOffset) * (scale.max - scale.min);
                    switch (scale.mode) {
                        case DAW::ModulationMode::ADD:
                            *inOut++ += valScaled;
                            break;
                        case DAW::ModulationMode::MUL:
                            *inOut++ *= valScaled;
                            break;
                        case DAW::ModulationMode::REPLACE:
                            *inOut++ = valScaled;
                            break;
                        default:
                            break;
                    }
                    if (scale.bClamp) {
                        *inOut = math::clamp(*inOut, scale.min, scale.max);
                    }
                }
            }
            void setRange(tick_t tickBegin, tick_t tickEnd, std::vector<automation_point_t>& data) override {
            }
            void copyRange(tick_t tickBegin, tick_t tickEnd, std::vector<automation_point_t>& data) const override {
            }
            bool isActive() const override { //??
                return true;
            }
            bool isAutomated() const override { //??
                return true; 
            }
            float getValueAt(tick_t tick) const override {
                return sampleCurve(tick);
            }
            float getValueAtExact(double dTick) const override {
                return sampleCurve(dTick);
            }
            String getName() const override {
                return StringFormat("LFO %d", paramIdx+1);
            }
        };
        module_lfo* module;
        std::array<lfo_automation_src_param_t, NUM_CHANNELS> macroAutomationSrcParams;
        bool bIsSync = false;
        explicit lfo_impl_t(DawInstance* daw, module_lfo* module, const DAW::Shape::shape_base_t& initShape) 
            : PluginLockable(daw),
            module(module)
        {
            for (int32_t i = 0; i < NUM_CHANNELS; ++i) {
                macroAutomationSrcParams[i].module = module;
                macroAutomationSrcParams[i].paramIdx = i;
                macroAutomationSrcParams[i].shape.setShape(initShape);
                macroAutomationSrcParams[i].bIsSync = bIsSync;
                macroAutomationSrcParams[i].syncRatios = GetSyncRatios(NORMAL | DOTTED | TRIPLET);
            }
        }
        const lfo_automation_src_param_t* getModulationOutputData(const DAW::modulation_channel_ref& channel) {
            auto chIdx = channel.refSrc.paramIdx;
            if (!assert_expr(chIdx >= 0 && chIdx < NUM_CHANNELS))
                return nullptr;
            if (!assert_expr(chIdx < CtrSize(macroAutomationSrcParams)))
                return nullptr;
            return &macroAutomationSrcParams[chIdx];
        }
        DAW::Shape::shape_t& getShape(int chIdx) {
            static DAW::Shape::shape_t shapeDummy{};
            if (!assert_expr(chIdx >= 0 && chIdx < NUM_CHANNELS))
                return shapeDummy;
            if (!assert_expr(chIdx < CtrSize(macroAutomationSrcParams)))
                return shapeDummy;
            return this->macroAutomationSrcParams[chIdx].shape;
        }
        bool getIsSync() const {
            return bIsSync;
        }
        void setIsSync(bool bIsSync) {
            this->bIsSync = bIsSync;
            for (auto& param : macroAutomationSrcParams) {
                param.bIsSync = bIsSync;
            }
        }
        bool getSnapshot(snapshot_t& snapshot) {
            snapshot.version = BINARY_SNAPSHOT_VERSION;
            for (int32_t i = 0; i < NUM_CHANNELS; ++i) {
                auto shapeSnapshot = DAW::Shape::shape_snapshot_t{ i, DAW::Shape::shape_preset_t{1, "LFO", macroAutomationSrcParams[i].shape} };
                impl_channel_snapshot_t channelSnapshot{ std::move(shapeSnapshot), macroAutomationSrcParams[i].bIsSync };
                snapshot.channels.push_back(std::move(channelSnapshot));
            }
            return true;
        }

        bool setSnapshot(const snapshot_t& snapshot) {
            for (int32_t i = 0; i < NUM_CHANNELS && i < CtrSize(snapshot.channels); ++i) {
                auto& channelSnapshot = snapshot.channels[i];
                macroAutomationSrcParams[i].shape.pts = channelSnapshot.shape.shape.curve.pts;
                macroAutomationSrcParams[i].bIsSync = channelSnapshot.bSync;
            }
            return true;
        }
        void process(const DAW::Host::Host* const host, AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) {
            for (int32_t i = 0; i < NUM_CHANNELS; ++i) {
                auto& channel = macroAutomationSrcParams[i];
                float phase = channel.getPhase(tick);
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
        impl->setIsSync(true);
        outputModChannelsDesc.push_back({0, "LFO 0"});
    }

    void module_lfo::process(const DAW::Host::Host* const host, AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) {
        impl->process(host, in, out, tick, samplePos, numSamples, state);
        internalplugin::process(host, in, out, tick, samplePos, numSamples, state);
    }

    module_lfo::module_lfo(int32_t _projectGlobalId, IHostCallback* _hostCallback)
        : internal_modulator("LFO", getModuleType(), _projectGlobalId, _hostCallback),
        impl(new lfo_impl_t{ DawInstance::get(), this, DAW::Shape::GetShapeSaw() })
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
        out.write(size_t{snapshot.channels.size()});
        out.write(size_t{snapshot.uiLayout.size()});
        for (const auto& channel : snapshot.channels) {
            DAW::Shape::writeShape(out, channel.shape);
            out.write(channel.bSync);
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
                if (!in.read(channel.bSync))
                    return false;
            }
        }
        else {
            snapshot.channels[0].shape = {0, {0, "LFO", DAW::Shape::GetShapeSaw()}};
            snapshot.channels[0].bSync = true;
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

    class guictr_vert_layout : public guictr_base {
        struct layout_entry_t {
            float scale;
            guibase* gui;
        };
        std::vector<layout_entry_t> layouts;
    public:
        explicit guictr_vert_layout()
            : guictr_base()
        {
            padding = margin = 0;
        }
        ~guictr_vert_layout() override {
            destroyGuis();
        };
        void layout() override {
            auto cs = getSizeContent();
            float y = 0;
            for (auto& entry : layouts) {
                entry.gui->size = { cs.x, cs.y * entry.scale };
                entry.gui->pos = { 0, y };
                y = entry.gui->bottom();
            }
            guictr_base::layout();
        }
        void addElement(const layout_entry_t& entry) {
            layouts.push_back(entry);
            add(entry.gui);
        }
    };
    class guictr_module_lfo : public guictr_base {
        module_lfo* const module;
        std::vector<guiknob_pluginparam*> guiParams;
        std::vector<gui_slider_textfield*> guiParamsTextfields;
        i_ctr_shape_editor* const shapeEditor;
        guictr_base* ctrShapeEditor;
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
            shapeEditor(makeShapeEditor())
        {
            init();
            std::vector<automatable_param_t*> paramsSorted;
            module->getSortedParams(paramsSorted);
            auto firstCtr = new guictr_vert_layout();
            {
                auto p = module->getParam(PARAM_LFO_RATE);
                auto gui = new guiknob_pluginparam(p->idx, p->idx, guiknob::knobtype::SLIDER_LABELED);
                gui->setAutomationRef(module, p->idx);
                firstCtr->addElement({0.5f, gui});
                guiParams.push_back(gui);
            }
            {
                auto p = module->getParam(PARAM_LFO_PHASE);
                auto gui = new guiknob_pluginparam(p->idx, p->idx, guiknob::knobtype::KNOB_LABELED);
                gui->setAutomationRef(module, p->idx);
                firstCtr->addElement({0.2f, gui});
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
            firstCtr->addElement({0.15f, minMaxCtr});
            firstCtr->addElement({0.15f, new DAW::UI::Modulation::guibutton_modulate(module->getModulationChannel(0))});
            add(firstCtr);
            shapeEditor->setShapeEditorShapeRef(&module->getShape(0));
            shapeEditor->setShapeEditorCallback([module=this->module](const DAW::Shape::shape_base_t& shape) -> void {
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
            add(ctrShapeEditor);
            add(&editfield);
        }

        bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
            return guictr_base::mouseHitTest(mpos, evt);
        }

        ~guictr_module_lfo() override {
            remove(&editfield);
            destroyGuis();
        }

        void getSizeScale(int& w, int& h) const {
            w = 350;
            h = 300;
        }

        void layoutEntries(ivec2 pos, ivec2 cs, ivec2 dir) override {
            auto shapeWidth = cs.x > cs.y ? cs.y : cs.x*2/3;
            this->ctrShapeEditor->pos = {cs.x-shapeWidth, 0};
            this->ctrShapeEditor->size = {shapeWidth, cs.y};
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
    };

    using ViewCtrType = SinglePluginViewContainers<guictr_module_lfo, module_lfo>;
    std::shared_ptr<PluginViewContainers> module_lfo::createViewCtrInternal() {
        return std::make_shared<ViewCtrType>(this, 100, 150);
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


    param_converted_t module_lfo::convertParamValueDisplay(int32_t idx, const param_unit_t& displayValue) {
        //TODO: use std::from_chars when floating point version arrives in libc++
        auto fTextFieldVal = static_cast<float>(atof(StringAsCStr(displayValue.value)));
        switch (idx) {
            case PARAM_LFO_RATE: {
                auto& first = impl->macroAutomationSrcParams[0];
                if (first.bIsSync) {
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
                return {math::clamp(math::roundfS64(fTextFieldVal) * 0.5f + 0.5f, 0.0f, 1.0f), true};
            }
            case PARAM_LFO_PHASE: {
                return {math::clamp(math::roundfS64(fTextFieldVal) / 360.0f, 0.0f, 1.0f), true};
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
            auto& firstInstance = impl->macroAutomationSrcParams[0];
            auto lfoRateStr = FormatSyncRate(firstInstance.syncRatios, firstInstance.bIsSync, value);
            return {lfoRateStr, impl->getIsSync() ? "" : param->unit};
        }
        if (param->idx == PARAM_LFO_PHASE) {
            return {StringFormat("%.2f", value*360.0f), param->unit};
        }
        if (param->idx == PARAM_LFO_MINIMUM || param->idx == PARAM_LFO_MAXIMUM) {
            return {StringFormat("%.2f", value*2.0f-1.0f), param->unit};
        }
        return internalplugin::convertParamValueToDisplay(idx, value);
    }
}// namespace PluginLFO

template<>
effectbase* makeInstance<PluginLFO::module_lfo>(int32_t _projectGlobalId, IHostCallback* _hostCallback) {
    return new PluginLFO::module_lfo(_projectGlobalId, _hostCallback);
}

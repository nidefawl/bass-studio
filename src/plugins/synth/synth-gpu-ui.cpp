#include "gui/gui.h"
#include "logging.h"
#include "math/seq_math.h"
#include "synth-gpu-parameters.h"
#include "synth-gpu-snapshot.hpp"
#include "synth-gpu-impl.hpp"
#include "gui/container/container.h"
#include "gui/container/container_layout.h"
#include "gui/contextmenu/contextmenu_base.h"
#include "gui/controls/button.h"
#include "gui/controls/inputfield.h"
#include "gui/controls/knobpluginparam.h"
#include "gui/shape/shape-render.hpp"
#include "gui/shape/shape-sampled.hpp"
#include "gui/shape/shapeeditor.h"
#include "math/simd_math.h"
#include "host/shape/shape.h"
#include "platform.h"
#include "renderresources.h"
#include "saferef.h"
#include "synth-modulations-ui.hpp"
#include "synth-types.hpp"
#include <cstddef>
#include <nanovg.h>

namespace PluginSynth::GPU {

class guicontainer_plugin_synth_adsr_parameters final : public guictr_base {
    module_synth_gpu* const moduleInstance;
    std::array<guiknob_synthparam_textfield, 8> knobs;
public:
    explicit guicontainer_plugin_synth_adsr_parameters(module_synth_gpu* module, int32_t idx) 
        : moduleInstance(module)
    {
        padding = 0;
        margin  = 0;
        setBackgroundRendered(false);
        const int envBase[2] = {int32_t(SynthImplGPU::Parameters::ADSR_1_A_Duration), int32_t(SynthImplGPU::Parameters::ADSR_2_A_Duration)};
        for (size_t i = 0; i < knobs.size(); ++i) {
            auto& knob = knobs[i];
            knob.setAlignment(gui_textfield::Alignment::Center);
            knob.setSynthParam(moduleInstance->getSynth(), envBase[idx] + i);
            knob.setFontScale(0.75f);
            knob.setAutomationRef(moduleInstance, PARAM_OFFSET_IMPL + envBase[idx] + i);
            bool bIsAttackShape = i == 5;
            bool bIsDecayShape = i == 6;
            bool bIsReleaseShape = i == 7;
            knob.setRenderAsShaper(bIsAttackShape || bIsDecayShape || bIsReleaseShape, ivec2(bIsAttackShape, bIsDecayShape || bIsReleaseShape));
            if (knob.isRenderAsShaper()){
                knob.setFlag(FLG_RENDER_LABEL, false);
            }
            add(&knob);
        }
    }
    ~guicontainer_plugin_synth_adsr_parameters() override {
        removeGuis();
    }
    void layout() override {
        auto cs = getSizeContent();
        const int32_t numRows = 5;
        auto btnH = (cs.y-padding*(numRows-1)) / numRows;
        auto shapeW = btnH;

        knobs[0].pos = {0, 0};
        knobs[0].size = {cs.x - shapeW - padding, btnH};
        knobs[5].pos = {cs.x - shapeW, 0};
        knobs[5].size = {shapeW, shapeW};
    
        knobs[1].pos = {0, knobs[0].bottom()+padding};
        knobs[1].size = {cs.x - shapeW, btnH};
    

        knobs[2].pos = {0, knobs[1].bottom()+padding};
        knobs[2].size = {cs.x - shapeW - padding, btnH};
        knobs[6].pos = {cs.x - shapeW, knobs[2].top()};
        knobs[6].size = {shapeW, shapeW};

        knobs[3].pos = {0, knobs[2].bottom()+padding};
        knobs[3].size = {cs.x - shapeW, btnH};

        knobs[4].pos = {0, knobs[3].bottom()+padding};
        knobs[4].size = {cs.x - shapeW - padding, btnH};
        knobs[7].pos = {cs.x - shapeW, knobs[4].top()};
        knobs[7].size = {shapeW, shapeW};
        guictr_base::layout();
    }
};

class guicontainer_plugin_synth_other_parameters final : public guictr_base {
    module_synth_gpu* const moduleInstance;
    std::array<gui_numberinput_double, 1> knobs;
    gui_numberinput_u32 debugFlags;
    gui_numberinput_i32 maxPolyVoices;
    gui_numberinput_i32 maxUnisonVoices;
public:
    explicit guicontainer_plugin_synth_other_parameters(module_synth_gpu* module) 
        : moduleInstance(module)
    {
        padding = 0;
        margin  = 0;
        setBackgroundRendered(false);
        setLayoutMode(autolayout_mode::LAYOUT_VERTICAL);
        const auto synth = moduleInstance->getSynth();
        for (size_t i = 0; i < knobs.size(); ++i) {
            auto& nrInput = knobs[i];
            nrInput.fnClamp = [](double value) -> double {
                return math::clamp(value, 0.0, 1.0);
            };
            switch (i) {
                case 0:
                    nrInput.setRef(&synth->getOtherParamsDouble()[0]);
                    nrInput.setLabel("Fade Note Ends");
                    add(&nrInput);
                    break;
                default:
                    dbgassert(0);
                    break;
            }
        }
        maxPolyVoices.setLabel("Max Poly Voices");
        maxPolyVoices.setRef(&synth->getRefPolyVoiceCount());
        maxPolyVoices.fnClamp = [](int32_t value) -> int32_t {
            return math::clamp<int32_t>(value, 1, MAX_POLY_VOICES);
        };
        maxUnisonVoices.setRef(&synth->getRefUnisonVoiceCount());
        maxUnisonVoices.setLabel("Max Unison Voices");
        maxUnisonVoices.fnClamp = [](int32_t value) -> int32_t {
            return math::clamp<int32_t>(value, 1, MAX_UNISON_VOICES);
        };
        debugFlags.setLabel("Debug Flags");
        debugFlags.setRef(&gDebugBenchmarkFlags);
        add(&maxPolyVoices);
        add(&maxUnisonVoices);
        add(&debugFlags);
    }
    ~guicontainer_plugin_synth_other_parameters() override {
        removeGuis();
    }
};


class guictr_sampled_curve_shape final : public guictr_base, public DAW::Shape::RenderShape<DAW::Shape::sampled_curved_t> {
    module_synth_gpu* const moduleInstance;
    DAW::Shape::sampled_curved_t curveInternal;
    bool bIsNormalized = true;
    std::pair<DAW::Shape::sampled_pt_t, DAW::Shape::sampled_pt_t> minmax;
public:
    explicit guictr_sampled_curve_shape(module_synth_gpu* module) 
        : moduleInstance(module)
    {
        curveInternal.pts.push_back({ { 0, 0 } });
        padding = 4;
        margin = 4;
        setBackgroundRendered(true);
        setCanMouseHit(true);
    }
    void setIsNormalized(bool b) {
        bIsNormalized = b;
    }
    void setMinMax(const DAW::Shape::sampled_pt_t& min, const DAW::Shape::sampled_pt_t& max) {
        minmax = {min, max};
    }
    GuiColor::constant_t getOuterBackgroundColorFromState(int32_t stateflags) const override {
        return GuiColor::COL_BG_DRKER2;
    }
    void render(NVGcontext* vg) override {
        if (isBackgroundRendered()) {
            renderBackground(vg);
        }
        if (!setScissorTransform(vg)) {
            return;
        }
        const auto cs = getSizeContent();
        if (!bIsNormalized){
            auto* hostInfo = moduleInstance->getHostCallback();
            // auto timeSeconds = hostInfo->m_vstTimeInfo.samplePos / hostInfo->m_vstTimeInfo.sampleRate;
            double barDurationInSeconds = toSecondsDD(TICKS_BAR, 1.0 / (hostInfo->m_vstTimeInfo.tempo * 100.0));
            auto secondsMin = minmax.first.pos.x;
            auto secondsMax = minmax.second.pos.x;
            auto secondsDura = secondsMax - secondsMin;
            auto barsDura = secondsDura / barDurationInSeconds;
            auto barDuraInt = math::rounddS32(barsDura);
            barDuraInt = math::max(1, barDuraInt);
            DAW::Shape::DrawGrid(vg, theme, {}, cs, barDuraInt, 2, false, false);
            renderShapeView(vg, theme, &curveInternal, {}, cs);
        } else {
            if (curveInternal.pts.empty()) {
                return;
            }
            if (minmax.first.pos.x < 0.0f || minmax.second.pos.x > 1.0f) {
                // only draw grid in 0.0 - 1.0 range
                float range = minmax.second.pos.x - minmax.first.pos.x;
                float zeroNormalized = (0.0f - minmax.first.pos.x) / range;
                float oneNormalized = (1.0f - minmax.first.pos.x) / range;
                auto posGrid = vec2(cs.x * zeroNormalized, 0);
                auto sizeGrid = vec2(cs.x * (oneNormalized - zeroNormalized), cs.y);
                DAW::Shape::DrawGrid(vg, theme, posGrid, sizeGrid, 8, 2, true, true);
                renderShapeView(vg, theme, &curveInternal, {}, cs);
            } else {
                renderShapeView(vg, theme, &curveInternal, {}, cs);
            }
        }
    }
    DAW::Shape::sampled_curved_t& getShape() {
        return curveInternal;
    }
    const DAW::Shape::sampled_curved_t& getShape() const {
        return curveInternal;
    }
};
class guicontainer_plugin_synth_adsr_shape final : public guictr_base {
    module_synth_gpu* const moduleInstance;
    int32_t idx;
    guictr_sampled_curve_shape shapeAdsr;
    DAW::Shape::guictr_curve_shape* const shapeAdsrControls;
    bool bNeedsShapeSet = true;
    int32_t ticks = 0;
public:
    explicit guicontainer_plugin_synth_adsr_shape(module_synth_gpu* module, int32_t idx) 
        : moduleInstance(module), idx(idx),
        shapeAdsr(module),
        shapeAdsrControls(DAW::Shape::makeShapeCurveView())
    {
        setLayoutMode(autolayout_mode::LAYOUT_STACK);
        padding = 0;
        margin  = 0;
        setBackgroundRendered(false);
        shapeAdsrControls->setBackgroundRendered(false);
        shapeAdsrControls->setBackgroundRenderedInset(false);
        shapeAdsrControls->setCanMouseHit(true);
        shapeAdsrControls->id = 4;
        shapeAdsrControls->margin = 0;
        shapeAdsrControls->padding = 2;
        add(&shapeAdsr);
        add(shapeAdsrControls);
        shapeAdsrControls->zOrder = 1;
    }
    ~guicontainer_plugin_synth_adsr_shape() override {
        removeGuis();
        delete this->shapeAdsrControls;
    }
    void onTick(AppCtrl* ctrl) override {
        guictr_base::onTick(ctrl);
        if (ticks++ > 40) {
            ticks = 0;
            bNeedsShapeSet = true;
        }
    }

    void prerender(NVGcontext* vg) override {
        guictr_base::prerender(vg);
        if (bNeedsShapeSet) {
            // if (idx == 0) {
                setShapeFromAdsr();
            // } else {
            //     setShapeFromLogFunction();
            // }
            bNeedsShapeSet = false;
        }
    }
    void flagNeedsShapeSet() {
        bNeedsShapeSet = true;
    }
    void setShapeFromAdsr() {
        // convert synth impls outputBufferWaveform to shape
        const auto& sampleFormat = moduleInstance->getSampleFormat();
        // sample ADSR and show in second shape editor
        auto& shapeAdsrSampled = this->shapeAdsr.getShape();
        auto& shapeAdsrControls = this->shapeAdsrControls->getShape();
        shapeAdsrSampled.pts.clear();
        

        auto synth = moduleInstance->getSynth();
        auto& voiceTempUi = synth->getTempVoiceUI();
        synth->updateEnvelopeParameters(voiceTempUi);
        Envelope& envelope = voiceTempUi.envelopes[idx];
        envelope.Reset();
        envelope.Start();
        const auto sampleRate = sampleFormat.sampleRate;
        const auto oneOverSr = 1.0 / sampleRate;
        const auto stepsize = samplecount_t(128);


        auto maxIterations = samplecount_t(1024);
        shapeAdsrSampled.pts.reserve(maxIterations);
        shapeAdsrControls.pts.resize(6);
        shapeAdsrControls.pts[0] = {{ 0.0, 0.0 }, 0.5};
        // static std::vector<float> test;
        // test.resize(maxIterations);

        auto pos = samplecount_t(0);
        auto lastSample = samplecount_t(0);
        // auto outIdx = samplecount_t(0);
        bool bFinished = false;
        //std::array<double, 4> durationPhaseSeconds{};
        while (maxIterations-- > 0 && !bFinished) {
            lastSample = pos + stepsize;
            for (samplecount_t s = 0; s < stepsize && !bFinished; s++) {
                auto envState = envelope.stage;
                envelope.Update(oneOverSr);
                auto envStateNew = envelope.stage;
                bool bAddPoint = s == 0;
                if (envState != envStateNew && envState == EnvelopeStages::Attack) {
                    //durationPhaseSeconds[0] = (pos + s) * oneOverSr;
                    shapeAdsrControls.pts[1].pos = { (pos + s), envelope.value };
                    bAddPoint = true;
                }
                if (envState != envStateNew && envState == EnvelopeStages::Hold) {
                    //durationPhaseSeconds[1] = (pos + s) * oneOverSr;
                    shapeAdsrControls.pts[2].pos = { (pos + s), envelope.value };
                    bAddPoint = true;
                }
                if (envelope.IsSustain()) {
                    //durationPhaseSeconds[2] = (pos + s) * oneOverSr;
                    shapeAdsrControls.pts[3].pos = { (pos + s), envelope.value };
                    envelope.Release();
                    bAddPoint = true;
                } else if(envelope.IsIdle()) {
                    //durationPhaseSeconds[4] = (pos + s) * oneOverSr;
                    shapeAdsrControls.pts[5].pos = { (pos + s), envelope.value };
                    bFinished = true;
                    bAddPoint = true;
                    lastSample = pos + s;
                }
                if (bAddPoint) {
                    shapeAdsrSampled.pts.push_back({{ pos + s, envelope.value }});
                    // if (outIdx < samplecount_t(test.size())) {
                    //     test[outIdx] = envelope.value;
                    // }
                    // outIdx++;
                }
            }
            pos += stepsize;
        }
        // normalize x axis to 1.0
        if (lastSample > 0) {
            for (auto& pt : shapeAdsrSampled.pts) {
                pt.pos.x /= lastSample;
            }
            for (auto& pt : shapeAdsrControls.pts) {
                pt.pos.x /= lastSample;
            }
        }
        shapeAdsrSampled.flags = DAW::Shape::SHAPE_UNCLAMPPED | DAW::Shape::SHAPE_LOCK_POINTS;
        shapeAdsrControls.flags = DAW::Shape::SHAPE_UNCLAMPPED | DAW::Shape::SHAPE_SHOW_ONLY_CONTROL_POINTS;
    }
    void setShapeFromLogFunction() {
        auto& shapeAdsrSampled = this->shapeAdsr.getShape();
        auto& shapeAdsrControls = this->shapeAdsrControls->getShape();
        shapeAdsrControls.pts.clear();
        shapeAdsrSampled.pts.clear();
        
        samplecount_t numSamples = 1024;
        shapeAdsrSampled.pts.reserve(numSamples);
        for (samplecount_t s = 0; s < numSamples; s++) {
            alignas(64) float envParamVals[8]{};
            alignas(64) float envParamValsScaled[8]{};
            float stepPos = s / float(numSamples);
            std::fill(std::begin(envParamVals), std::end(envParamVals), stepPos);
            ShapeLogLikeSIMD<float>(envParamVals, envParamValsScaled);
            shapeAdsrSampled.pts.push_back({{ stepPos, envParamValsScaled[0] }});
        }
        shapeAdsrSampled.flags = DAW::Shape::SHAPE_UNCLAMPPED | DAW::Shape::SHAPE_LOCK_POINTS;
        shapeAdsrControls.flags = DAW::Shape::SHAPE_UNCLAMPPED | DAW::Shape::SHAPE_SHOW_ONLY_CONTROL_POINTS;
    }
};
class guicontainer_plugin_synth_adsr final : public guictr_stacked {
    module_synth_gpu* const moduleInstance;
    guicontainer_plugin_synth_adsr_shape shape;
    guicontainer_plugin_synth_adsr_parameters parameters;
public:
    explicit guicontainer_plugin_synth_adsr(module_synth_gpu* module, int32_t idx)
        : moduleInstance(module),
        shape(module, idx),
        parameters(module, idx)
    {
        setVerticalLayout(true);
        (void) moduleInstance;
        shape.setBackgroundRendered(false);
        shape.setCanMouseHit(true);
        shape.id = 1;
        parameters.setBackgroundRendered(false);
        parameters.setCanMouseHit(true);
        parameters.id = 2;
        addEntry(&shape);
        addEntry(&parameters);
        setSplitters({ 0.5f });
    }
    void buttonClicked(guibase* gui) override {
        guictr_stacked::buttonClicked(gui);
        if (gui == &parameters) {
            shape.flagNeedsShapeSet();
        }
    }
    void flagNeedsShapeSet() {
        shape.flagNeedsShapeSet();
    }
    ~guicontainer_plugin_synth_adsr() override {
        removeGuis();
        removeEntries();
    }
};

class ctxtmenu_entry_adsr_shape_function_select final : public ctxtmenu_enum_option_select_base<ctxmenu_enum_select_entry> {
    module_synth_gpu* const moduleInstance;
    int32_t channel;
public:
    ctxtmenu_entry_adsr_shape_function_select(module_synth_gpu* _module, int32_t _channel, String _title, int32_t _id)
        : ctxtmenu_enum_option_select_base(_id, std::move(_title)), moduleInstance(_module), channel(_channel) 
    {
        using F = DAW::CurveShapingFunction;
        entries.push_back({ int32_t(F::Linear), "Linear" });
        entries.push_back({ int32_t(F::Pow), "Pow" });
        entries.push_back({ int32_t(F::Exp), "Exp" });
    }
    bool isEntrySelected(ctxmenu_enum_select_entry& e) const override {
        auto const synth = moduleInstance->getSynth();
        return int32_t(synth->getAdsrShapeMode(channel)) == e.id;
    }
};

class guictr_module_adsr_context_menu final : public guictxtmenu {
    module_synth_gpu* const moduleInstance;
    int32_t channel;
public:
    explicit guictr_module_adsr_context_menu(module_synth_gpu* _module, int32_t _channel)
        : guictxtmenu(), moduleInstance(_module), channel(_channel) 
    {
        this->size.x   = 220;
        maxHeight = 0;
        this->fontSize = FONT_SIZE_CTXT_SMALL;
        this->paddingV = 0;
        addEntry(new ctxtmenu_entry_adsr_shape_function_select(moduleInstance, channel, "Shaping", 100));
    }
    bool clickedElement(ctxtmenu_entry* e, int _id) override {
        auto const synth = moduleInstance->getSynth();
        if (_id >= 100) {
            auto lock = synth->lock();
            int clicked = _id - 100;
            if (clicked >= 0 && clicked <= int32_t(DAW::CurveShapingFunction::Exp)) {
                synth->setAdsrShapeMode(channel, DAW::CurveShapingFunction(clicked));
                moduleInstance->onPresetLoaded(); // meh
            }
            return true;
        }
        closeContextMenu();
        return true;
    }
};

class guictr_module_lfo_context_menu final : public guictxtmenu {
    module_synth_gpu* const moduleInstance;
    int32_t channel;
public:
    explicit guictr_module_lfo_context_menu(module_synth_gpu* _module, int32_t _channel)
        : guictxtmenu(), moduleInstance(_module), channel(_channel) 
    {
        this->size.x   = 220;
        maxHeight = 0;
        this->fontSize = FONT_SIZE_CTXT_SMALL;
        this->paddingV = 0;
        using namespace DAW::LFO;
        auto const synth = moduleInstance->getSynth();
        addEntry(new ctxtmenu_lfo_sync(synth, channel, "Sync", 100));
        addEntry(new ctxtmenu_lfo_mode(synth, channel, "Mode", 200));
        addEntry(new DAW::Shape::ctxtmenu_lfo_shape_select("Shape", 400));
        addEntry(new ctxtmenu_lfo_random_mode(synth, channel, "Random", 300));
    }
    bool clickedElement(ctxtmenu_entry* e, int _id) override {
        auto const synth = moduleInstance->getSynth();
        if (_id >= 400) {
            using DAW::Shape::ShapeWaveform;
            auto shapeIdx = _id - 400;
            if (shapeIdx < 0 || shapeIdx > ShapeWaveform::SHAPE_PULSE_INV) {
                return false;
            }
            auto lock = synth->lock();
            auto& params = synth->getLFOParams(channel);
            params.shape.pts = GetShape(ShapeWaveform(shapeIdx));
            synth->setShapeMode(channel);
        } else if (_id >= 300) {
            auto randomIdx = _id - 300;
            auto lock = synth->lock();
            synth->setRandomMode(channel, randomIdx);
        } else if (_id >= 200) {
            auto lock = synth->lock();
            if (_id == 200) {
                synth->setShapeMode(channel);
            } else {
                synth->setRandomMode(channel, -1);
            }
        } else if (_id >= 100) {
            auto lock = synth->lock();
            int flags = synth->getSyncRatio(channel);
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
            synth->setSyncRatio(channel, flags);
            return true;
        }
        closeContextMenu();
        return true;
    }
};
struct _synth_gui_param_knob {
    ParametersSynthGPU param = ParametersSynthGPU::MasterVolume;
    guiknob_pluginparam* knob = nullptr;
};

class guictr_3buttons : public guictr_base {
    public:
    std::array<guibutton, 3> btnPos;
    guictr_3buttons() : guictr_base() {
        padding = 4;
        margin = 2;
        for (int i = 0; i < 3; i++) {
            btnPos[i].setLabel("O" + std::to_string(i));
            btnPos[i].setText(btnPos[i].getLabel());
            add(&btnPos[i]);
        }
        setLayoutMode(autolayout_mode::LAYOUT_HORIZONTAL);
    }
    ~guictr_3buttons() override {
        removeGuis();
    }
    void buttonClicked(guibase* button) override {
        parent->buttonClicked(button);
    }
};
class guictr_module_synth_lfo_container final : public guictr_stacked {
    static constexpr auto N_TRIGGER_MODES = DAW::LFO::LFOTriggerMode::Free+1;
    module_synth_gpu* const moduleInstance;
    int32_t lfoIdx;
    guictr_sampled_curve_shape sampledShaped;
    i_ctr_shape_editor* const shapeEditor;
    guictr_base* lfoShapeCtr;
    guictr_base ctrParams;
    guictr_select_enum ctrParamTriggerMode;
public:
    explicit guictr_module_synth_lfo_container(module_synth_gpu* module, std::vector<_synth_gui_param_knob>& vecParamUI, int32_t _idx) 
        : moduleInstance(module), lfoIdx(_idx), sampledShaped(module),
        shapeEditor(makeShapeEditor()),
        lfoShapeCtr(shapeEditor->getGuiContainer()),
        ctrParams(),
        ctrParamTriggerMode(N_TRIGGER_MODES)
    {
        padding = 0;
        margin = 0;
        setVerticalLayout(true);
        setBackgroundRendered(false);
        setCanMouseHit(false);
        ctrParams.setLayoutMode(autolayout_mode::LAYOUT_HORIZONTAL);
        ctrParams.setBackgroundRendered(false);
        ctrParams.padding = 4;
        // ctrParams.setBackgroundRendered(true);
        // ctrParams.setFlag(FLG_RENDER_LABEL, false);
        ctrParams.setCanMouseHit(true);

        auto* const synth = module->getSynth();
        ctrParamTriggerMode.setBackgroundRendered(false);
        ctrParamTriggerMode.setTooltipText("LFO " + std::to_string(lfoIdx + 1));
        ctrParamTriggerMode.setLayoutMode(autolayout_mode::LAYOUT_HORIZONTAL);
        ctrParamTriggerMode.padding = 2;
        auto paramTrigger = synth->getParam(ParametersSynthGPU(LFO_1_TriggerMode + size_t(lfoIdx * MAX_PARAMS_PER_LFO)));
        dbgassert(paramTrigger);
        for (size_t i = 0; i < N_TRIGGER_MODES; ++i) {
            auto& btn = ctrParamTriggerMode.getButton(i);
            auto name = paramTrigger->getValueDisplay(double(i) / double(N_TRIGGER_MODES - 1));
            btn.setTooltipText(String("LFO " + std::to_string(lfoIdx) + " Trigger Mode: ") + name);
            btn.setText(name);
            btn.setButtonColor(GuiColor::COL_KNOB);
        }
        ctrParamTriggerMode.setAutomationRef(module, PARAM_OFFSET_IMPL + paramTrigger->enumParam);
        shapeEditor->setShapeEditorShapeRef(&synth->getLFOParams(lfoIdx).shape);
        shapeEditor->setShapeEditorCallback([synth, _idx](const DAW::Shape::shape_t& shape, bool bIsDragMove) -> void {
            auto lock = synth->lock();
            auto& synthShape = synth->getLFOParams(_idx).shape;
            synthShape.pts = shape.pts;
            synthShape.eraseDuplicates();
        });
        lfoShapeCtr->setBackgroundRendered(false);
        lfoShapeCtr->setBackgroundRenderedInset(false);
        sampledShaped.setBackgroundRendered(false);
        sampledShaped.setBackgroundRenderedInset(false);
        sampledShaped.setCanMouseHit(false);
        lfoShapeCtr->zOrder = 1;
        for (auto p : {
            LFO_1_Frequency, 
            LFO_1_Phase,
            LFO_1_RampDuration,
        }) {
            auto pOffset = static_cast<decltype(p)>(p + size_t(lfoIdx * MAX_PARAMS_PER_LFO));
            auto pKnob = new guiknob_synthparam_textfield();
            pKnob->setSynthParam(moduleInstance->getSynth(), pOffset);
            pKnob->setFontScale(0.75f);
            pKnob->setAutomationRef(moduleInstance, PARAM_OFFSET_IMPL + pOffset);
            pKnob->setAlignment(gui_textfield::Alignment::Right);
            ctrParams.add(pKnob);
        }
        addEntry(lfoShapeCtr);
        addEntry(&sampledShaped);
        addEntry(&ctrParams);
        add(&ctrParamTriggerMode);
        setSplitters({ 0.45f, 0.9f });
    }
    ~guictr_module_synth_lfo_container() override {
        ctrParams.destroyGuis();
        removeGuis();
        delete shapeEditor->getGuiContainer();
    }
    void layout() override {
        auto titleHeight = getTitleHeight();
        shapeEditor->setInputHeight(math::floorfS32(titleHeight));
        guictr_stacked::layout();
        ivec2 csize = getSizeContent();
        ctrParamTriggerMode.size = { math::min(csize.x/2, 340), titleHeight };
        ctrParamTriggerMode.pos = { csize.x - ctrParamTriggerMode.size.x, 0 };
        ctrParamTriggerMode.layout();
    }
    double getPlayingTick() {
        double t = 0;
#if BUILD_DAW_HOST
        auto dawOptional = dawCtrl ? dawCtrl->getDaw() : nullptr;
        if (dawOptional) {
            t = !dawOptional->isPlaying() ? dawOptional->getIdleTickPos() : dawOptional->getPlaybackPos();
        } else {
#endif
            t = moduleInstance->getHostCallback()->m_vstTimeInfo.ppqPos;
#if BUILD_DAW_HOST
        }
#endif

        return t;
    }
    void prerender(NVGcontext* vg) override {
        guictr_stacked::prerender(vg);
        if (sampledShaped.isVisible()) {
            auto synth = moduleInstance->getSynth();
            auto& params = synth->getLFOParams(lfoIdx);
            auto& shape = sampledShaped.getShape();
            DAW::LFO::LFO lfoCopy = synth->getGlobalLFO(lfoIdx);
            double begin = 0.0;
            double range = 1.0;
            auto* hostInfo = moduleInstance->getHostCallback();
            auto timeSeconds = hostInfo->m_vstTimeInfo.samplePos / hostInfo->m_vstTimeInfo.sampleRate;
            double barDurationInSeconds = toSecondsDD(TICKS_BAR, 1.0 / (hostInfo->m_vstTimeInfo.tempo * 100.0));
            double freq = params.freqHz;
            switch (params.trigger) {
                case DAW::LFO::LFOTriggerMode::Free: {
                    begin = timeSeconds - range;
                    range = barDurationInSeconds * 4.0;
                    sampledShaped.setIsNormalized(false);
                    break;
                }
                case DAW::LFO::LFOTriggerMode::OneShot:
                    begin = -0.2;
                    range = 1.4;
                    freq = 1.0;
                    sampledShaped.setIsNormalized(true);
                    break;
                case DAW::LFO::LFOTriggerMode::NoteOn:
                    range = barDurationInSeconds * 1.0;
                    begin = timeSeconds - range;
                    sampledShaped.setIsNormalized(false);
                    break;
            }
            auto numPoints = samplecount_t(math::clamp(size.x, 16, 1024));
            shape.pts.resize(numPoints);
            shape.flags |= DAW::Shape::ShapeFlags::SHAPE_LOCK_POINTS;
            for (samplecount_t j = 0; j < samplecount_t(numPoints); ++j) {
                auto t = begin + double(j * range) / (numPoints - 1);
                // auto normalizedT = float((t - begin) / range);
                lfoCopy.setPhase(t * freq);
                const auto v = lfoCopy.GetLfo();
                auto& pt = shape.pts[j];
                pt.pos.x = float(t);
                pt.pos.y = float(v);
            }
            auto minmax = DAW::Shape::getMinMax<DAW::Shape::sampled_pt_t>(shape.pts);
            sampledShaped.setMinMax(minmax.first, minmax.second);
            // normalize x axis to 1.0
            auto min = minmax.first.pos.x;
            auto rangeX = minmax.second.pos.x - min;
            if (rangeX > 0.0) {
                for (auto& pt : shape.pts) {
                    pt.pos.x = (pt.pos.x - min) / rangeX;
                }
            }

        }
    }
};
class guictr_synth_main_master_gain final : public guictr_stacked {
    friend class guictr_synth_main_section;
    module_synth_gpu* const moduleInstance;
    guiknob_synthparam knobMasterVolume;
    gui_trackmeter  guiMeter;
public:
    explicit guictr_synth_main_master_gain(module_synth_gpu* module) 
        : moduleInstance(module),
        knobMasterVolume(PARAM_OFFSET_IMPL + MasterVolume, PARAM_OFFSET_IMPL + MasterVolume, module->getSynth(), MasterVolume, guiknob::knobtype::SLIDER_LABELED),
        guiMeter(&moduleInstance->meter)
    {
        setVerticalLayout(false);
        addEntry(&knobMasterVolume);
        addEntry(&guiMeter);
        setSplitters({ 0.5f });
    }
};

class guictr_synth_main_section final : public guictr_stacked {
    module_synth_gpu* const moduleInstance;
    guictr_synth_main_master_gain ctrMasterGain;
    guiknob_synthparam knobVoiceMode;
public:

    explicit guictr_synth_main_section(module_synth_gpu* module, std::vector<_synth_gui_param_knob>& vecParamUI) 
        : moduleInstance(module),
        ctrMasterGain(module),
        knobVoiceMode(PARAM_OFFSET_IMPL + VoiceMode, PARAM_OFFSET_IMPL + VoiceMode, module->getSynth(), VoiceMode, guiknob::knobtype::KNOB_LABELED)
    {
        setVerticalLayout(true);
        (void) moduleInstance;
        addEntry(&ctrMasterGain);
        addEntry(&knobVoiceMode);
        setSplitters({ 0.8f });
        vecParamUI.push_back({ VoiceMode, &knobVoiceMode });
        vecParamUI.push_back({ MasterVolume, &ctrMasterGain.knobMasterVolume });
    }
    ~guictr_synth_main_section() override {
        removeGuis();
    }
};

class guicontainer_plugin_synth_gpu final : public guictr_base {
    module_synth_gpu* const moduleInstance;
    std::vector<_synth_gui_param_knob> vecParamUI;
    guictr_sampled_curve_shape shapeOscWaveform;
    guictr_module_synth_lfo_container ctrLfo1;
    guictr_module_synth_lfo_container ctrLfo2;
    guicontainer_plugin_synth_adsr adsr1;
    guicontainer_plugin_synth_adsr adsr2;
    guicontainer_plugin_synth_other_parameters otherParams;
    guictr_base ctrMacros;
    guicontainer_modulation ctrModulation;
    guictr_stacked ctrHorizontal;
    guictr_stacked ctrStackedOSC;
    guictr_stacked ctrStackedADSR;
    guictr_stacked ctrStackedBothLFOs;
    guictr_stacked ctrStackedBothMacrosAndModulation;
    guictr_synth_main_section ctrMainSection;
    guictr_base ctrOscParams;
    gui_textfield editfield;
    std::vector<guictr_synth_title*> containers;
    seq_rand synthRandUI;
    bool bGuiNeedsRefresh = true;
public:

    explicit guicontainer_plugin_synth_gpu(module_synth_gpu* module) 
        : moduleInstance(module),
        shapeOscWaveform(module),
        ctrLfo1(module, vecParamUI, 0),
        ctrLfo2(module, vecParamUI, 1),
        adsr1(module, 0),
        adsr2(module, 1),
        otherParams(module),
        ctrModulation(dynamic_cast<PluginLockable*>(module->getSynth()), module->getSynth()),
        ctrMainSection(module, vecParamUI)
    {
        padding = 0;
        ctrStackedADSR.setVerticalLayout(true);
        ctrStackedOSC.setVerticalLayout(true);
        ctrStackedBothLFOs.setVerticalLayout(true);
        ctrStackedBothMacrosAndModulation.setVerticalLayout(true);
        auto const synth = module->getSynth();
        auto makeParamKnob = [synth](auto p, auto knobType) {
            return new guiknob_synthparam(PARAM_OFFSET_IMPL + p, PARAM_OFFSET_IMPL + p, synth, p, knobType);
        };
        for (auto p : {
            Osc1Gain,
            Osc1Waveform,
            Osc1UnisonVoiceCount,
            Osc1UnisonDetune,
            Osc1Filter,
            Osc1KeytrackFilter,
            Osc1KeytrackDetune,
            Osc1KeytrackStereoWidth,
            Osc1Coarse,
            Osc1Fine,
            Osc1Stereo,
            Osc1PulseWidth,
            Osc1PulseWidthModDepth,
            Osc1PulseWidthModRate,
        }) {
            auto knob = makeParamKnob(p, guiknob::knobtype::KNOB_LABELED);
            ctrOscParams.add(knob);
            vecParamUI.push_back({ p, knob });
        }
        for (auto p : {
            Macro_1,
            Macro_2,
            Macro_3,
            Macro_4,
        }) {
            auto knob = makeParamKnob(p, guiknob::knobtype::SLIDER_LABELED);
            ctrMacros.add(knob);
            vecParamUI.push_back({ p, knob });
        }
        ctrOscParams.setLayoutMode(autolayout_mode::LAYOUT_GRID);
        ctrMacros.setLayoutMode(autolayout_mode::LAYOUT_GRID);
        auto padding = 4;
        auto margin = 4;
        ctrMainSection.padding      = padding;
        ctrMainSection.margin       = margin;
        ctrOscParams.padding        = padding;
        ctrOscParams.margin         = margin;
        ctrMacros.padding           = padding;
        ctrMacros.margin            = margin;
        ctrLfo1.padding             = padding;
        ctrLfo1.margin              = margin;
        ctrLfo2.padding             = padding;
        ctrLfo2.margin              = margin;
        ctrHorizontal.padding       = padding;
        ctrHorizontal.margin        = margin;
        ctrStackedOSC.padding       = padding;
        ctrStackedOSC.margin        = margin;
        ctrStackedADSR.padding      = 0;
        ctrStackedADSR.margin       = 0;
        adsr2.padding               = padding;
        adsr2.margin                = margin;
        adsr1.padding               = padding;
        adsr1.margin                = margin;
        ctrModulation.padding       = padding;
        ctrModulation.margin        = margin;
        ctrStackedBothLFOs.padding  = 0;
        ctrStackedBothLFOs.margin   = 0;
        ctrStackedBothMacrosAndModulation.padding  = 0;
        ctrStackedBothMacrosAndModulation.margin   = 0;
        bool bRenderBackgroundInset = true;
            ctrStackedOSC.addEntry(&shapeOscWaveform);
            ctrStackedOSC.addEntry(&ctrOscParams);
            ctrStackedOSC.setLabel("OSC 1");
            shapeOscWaveform.setBackgroundRendered(false);
            ctrOscParams.setBackgroundRendered(false);
            ctrStackedOSC.setBackgroundRendered(true);
            ctrStackedOSC.setBackgroundRenderedInset(bRenderBackgroundInset);
        ctrHorizontal.addEntry(&ctrStackedOSC);
            adsr1.setLabel("ADSR 1");
            adsr2.setLabel("ADSR 2");
            adsr1.setBackgroundRendered(true);
            adsr1.setBackgroundRenderedInset(bRenderBackgroundInset);
            adsr2.setBackgroundRendered(true);
            adsr2.setBackgroundRenderedInset(bRenderBackgroundInset);
            otherParams.setBackgroundRendered(true);
            otherParams.setBackgroundRenderedInset(bRenderBackgroundInset);
            ctrStackedADSR.addEntry(&adsr1);
            ctrStackedADSR.addEntry(&adsr2);
            ctrStackedADSR.addEntry(&otherParams);
            ctrStackedADSR.setBackgroundRendered(false);
        ctrHorizontal.addEntry(&ctrStackedADSR);
                ctrLfo1.setLabel("LFO 1");
                ctrLfo1.setBackgroundRendered(true);
                ctrLfo1.setBackgroundRenderedInset(bRenderBackgroundInset);
            ctrStackedBothLFOs.addEntry(&ctrLfo1);
                ctrLfo2.setLabel("LFO 2");
                ctrLfo2.setBackgroundRendered(true);
                ctrLfo2.setBackgroundRenderedInset(bRenderBackgroundInset);
            ctrStackedBothLFOs.addEntry(&ctrLfo2);
        ctrHorizontal.addEntry(&ctrStackedBothLFOs);
                ctrModulation.setLabel("Modulation");
                ctrModulation.setBackgroundRendered(true);
                ctrModulation.setBackgroundRenderedInset(bRenderBackgroundInset);
            ctrStackedBothMacrosAndModulation.addEntry(&ctrModulation);
                ctrMacros.setLabel("Macros");
                ctrMacros.setBackgroundRendered(true);
                ctrMacros.setBackgroundRenderedInset(bRenderBackgroundInset);
            ctrStackedBothMacrosAndModulation.addEntry(&ctrMacros);
        ctrHorizontal.addEntry(&ctrStackedBothMacrosAndModulation);
            ctrMainSection.setLabel("");
            ctrMainSection.setBackgroundRendered(true);
            ctrMainSection.setBackgroundRenderedInset(bRenderBackgroundInset);
        ctrHorizontal.addEntry(&ctrMainSection);
        setLayoutMode(autolayout_mode::LAYOUT_HORIZONTAL);
        add(&ctrHorizontal);
        std::vector splitterPositions = {
            0.27f, 0.52f, 0.74f, 0.92f
        };
        ctrHorizontal.setSplitters(splitterPositions);
        ctrStackedOSC.setSplitters({ 0.25f });
        ctrStackedADSR.setSplitters({ 0.45f, 0.9f });
        ctrStackedBothLFOs.setSplitters({ 0.5f });
        ctrStackedBothMacrosAndModulation.setSplitters({ 0.5f });
        editfield.setFlag(FLG_NO_LAYOUT, true);
        editfield.setVisible(false);
        editfield.setAlignment(gui_textfield::Alignment::Center);
        editfield.setReturnCommits(true);
        add(&editfield);
    }
    ~guicontainer_plugin_synth_gpu() override {
        ctrOscParams.destroyGuis();
        ctrMacros.destroyGuis();
        removeGuis();
        ctrHorizontal.removeEntries();
        ctrStackedOSC.removeEntries();
        ctrStackedADSR.removeEntries();
    }

    void layout() override {
        const auto cs = getSizeContent();
        const auto titleHeight = math::clamp(math::roundfS32(cs.y * 0.05f), 14, 32);
        for (auto& knob : vecParamUI) {
            knob.knob->setLabelsScale(titleHeight*0.75f, titleHeight*1.0f);
            knob.knob->setLabelsFontScale(1.2f, 1.2f);
        }
        ctrMainSection.setTitleHeight(titleHeight);
        ctrLfo1.setTitleHeight(titleHeight);
        ctrLfo2.setTitleHeight(titleHeight);
        ctrModulation.setTitleHeight(titleHeight);
        ctrHorizontal.setTitleHeight(titleHeight);
        ctrStackedOSC.setTitleHeight(titleHeight);
        adsr1.setTitleHeight(titleHeight);
        adsr2.setTitleHeight(titleHeight);
        guictr_base::layout();
    }

    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
        if (this->contains(mpos)) {
            ivec2 localMouse = this->toContainerSpace(mpos);
            // iterate over guis vector in reverse
            for (auto it = guis.rbegin(); it != guis.rend(); ++it) {
                auto gui = *it;
                if (gui->isVisible() && gui->mouseHitTest(localMouse, evt)) {
                    return true;
                }
            }
            if (evt.type == MouseHitType::MOUSE_DRAGDROP_OBJECT) {
                evt.requestFocus(this);
                return true;
            }
            if (canMouseHit()) {
                evt.requestFocus(this);
                return true;
            }
        }
        return false;
    }

    void onChildLayoutChanged(guibase* g) override {
        bGuiNeedsRefresh = true;
        if (this->parent) {
            this->parent->onChildLayoutChanged(this);
        }
    }
    void onTick(AppCtrl* ctrl) override {
        guictr_base::onTick(ctrl);
        if (bGuiNeedsRefresh) {
            ctrModulation.setFromSynth();
            layout();
            bGuiNeedsRefresh = false;
        }
    }

    void onPresetLoaded() {
        bGuiNeedsRefresh = true;
        adsr1.flagNeedsShapeSet();
        adsr2.flagNeedsShapeSet();
    }

    void onSetParameter(int32_t index, float value) {
        if (index == -1) {
            bGuiNeedsRefresh = true;
            auto* const synth = moduleInstance->getSynth();
            for (auto& synthKnob : vecParamUI) {
                if (synthKnob.knob) {
                    auto param = synth->getParam(synthKnob.param);
                    if (!assert_expr(param)) {
                        continue;
                    }
                    synthKnob.knob->setValueInit(float(param->getAsDouble()));
                }
            }
            return;
        }
        auto internalIdx = index - PARAM_OFFSET_IMPL;
        if (moduleInstance->isValidParamIdx(internalIdx)) {
            if (internalIdx >= int32_t(ADSR_1_A_Duration) && internalIdx - int32_t(ADSR_1_A_Duration) < MAX_PARAMS_PER_ADSR)
                adsr1.flagNeedsShapeSet();
            if (internalIdx >= int32_t(ADSR_2_A_Duration) && internalIdx - int32_t(ADSR_2_A_Duration) < MAX_PARAMS_PER_ADSR)
                adsr2.flagNeedsShapeSet();
            for (auto& synthKnob : vecParamUI) {
                if (synthKnob.param == ParametersSynthGPU(internalIdx)) {
                    synthKnob.knob->setValueInit(value);
                }
            }
        }
    }

    void onGuiOpen() {
        for (auto& synthKnob : vecParamUI) {
            if (!synthKnob.knob)
                continue;
            synthKnob.knob->setEffectInstance(moduleInstance);
            auto* param = moduleInstance->getSynth()->getParam(synthKnob.param);
            if (param) {
                synthKnob.knob->setLabel(param->getHierarchicalName());
            }
        }
        bGuiNeedsRefresh = true;
    }

    void onGuiClose() {
        for (auto& synthKnob : vecParamUI) {
            if (!synthKnob.knob)
                continue;
            synthKnob.knob->setEffectInstance(nullptr);
        }
    }

    void rightClicked(MouseEvent& evt, guibase* what) override {
        while (what) {
            int32_t lfoIdx = -1;
            if (what == &this->ctrLfo1) {
                lfoIdx = 0;
            } else if (what == &this->ctrLfo2) {
                lfoIdx = 1;
            }

            if (lfoIdx > -1) {
                parentCtrl->openContextMenu(new guictr_module_lfo_context_menu(moduleInstance, lfoIdx), evt.mousepos);
                break;
            }

            int32_t adsrIdx = -1;
            if (what == &this->adsr1) {
                adsrIdx = 0;
            } else if (what == &this->adsr2) {
                adsrIdx = 1;
            }

            if (adsrIdx > -1) {
                parentCtrl->openContextMenu(new guictr_module_adsr_context_menu(moduleInstance, adsrIdx), evt.mousepos);
                break;
            }

            what = what->parent;
        }
    }

    void getSizeScale(int& w, int& h) {
        auto size = ivec2(1560, 730);
        w = size.x;
        h = size.y;
    }

    void prerender(NVGcontext* vg) override {
        guictr_base::prerender(vg);
        // convert synth impls outputBufferWaveform to shape
        auto synth = moduleInstance->getSynth();
        auto& shape = this->shapeOscWaveform.getShape();
        auto& waveform = synth->ssboOutputWaveform.buffer;
        shape.pts.clear();
        shape.pts.reserve(waveform.size());
        for (size_t i = 0; i < waveform.size(); i++) {
            float x = i / float(waveform.size());
            shape.pts.push_back({{ x, waveform[i] * 0.5 + 0.5 }});
        }
        shape.flags = DAW::Shape::SHAPE_CYCLIC | DAW::Shape::SHAPE_LOCK_POINTS;
    }


    void setUiLayout(const ui_layout_t& layout) {
    }

    bool getUiLayout(ui_layout_t& layout) const {
        return true;
    }

    void buttonClicked(guibase* button) override {
        auto param = dynamic_cast<guiknob_pluginparam*>(button);
        if (param && moduleInstance) {
            auto paramIdx          = param->getParamIdx();
            auto paramValue        = moduleInstance->getParamValueDisplay(paramIdx);
            editfield.mCallbackEnd = [this, param, paramValue, paramIdx](const std::string& str) {
                auto paramConverted = moduleInstance->convertParamValueDisplay(param->getParamIdx(), param_unit_t{ str, paramValue.unit });
                if (paramConverted.success) {
                    moduleInstance->setParamValue(paramIdx, paramConverted.floatVal, FLG_PAR_UPDATE_USER | FLG_PAR_UPDATE_FINISH);
                    if (param->fnValueEditChanged)
                        param->fnValueEditChanged(param->getValue(), paramConverted.floatVal);
                }
                editfield.setVisible(false);
                return true;
            };
            auto layout    = param->getLayout();
            editfield.pos  = button->parent->toParentSpace(layout.pValue);
            editfield.size = layout.sValue;
            editfield.setVisible(true);
            editfield.layout();
            editfield.setValue(paramValue.value);
            editfield.setSelectionRange(-1, -1);
            editfield.setFontSize(layout.valueHeight * layout.fontScaleValue);
            parentCtrl->focusGui(&editfield);
            return;
        }
        guictr_base::buttonClicked(button);
    }
};

class PluginViewContainerSynthGPU final : public PluginViewContainer {
public:
    guicontainer_plugin_synth_gpu ctr_main;
    explicit PluginViewContainerSynthGPU(module_synth_gpu* eff)
        : ctr_main(eff) {
    }
    ~PluginViewContainerSynthGPU() override = default;
    guicontainer_plugin_synth_gpu& getPluginUI() {
        return ctr_main;
    }
    const guicontainer_plugin_synth_gpu& getPluginUI() const {
        return ctr_main;
    }
    void layout(int32_t winW, int32_t winH) override {
        ctr_main.pos  = { 0, 0 };
        ctr_main.size = { winW, winH };
    }
    void addTo(std::vector<guictr_base*>& v) override {
        v.push_back(&ctr_main);
    }
    void onGuiOpen() override {
        ctr_main.onGuiOpen();
    }
    void onGuiClose() override {
        ctr_main.onGuiClose();
    }
    void onSetParameter(int32_t index, float value) override {
        ctr_main.onSetParameter(index, value);
    }
    void getFixedSize(int32_t* w, int32_t* h) override {
        ctr_main.getSizeScale(*w, *h);
    }
    bool isViewSupported(int32_t uiId) const override {
        return uiId != UID_VIEW_CTR_NODES;
    }
    void onPresetLoaded() override {
        ctr_main.onPresetLoaded();
    }
    bool hasMeter() const override {
        return true;
    }
};

std::shared_ptr<PluginViewContainer> SynthImplGPU::createViewCtrImpl() {
    if (this->moduleSynthInstance) {
        this->views.push_back(std::make_shared<PluginViewContainerSynthGPU>(static_cast<module_synth_gpu*>(this->moduleSynthInstance)));
        return this->views.back();
    }
    return nullptr;
}


void module_synth_gpu::getUiSnapshot(snapshot_t& snapshot) {
    for (auto& view : views) {
        auto implCtrType = dynamic_cast<PluginViewContainerSynthGPU*>(view.get());
        ui_layout_t layout{};
        if (implCtrType && implCtrType->getPluginUI().getUiLayout(layout)) {
            layout.uiId = view->getUiId();
            snapshot.uiLayout.push_back(layout);
        }
    }
}

void module_synth_gpu::setUiSnapshot(snapshot_t& snapshot) {
    for (auto& uis : snapshot.uiLayout) {
        std::vector<std::shared_ptr<PluginViewContainer>> views;
        getAllViewCtrs(uis.uiId, views);
        for (auto& view : views) {
            auto implCtrType = dynamic_cast<PluginViewContainerSynthGPU*>(view.get());
            if (implCtrType) {
                implCtrType->getPluginUI().setUiLayout(uis);
            }
        }
    }
}

} // namespace PluginSynth::GPU

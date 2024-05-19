#include "assert_dbg.h"
#include "gui/controls/button.h"
#include "math/seq_math.h"
#include "synth-plugin.h"
#include "synth-template.hpp"
#include "synth-types.hpp"
#include <nanovg_min.h>

namespace PluginSynth {

class SynthImplBasic final : public SynthImpl<SynthImplBasic>, public SynthState {
    static constexpr uint16_t NUM_POLY_VOICES   = 8;
private:

    void initImpl() {
        lfoShape = DAW::Shape::GetShapeSaw(DAW::Shape::SHAPE_SHAPED|DAW::Shape::SHAPE_CYCLIC);
        lfoShape.pts = DAW::Shape::GetShape(DAW::Shape::ShapeWaveform::SHAPE_SAW);
        auto pShape = &lfoShape;
        for (Voice& v : voices) {
            v.osc1a.setShape(pShape);
            v.osc1b.setShape(pShape);
            v.osc2a.setShape(pShape);
            v.osc2b.setShape(pShape);
            v.oscFm.setShape(pShape);
            v.lfo1.setShape(pShape);
            v.lfo2.setShape(pShape);
        }

        auto addParam = [this](SynthParamBase* param, Parameters enumParam) {
            auto idx = static_cast<size_t>(enumParam);
            while (this->vecParams.size() <= idx) {
                this->vecParams.push_back(nullptr);
            }
            this->vecParams[idx] = param;
        };
        auto addFloatParam = [&addParam](Parameters enumParam) -> SynthParam_Float* {
            SynthParam_Float* param = new SynthParam_Float(enumParam);
            addParam(param, enumParam);
            return param;
        };
        auto addIntParam = [&addParam](Parameters enumParam) -> SynthParam_Int* {
            SynthParam_Int* param = new SynthParam_Int(enumParam);
            addParam(param, enumParam);
            return param;
        };
        auto addEnumParam = [&addParam](Parameters enumParam) -> SynthParam_Enum* {
            SynthParam_Enum* param = new SynthParam_Enum(enumParam);
            addParam(param, enumParam);
            return param;
        };
        auto setParamName = [](SynthParamBase* p, String name, String shortName = "", String hierarchicalName = "", String unit = "", String format = "") {
            if (shortName.empty()) {
                p->shortName = name;
            } else {
                p->shortName = std::move(shortName);
            }
            if (hierarchicalName.empty()) {
                p->hierarchicalName = p->shortName.empty() ? name : p->shortName;
            } else {
                p->hierarchicalName = std::move(hierarchicalName);
            }
            p->name = std::move(name);
            p->unit = std::move(unit);
            if (format.empty()) {
                switch (p->type) {
                    case SynthParam::ParamType::FLOAT:
                        p->format = "%.3f";
                        break;
                    case SynthParam::ParamType::INT:
                        p->format = "%d";
                        break;
                    case SynthParam::ParamType::ENUM:
                        p->format = "%s";
                        break;
                }
            } else {
                p->format = std::move(format);
            }
            dbgassert(!p->name.empty());
            dbgassert(!p->shortName.empty());
            dbgassert(!p->hierarchicalName.empty());
        };

        addFloatParam(Parameters::Osc1Fine)->setRange(-1.0, 1.0)->setInitialValue(0.0);
        setParamName(getParam(Parameters::Osc1Fine), "Oscillator 1 fine", "OSC1 Fine", "Fine");
        addIntParam(Parameters::Osc1Coarse)->setRange(-24, 24)->setInitialValue(0);
        setParamName(getParam(Parameters::Osc1Coarse), "Oscillator 1 coarse", "OSC1 Semi", "Semi");
        addEnumParam(Parameters::Osc1Wave)->setStrings(stringsWaveform.begin(), stringsWaveform.end())->setInitialValue(static_cast<int32_t>(Waveforms::Saw));
        setParamName(getParam(Parameters::Osc1Wave), "Osc1 Waveform", "Osc1 Waveform", "Waveform");

        addFloatParam(Parameters::GlideLength)->setRange(0.0, 1.0)->setInitialValue(0.0);
        addFloatParam(Parameters::MasterVolume)->setRange(0.0, 0.5)->setInitialValue(0.25);

        setParamName(getParam(Parameters::GlideLength), "Glide length", "Glide");
        setParamName(getParam(Parameters::MasterVolume), "Volume", "Volume", "Volume", "%");

        addEnumParam(Parameters::VoiceMode)->setStrings(stringsVoiceMode.begin(), stringsVoiceMode.end())->setInitialValue(0);
        setParamName(getParam(Parameters::VoiceMode), "Voice Mode");

        addIntParam(Parameters::Voices)->setRange(1, NUM_POLY_VOICES)->setInitialValue(32);

        setParamName(getParam(Parameters::Voices), "Polyphonic Voice Maximum", "Voices");

        addFloatParam(Parameters::Panning)->setRange(-1.0, 1.0)->setInitialValue(0.0);
        setParamName(getParam(Parameters::Panning), "Stereo Panning", "Pan");
    }
    module_synth_basic* const moduleSynthInstance;
    seq_rand synthRand;
    std::array<Voice, NUM_POLY_VOICES> voices;
    DAW::Shape::shape_t lfoShape;
    std::vector<std::shared_ptr<PluginViewContainer>> views;
public:
    explicit SynthImplBasic(module_synth_basic* module);

    ~SynthImplBasic()
    {
        for (auto* ptr : vecParams) {
            delete ptr;
        }
    }
    void init() override {
        for (auto param : this->vecParams) {
            if (!param) continue;
            OnParamChange(static_cast<Parameters>(param->enumParam));
        }
    }

    void OnParamChange(Parameters parameter) override {
        double value                         = 0.0;
        auto paramInstance                   = getParam(parameter);
        SynthParam_Float* paramFloatOptional = nullptr;
        SynthParam_Int* paramIntOptional     = nullptr;
        SynthParam_Enum* paramEnumOptional   = nullptr;
        switch (paramInstance->getType()) {
            case SynthParam::ParamType::FLOAT:
                paramFloatOptional = static_cast<SynthParam_Float*>(paramInstance);
                value              = paramFloatOptional->Value();
                break;
            case SynthParam::ParamType::INT:
                paramIntOptional = static_cast<SynthParam_Int*>(paramInstance);
                value            = paramIntOptional->Value();
                break;
            case SynthParam::ParamType::ENUM:
                paramEnumOptional = static_cast<SynthParam_Enum*>(paramInstance);
                value             = paramEnumOptional->Value();
                break;
        }

        switch (parameter) {
            case Parameters::Voices: {
                // auto polyVoicesCurrent = this->polyVoiceCount;
                // auto polyVoicesTarget  = paramIntOptional->Value();
                // if (polyVoicesCurrent != polyVoicesTarget) {
                //     this->maxPolyVoiceIndex = math::max(maxPolyVoiceIndex, math::max(polyVoicesCurrent, polyVoicesTarget));
                //     this->polyVoiceCount    = polyVoicesTarget;
                //     for (int i = this->polyVoiceCount; i < this->maxPolyVoiceIndex; ++i) {
                //         voices[i].Release();
                //     }
                // }
                break;
            }
            case Parameters::Osc1Wave:
                // osc1Wave.Switch(value);
                break;
            case Parameters::Osc1Split:
                targetOsc1SplitMix = value != 0.0 ? 1.0 : 0.0;
                osc1SplitFactorA   = pitchFactor(value);
                osc1SplitFactorB   = 1.0;//pitchFactor(value);
                break;
            case Parameters::VoiceMode:
                // switch (GetParamEnum(parameter)->getEnumValue<VoiceModes>()) {
                //     case VoiceModes::Mono:
                //     case VoiceModes::Legato:
                //         // for (int i = 1; i < NUM_POLY_VOICES; i++) {
                //         //     voices[i].Release();
                //         // }
                //         break;
                //     default:
                //         break;
                // }
                break;
            case Parameters::GlideLength:
                glideLength = 1000 - 999.0 * (.5 - .5 * cos(pow(value, .1) * M_PI));
                break;
            case Parameters::MasterVolume:
                targetMasterVolume = value;
                break;
            default:
                break;
        }
    }

    double GetVoiceImplBasic(double dt, Voice& voice, FilterModes filtermode, double& data) {
        auto volEnvValue   = voice.velocity;
        auto osc1Waveform  = GetParamEnum(Parameters::Osc1Wave)->getEnumValue<Waveforms>();
        auto baseFrequency = voice.frequency * voice.pitchBend;
        auto coarse = GetParamInt(Parameters::Osc1Coarse)->Value();
        auto fine   = GetParamFloat(Parameters::Osc1Fine)->Value();
        osc1Tune    = pitchFactor(coarse + fine);
        auto osc1Frequency = osc1Tune * baseFrequency;
        auto out      = 0.0;
        auto osc1Out             = 0.0;
        osc1Out += voice.osc1a.GetWaveform(dt, osc1Frequency, osc1Waveform, true);
        dbgassert(!fp_math::isNanOrInfd(osc1Out));
        out += osc1Out;
        dbgassert(!fp_math::isNanOrInfd(out));
        double volEnvSmoothed = volEnvValue;
        if (voice.bTriggerSmoothing) {
            volEnvSmoothed = voice.prevVolEnv + dt * 8000.0 * (volEnvValue - voice.prevVolEnv);
        }
        voice.prevVolEnv = volEnvSmoothed;
        out *= volEnvSmoothed;
        return out;
    }

    void StartVoice(Voice& v, bool bTriggerMono) {
        auto holdVolEnv = false;//GetParamEnum(Parameters::VolEnvTriggerMode)->Value() == 1;
        auto holdModEnv = false;//GetParamEnum(Parameters::ModEnvTriggerMode)->Value() == 1;
        auto holdLfo1 = false;//GetParamEnum(Parameters::Lfo1TriggerMode)->Value() == 1;
        auto holdLfo1Ramp = false;//GetParamEnum(Parameters::Lfo1RampTriggerMode)->Value() == 1;
        auto holdOsc1Phase = false;//GetParamEnum(Parameters::Osc1PhaseResetMode)->Value() == 1;
        auto holdOsc2Phase = false;//GetParamEnum(Parameters::Osc2PhaseResetMode)->Value() == 1;
        
        // voice.Start(tempo, lfoPhaseDrift);
        // dbgassert(voice.numUnisonActive == unisonVoiceCount);
        // voice.visitVoices([&](Voice& v) 
        {
            bool isSilent = v.volEnv.stage >= EnvelopeStages::Idle || !v.bIsActive;
            // UpdateVoiceEnvelopeModulations(voice, v);
            // UpdateVoiceModulations(voice, v, modSrcData);
            v.bTriggerSmoothing = !isSilent;
            v.Start(holdOsc1Phase, holdOsc2Phase);
            if (!holdVolEnv || isSilent) {
                v.volEnv.Reset();
                v.filter.Reset();
            }
            if (!holdModEnv || isSilent) {
                v.modEnv.Reset();
            }
            if (!holdLfo1Ramp || isSilent) {
                v.lfoEnv.Reset();
            }
            if (!holdLfo1 || isSilent) {
                bool bFadeLfo = !isSilent;//v.volEnv.stage < EnvelopeStages::Idle;
                v.lfo1.initPhase(0, bFadeLfo);
                v.lfo2.initPhase(0, bFadeLfo);
            }
        }
        // );
        // maxUnisonVoice    = math::max(maxUnisonVoice, unisonVoiceCount);
        // maxPolyVoiceIndex = math::max(maxPolyVoiceIndex, static_cast<int32_t>(&voice - &voices[0]) + 1);
    }

    void FlushMidi(int sample) {
        while (!midiQueue.Empty()) {
            auto message = midiQueue.Peek();
            if (message.mOffset > sample) break;

            auto voiceMode      = GetParamEnum(Parameters::VoiceMode)->getEnumValue<VoiceModes>();
            auto status         = message.StatusMsg();
            auto ctrl           = message.ControlChangeIdx();
            auto note           = message.NoteNumber();
            auto velocity       = pow(message.Velocity() * .0078125, 1.25);

            if (status == IMidiMsg::kNoteOn && velocity == 0) status = IMidiMsg::kNoteOff;

            switch (status) {
                case IMidiMsg::kNoteOff:
                    heldNotes.erase(
                        std::remove(
                                std::begin(heldNotes),
                                std::end(heldNotes),
                                note),
                        std::end(heldNotes));

                    switch (voiceMode) {
                        case VoiceModes::Poly:
                            for (auto& voice : voices)
                                if (voice.note == note) voice.Release();
                            break;
                        case VoiceModes::Mono:
                        case VoiceModes::Legato:
                            if (heldNotes.empty())
                                voices[0].Release();
                            else
                                voices[0].SetNote(heldNotes.back());
                            break;
                        default:
                            break;
                    }
                    break;
                case IMidiMsg::kNoteOn:
                    switch (voiceMode) {
                        case VoiceModes::Poly: {
                            // get the quietest voice, prioritizing voices that are released
                            auto voiceEnd = std::end(voices);
                            auto voice    = std::min_element(
                                    std::begin(voices),
                                    voiceEnd,
                                    [](auto& a, auto& b) {
                                        bool aReleased = !a.bIsActive;
                                        if (aReleased == !b.bIsActive) {
                                            auto volA = a.GetVolume();
                                            auto volB = b.GetVolume();
                                            // if (volA <= 0.0 && volB <= 0.0) {
                                            //     return a.seqNr < b.seqNr;
                                            // }
                                            return volA < volB;
                                        }
                                        return aReleased;
                                    });
                            // dbgassert(voice->getNumUnisonVoices() == unisonVoiceCount);
                            voice->SetNote(note);
                            voice->SetVelocity(velocity);
                            voice->ResetPitch();
                            StartVoice(*voice, false);
                            // voice->seqNr = seq++;
                            break;
                        }
                        default:
                        case VoiceModes::Mono:
                            voices[0].SetNote(note);
                            voices[0].SetVelocity(velocity);
                            StartVoice(voices[0], true);
                            // voices[0].seqNr = 1;
                            break;
                        case VoiceModes::Legato:
                            voices[0].SetNote(note);
                            if (heldNotes.empty()) {
                                voices[0].SetVelocity(velocity);
                                voices[0].ResetPitch();
                                StartVoice(voices[0], true);
                                // voices[0].seqNr = 1;
                            }
                            break;
                    }

                    heldNotes.push_back(note);
                    break;
                case IMidiMsg::kPitchWheel: {
                    auto pitchBendFactor = pitchFactor(message.PitchWheel() * 2.0);
                    for (auto& voice : voices) voice.SetPitchBendFactor(pitchBendFactor);
                    break;
                }
                case IMidiMsg::kControlChange: {
                    switch (ctrl) {
                        case IMidiMsg::kAllNotesOff:
                            for (auto& voice : voices) {
                                voice.Release();
                            }
                            heldNotes.clear();
                            break;
                        default:
                            break;
                    }
                } break;
                default:
                    log_lf(Log::L_WARN, "Unhandled midi msg %d\n", (int32_t) status);
                    break;
            }
            midiQueue.Remove();
        }
    }

    void ProcessSynth(AudioBlock* in, float * const * outputs, int nFrames, const DAW::Host::Host* const host, double tick, playback_state state) override {
        // lockProcessing only locks VST2 versions of the plugin
        auto lock = this->lockProcessing();

        const auto voiceMode         = GetParamEnum(Parameters::VoiceMode)->getEnumValue<VoiceModes>();
        const FilterModes filterMode = FilterModes::Off;//GetParamEnum(Parameters::FilterMode)->getEnumValue<FilterModes>();
        const bool bIsGlideEnabled   = voiceMode != VoiceModes::Poly;
        const auto dt                = oneOverSR;
        float* synthOutputs[2]       = {};
        synthOutputs[0]              = outputs[0];
        synthOutputs[1]              = outputs[1];
        int nOversample              = 1;

        /**
            * framesPerAutomationUpdate 
            * 1 is highest precission, automation is updated every sample
            * this can be lowered to lower CPU load
            */
        // int framesPerAutomationUpdate = state == playback_state::status_render ? 1 : 8;
        for (int s = 0; s < nFrames; s++) {
            /* if (host && moduleSynthUnisonInstance && (s % framesPerAutomationUpdate) == 0) {
                ReadAutomation(host, tick, state, s, nFrames, nOversample);
            } */
            if (s % nOversample == 0) {
                FlushMidi(s / nOversample);
            }
            for (auto& v : voices) {
                if (bIsGlideEnabled) {
                    v.frequency += (v.targetFrequency - v.frequency) * glideLength * dt;
                }
                v.volEnv.Update(dt);
            }

            auto outL = 0.0;
            auto outR = 0.0;
            for (auto& v : voices) {
                if (v.bIsActive) {
                    v.bIsActive = v.isVoiceActive(filterMode);
                }
            }
            for (auto& v : voices) {
                if (v.bIsActive) {
                    auto voiceVolume = GetParamFloat(Parameters::MasterVolume)->Value();
                    // auto noise = (synthRand.rng_double()*2-1)*0.002;
                    auto vData                = -1.0;
                    double vVal               = GetVoiceImplBasic(dt, v, filterMode, vData);
                    auto voice                = vVal * voiceVolume;
                    auto panningMinusOneToOne = GetParamFloat(Parameters::Panning)->Value();
                    auto panningUnipolar      = panningMinusOneToOne * 0.5 + 0.5;
                    outR += voice * sqrt(panningUnipolar);
                    outL += voice * sqrt(1.0 - panningUnipolar);
                }
            }
            synthOutputs[0][s] = fp_math::silenceNanInff(static_cast<float>(outL));
            synthOutputs[1][s] = fp_math::silenceNanInff(static_cast<float>(outR));
        }
    }
    std::shared_ptr<PluginViewContainer> createViewCtrImpl() override;
};


class module_synth_basic final : public module_synth_template<SynthImplBasic> {
public:
    explicit module_synth_basic(int32_t _projectGlobalId, IHostCallback* _hostCallback)
        : module_synth_template<SynthImplBasic>(new SynthType(this), "Synth Basic", _projectGlobalId, _hostCallback)
    {
        bCanReceiveMidi = true;
        isSynth = true;
        for (const auto& paramEntry : vecParams) {
            if (!paramEntry)
                continue;
            int idx = PARAM_ENABLE + 1 + (&paramEntry - &vecParams.front());
            automatable_param_t* regparam = registerParam(idx);
            dbgassert(regparam && regparam->idx > 0);
            regparam->setInitial(paramEntry->getAsDouble());
            regparam->name  = paramEntry->shortName;
            regparam->unit  = paramEntry->unit;
            switch (paramEntry->type) {
                case SynthParam::ParamType::FLOAT:
                    break;
                case SynthParam::ParamType::INT:
                case SynthParam::ParamType::ENUM:
                    auto paramInt = dynamic_cast<SynthParam_Int*>(paramEntry);
                    dbgassert(paramInt);
                    auto params = paramInt->iMax - paramInt->iMin;
                    regparam->quantizationSteps = params;
                    break;
            }
            switch (paramEntry->enumParam) {
                case Parameters::Osc1Fine:
                case Parameters::Osc2Fine:
                case Parameters::FmFine:
                case Parameters::ModEnvFm:
                case Parameters::VolEnvFm:
                case Parameters::OscMix:
                case Parameters::LfoFm:
                case Parameters::FmCoarse:
                case Parameters::Osc1Coarse:
                case Parameters::Osc2Coarse:
                case Parameters::FilterDrive:
                case Parameters::Osc1Split:
                case Parameters::Osc2Split:
                case Parameters::LfoPhase:
                case Parameters::FilterCutoff:
                case Parameters::FilterKeyTracking:
                case Parameters::VolEnvCutoff:
                case Parameters::ModEnvCutoff:
                case Parameters::LfoCutoff:
                case Parameters::Panning:
                case Parameters::LfoShape:
                    regparam->isBiPolar = true;
                    break;
                default:
                    break;
            }
        }
        impl->init();
    }

    ~module_synth_basic() override {
        delete impl;
    }

    PluginType getPluginType() override { return PLUGIN_TYPE_SYNTH_BASIC; };

    std::shared_ptr<PluginViewContainer> createViewCtrInternal() override {
        return this->impl->createViewCtrImpl();
    }
};

static constexpr auto N_WAVEFORMS = size_t(Waveforms::NumWaveforms);
static constexpr auto N_VOICEMODES = size_t(VoiceModes::NumVoiceModes);
template<int N>
class guictr_synth_select_enum final : public guictr_base, public DAW::UI::IModulateable {
    class guibutton_select_enum : public guibutton {
    public:
        guibutton_select_enum() = default;
        void renderButtonLabel(NVGcontext* vg, int32_t stateFlags) override {
            nvgSave(vg);
            if (setScissorTransform(vg)) {
                static_cast<guictr_synth_select_enum<N>*>(parent)->renderButtonLabel(this, vg, stateFlags);
            }
            nvgRestore(vg);
        }
        bool getState() const override {
            return static_cast<guictr_synth_select_enum<N>*>(parent)->getButtonStateState(this);
        }
    };
    std::array<guibutton_select_enum, N> buttons;
    automatable_t* paramAutomatable = nullptr;
    int32_t paramIdx                = -1;
public:
    std::function<void(guibutton*, int32_t, NVGcontext*, int32_t, ivec2)> fnRenderButtonLabel;
    guictr_synth_select_enum() {
        padding = 0;
        setLayoutMode(autolayout_mode::LAYOUT_VERTICAL);
        for (size_t i = 0; i < N; ++i) {
            buttons[i].id = i;
            add(&buttons[i]);
        }
    }
    ~guictr_synth_select_enum() override {
        removeGuis();
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
};
class guicontainer_plugin_synth_basic final : public guictr_base {
    module_synth_basic* const moduleInstance;
    guictr_synth_select_enum<N_WAVEFORMS> ctr_waveform;
    guictr_synth_select_enum<N_VOICEMODES> ctr_voicemode;
public:

    explicit guicontainer_plugin_synth_basic(module_synth_basic* module) : moduleInstance(module)
    {
        padding = 0;
        margin  = 0;
        setBackgroundRendered(false);
        for (size_t i = 0; i < N_WAVEFORMS; ++i) {
            auto& btn = ctr_waveform.getButton(i);
            const auto& name = stringsWaveform[i];
            btn.setTooltipText(String("Select ") + name);
            btn.setText(name);
            btn.setButtonColor(GuiColor::COL_BTN_BG_SHOW_ACTIVE);
        }
        ctr_waveform.fnRenderButtonLabel = [this](guibutton* button, int32_t idx, NVGcontext* vg, int32_t stateFlags, ivec2 size) {
            auto waveform = static_cast<Waveforms>(idx);
            ivec2 renderFrame = size;
            ivec2 renderPos(0);
            // auto fontScale = math::clamp(math::min(size.y, size.x), 4, 48) * FONT_AUTOSCALE;
            // renderCenteredMultilineText(vg, theme, str, fontScale, getLabelColor(), renderPos, renderFrame);
            auto col = theme->getColor(GuiColor::COL_TEXT);
            int32_t waveformIdx = -1;
            switch (waveform) {
                case Waveforms::Sine:
                    waveformIdx = DAW::Shape::ShapeWaveform::SHAPE_SINE;
                    break;
                case Waveforms::Triangle:
                    waveformIdx = DAW::Shape::ShapeWaveform::SHAPE_TRIANGLE;
                    break;
                case Waveforms::Saw:
                    waveformIdx = DAW::Shape::ShapeWaveform::SHAPE_SAW;
                    break;
                case Waveforms::Square:
                    waveformIdx = DAW::Shape::ShapeWaveform::SHAPE_SQUARE;
                    break;
                case Waveforms::Pulse:
                case Waveforms::Shaper:
                case Waveforms::Noise:
                case Waveforms::NumWaveforms:
                    break;
            }
            if (waveformIdx >= 0) {
                float inset = math::max(4.0f, size.x / 10.0f) * 1.8f;
                auto renderFrameIcon = renderFrame - ivec2(inset * 2);
                if (renderFrameIcon.x > 0 && renderFrameIcon.y > 0)
                    drawWaveform(vg, renderPos + ivec2(inset), renderFrameIcon, waveformIdx, col);
            }
            drawSquareInset(vg, renderPos, renderFrame, theme->getColor(getBackgroundColor()), 0, 0);
        };
        for (size_t i = 0; i < N_VOICEMODES; ++i) {
            auto& btn = ctr_voicemode.getButton(i);
            const auto& name = stringsVoiceMode[i];
            btn.setTooltipText(String("Select ") + name);
            btn.setText(name);
            btn.setButtonColor(GuiColor::COL_BTN_BG_SHOW_ACTIVE);
        }
        add(&ctr_waveform);
        add(&ctr_voicemode);
    }

    ~guicontainer_plugin_synth_basic() override {
        removeGuis();
    }

    // void onTick(AppCtrl* ctrl) override {
    //     guictr_base::onTick(ctrl);
    // }

    void onSetParameter(int32_t index, float value) {
    }

    void getSizeScale(int& w, int& h) const {
        h = 720;
        // w = h / N_WAVEFORMS + h / N_VOICEMODES;
        w = h / (N_WAVEFORMS + N_VOICEMODES);
    }

    void layout() override {
        auto bHeight = size.y / 10;

        ctr_voicemode.pos  = { 0, 0 };
        ctr_voicemode.size = { bHeight, (bHeight * ctr_voicemode.getNumButtons()) / 2 };
        ctr_waveform.pos  = ctr_voicemode.getLeftBottom();
        ctr_waveform.size = { bHeight, bHeight * ctr_waveform.getNumButtons() };
        guictr_base::layout();
    }

    void onGuiOpen() {
        ctr_waveform.setAutomationRef(moduleInstance, 1 + Parameters::Osc1Wave);
        ctr_voicemode.setAutomationRef(moduleInstance, 1 + Parameters::VoiceMode);
    }

    void onGuiClose() {
        ctr_waveform.setAutomationRef(nullptr, -1);
        ctr_voicemode.setAutomationRef(nullptr, -1);
    }
};

class PluginViewContainerSynthBasic final : public PluginViewContainer {
public:
    guicontainer_plugin_synth_basic ctr_main;
    explicit PluginViewContainerSynthBasic(module_synth_basic* eff)
        : ctr_main(eff) {
    }
    ~PluginViewContainerSynthBasic() override = default;
    guicontainer_plugin_synth_basic& getPluginUI() {
        return ctr_main;
    }
    const guicontainer_plugin_synth_basic& getPluginUI() const {
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
};

std::shared_ptr<PluginViewContainer> SynthImplBasic::createViewCtrImpl() {
    if (this->moduleSynthInstance) {
        this->views.push_back(std::make_shared<PluginViewContainerSynthBasic>(this->moduleSynthInstance));
        return this->views.back();
    }
    return nullptr;
}

SynthImplBasic::SynthImplBasic(module_synth_basic* module)
    : SynthImpl<SynthImplBasic>(module),
    moduleSynthInstance(module)
{
    initImpl();
}

} // namespace PluginSynth

template<>
effectbase* makeInstance<PluginSynth::module_synth_basic>(int32_t _projectGlobalId, IHostCallback* _hostCallback) {
    return new PluginSynth::module_synth_basic(_projectGlobalId, _hostCallback);
}

#include "synth-template.hpp"

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
        int framesPerAutomationUpdate = state == playback_state::status_render ? 1 : 8;
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

class guicontainer_plugin_synth_basic final : public guictr_base {

public:

    explicit guicontainer_plugin_synth_basic(module_synth_basic* module)
    {
        padding = 0;
        margin  = 0;
        setBackgroundRendered(false);
    }

    ~guicontainer_plugin_synth_basic() override {
    }

    void layout() override {
        guictr_base::layout();
    }

    void onTick(AppCtrl* ctrl) override {
        guictr_base::onTick(ctrl);
    }

    void onSetParameter(int32_t index, float value) {
    }

    void getSizeScale(int& w, int& h) const {
        w = 1280*1.25;
        h = 720;
    }

    void onGuiOpen() {
    }

    void onGuiClose() {
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

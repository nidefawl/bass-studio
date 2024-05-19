#include "assert_dbg.h"
#include "event.h"
#include "gui/container/container.h"
#include "gui/contextmenu/contextmenu_base.h"
#include "gui/controls/button.h"
#include "gui/shape/shapeeditor.h"
#include "host/shape/shape.h"
#include "math/seq_math.h"
#include "note.h"
#include "rand.h"
#include "renderresources.h"
#include "saferef.h"
#include "seq_time.h"
#include "synth-plugin.h"
#include "synth-snapshot.h"
#include "synth-template.hpp"
#include "synth-types.hpp"
#include <nanovg.h>
#include <nanovg_min.h>

class ctxtmenu_toggle_setting final : public ctxtmenu_entry {
public:
    ctxtmenu_toggle_setting(String _title, int _id)
        : ctxtmenu_entry(_title, _id)
    {
        auto icon = ICON_PLUS;
        setIcon(&RenderResources::imgIcons[icon], GuiColor::COL_WHITE);
    }

    void render(ivec2 p, NVGcontext* vg, int, ivec2 mouse) override {
        ctxtmenu_entry::render(p, vg, 0, mouse);
    }
};
namespace PluginSynth {
    
struct VoiceSynth {
    std::array<double, 64> modValues{};
    std::array<float, 8> envelopeValuesCached{};

    double velocity     = 0.0;
    int32_t indexUnison = 0;
    note_t noteT = {};
    Envelope volEnv;
    Oscillator osc1a;
    seq_rand rand;
    // double driftVelocity   = 0.0;
    // double driftPhase      = 0.0;
    // double driftValue      = 0.0;
    double frequency       = 0.0;
    double targetFrequency = 0.0;
    double pitchBend       = 1.0;
    bool bIsActive         = false;
    // double prevVolEnv = 0.0;
    // double prevCutoff = 0.0;

    double getRandom() {
        return rand.rng_double();
    }

    double getRandomPhase() {
        return rand.rng_double() * 0.5;
    }

    bool isVoiceActive(const FilterModes mode) const {
        if (hint_likely(!bIsActive)) {
            return false;
        }
        return this->volEnv.stage < EnvelopeStages::Idle;
        // return this->volEnv.stage < EnvelopeStages::Idle || !this->filter.IsSilent(mode);
        // return true;
    }

    bool IsReleased() const { return volEnv.IsReleased(); }
    double GetVolume() const { return volEnv.value; }

    void ResetPhases(bool bRandomPhase) {
    }

    void ResetEnvelopes() {
        volEnv.Reset();
    }

    void Release() {
        volEnv.Release();
    }

    void SetNote(const note_t& n) {
        noteT  = n;
        targetFrequency = pitchToFrequency(noteT.pitch);
    }

    void SetPitchBendFactor(double f) { pitchBend = f; }
    void ResetPitch() { frequency = targetFrequency; }
    void SetVelocity(double v) { velocity = v; }

    void Start(bool holdOsc1Phase, bool holdOsc2Phase) {
        bIsActive = true;
        if (!holdOsc1Phase) {
            osc1a.phase = rand.rng_double();
        }
        volEnv.Start();
    }
};
}
namespace PluginSynth::Mono {
enum ParametersSynthMono {
    MasterVolume = 0,
    VoiceMode,
    GlideLength,
    Osc1Wave,
    Osc1Coarse,
    Osc1Fine,
    Panning,
    Osc1PhaseResetMode,
};

const std::array<const char*, 2> stringsVoiceModeMono = {
    "Mono", "Legato"
};

struct ui_layout_t {
    int32_t uiId = 0;
};
struct snapshot_t {
    int32_t version = 0;
    std::vector<param_float_snapshot_t> params;
    std::vector<ui_layout_t> uiLayout;
};
static constexpr int32_t SYNTH_MONO_SNAPSHOT_VERSION = 1;

std::shared_ptr<std::vector<std::byte>> serializeSnapshot(const snapshot_t& snapshot) {
    dbgassert(snapshot.version == SYNTH_MONO_SNAPSHOT_VERSION);
    auto shrdHeapVec = std::make_shared<std::vector<std::byte>>();
    shrdHeapVec->resize(256);
    DAW::ByteBuffer::stream_write<std::vector<std::byte>> out{*shrdHeapVec, 0};
    out.write(size_t(0));
    out.write(snapshot.version);
    out.write(size_t{snapshot.params.size()});
    out.write(size_t{snapshot.uiLayout.size()});
    for (const auto& p : snapshot.params) {
        out.write(p.paramIdx);
        out.write(p.value);
    }
    for (const auto& modulation : snapshot.uiLayout) {
        out.write(modulation.uiId);
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
    if (snapshot.version > SYNTH_MONO_SNAPSHOT_VERSION)
        return false;
    size_t numParams = 0;
    size_t numUiLayouts = 0;
    if (!in.read(numParams) || numParams > 1000)
        return false;
    if (!in.read(numUiLayouts) || numUiLayouts > 1000)
        return false;
    snapshot.params.resize(numParams);
    snapshot.uiLayout.resize(numUiLayouts);

    for (auto& p : snapshot.params) {
        if (!in.read(p.paramIdx))
            return false;
        if (!in.read(p.value))
            return false;
    }
    for (auto& layout : snapshot.uiLayout) {
        if (!in.read(layout.uiId))
            return false;
    }
    snapshotOut = std::move(snapshot);
    return true;
}

class SynthStateMono {
public:
    double osc1Tune                = 1.0;
    double glideLength             = 0.0;
    double targetMasterVolume      = 0.0;
    double masterVolume            = 0.0;
};
class SynthImplMono final : public ::PluginSynth::SynthImpl<PluginSynth::Mono::SynthImplMono, PluginSynth::Mono::ParametersSynthMono>, public SynthStateMono {
    static constexpr uint16_t NUM_POLY_VOICES   = 8;
private:

    SynthParamBase* getParamTest(Parameters enumParam) {
        if (enumParam >= 0 && enumParam < vecParams.size()) {
            return vecParams[enumParam];
        }
        return nullptr;
    }
    const SynthParamBase* getParamTest(Parameters enumParam) const {
        if (enumParam >= 0 && enumParam < vecParams.size()) {
            return vecParams[enumParam];
        }
        return nullptr;
    }
    void initImpl() {
        lfoShape = DAW::Shape::GetShapeSaw(DAW::Shape::SHAPE_SHAPED|DAW::Shape::SHAPE_CYCLIC);
        lfoShape.pts = DAW::Shape::GetShape(DAW::Shape::ShapeWaveform::SHAPE_SAW);
        auto pShape = &lfoShape;
        for (auto& v : voices) {
            v.osc1a.setShape(pShape);
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
        setParamName(getParamTest(Parameters::Osc1Fine), "Oscillator 1 fine", "OSC1 Fine", "Fine");
        addIntParam(Parameters::Osc1Coarse)->setRange(-24, 24)->setInitialValue(0);
        setParamName(getParam(Parameters::Osc1Coarse), "Oscillator 1 coarse", "OSC1 Semi", "Semi");
        addEnumParam(Parameters::Osc1Wave)->setStrings(stringsWaveform.begin(), stringsWaveform.end())->setInitialValue(static_cast<int32_t>(Waveforms::Saw));
        setParamName(getParam(Parameters::Osc1Wave), "Osc1 Waveform", "Osc1 Waveform", "Waveform");

        addFloatParam(Parameters::GlideLength)->setRange(0.0, 1.0)->setInitialValue(0.0);
        addFloatParam(Parameters::MasterVolume)->setRange(0.0, 0.5)->setInitialValue(0.25);

        setParamName(getParam(Parameters::GlideLength), "Glide length", "Glide");
        setParamName(getParam(Parameters::MasterVolume), "Volume", "Volume", "Volume", "%");
        addEnumParam(Parameters::VoiceMode)->setStrings(stringsVoiceModeMono.begin(), stringsVoiceModeMono.end())->setInitialValue(0);
        setParamName(getParam(Parameters::VoiceMode), "Voice Mode");
        addEnumParam(Parameters::Osc1PhaseResetMode)->setStrings(stringsReset.begin(), stringsReset.end())->setInitialValue(0);
        setParamName(getParam(Parameters::Osc1PhaseResetMode), "OSC1 phase reset mode", "OSC1 phase reset", "OSC1 phase reset");
        addFloatParam(Parameters::Panning)->setRange(-1.0, 1.0)->setInitialValue(0.0);
        setParamName(getParam(Parameters::Panning), "Stereo Panning", "Pan");
    }
    module_synth_template<SynthImplMono>* const moduleSynthInstance;
    seq_rand synthRand;
    std::array<VoiceSynth, 1> voices;
    DAW::Shape::shape_t lfoShape;
    std::vector<std::shared_ptr<PluginViewContainer>> views;
public:
    explicit SynthImplMono(module_synth_template<SynthImplMono>* module);

    ~SynthImplMono()
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

    double GetVoiceImplBasic(double dt, VoiceSynth& voice, FilterModes filtermode, double tickPos) {
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
        if (voice.noteT.len > 0) {
            const float noteProgress = voice.noteT.end() - tickPos;
            const float fFadeLen = 128.0f;
            float fFadeIn = math::smoothstep(math::clamp(noteProgress / fFadeLen, 0.0f, 1.0f));
            float fFadeOut = math::smoothstep(math::clamp((voice.noteT.len - noteProgress) / fFadeLen, 0.0f, 1.0f));
            volEnvSmoothed = volEnvValue * fFadeIn * fFadeOut;
        }
        out *= volEnvSmoothed;
        return out;
    }

    void StartVoice(VoiceSynth& v, bool bTriggerMono) {
        auto holdOsc1Phase = GetParamEnum(Parameters::Osc1PhaseResetMode)->Value() == 1;
        // bool isSilent = v.volEnv.stage >= EnvelopeStages::Idle || !v.bIsActive;
        v.Start(holdOsc1Phase, holdOsc1Phase);
        if (!holdOsc1Phase) {
            v.osc1a.phase = 0.0;
        }
        v.volEnv.Reset();
    }

    void FlushMidi(int sample) {
        while (!midiQueue.Empty()) {
            auto message = midiQueue.Peek();
            if (message.mOffset > sample) break;

            auto voiceMode      = GetParamEnum(Parameters::VoiceMode)->getEnumValue<VoiceModesMono>();
            auto status         = message.StatusMsg();
            auto ctrl           = message.ControlChangeIdx();
            auto velocity       = pow(message.Velocity() * .0078125, 1.25);

            if (status == IMidiMsg::kNoteOn && velocity == 0) status = IMidiMsg::kNoteOff;
            note_t noteDaw;
            if (message.note) {
                noteDaw = message.note.value();
            } else {
                noteDaw = note_t{
                    .pitch = message.NoteNumber(),
                    .velocity = message.Velocity(),
                    .time = math::floordS32(moduleSynthInstance->hostCallback->m_vstTimeInfo.ppqPos),
                    .len = TICKS_QUARTER,
                    .flags = NoteFlags::ENABLED | NoteFlags::IS_HELD | NoteFlags::REALTIME,
                    .channel = static_cast<int8_t>(message.Channel()),
                };
            }
            switch (status) {
                case IMidiMsg::kNoteOff:
                    heldNotes.erase(
                        std::remove_if(
                                std::begin(heldNotes),
                                std::end(heldNotes),
                                [noteB=noteDaw](const auto& noteA) { return noteA.pitch == noteB.pitch && noteA.channel == noteB.channel; }),
                        std::end(heldNotes));

                    switch (voiceMode) {
                        case VoiceModesMono::Mono:
                        case VoiceModesMono::Legato:
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
                        default:
                        case VoiceModesMono::Mono:
                            voices[0].SetNote(noteDaw);
                            voices[0].SetVelocity(velocity);
                            StartVoice(voices[0], true);
                            // voices[0].seqNr = 1;
                            break;
                        case VoiceModesMono::Legato:
                            voices[0].SetNote(noteDaw);
                            if (heldNotes.empty()) {
                                voices[0].SetVelocity(velocity);
                                voices[0].ResetPitch();
                                StartVoice(voices[0], true);
                                // voices[0].seqNr = 1;
                            }
                            break;
                    }

                    heldNotes.push_back(noteDaw);
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

        const FilterModes filterMode = FilterModes::Off;//GetParamEnum(Parameters::FilterMode)->getEnumValue<FilterModes>();
        const bool bIsGlideEnabled   = true;
        const auto dt                = oneOverSR;
        float* synthOutputs[2]       = {};
        synthOutputs[0]              = outputs[0];
        synthOutputs[1]              = outputs[1];
        int nOversample              = 1;
        auto bpm100 = host->prjGlobals.tempo100;

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
            auto tickPosOffset = tick + sampleToTickConvert<double, roundmode::none>(s, bpm100, host->m_sampleFormatInternal.sampleRate * nOversample);
            for (auto& v : voices) {
                if (v.bIsActive) {
                    auto voiceVolume = GetParamFloat(Parameters::MasterVolume)->Value();
                    // auto noise = (synthRand.rng_double()*2-1)*0.002;
                    double vVal               = GetVoiceImplBasic(dt, v, filterMode, tickPosOffset);
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
    DAW::Shape::shape_t& getShape() {
        return lfoShape;
    }

    bool getSnapshot(snapshot_t& snapshot) const {
        snapshot.version     = SYNTH_MONO_SNAPSHOT_VERSION;
        const auto numParams = CtrSize(vecParams);
        snapshot.params.reserve(numParams);
        for (int32_t i = 0; i < numParams; ++i) {
            if (!vecParams[i]) continue;
            // dbgassert(vecParams[i]->getAsDouble() >= 0.0 && vecParams[i]->getAsDouble() <= 1.0);
            snapshot.params.push_back({ i, vecParams[i]->getAsDouble() });
        }
        return true;
    }

    bool setSnapshot(const snapshot_t& snapshot) {
        if (snapshot.version != SYNTH_MONO_SNAPSHOT_VERSION) {
            return false;
        }
        const auto numParams = CtrSize(vecParams);
        
        for (auto& param : vecParams) {
            if (!param) continue;
            param->resetToInitial();
            dbgassert(param->getAsDouble() >= 0.0 && param->getAsDouble() <= 1.0);
        }
        for (auto& ps : snapshot.params) {
            if (ps.paramIdx >= 0 && ps.paramIdx < numParams) {
                if (!vecParams[ps.paramIdx]) continue;
                vecParams[ps.paramIdx]->set(math::clamp(ps.value, 0.0, 1.0), math::clamp(ps.value, 0.0, 1.0));
            } else {
                dbgassert(0);
            }
        }

        for (auto& param : vecParams) {
            if (!param) continue;
            OnParamChange(static_cast<SynthImplMono::Parameters>(param->enumParam));
        }
        return true;
    }
};


class module_synth_mono final : public module_synth_template<SynthImplMono> {
public:
    explicit module_synth_mono(int32_t _projectGlobalId, IHostCallback* _hostCallback)
        : module_synth_template<SynthImplMono>(new SynthType(this), "Synth Mono", _projectGlobalId, _hostCallback)
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
            using P = ParametersSynthMono;
            switch (paramEntry->enumParam) {
                case P::Osc1Fine:
                case P::Osc1Coarse:
                case P::Panning:
                    regparam->isBiPolar = true;
                    break;
                default:
                    break;
            }
        }
        impl->init();
    }

    ~module_synth_mono() override {
        delete impl;
    }

    PluginType getPluginType() override { return PLUGIN_TYPE_SYNTH_MONO; };

    std::shared_ptr<PluginViewContainer> createViewCtrInternal() override {
        return this->impl->createViewCtrImpl();
    }
    std::shared_ptr<std::vector<std::byte>> storePresetData() override {
        snapshot_t snapshot;
        if (impl->getSnapshot(snapshot)) {
            getUiSnapshot(snapshot);
            return serializeSnapshot(snapshot);
        }
        return nullptr;
    }

    bool loadPresetData(const std::shared_ptr<std::vector<std::byte>>& buf) override {
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

    void onPresetLoaded() {
        for (int32_t idx = 0; idx < CtrSize(vecParams); idx++) {
            if (!vecParams[idx]) {
                continue;
            }
            auto param = getParam(idx + 1);
            param->setAll(vecParams[idx]->getAsDouble());
        }
    }
    void getUiSnapshot(snapshot_t& snapshot);
    void setUiSnapshot(snapshot_t& snapshot);
};

static constexpr auto N_WAVEFORMS = size_t(Waveforms::NumWaveforms);
static constexpr auto N_VOICEMODES = size_t(2);

class guicontainer_plugin_synth_mono final : public guictr_base {
    module_synth_mono* const moduleInstance;
    guictr_select_enum<N_WAVEFORMS> ctr_waveform;
    guictr_select_enum<N_VOICEMODES> ctr_voicemode;
    guictr_select_enum<2> ctr_phasresetmode;
    seq_rand synthRandUI;
    const int buttonScale = 10;
public:

    explicit guicontainer_plugin_synth_mono(module_synth_mono* module) : moduleInstance(module)
    {
        padding = 0;
        margin  = 0;
        setBackgroundRendered(false);
        for (size_t i = 0; i < N_WAVEFORMS; ++i) {
            auto& btn = ctr_waveform.getButton(i);
            const auto& name = stringsWaveform[i];
            btn.setTooltipText(String("Select ") + name);
            btn.setText(name);
            btn.setButtonColor(GuiColor::COL_KNOB);
        }
        ctr_waveform.fnRenderButtonLabel = [this](guibutton* button, int32_t idx, NVGcontext* vg, int32_t stateFlags, ivec2 size) {
            auto waveform = static_cast<Waveforms>(idx);
            ivec2 renderFrame = size;
            auto col = GuiColor::COL_TEXT;
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
                    waveformIdx = DAW::Shape::ShapeWaveform::SHAPE_PULSE;
                    break;
                // case Waveforms::Shaper:
                case Waveforms::Noise:
                case Waveforms::NumWaveforms:
                    break;
            }
            const auto inset = math::clamp(size.x/16, 2, 16);
            ivec2 renderPos = ivec2(inset);
            auto renderFrameIcon = renderFrame - ivec2(inset * 2);
            if (renderFrameIcon.x > 0 && renderFrameIcon.y > 0) {
                if (waveformIdx >= 0) {
                    drawWaveform(vg, renderPos, renderFrameIcon, waveformIdx, theme->getColor(col), 3.0f);
               /*  } else if (waveform == Waveforms::Shaper) {
                    DAW::Shape::shape_t shape;
                    shape.flags = DAW::Shape::SHAPE_SHAPED | DAW::Shape::SHAPE_CYCLIC | DAW::Shape::SHAPE_LOCK_POINTS;
                    shape.pts.push_back(DAW::Shape::shape_pt_t{{0.0f, 0.0f}, 0.3f});
                    shape.pts.push_back(DAW::Shape::shape_pt_t{{0.2f, 1.0f}, 0.3f});
                    shape.pts.push_back(DAW::Shape::shape_pt_t{{0.7f, 0.3f}, 0.7f});
                    shape.pts.push_back(DAW::Shape::shape_pt_t{{1.0f, 0.0}, 0.6f});
                    auto hit = DAW::Shape::shape_t::hit_result();
                    nvgSave(vg);
                    nvgIntersectScissor(vg, renderPos.x, renderPos.y, renderFrameIcon.x, renderFrameIcon.y);
                    DAW::Shape::DrawShapeUnclamped(
                            shape,
                            vg,
                            theme,
                            col,
                            GuiColor::COL_SHAPE_CURVE_HIGHLIGHT,
                            renderPos, renderFrameIcon,
                            hit, nullptr
                    );
                    nvgRestore(vg); */
                } else if (waveform == Waveforms::Noise) {
                    DAW::Shape::shape_t shape;
                    shape.flags = DAW::Shape::SHAPE_CYCLIC | DAW::Shape::SHAPE_LOCK_POINTS;
                    /* use synthRandUI to generate some random peaks */
                    std::array<float, 16> peaks;
                    float phase = 0.0f;
                    float phaseStep = 1.0f / peaks.size();
                    for (auto& peak : peaks) {
                        peak = synthRandUI.rng_double() * 0.5f + 0.5f;
                        shape.pts.push_back(DAW::Shape::shape_pt_t{{phase, peak*2.0f - 1.0f}, 0.5f});
                        phase += phaseStep;
                    }
                    auto hit = DAW::Shape::shape_t::hit_result();
                    nvgSave(vg);
                    nvgIntersectScissor(vg, renderPos.x, renderPos.y, renderFrameIcon.x, renderFrameIcon.y);
                    DAW::Shape::DrawShapeUnclamped(
                            shape,
                            vg,
                            theme,
                            col,
                            GuiColor::COL_SHAPE_CURVE_HIGHLIGHT,
                            renderPos, renderFrameIcon,
                            hit, nullptr
                    );
                    nvgRestore(vg);
                }
            }
            renderPos = ivec2(inset/2);
            renderFrameIcon = renderFrame - ivec2(inset);
            if (renderFrameIcon.x > 0 && renderFrameIcon.y > 0) {
                auto color = theme->getColor(getBackgroundColor());
                nvgBeginPath(vg);
                nvgRect(vg, renderPos.x, renderPos.y, renderFrameIcon.x, renderFrameIcon.y);
                nvgStrokeColor(vg, getContrastFontColorNvg(color));
                nvgStrokeWidth(vg, 1.0f);
                nvgStroke(vg);
            }
        };
        for (size_t i = 0; i < N_VOICEMODES; ++i) {
            auto& btn = ctr_voicemode.getButton(i);
            const auto& name = stringsVoiceModeMono[i];
            btn.setTooltipText(String("Select ") + name);
            btn.setText(name);
            btn.setButtonColor(GuiColor::COL_KNOB);
        }
        for (size_t i = 0; i < 2; ++i) {
            auto& btn = ctr_phasresetmode.getButton(i);
            const auto& name = stringsReset[i];
            btn.setTooltipText(String("Select ") + name);
            btn.setText(name);
            btn.setButtonColor(GuiColor::COL_KNOB);
        }
        add(&ctr_voicemode);
        add(&ctr_phasresetmode);
        add(&ctr_waveform);
        setCanMouseHit(true);
    }
    class guictr_module_synth_basic_context_menu final : public guictxtmenu {
        module_synth_mono* const moduleInstance;
    public:
        explicit guictr_module_synth_basic_context_menu(module_synth_mono* _module)
            : guictxtmenu(), moduleInstance(_module)
        {
            (void) moduleInstance;
            this->size.x   = 220;
            maxHeight = 0;
            this->fontSize = FONT_SIZE_CTXT_SMALL;
            this->paddingV = 0;
        }
        bool clickedElement(ctxtmenu_entry* e, int _id) override {
            closeContextMenu();
            return true;
        }
    };

    ~guicontainer_plugin_synth_mono() override {
        removeGuis();
    }

    void rightClicked(MouseEvent& evt, guibase* what) override {
        parentCtrl->openContextMenu(new guictr_module_synth_basic_context_menu(moduleInstance), evt.mousepos);
    }

    void onSetParameter(int32_t index, float value) {
    }

    void getSizeScale(int& w, int& h) {
        auto size = ivec2(64);
        size.y *= 8;
        w = size.x;
        h = size.y;
    }
    void layoutImpl(ivec2 size) {
        auto shapeEditorSize = ivec2(math::clamp(size.y, 0, math::max(16, size.x-64)), size.y);
        auto bHeight = math::max(64, size.y / buttonScale);
        auto bWidth = size.x;
        bWidth = math::clamp(bWidth, 64, math::min(256, bHeight * 3));
        bWidth = math::min(size.x, bWidth);
        ctr_voicemode.pos  = { 0, 0 };
        ctr_voicemode.size = { bWidth, (bHeight * ctr_voicemode.getNumButtons()) / 2 };
        ctr_phasresetmode.pos  = ctr_voicemode.getLeftBottom();
        ctr_phasresetmode.size = { bWidth, (bHeight * ctr_phasresetmode.getNumButtons()) / 2 };
        ctr_waveform.pos  = ctr_phasresetmode.getLeftBottom();
        ctr_waveform.size = { bWidth, size.y - ctr_phasresetmode.bottom() };
    }
    void layout() override {
        layoutImpl(size);
        guictr_base::layout();
    }

    void onGuiOpen() {
        ctr_waveform.setAutomationRef(moduleInstance, 1 + ParametersSynthMono::Osc1Wave);
        ctr_voicemode.setAutomationRef(moduleInstance, 1 + ParametersSynthMono::VoiceMode);
        ctr_phasresetmode.setAutomationRef(moduleInstance, 1 + ParametersSynthMono::Osc1PhaseResetMode);
    }

    void onGuiClose() {
        ctr_waveform.setAutomationRef(nullptr, -1);
        ctr_voicemode.setAutomationRef(nullptr, -1);
        ctr_phasresetmode.setAutomationRef(nullptr, -1);
    }
    void setUiLayout(const ui_layout_t& layout) {
    }

    bool getUiLayout(ui_layout_t& layout) const {
        return true;
    }
};

class PluginViewContainerSynthBasic final : public PluginViewContainer {
public:
    guicontainer_plugin_synth_mono ctr_main;
    explicit PluginViewContainerSynthBasic(module_synth_mono* eff)
        : ctr_main(eff) {
    }
    ~PluginViewContainerSynthBasic() override = default;
    guicontainer_plugin_synth_mono& getPluginUI() {
        return ctr_main;
    }
    const guicontainer_plugin_synth_mono& getPluginUI() const {
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

std::shared_ptr<PluginViewContainer> SynthImplMono::createViewCtrImpl() {
    if (this->moduleSynthInstance) {
        this->views.push_back(std::make_shared<PluginViewContainerSynthBasic>(static_cast<module_synth_mono*>(this->moduleSynthInstance)));
        return this->views.back();
    }
    return nullptr;
}

SynthImplMono::SynthImplMono(module_synth_template<SynthImplMono>* module)
    : SynthImpl<SynthImplMono, ParametersSynthMono>(module),
    moduleSynthInstance(module)
{
    initImpl();
}



void module_synth_mono::getUiSnapshot(snapshot_t& snapshot) {
    for (auto& view : views) {
        auto implCtrType = dynamic_cast<PluginViewContainerSynthBasic*>(view.get());
        ui_layout_t layout{};
        if (implCtrType && implCtrType->getPluginUI().getUiLayout(layout)) {
            layout.uiId = view->getUiId();
            snapshot.uiLayout.push_back(layout);
        }
    }
}

void module_synth_mono::setUiSnapshot(snapshot_t& snapshot) {
    for (auto& uis : snapshot.uiLayout) {
        std::vector<std::shared_ptr<PluginViewContainer>> views;
        getAllViewCtrs(uis.uiId, views);
        for (auto& view : views) {
            auto implCtrType = dynamic_cast<PluginViewContainerSynthBasic*>(view.get());
            if (implCtrType) {
                implCtrType->getPluginUI().setUiLayout(uis);
            }
        }
    }
}
} // namespace PluginSynth::Mono

namespace PluginSynth::Shaper {

enum ParametersSynthShaper {
    MasterVolume = 0,
    VoiceMode,
    GlideLength,
    Osc1Coarse,
    Osc1Fine,
    Panning,
    Voices,
    Osc1PhaseResetMode
};

struct ui_layout_t {
    int32_t uiId = 0;
};
struct snapshot_t {
    int32_t version = 0;
    std::vector<param_float_snapshot_t> params;
    std::vector<DAW::Shape::shape_snapshot_t> shapes;
    std::vector<ui_layout_t> uiLayout;
};
static constexpr int32_t SYNTH_SHAPER_SNAPSHOT_VERSION = 2;

std::shared_ptr<std::vector<std::byte>> serializeSnapshot(const snapshot_t& snapshot) {
    dbgassert(snapshot.version == SYNTH_SHAPER_SNAPSHOT_VERSION);
    auto shrdHeapVec = std::make_shared<std::vector<std::byte>>();
    shrdHeapVec->resize(256);
    DAW::ByteBuffer::stream_write<std::vector<std::byte>> out{*shrdHeapVec, 0};
    out.write(size_t(0));
    out.write(snapshot.version);
    out.write(size_t{snapshot.params.size()});
    out.write(size_t{snapshot.uiLayout.size()});
    out.write(size_t{snapshot.shapes.size()});
    for (const auto& p : snapshot.params) {
        out.write(p.paramIdx);
        out.write(p.value);
    }
    for (const auto& modulation : snapshot.uiLayout) {
        out.write(modulation.uiId);
    }
    for (const auto& shape : snapshot.shapes) {
        DAW::Shape::writeShape(out, shape);
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
    if (snapshot.version > SYNTH_SHAPER_SNAPSHOT_VERSION)
        return false;
    size_t numParams = 0;
    size_t numUiLayouts = 0;
    size_t numShapes = 0;
    if (!in.read(numParams) || numParams > 1000)
        return false;
    if (!in.read(numUiLayouts) || numUiLayouts > 1000)
        return false;
    if (!in.read(numShapes) || numShapes > 1000)
        return false;
    snapshot.params.resize(numParams);
    snapshot.uiLayout.resize(numUiLayouts);

    for (auto& p : snapshot.params) {
        if (!in.read(p.paramIdx))
            return false;
        if (!in.read(p.value))
            return false;
    }
    for (auto& layout : snapshot.uiLayout) {
        if (!in.read(layout.uiId))
            return false;
    }
    snapshot.shapes.reserve(numShapes);
    for (size_t i = 0; i < numShapes; ++i) {
        DAW::Shape::shape_snapshot_t shape;
        if (!DAW::Shape::readShape(in, shape)) {
            return false;
        }
        snapshot.shapes.push_back(std::move(shape));
    }
    snapshotOut = std::move(snapshot);
    return true;
}

class SynthStateShaper {
public:
    double osc1Tune                = 1.0;
    double glideLength             = 0.0;
    double targetMasterVolume      = 0.0;
    double masterVolume            = 0.0;
};

class SynthImplShaper final : public SynthImpl<SynthImplShaper, ParametersSynthShaper>, public SynthStateShaper {
    static constexpr uint16_t NUM_POLY_VOICES   = 8;
private:

    void initImpl() {
        oscShape = DAW::Shape::GetShapeSaw(DAW::Shape::SHAPE_SHAPED|DAW::Shape::SHAPE_CYCLIC);
        oscShape.pts = DAW::Shape::GetShape(DAW::Shape::ShapeWaveform::SHAPE_SAW);
        auto pShape = &oscShape;
        for (auto& v : voices) {
            v.osc1a.setShape(pShape);
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
        
        addEnumParam(Parameters::Osc1PhaseResetMode)->setStrings(stringsReset.begin(), stringsReset.end())->setInitialValue(1);
        setParamName(getParam(Parameters::Osc1PhaseResetMode), "OSC1 phase reset mode", "OSC1 phase reset", "OSC1 phase reset");
    }
    module_synth_template<SynthImplShaper>* const moduleSynthInstance;
    seq_rand synthRand;
    std::array<VoiceSynth, NUM_POLY_VOICES> voices;
    DAW::Shape::shape_t oscShape;
    std::vector<std::shared_ptr<PluginViewContainer>> views;
public:
    explicit SynthImplShaper(module_synth_template<SynthImplShaper>* module);

    ~SynthImplShaper()
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
            case Parameters::VoiceMode:
                switch (GetParamEnum(parameter)->getEnumValue<VoiceModes>()) {
                    case VoiceModes::Mono:
                    case VoiceModes::Legato:
                        for (int i = 1; i < NUM_POLY_VOICES; i++) {
                            voices[i].Release();
                        }
                        break;
                    default:
                        break;
                }
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

    double GetVoiceImplBasic(double dt, VoiceSynth& voice, FilterModes filtermode, double tickPos) {
        auto volEnvValue   = voice.velocity;
        auto baseFrequency = voice.frequency * voice.pitchBend;
        auto coarse = GetParamInt(Parameters::Osc1Coarse)->Value();
        auto fine   = GetParamFloat(Parameters::Osc1Fine)->Value();
        osc1Tune    = pitchFactor(coarse + fine);
        auto osc1Frequency = osc1Tune * baseFrequency;
        auto out      = 0.0;
        auto osc1Out             = 0.0;
        osc1Out += voice.osc1a.GetWaveformShaper(dt, osc1Frequency, true);
        dbgassert(!fp_math::isNanOrInfd(osc1Out));
        out += osc1Out;
        dbgassert(!fp_math::isNanOrInfd(out));
        double volEnvSmoothed = volEnvValue;
        if (voice.noteT.len > 0) {
            const float noteProgress = voice.noteT.end() - tickPos;
            const float fFadeLen = 64.0f;
            float fFadeIn = math::smoothstep(math::clamp(noteProgress / fFadeLen, 0.0f, 1.0f));
            float fFadeOut = math::smoothstep(math::clamp((voice.noteT.len - noteProgress) / fFadeLen, 0.0f, 1.0f));
            volEnvSmoothed = volEnvValue * fFadeIn * fFadeOut;
        }
        out *= volEnvSmoothed;
        return out;
    }

    void StartVoice(VoiceSynth& v, bool bTriggerMono) {
        auto holdOsc1Phase = GetParamEnum(Parameters::Osc1PhaseResetMode)->Value() == 1;
        // bool isSilent = v.volEnv.stage >= EnvelopeStages::Idle || !v.bIsActive;
        v.Start(holdOsc1Phase, holdOsc1Phase);
        if (!holdOsc1Phase) {
            v.osc1a.phase = 0.0;
        }
        v.volEnv.Reset();
    }

    void FlushMidi(int sample) {
        while (!midiQueue.Empty()) {
            auto message = midiQueue.Peek();
            if (message.mOffset > sample) break;

            auto voiceMode      = GetParamEnum(Parameters::VoiceMode)->getEnumValue<VoiceModes>();
            auto status         = message.StatusMsg();
            auto ctrl           = message.ControlChangeIdx();
            auto velocity       = pow(message.Velocity() * .0078125, 1.25);

            if (status == IMidiMsg::kNoteOn && velocity == 0) status = IMidiMsg::kNoteOff;
            note_t noteDaw;
            if (message.note) {
                noteDaw = message.note.value();
            } else {
                noteDaw = note_t{
                    .pitch = message.NoteNumber(),
                    .velocity = message.Velocity(),
                    .time = math::floordS32(moduleSynthInstance->hostCallback->m_vstTimeInfo.ppqPos),
                    .len = TICKS_QUARTER,
                    .flags = NoteFlags::ENABLED | NoteFlags::IS_HELD | NoteFlags::REALTIME,
                    .channel = static_cast<int8_t>(message.Channel()),
                };
            }

            switch (status) {
                case IMidiMsg::kNoteOff:
                    heldNotes.erase(
                        std::remove_if(
                                std::begin(heldNotes),
                                std::end(heldNotes),
                                [noteB=noteDaw](const auto& noteA) { return noteA.pitch == noteB.pitch && noteA.channel == noteB.channel; }),
                        std::end(heldNotes));

                    switch (voiceMode) {
                        case VoiceModes::Poly:
                            for (auto& voice : voices)
                                if (voice.noteT.pitch == noteDaw.pitch) voice.Release();
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
                            voice->SetNote(noteDaw);
                            voice->SetVelocity(velocity);
                            voice->ResetPitch();
                            StartVoice(*voice, false);
                            // voice->seqNr = seq++;
                            break;
                        }
                        default:
                        case VoiceModes::Mono:
                            voices[0].SetNote(noteDaw);
                            voices[0].SetVelocity(velocity);
                            StartVoice(voices[0], true);
                            // voices[0].seqNr = 1;
                            break;
                        case VoiceModes::Legato:
                            voices[0].SetNote(noteDaw);
                            if (heldNotes.empty()) {
                                voices[0].SetVelocity(velocity);
                                voices[0].ResetPitch();
                                StartVoice(voices[0], true);
                                // voices[0].seqNr = 1;
                            }
                            break;
                    }

                    heldNotes.push_back(noteDaw);
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
        auto bpm100 = host->prjGlobals.tempo100;

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
            auto tickPosOffset = tick + sampleToTickConvert<double, roundmode::none>(s, bpm100, host->m_sampleFormatInternal.sampleRate * nOversample);
            for (auto& v : voices) {
                if (v.bIsActive) {
                    auto voiceVolume = GetParamFloat(Parameters::MasterVolume)->Value();
                    // auto noise = (synthRand.rng_double()*2-1)*0.002;
                    double vVal               = GetVoiceImplBasic(dt, v, filterMode, tickPosOffset);
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
    DAW::Shape::shape_t& getShape() {
        return oscShape;
    }
    bool getSnapshot(snapshot_t& snapshot) const {
        snapshot.version     = SYNTH_SHAPER_SNAPSHOT_VERSION;
        const auto numParams = CtrSize(vecParams);
        snapshot.params.reserve(numParams);
        for (int32_t i = 0; i < numParams; ++i) {
            if (!vecParams[i]) continue;
            // dbgassert(vecParams[i]->getAsDouble() >= 0.0 && vecParams[i]->getAsDouble() <= 1.0);
            snapshot.params.push_back({ i, vecParams[i]->getAsDouble() });
        }
        snapshot.shapes.push_back(DAW::Shape::shape_snapshot_t{ 0, DAW::Shape::shape_preset_t{2, oscShape} });
        return true;
    }

    bool setSnapshot(const snapshot_t& snapshot) {
        if (snapshot.version != SYNTH_SHAPER_SNAPSHOT_VERSION) {
            return false;
        }
        const auto numParams = CtrSize(vecParams);
        
        for (auto& param : vecParams) {
            if (!param) continue;
            param->resetToInitial();
            dbgassert(param->getAsDouble() >= 0.0 && param->getAsDouble() <= 1.0);
        }
        for (auto& ps : snapshot.params) {
            if (ps.paramIdx >= 0 && ps.paramIdx < numParams) {
                if (!vecParams[ps.paramIdx]) continue;
                vecParams[ps.paramIdx]->set(math::clamp(ps.value, 0.0, 1.0), math::clamp(ps.value, 0.0, 1.0));
            } else {
                dbgassert(0);
            }
        }
        oscShape = {};
        for (auto& shape : snapshot.shapes) {
            if (shape.type == 0) {
                oscShape = shape.shape.curve;
            } else {
                dbgassert(0);
            }
        }
        oscShape.flags = DAW::Shape::SHAPE_SHAPED|DAW::Shape::SHAPE_CYCLIC;
        if (oscShape.pts.size() < 2) {
            oscShape.pts = DAW::Shape::GetShape(DAW::Shape::ShapeWaveform::SHAPE_SAW);
        }

        for (auto& param : vecParams) {
            if (!param) continue;
            OnParamChange(static_cast<Parameters>(param->enumParam));
        }
        return true;
    }
};

class module_synth_shaper final : public module_synth_template<SynthImplShaper> {
public:
    explicit module_synth_shaper(int32_t _projectGlobalId, IHostCallback* _hostCallback)
        : module_synth_template<SynthImplShaper>(new SynthType(this), "Synth Shaper", _projectGlobalId, _hostCallback)
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
            using P = ParametersSynthShaper;
            switch (paramEntry->enumParam) {
                case P::Osc1Fine:
                case P::Osc1Coarse:
                case P::Panning:
                    regparam->isBiPolar = true;
                    break;
                default:
                    break;
            }
        }
        impl->init();
    }

    ~module_synth_shaper() override {
        delete impl;
    }

    PluginType getPluginType() override { return PLUGIN_TYPE_SYNTH_SHAPER; };

    std::shared_ptr<PluginViewContainer> createViewCtrInternal() override {
        return this->impl->createViewCtrImpl();
    }
    std::shared_ptr<std::vector<std::byte>> storePresetData() override {
        snapshot_t snapshot;
        if (impl->getSnapshot(snapshot)) {
            getUiSnapshot(snapshot);
            return serializeSnapshot(snapshot);
        }
        return nullptr;
    }

    bool loadPresetData(const std::shared_ptr<std::vector<std::byte>>& buf) override {
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

    void onPresetLoaded() {
        for (int32_t idx = 0; idx < CtrSize(vecParams); idx++) {
            if (!vecParams[idx]) {
                continue;
            }
            auto param = getParam(idx + 1);
            param->setAll(vecParams[idx]->getAsDouble());
        }
    }
    void getUiSnapshot(snapshot_t& snapshot);
    void setUiSnapshot(snapshot_t& snapshot);
};

class guicontainer_plugin_synth_shaper final : public guictr_base {
    module_synth_shaper* const moduleInstance;
    i_ctr_shape_editor* const shapeEditor;
    guictr_base* shapeEditorCtr = nullptr;
    seq_rand synthRandUI;
public:

    explicit guicontainer_plugin_synth_shaper(module_synth_shaper* module) : moduleInstance(module), shapeEditor(makeShapeEditor())
    {
        padding = 0;
        margin  = 0;
        setBackgroundRendered(false);
        auto synth = moduleInstance->getSynth();
        shapeEditor->setShapeEditorShapeRef(&synth->getShape());
        shapeEditor->setShapeEditorCallback([synth=moduleInstance->getSynth()](const DAW::Shape::shape_t& shape, bool bIsDragMove) -> void {
            auto lock = synth->lock();
            auto& synthShape = synth->getShape();
            synthShape.pts = shape.pts;
            synthShape.eraseDuplicates();
        });
        shapeEditorCtr = shapeEditor->getGuiContainer();
        shapeEditorCtr->setBackgroundRendered(false);
        shapeEditorCtr->setBackgroundRenderedInset(false);
        shapeEditorCtr->setCanMouseHit(false);
        shapeEditorCtr->id = 2;
        shapeEditorCtr->margin = 0;
        shapeEditorCtr->padding = 2;
        add(shapeEditorCtr);
        setCanMouseHit(true);
    }
    class guictr_module_synth_shaper_context_menu final : public guictxtmenu {
        module_synth_shaper* const moduleInstance;
        SafeRef<guibase> refGui;
    public:
        explicit guictr_module_synth_shaper_context_menu(module_synth_shaper* _module, SafeRef<guibase> ref)
            : guictxtmenu(), moduleInstance(_module), refGui(std::move(ref))
        {
            this->size.x   = 220;
            maxHeight = 0;
            this->fontSize = FONT_SIZE_CTXT_SMALL;
            this->paddingV = 0;
            addEntry(new ctxtmenu_toggle_setting("Show Editor", 0));
            addEntry(new DAW::Shape::ctxtmenu_lfo_shape_select("Shape", 1));
        }
        bool clickedElement(ctxtmenu_entry* e, int _id) override {
            if (_id == 0) {
                auto guiCtr = safeRefGet(refGui);
                if (guiCtr) {
                    auto bVisible = !guiCtr->isVisible();
                    guiCtr->setVisible(bVisible);
                    static_cast<ctxtmenu_toggle_setting*>(e)->setIcon(&RenderResources::imgIcons[bVisible ? ICON_MINUS : ICON_PLUS], GuiColor::COL_WHITE);
                    guiCtr->parent->onChildLayoutChanged(guiCtr);
                    return true;
                }
                return true;
            }
            if (_id >= 1) {
                using DAW::Shape::ShapeWaveform;
                auto shapeIdx = _id - 1;
                if (shapeIdx < 0 || shapeIdx > ShapeWaveform::SHAPE_PULSE_INV) {
                    return false;
                }
                auto waveform = static_cast<ShapeWaveform>(shapeIdx);
                auto lock = moduleInstance->getSynth()->lock();
                auto& shape = moduleInstance->getSynth()->getShape();
                shape.pts = GetShape(waveform);
            }
            closeContextMenu();
            return true;
        }
    };

    void rightClicked(MouseEvent& evt, guibase* what) override {
        auto safeRef = this->shapeEditorCtr ? this->shapeEditorCtr->toRef() : SafeRef<guibase>();
        parentCtrl->openContextMenu(new guictr_module_synth_shaper_context_menu(moduleInstance, safeRef), evt.mousepos);
    }

    ~guicontainer_plugin_synth_shaper() override {
        removeGuis();
        delete this->shapeEditorCtr;
    }
    void onSetParameter(int32_t index, float value) {
    }
    void getSizeScale(int& w, int& h) {
        auto size = ivec2(128);
        w = size.x;
        h = size.y;
    }
    void layout() override {
        if (shapeEditorCtr) {
            shapeEditorCtr->pos = { 0, 0 };
            shapeEditorCtr->size = size;
        }
        guictr_base::layout();
    }

    void onGuiOpen() {
    }

    void onGuiClose() {
    }
    void setUiLayout(const ui_layout_t& layout) {
        if (shapeEditorCtr) {
            // shapeEditorCtr->setVisible(layout.bShapeEditorVisible);
        }
    }

    bool getUiLayout(ui_layout_t& layout) const {
        // layout.bShapeEditorVisible = shapeEditorCtr->isVisible();
        return true;
    }
};

class PluginViewContainerSynthShaper final : public PluginViewContainer {
public:
    guicontainer_plugin_synth_shaper ctr_main;
    explicit PluginViewContainerSynthShaper(module_synth_shaper* eff)
        : ctr_main(eff) {
    }
    ~PluginViewContainerSynthShaper() override = default;
    guicontainer_plugin_synth_shaper& getPluginUI() {
        return ctr_main;
    }
    const guicontainer_plugin_synth_shaper& getPluginUI() const {
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
        *w = *h = 128;
    }
    bool isViewSupported(int32_t uiId) const override {
        return uiId != UID_VIEW_CTR_NODES;
    }
};

std::shared_ptr<PluginViewContainer> SynthImplShaper::createViewCtrImpl() {
    if (this->moduleSynthInstance) {
        this->views.push_back(std::make_shared<PluginViewContainerSynthShaper>(static_cast<module_synth_shaper*>(this->moduleSynthInstance)));
        return this->views.back();
    }
    return nullptr;
}

SynthImplShaper::SynthImplShaper(module_synth_template<SynthImplShaper>* module)
    : SynthImpl<SynthImplShaper, ParametersSynthShaper>(module),
    moduleSynthInstance(module)
{
    initImpl();
}


void module_synth_shaper::getUiSnapshot(snapshot_t& snapshot) {
    for (auto& view : views) {
        auto implCtrType = dynamic_cast<PluginViewContainerSynthShaper*>(view.get());
        ui_layout_t layout{};
        if (implCtrType && implCtrType->getPluginUI().getUiLayout(layout)) {
            layout.uiId = view->getUiId();
            snapshot.uiLayout.push_back(layout);
        }
    }
}

void module_synth_shaper::setUiSnapshot(snapshot_t& snapshot) {
    for (auto& uis : snapshot.uiLayout) {
        std::vector<std::shared_ptr<PluginViewContainer>> views;
        getAllViewCtrs(uis.uiId, views);
        for (auto& view : views) {
            auto implCtrType = dynamic_cast<PluginViewContainerSynthShaper*>(view.get());
            if (implCtrType) {
                implCtrType->getPluginUI().setUiLayout(uis);
            }
        }
    }
}

} // namespace PluginSynth::Shaper

template<>
effectbase* makeInstance<PluginSynth::Mono::module_synth_mono>(int32_t _projectGlobalId, IHostCallback* _hostCallback) {
    return new PluginSynth::Mono::module_synth_mono(_projectGlobalId, _hostCallback);
}

template<>
effectbase* makeInstance<PluginSynth::Shaper::module_synth_shaper>(int32_t _projectGlobalId, IHostCallback* _hostCallback) {
    return new PluginSynth::Shaper::module_synth_shaper(_projectGlobalId, _hostCallback);
}

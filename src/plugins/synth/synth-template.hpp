#pragma once
#include "host/host.h"
#include "host/plugin/internal/internal-plugin.h"
#include "synth-types.hpp"
#include "synth-param.hpp"
#include "host/plugin/plugin-lockable.h"
#include "types.h"

namespace PluginSynth {

template<typename T>
class module_synth_template;
class module_synth_unison;
class module_synth_mono;

struct HostTempo {
    double barPos;
    double bpm;
    double ppqPos;
};


template <typename T, class P>
class SynthImpl : public PluginLockable {
public:
    using Parameters = P;
    friend class PluginVST2_Synth;
    friend class module_synth_unison;
    friend class module_synth_mono;
protected:
    module_synth_template<T>* const moduleInstance;
protected:
    std::vector<SynthParamBase*> vecParams;
    std::vector<note_t> heldNotes;
    HostTempo tempo{};
    IMidiQueue midiQueue;
    double oneOverSR = 1.0 / 44100.0;
    bool bIsInitSamplerate = false;
    int32_t noteSequenceNr = 0;
public:
    explicit SynthImpl(module_synth_template<T>* module)
        : 
        PluginLockable(daw_tls::getTls().dawInstance),
        moduleInstance(module)
    {
    }
    std::vector<SynthParamBase*>& getParams() {
        return vecParams;
    }
    virtual void ProcessSynth(AudioBlock* in, float * const * outputs, int nFrames, const DAW::Host::Host* const host, double tick, playback_state state) = 0;
    virtual void init() = 0;
    virtual std::shared_ptr<PluginViewContainer> createViewCtrImpl() { return nullptr; };
    virtual void OnParamChange(Parameters parameter) {};
    virtual void onTransportChanged(bool bIsPlaying) {}
    // virtual bool getSnapshot(snapshot_t& snapshot) const { return false; }
    // virtual bool setSnapshot(const snapshot_t& snapshot) { return false; }
    virtual samplecount_t getLatency() { return 0; }
    SynthParamBase* getParam(Parameters enumParam) {
        if (enumParam >= 0 && enumParam < vecParams.size()) {
            return vecParams[enumParam];
        }
        return nullptr;
    }
    const SynthParamBase* getParam(Parameters enumParam) const {
        if (enumParam >= 0 && enumParam < vecParams.size()) {
            return vecParams[enumParam];
        }
        return nullptr;
    }

    inline SynthParam_Float* GetParamFloat(Parameters param) noexcept {
        // dbgassert(getParam(param) && getParam(param)->type == SynthParam::ParamType::FLOAT);
        return static_cast<SynthParam_Float*>(this->vecParams[param]);
    }

    inline SynthParam_Int* GetParamInt(Parameters param) noexcept {
        // dbgassert(getParam(param) && getParam(param)->type == SynthParam::ParamType::INT);
        return static_cast<SynthParam_Int*>(this->vecParams[param]);
    }

    inline SynthParam_Enum* GetParamEnum(Parameters param) noexcept {
        // dbgassert(getParam(param) && getParam(param)->type == SynthParam::ParamType::ENUM);
        return static_cast<SynthParam_Enum*>(this->vecParams[param]);
    }

    const SynthParam_Enum* GetParamEnum(Parameters param) const noexcept {
        // dbgassert(getParam(param) && getParam(param)->type == SynthParam::ParamType::ENUM);
        return static_cast<SynthParam_Enum*>(this->vecParams[param]);
    }

    void ProcessMidiMsg(IMidiMsg& msg) {
        midiQueue.Add(msg);
    }

    void processMidiMessages(const std::vector<IMidiMsg>& midiEvents) {
        midiQueue.AddAll(midiEvents);
    }

    std::vector<note_t> getHeldNotes() {
        return heldNotes;
    }

    void setTempo(double d) {
        tempo.bpm = d;
    }

    void setBarPos(double d) {
        tempo.barPos = d;
    }

    void setPPQPos(double d) {
        tempo.ppqPos = d;
    }

    double getSamplerate() const {
        return 1.0 / oneOverSR;
    }

    void setSamplerate(float sr) {
        if (sr < 1) sr = 1;
        oneOverSR = 1.0 / sr;
        if (!bIsInitSamplerate) {
            bIsInitSamplerate = true;
            initSampleRate();
        }
    }
    virtual void initSampleRate() {
    }
    virtual void setBlocksize(samplecount_t bs) {
    }

    void ReadAutomation(const DAW::Host::Host* const host, double tick, playback_state state, samplecount_t samplePos, samplecount_t sampleCount, int nOversample) {
        auto bpm100 = host->prjGlobals.tempo100;
        auto tickPosOffset = tick + sampleToTickConvert<double, roundmode::none>(samplePos, bpm100, host->m_sampleFormatInternal.sampleRate * nOversample);
        this->moduleInstance->updateAutomatedParameters(host, math::floordS32(tickPosOffset), state);
    }

    template<typename VoiceListType>
    void ProcessMidiSample(T& meAsDerived, VoiceListType& voices, VoiceModes voiceMode, samplecount_t sampleInBlock, double tickPos, size_t polyVoiceLimit) {
        while (!midiQueue.Empty()) {
            auto message = midiQueue.Peek();
            if (message.mOffset > sampleInBlock) break;

            // auto voiceMode      = GetParamEnum(Parameters::VoiceMode)->getEnumValue<VoiceModes>();
            auto status         = message.StatusMsg();
            auto ctrl           = message.ControlChangeIdx();
            auto velocity       = pow(message.Velocity() * .0078125, 1.25);

            if (status == IMidiMsg::kNoteOn && velocity == 0) status = IMidiMsg::kNoteOff;
            note_t noteDaw;
            bool bHasNoteDaw = message.note.has_value();
            if (message.note) {
                noteDaw = message.note.value();
            } else {
                noteDaw = note_t{
                    .pitch = message.NoteNumber(),
                    .velocity = message.Velocity(),
                    .time = math::floordS32(tickPos),
                    .len = TICKS_QUARTER,
                    .flags = NoteFlags::ENABLED | NoteFlags::IS_HELD | NoteFlags::REALTIME,
                    .channel = static_cast<int8_t>(message.Channel()),
                };
            }

            switch (status) {
                case IMidiMsg::kNoteOff:
                    if (bHasNoteDaw) {
                        auto itRemoved = std::remove_if(
                                    std::begin(heldNotes),
                                    std::end(heldNotes),
                                    [noteB=noteDaw](const auto& noteA) { return noteA.pitch == noteB.pitch && noteA.channel == noteB.channel && noteA.time == noteB.time; });
                        // check if something has been removed:
                        bool bRemoved = itRemoved != std::end(heldNotes);
                        if (bRemoved) {
                            heldNotes.erase(itRemoved, std::end(heldNotes));
                        } else {
                            log_lf(Log::L_WARN, "NoteOff without NoteOn %s\n", noteName(noteDaw.pitch));
                        }

                        switch (voiceMode) {
                            case VoiceModes::Poly:
                                for (auto& voice : voices) {
                                    if (voice.isNotReleased() && voice.noteT.pitch == noteDaw.pitch
                                        && voice.noteT.channel == noteDaw.channel
                                        && voice.noteT.time == noteDaw.time) {
                                            voice.Release();
                                            break;
                                        }
                                }
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
                    } else {
                        auto itRemoved = std::remove_if(
                                    std::begin(heldNotes),
                                    std::end(heldNotes),
                                    [noteB=noteDaw](const auto& noteA) { return noteA.pitch == noteB.pitch && noteA.channel == noteB.channel; });
                        // check if something has been removed:
                        bool bRemoved = itRemoved != std::end(heldNotes);
                        if (bRemoved) {
                            heldNotes.erase(itRemoved, std::end(heldNotes));
                        } else {
                            log_lf(Log::L_WARN, "NoteOff without NoteOn %s\n", noteName(noteDaw.pitch));
                        }

                        switch (voiceMode) {
                            case VoiceModes::Poly:
                                for (auto& voice : voices)
                                    if (voice.noteT.pitch == noteDaw.pitch && voice.noteT.channel == noteDaw.channel) voice.Release();
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
                    }
                    break;
                case IMidiMsg::kNoteOn:
                    switch (voiceMode) {
                        case VoiceModes::Poly: {
                            // get the quietest voice, prioritizing voices that are released
                            auto voiceEnd = std::begin(voices) + polyVoiceLimit;
                            auto voice    = std::min_element(
                                    std::begin(voices),
                                    voiceEnd,
                                    [](auto& a, auto& b) {
                                        bool aReleased = !a.isVoiceActive();
                                        if (aReleased == !b.isVoiceActive()) {
                                            auto volA = a.GetVolume();
                                            auto volB = b.GetVolume();
                                            if (volA <= 0.0 && volB <= 0.0) {
                                                return a.seqNr < b.seqNr;
                                            }
                                            return volA < volB;
                                        }
                                        return aReleased;
                                    });
                            voice->SetNote(noteDaw);
                            voice->SetVelocity(velocity);
                            voice->ResetPitch();
                            meAsDerived.StartVoice(*voice, voiceMode);
                            voice->seqNr = noteSequenceNr++;
                            break;
                        }
                        default:
                        case VoiceModes::Mono:
                            voices[0].SetNote(noteDaw);
                            voices[0].SetVelocity(velocity);
                            meAsDerived.StartVoice(voices[0], voiceMode);
                            voices[0].seqNr = noteSequenceNr++;
                            break;
                        case VoiceModes::Legato:
                            voices[0].SetNote(noteDaw);
                            if (heldNotes.empty()) {
                                voices[0].SetVelocity(velocity);
                                voices[0].ResetPitch();
                                voices[0].Start(true);
                                meAsDerived.StartVoice(voices[0], voiceMode);
                                voices[0].seqNr = noteSequenceNr++;
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
};

template<typename T>
class module_synth_template : public internalplugin {
protected:
    T* const impl;
    std::vector<SynthParamBase*>& vecParams;
public:
    using ThreadLock = std::lock_guard<std::recursive_mutex>;
    using SynthType = T;
    explicit module_synth_template(T* const _impl, const String& name, int32_t _projectGlobalId, IHostCallback* _hostCallback)
        : internalplugin(name, _projectGlobalId, _hostCallback), impl(_impl), vecParams(impl->getParams()) {
    }
    samplecount_t getPluginLatency() override { return impl->getLatency(); }
    T* getSynth() { return impl; }
    const T* getSynth() const { return impl; }
    void processMidiMessages(std::vector<IMidiMsg>& midiEvents) override { 
        this->impl->processMidiMessages(midiEvents);
    }

    void setSampleFormat(sampleformat_t sampleFormat) override {
        internalplugin::setSampleFormat(sampleFormat);
        this->impl->setSamplerate(sampleFormat.sampleRate);
        this->impl->setBlocksize(sampleFormat.blockSize);
    }

    bool isValidParamIdx(int32_t idxInternal) const {
        return idxInternal >= 0 && idxInternal < CtrSize(vecParams) && vecParams[idxInternal];
    }

    param_converted_t convertParamValueDisplay(int32_t idx, const param_unit_t& displayValue) override {
        const auto idxInternal = idx - PARAM_OFFSET_IMPL;
        if (isValidParamIdx(idxInternal)) {
            SynthParamBase* param = vecParams[idxInternal];
            if (param->unit != "dB") {
                return param->convertValueDisplay(displayValue);
            }
        }
        return internalplugin::convertParamValueDisplay(idx, displayValue);
    }

    param_unit_t convertParamValueToDisplay(int32_t idx, float value) override {
        const auto idxInternal = idx - PARAM_OFFSET_IMPL;
        if (isValidParamIdx(idxInternal)) {
            SynthParamBase* param = vecParams[idxInternal];
            if (param->unit != "dB") {
                String valDisplay     = param->getValueDisplay(value);
                return {valDisplay, param->unit};
            }
        }
        return internalplugin::convertParamValueToDisplay(idx, value);
    }

    void postSetParameter(int32_t idx, float preVal, float val, int flags) override {
        const auto idxInternal = idx - PARAM_OFFSET_IMPL;
        if (isValidParamIdx(idxInternal)) {
            SynthParamBase* param = vecParams[idxInternal];
            if (flags & FLG_PAR_UPDATE_MODULATED) {
                param->setModulated(val);
            } else {
                param->setAll(val);
            }
            this->impl->OnParamChange(static_cast<T::Parameters>(param->enumParam));
        }
        internalplugin::postSetParameter(idx, preVal, val, flags);
    }

    void process(const DAW::Host::Host* const host, AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) override {
        dbgassert(format.sampleRate > 0);
        dbgassert(out->channels >= 2);
        dbgassert(out->samples >= numSamples);
        auto ppqPos = tick / double(TICKS_QUARTER);
        auto barStartPos = math::floord(tick / double(TICKS_BAR)) * 4;
        this->impl->setPPQPos(ppqPos);
        this->impl->setBarPos(barStartPos);
        this->impl->setTempo(host->prjGlobals.tempo100 / 100.0); //TODO: use hostCallback or provide time info struct in process() parameter list
        // TODO: transport changes
        // if (timeinfo && timeinfo->flags & kVstTransportChanged) {
        //     this->impl->onTransportChanged(timeinfo->flags & kVstTransportPlaying);
        // }
        out->clear();
        this->impl->ProcessSynth(in, out->buf, numSamples, host, tick, state);
    }
    void notifyUiChanges() {
        for (auto& pviewctr : this->views) {
            if (pviewctr->isInUse()) {
                pviewctr->onSetParameter(-1, 0.0f);
            }
        }
        if (hostCallback) {
            hostCallback->onUiChanged(this);
        }
    }
};



} // namespace PluginSynth

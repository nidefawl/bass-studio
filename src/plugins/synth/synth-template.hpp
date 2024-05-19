#pragma once
#include "host/host.h"
#include "host/plugin/internal/internal-plugin.h"
#include "synth-types.hpp"
#include "synth-param.hpp"
#include "host/plugin/plugin-lockable.h"

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
private:
    module_synth_template<T>* const moduleInstance;
protected:
    std::vector<SynthParamBase*> vecParams;
    std::vector<int> heldNotes;
    HostTempo tempo{};
    IMidiQueue midiQueue;
    double oneOverSR = 1.0 / 44100.0;
    bool bIsInitSamplerate = false;
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
        dbgassert(getParam(param) && getParam(param)->type == SynthParam::ParamType::FLOAT);
        return static_cast<SynthParam_Float*>(this->vecParams[param]);
    }

    inline SynthParam_Int* GetParamInt(Parameters param) noexcept {
        dbgassert(getParam(param) && getParam(param)->type == SynthParam::ParamType::INT);
        return static_cast<SynthParam_Int*>(this->vecParams[param]);
    }

    inline SynthParam_Enum* GetParamEnum(Parameters param) noexcept {
        dbgassert(getParam(param) && getParam(param)->type == SynthParam::ParamType::ENUM);
        return static_cast<SynthParam_Enum*>(this->vecParams[param]);
    }

    const SynthParam_Enum* GetParamEnum(Parameters param) const noexcept {
        dbgassert(getParam(param) && getParam(param)->type == SynthParam::ParamType::ENUM);
        return static_cast<SynthParam_Enum*>(this->vecParams[param]);
    }

    void ProcessMidiMsg(IMidiMsg& msg) {
        midiQueue.Add(msg);
    }

    void processMidiMessages(const std::vector<IMidiMsg>& midiEvents) {
        midiQueue.AddAll(midiEvents);
    }

    std::vector<int> getHeldNotes() {
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
    virtual void setBlocksize(blocksize_t bs) {
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

    param_converted_t convertParamValueDisplay(int32_t idx, const param_unit_t& displayValue) override {
        if (idx > 0 && idx - 1 < CtrSize(vecParams)) {
            SynthParamBase* param = vecParams[idx-1];
            return param->convertValueDisplay(displayValue);
        }
        return internalplugin::convertParamValueDisplay(idx, displayValue);
    }

    param_unit_t convertParamValueToDisplay(int32_t idx, float value) override {
        if (idx > 0 && idx - 1 < CtrSize(vecParams)) {
            SynthParamBase* param = vecParams[idx-1];
            String valDisplay     = param->getValueDisplay(value);
            return {valDisplay, param->unit};
        }
        return internalplugin::convertParamValueToDisplay(idx, value);
    }

    void postSetParameter(int32_t idx, float preVal, float val, int flags) override {
        if (idx > 0 && idx - 1 < CtrSize(vecParams)) {
            SynthParamBase* param = vecParams[idx-1];
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


struct Voice {
    std::array<double, 64> modValues{};
    std::array<float, 8> envelopeValuesCached{};

    Oscillator lfo1;
    Oscillator lfo2;
    double lfoValue     = 0.0;
    double prevLfoValue = 0.0;

    double velocity     = 0.0;
    int32_t indexUnison = 0;
    int note            = 0;
    Envelope volEnv;
    Envelope modEnv;
    Envelope lfoEnv;
    Oscillator oscFm;
    Oscillator osc1a;
    Oscillator osc1b;
    Oscillator osc2a;
    Oscillator osc2b;
    Filter filter;
    seq_rand rand;
    double driftVelocity   = 0.0;
    double driftPhase      = 0.0;
    double driftValue      = 0.0;
    double frequency       = 0.0;
    double targetFrequency = 0.0;
    double pitchBend       = 1.0;
    bool bIsActive         = false;
    bool bTriggerSmoothing = false;
    double prevVolEnv = 0.0;
    double prevCutoff = 0.0;

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
        return this->volEnv.stage < EnvelopeStages::Idle || !this->filter.IsSilent(mode);
        // return true;
    }

    bool IsReleased() const { return volEnv.IsReleased(); }
    double GetVolume() const { return volEnv.value; }

    void ResetPhases(bool bRandomPhase) {
    }

    void ResetEnvelopes() {
        volEnv.Reset();
        modEnv.Reset();
        lfoEnv.Reset();
        filter.Reset();
    }

    void Release() {
        volEnv.Release();
        modEnv.Release();
        lfoEnv.Release();
    }

    void SetNote(int n) {
        note            = n;
        targetFrequency = pitchToFrequency(note);
    }

    void SetPitchBendFactor(double f) { pitchBend = f; }
    void ResetPitch() { frequency = targetFrequency; }
    void SetVelocity(double v) { velocity = v; }

    void Start(bool holdOsc1Phase, bool holdOsc2Phase) {
        bIsActive = true;
        // if (bRandomPhase) {
            if (!holdOsc1Phase) {
                oscFm.phase = rand.rng_double();
                osc1a.phase = rand.rng_double();
                osc1b.phase = rand.rng_double();
            }
            if (!holdOsc2Phase) {
                osc2a.phase = rand.rng_double();
                osc2b.phase = rand.rng_double();
            }
        // } else {
        //     oscFm.phase = 0.0;
        //     osc1a.phase = 0.0;
        //     osc1b.phase = 0.0;
        //     osc2a.phase = 0.0;
        //     osc2b.phase = 0.0;
        // }
        volEnv.Start();
        modEnv.Start();
        lfoEnv.Start();
    }

    void UpdateVoiceDrift(double dt, const HostTempo& tempo) {
        driftVelocity += getRandom() * 1.0 * dt;
        driftVelocity -= driftVelocity * 2.0 * dt;
        driftPhase += driftVelocity * dt;
        driftValue = .00001 * sin(driftPhase);
    }
};

} // namespace PluginSynth

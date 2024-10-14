#pragma once
#include "host/audiobuffer/audioblock.h"
#include "types.h"
#include "config.h"
#include "assert_dbg.h"
#include "logging.h"
#include "math/seq_math.h"
#include "note.h"
#include "rand.h"
#include "seq_time.h"
#include "seq_util.h"
#include "str_util.h"

#include "synth-types.hpp"
#include "synth-param.hpp"
#include "synth-template.hpp"
#include "synth-modulations.hpp"
#include "synth-modulations-ui.hpp"
#include "plugins/lfo/lfo-types.hpp"
#include "plugins/lfo/lfo-ui.hpp"
#include "synth-gpu-parameters.h"
#include "synth-gpu-snapshot.hpp"
#include "synth-gpu-gl.h"

#include <cstdint>
#include <nanovg_min.h>
#include <vector>

namespace PluginSynth::GPU {

extern uint32_t gDebugBenchmarkFlags;

constexpr uint16_t NUM_ADSR = 2;
constexpr uint16_t NUM_LFO = 3;
constexpr uint16_t NUM_MOD_SRC_RAND = 2;


enum ModDestinations : int32_t {
    ModDest_MasterVolume = 0,
    ModDest_Osc1Gain,
    ModDest_Osc1Filter,
    ModDest_Osc1PulseWidth,
    ModDest_ADSR_1_A_Duration,
    ModDest_ADSR_1_H_Duration,
    ModDest_ADSR_1_D_Duration,
    ModDest_ADSR_1_S_Amount,
    ModDest_ADSR_1_R_Duration,
    ModDest_ADSR_1_A_Shape,
    ModDest_ADSR_1_D_Shape,
    ModDest_ADSR_1_R_Shape,
    ModDest_ADSR_2_A_Duration,
    ModDest_ADSR_2_H_Duration,
    ModDest_ADSR_2_D_Duration,
    ModDest_ADSR_2_S_Amount,
    ModDest_ADSR_2_R_Duration,
    ModDest_ADSR_2_A_Shape,
    ModDest_ADSR_2_D_Shape,
    ModDest_ADSR_2_R_Shape,
    ModDest_LFO_1_Frequency,
    ModDest_LFO_1_TriggerMode,
    ModDest_LFO_1_Phase,
    ModDest_LFO_1_RampDuration,
    ModDest_LFO_2_Frequency,
    ModDest_LFO_2_TriggerMode,
    ModDest_LFO_2_Phase,
    ModDest_LFO_2_RampDuration,
};


constexpr std::array<const char*, 21> stringsModSource = {
    "None",
    "Function",
    "Constant",
    "VolEnv",
    "ModEnv",
    "Velocity",
    "Pitch",
    "Note",
    "Lfo1",
    "Lfo2",
    "Lfo3",
    "OSC1 Filter",
    "Note Alternating",
    "Note Random 1",
    "Note Random 2",
    "Note Phase (0-1)",
    "Note Held (Fade 0-1-0)",
    "Macro 1",
    "Macro 2",
    "Macro 3",
    "Macro 4",
};

constexpr std::array modSourceVarNames = {
    "x",
    "a",
    "m",
    "v",
    "p",
    "n",
    "l",
    "l2",
    "l3",
    "r",
    "alt",
    "f",
    "rnd",
    "rnd2",
    "ph",
    "held",
    "m1",
    "m2",
    "m3",
    "m4",
};

static constexpr size_t NUM_MODULATION_SOURCES = stringsModSource.size();

struct VoiceSynth {
    std::array<double, 64> modValues{};
    double velocity     = 0.0;
    note_t noteT = {};
    std::array<Envelope, NUM_ADSR> envelopes{};
    std::array<DAW::LFO::LFO, NUM_LFO> lfos{};
    std::array<double, NUM_MOD_SRC_RAND> randoms{};
    double frequency       = 0.0;
    double targetFrequency = 0.0;
    double pitchBend       = 1.0;
    double unisonDetune = 0.0;
    double unisonDetuneKeytrack = 0.0;
    bool bIsActive         = false;
    int32_t seqNr = 0;
    seq_rand rand;


    bool isVoiceActive() const {
        if (hint_likely(!bIsActive)) {
            return false;
        }
        return !GetVolumeEnvelope().IsIdle();
    }

    bool isNotReleased() const {
        return !GetVolumeEnvelope().IsReleased();
    }

    Envelope& GetVolumeEnvelope() { return envelopes[0]; }
    const Envelope& GetVolumeEnvelope() const { return envelopes[0]; }
    Envelope& GetFilterEnvelope() { return envelopes[1]; }
    const Envelope& GetFilterEnvelope() const { return envelopes[1]; }

    bool IsReleased() const { return GetVolumeEnvelope().IsReleased(); }
    double GetVolume() const { return GetVolumeEnvelope().value; }

    void ResetEnvelopes() {
        for (auto& env : envelopes) {
            env.Reset();
        }
    }

    void Release() {
        for (auto& env : envelopes) {
            env.Release();
        }
    }

    void SetNote(const note_t& n) {
        noteT  = n;
        targetFrequency = pitchToFrequency(noteT.pitch);
    }

    void SetPitchBendFactor(double f) { pitchBend = f; }
    void ResetPitch() { frequency = targetFrequency; }
    void SetVelocity(double v) { velocity = v; }

    void Start(bool bTriggerMono, double velocity) {
        ResetPitch();
        SetVelocity(velocity);
        bIsActive = true;
        for (auto& r : randoms) {
            r = rand.rng_double();
        }
        if (!bTriggerMono) {
            ResetEnvelopes();
            for (auto& lfo : lfos) {
                lfo.setPhase(lfo.getParameters().phaseOffset);
                lfo.resetRamp();
            }
        }
        for (auto& env : envelopes) {
            env.Start();
        }
    }

    void Kill() {
        bIsActive = false;
        for (auto& env : envelopes)
            env.Kill();
    }
};

class SynthImplGPU final : public SynthImpl<SynthImplGPU, ParametersSynthGPU>, public ModulationController, public DAW::GPU::GPUAudioProcessor {
private:
    friend class guicontainer_plugin_synth_gpu;
    friend class guicontainer_plugin_synth_adsr_shape;

private:
    PluginSynth::module_synth_template<SynthImplGPU>* const moduleSynthInstance;
    std::array<PluginSynth::GPU::VoiceSynth, MAX_POLY_VOICES> voices;
    std::array<DAW::LFO::LFO, NUM_LFO> lfosSongPos;
    std::array<DAW::LFO::LFOParameters, NUM_LFO> lfoParameters;
    std::array<Envelope::EnvelopeTimeRange, 4> envTimeRanges = {
        Envelope::EnvelopeTimeRange{ 0.0f, 1000.0f }, // attack
        Envelope::EnvelopeTimeRange{ 0.0f, 500.0f }, // hold
        Envelope::EnvelopeTimeRange{ 0.0f, 10000.0f }, // decay
        Envelope::EnvelopeTimeRange{ 0.0f, 10000.0f }, // release
    };
    enum SynthConstParam : uint8_t {
        PARAM_FADE_NOTE_ENDS = 0,
        PARAM_MAX_POLY_VOICES = 0,
        PARAM_MAX_UNISON_VOICES = 1,
    };
    std::array<double, 1> otherParamsDouble{}; // 0: fade note ends
    std::array<int32_t, 2> otherParamsInt{}; // 0: max poly voices, 1: max unison voices
    seq_rand synthRand;
    std::vector<std::shared_ptr<PluginViewContainer>> views;

    int64_t timePerfLog = 0;
    int64_t timeCheckShader = 0;
    double timeComputeAvg = -2.0;
    hires_timer_t perfTimer;
    VoiceSynth tmpVoice;
    size_t numActiveVoicesBlock = 0;
    size_t numActiveVoicesMax = 0;
    int64_t minVoiceIdx = -1;
    int64_t maxVoiceIdx = -1;
    AudioBlock audioOutputBuffer;
    samplecount_t sampleOffsetSubBlock = 0;
    samplecount_t readOffsetSubBlock = 0;

private:
    void initImpl();
    /**
    * UnshapeEnvTimeBaseParam
    * @param d: the shaped envelope parameter
    * @return the unshaped envelope parameter
    * WARNING: Slow! Only use this for initialization purposes! 
    */
    static double UnshapeEnvTimeBaseParam(double d);

public:
    explicit SynthImplGPU(module_synth_template<SynthImplGPU>* module);

    ~SynthImplGPU()
    {
        for (auto* ptr : vecParams) {
            delete ptr;
        }
    }
    void StartVoice(VoiceSynth& voice, VoiceModes mode, double velocity);

    void updateProgramList() override;

    void setBlocksize(samplecount_t blocksize) override;
    void reloadProgram();
    samplecount_t getProcessingSubBlockSampleMidiWriteOffset() const {
        return sampleOffsetSubBlock;
    }

    void init() override;

    const std::array<Envelope::EnvelopeTimeRange, 4>& getEnvTimeRanges() const {
        return envTimeRanges;
    }

    std::array<double, 1>& getOtherParamsDouble() {
        return otherParamsDouble;
    }

    std::array<int32_t, 2>& getOtherParamsInt() {
        return otherParamsInt;
    }

    int32_t& getRefPolyVoiceCount() {
        return otherParamsInt[PARAM_MAX_POLY_VOICES];
    }

    int32_t& getRefUnisonVoiceCount() {
        return otherParamsInt[PARAM_MAX_UNISON_VOICES];
    }

    DAW::LFO::LFOParameters& getLFOParams(int32_t lfoIdx) {
        return lfoParameters[lfoIdx];
    }

    DAW::LFO::LFO& getGlobalLFO(int32_t lfoIdx) {
        return lfosSongPos[lfoIdx];
    }

    DAW::CurveShapingFunction getAdsrShapeMode(int32_t chIdx) const {
        if (chIdx >= 0 && chIdx < CtrSize(tmpVoice.envelopes)) {
            return tmpVoice.envelopes[chIdx].shaping;
        }
        return DAW::CurveShapingFunction::Pow;
    }

    VoiceSynth& getTempVoiceUI() {
        return tmpVoice;
    }

    void setAdsrShapeMode(int32_t chIdx, DAW::CurveShapingFunction mode) {
        for (auto& voice : voices) {
            if (chIdx >= 0 && chIdx < CtrSize(voice.envelopes)) {
                voice.envelopes[chIdx].shaping = mode;
            }
        }
        if (chIdx >= 0 && chIdx < CtrSize(tmpVoice.envelopes)) {
            tmpVoice.envelopes[chIdx].shaping = mode;
        }
    }

    int32_t getSyncFlags(int32_t chIdx) const {
        dbgassert(chIdx >= 0 && chIdx < CtrSize(lfoParameters));
        return lfoParameters[chIdx].syncFlags;
    }

    void setSyncFlags(int32_t chIdx, int32_t flags) {
        dbgassert(chIdx >= 0 && chIdx < CtrSize(lfoParameters));
        lfoParameters[chIdx].syncFlags = flags;
        lfoParameters[chIdx].syncRatios = DAW::LFO::GetSyncRatios(flags);
    }
    int32_t getSyncRatio(int32_t chIdx) const {
        return getSyncFlags(chIdx); 
    }
    bool isShapeMode(int32_t chIdx) const {
        dbgassert(chIdx >= 0 && chIdx < CtrSize(lfoParameters));
        return lfoParameters[chIdx].modeIsShape;
    }
    void setSyncRatio(int32_t chIdx, int32_t ratio) {
        setSyncFlags(chIdx, ratio);
    }
    void setRandomMode(int32_t chIdx, int32_t mode) {
        dbgassert(chIdx >= 0 && chIdx < CtrSize(lfoParameters));
        if (mode >= 0)
            lfoParameters[chIdx].randomModeId = mode;
        auto randomMode = lfoParameters[chIdx].randomModeId;
        lfoParameters[chIdx].modeIsShape = false;
        for (auto& lfo : lfosSongPos) {
            lfo.setRandomMode(randomMode);
        }
        for (auto& voice : voices) {
            for (auto& lfo : voice.lfos) {
                lfo.setRandomMode(randomMode);
            }
        }
        // for (auto& view : views) {
        //     auto implCtrType = dynamic_cast<ViewCtrType*>(view.get());
        //     if (implCtrType) {
        //         implCtrType->ctr_main.setMode(false);
        //     }
        // }
    }
    void setShapeMode(int32_t chIdx) {
        dbgassert(chIdx >= 0 && chIdx < CtrSize(lfoParameters));
        lfoParameters[chIdx].modeIsShape = true;
        // for (auto& view : views) {
        //     auto implCtrType = dynamic_cast<ViewCtrType*>(view.get());
        //     if (implCtrType) {
        //         implCtrType->ctr_main.setMode(true);
        //     }
        // }
    }
    int32_t getRandomMode(int32_t chIdx) const {
        dbgassert(chIdx >= 0 && chIdx < CtrSize(lfoParameters));
        return lfoParameters[chIdx].randomModeId;
    }
    void OnParamChange(Parameters parameter) override;

    samplecount_t getLatency() override { return gpuProgram.blocksize1024Fixed * (ssboOutput.ssbo.size() - 1); }
    void updateEnvelopeParameters(VoiceSynth& v);
    void updateLFOParameters(DAW::LFO::LFOParameters& p, size_t lfoIdx);

    void updateVoiceModulations(ModulationSourceData& modSrcData, VoiceSynth& v, double tickPos);
    double getVoiceLfoValue(const VoiceSynth& v, int32_t lfoIdx) {
        auto bIsSongSync = lfoParameters[lfoIdx].trigger == DAW::LFO::LFOTriggerMode::Free;
        if (bIsSongSync) {
            return lfosSongPos[lfoIdx].GetLfo();
        }
        return v.lfos[lfoIdx].GetLfo();
    }

    void processGpuSynthInput(const DAW::Host::Host* const host, double tick, double samplePos, samplecount_t sampleOffsetInt, samplecount_t sampleOffsetExt, samplecount_t numSamples, DAW::Host::ProcessingQuality quality, playback_state state);
    void dispatchGpuSynth();
    void ProcessSynth(AudioBlock* in, AudioBlock* out, int nFrames, const DAW::Host::Host* const host, double tick, double samplePos, playback_state state) override;

    std::shared_ptr<PluginViewContainer> createViewCtrImpl() override;

    bool getSnapshot(snapshot_t& snapshot) const;

    bool setSnapshot(const snapshot_t& snapshot);

    void onPresetLoaded() {
        for (size_t i = 0; i < lfoParameters.size(); i++) {
            auto& lfoParams = lfoParameters[i];
            lfoParams.syncRatios = DAW::LFO::GetSyncRatios(lfoParams.syncFlags);
            getGlobalLFO(i).setRandomMode(lfoParams.randomModeId);
            for (auto& v : voices) {
                v.lfos[i].setRandomMode(lfoParams.randomModeId);
            }
        }
    }
};

class module_synth_gpu final : public module_synth_template<SynthImplGPU> {
public:
    explicit module_synth_gpu(int32_t _projectGlobalId, IHostCallback* _hostCallback);

    ~module_synth_gpu() override {
        delete impl;
    }
    void processMidi(midi_data_processing_t& midiEvents) override;

    PluginType getPluginType() override { return PLUGIN_TYPE_SYNTH_GPU; };

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
                onPresetLoaded();
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
            auto param = getParam(PARAM_OFFSET_IMPL + idx);
            if (assert_expr(param)) {
                auto paramValue = param->getValue();
                auto paramImpl = vecParams[idx]->valDouble;
                if (paramImpl != paramValue) {
                    log_lf(Log::L_WARN, "param %s: %f != %f\n", param->name.c_str(), paramImpl, paramValue);
                    param->setAll(float(vecParams[idx]->getAsDouble()));
                }
            }
        }
        getSynth()->onPresetLoaded();
        for (auto& view : views) {
            view->onPresetLoaded();
        }
    }
    void getUiSnapshot(snapshot_t& snapshot);
    void setUiSnapshot(snapshot_t& snapshot);

    param_unit_t convertParamValueToDisplay(int32_t idx, float value) override;

    param_converted_t convertParamValueDisplay(int32_t idx, const param_unit_t& displayValue) override;
};

} // namespace PluginSynth::GPU

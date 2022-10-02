#pragma once
#include <vector>
#include <array>
#include <cmath>
#include <mutex>
#include "../plugin-base.h"
#include "../plugin-base.h"
#include <vstsdk-plugin-2.4/audioeffectx.h>

namespace PluginSynth {

    class PluginVST2_Synth;

    enum {
        // Global
        kNumOutputs  = 2,
        kNumInputs   = 2,
    };
    enum Parameters {
        MasterVolume = 0,
        VoiceMode,
        GlideLength,
        FilterMode,
        FilterCutoff,
        FilterResonance,
        FilterKeyTracking,
        VolEnvCutoff,
        ModEnvCutoff,
        OscMix,
        Osc1Wave,
        Osc1Coarse,
        Osc1Fine,
        Osc1Split,
        Osc2Wave,
        Osc2Coarse,
        Osc2Fine,
        Osc2Split,
        LfoShape,
        LfoFrequency,
        LfoDelay,
        LfoCutoff,
        FmMode,
        FmCoarse,
        FmFine,
        VolEnvFm,
        ModEnvFm,
        LfoFm,
        VolEnvA,
        VolEnvD,
        VolEnvS,
        VolEnvR,
        VolEnvV,
        ModEnvA,
        ModEnvD,
        ModEnvS,
        ModEnvR,
        ModEnvV,
        LfoWave,
        Panning,
        Voices,
        UnisonVoices,
        FilterDrive,
        Macro01,
        Macro02,
        Macro03,
        Macro04,
        Macro05,
        Macro06,
        Macro07,
        Macro08,
        LfoPhase,
        VolEnvTriggerMode,
        ModEnvTriggerMode,
        Lfo1RampTriggerMode,
        Lfo1TriggerMode,
        Osc1PhaseResetMode,
        Osc2PhaseResetMode,
        kNumParams
    };
    const Parameters parametersOrdered[] = {
        MasterVolume,
        Voices,
        UnisonVoices,
        Panning,
        VoiceMode,
        GlideLength,
        FilterMode,
        FilterCutoff,
        FilterResonance,
        FilterDrive,
        FilterKeyTracking,
        VolEnvCutoff,
        ModEnvCutoff,
        OscMix,
        Osc1PhaseResetMode,
        Osc1Wave,
        Osc1Coarse,
        Osc1Fine,
        Osc1Split,
        Osc2PhaseResetMode,
        Osc2Wave,
        Osc2Coarse,
        Osc2Fine,
        Osc2Split,
        Lfo1RampTriggerMode,
        Lfo1TriggerMode,
        LfoWave,
        LfoShape,
        LfoPhase,
        LfoFrequency,
        LfoDelay,
        LfoCutoff,
        FmMode,
        FmCoarse,
        FmFine,
        VolEnvFm,
        ModEnvFm,
        LfoFm,
        VolEnvTriggerMode,
        VolEnvA,
        VolEnvD,
        VolEnvS,
        VolEnvR,
        VolEnvV,
        ModEnvTriggerMode,
        ModEnvA,
        ModEnvD,
        ModEnvS,
        ModEnvR,
        ModEnvV,
        Macro01,
        Macro02,
        Macro03,
        Macro04,
        Macro05,
        Macro06,
        Macro07,
        Macro08,
    };
    const Parameters parametersModulate[] = {
        MasterVolume,
        Panning,
        FilterCutoff,
        FilterResonance,
        FilterDrive,
        FilterKeyTracking,
        VolEnvCutoff,
        ModEnvCutoff,
        OscMix,
        Osc1Coarse,
        Osc1Fine,
        Osc1Split,
        Osc2Coarse,
        Osc2Fine,
        Osc2Split,
        LfoShape,
        LfoPhase,
        LfoFrequency,
        LfoDelay,
        LfoCutoff,
        FmCoarse,
        FmFine,
        VolEnvFm,
        ModEnvFm,
        LfoFm,
        VolEnvA,
        VolEnvD,
        VolEnvS,
        VolEnvR,
        VolEnvV,
        ModEnvA,
        ModEnvD,
        ModEnvS,
        ModEnvR,
        ModEnvV
    };
    static_assert(kNumParams == sizeof(parametersOrdered) / sizeof(Parameters), "parametersOrdered is not the correct size");

    enum Settings {
        FilterEnabled,
        ModulationEnabled,
        LfoEnabled,
        ClearModulationEnabled,
        ExprEvaluationEnabled,
        Lfo1OneShotEnabled,
        DiagnosticOutputEnabled,
        LfoShapeType,
        TuningDriftEnabled,
        FilterDriftEnabled,
        LfoPhaseDriftEnabled,
        Lfo1ResetByLfo2Enabled,
        ShowModulationRanges,
        NumSettings,
    };
    extern const std::array<const char*, 13> stringsSettings;

    class SynthState {
    public:
        int osc1Wave   = 0;
        int osc2Wave   = 0;
        int voiceMode  = 0;
        int filterMode = 0;
        int fmMode     = 0;

        double lfoValue      = 0.0;
        double lfo2Value     = 0.0;
        double driftVelocity = 0.0;
        double driftPhase    = 0.0;
        double driftValue    = 0.0;
        double tuningDrift   = 0.5;
        double filterDrift   = 0.7;
        double lfoPhaseDrift = 0.8;

        double osc1Tune                = 1.0;
        double targetOsc1SplitMix      = 0.0;
        double osc1SplitMix            = 0.0;
        double osc1SplitFactorA        = 1.0;
        double osc1SplitFactorB        = 1.0;
        double osc2Tune                = 1.0;
        double targetOsc2SplitMix      = 0.0;
        double osc2SplitMix            = 0.0;
        double osc2SplitFactorA        = 1.0;
        double osc2SplitFactorB        = 1.0;
        double targetOscMix            = 0.0;
        double oscMix                  = 0.0;
        double baseFmAmount            = 0.0;
        double targetFilterCutoff      = 0.0;
        double filterCutoff            = 0.0;
        double targetFilterResonance   = 0.0;
        double filterResonance         = 0.0;
        double targetFilterKeyTracking = 0.0;
        double filterKeyTracking       = 0.0;
        double glideLength             = 0.0;
        double targetMasterVolume      = 0.0;
        double masterVolume            = 0.0;
    };
    class SynthProgramParameters {
    public:
        ~SynthProgramParameters() = default;

    protected:
        double Osc1Wave          = 0.0;
        double Osc1Coarse        = 0.0;
        double Osc1Fine          = 0.0;
        double Osc1Split         = 0.0;
        double Osc2Wave          = 0.0;
        double Osc2Coarse        = 0.0;
        double Osc2Fine          = 0.0;
        double Osc2Split         = 0.0;
        double OscMix            = 0.0;
        double FmMode            = 0.0;
        double FmCoarse          = 0.0;
        double FmFine            = 0.0;
        double FilterMode        = 0.0;
        double FilterCutoff      = 0.0;
        double FilterResonance   = 0.0;
        double FilterKeyTracking = 0.0;
        double VolEnvA           = 0.0;
        double VolEnvD           = 0.0;
        double VolEnvS           = 0.0;
        double VolEnvR           = 0.0;
        double VolEnvV           = 0.0;
        double ModEnvA           = 0.0;
        double ModEnvD           = 0.0;
        double ModEnvS           = 0.0;
        double ModEnvR           = 0.0;
        double ModEnvV           = 0.0;
        double LfoAmount         = 0.0;
        double LfoFrequency      = 0.0;
        double LfoDelay          = 0.0;
        double LfoWave           = 0.0;
        double VolEnvFm          = 0.0;
        double VolEnvCutoff      = 0.0;
        double ModEnvFm          = 0.0;
        double ModEnvCutoff      = 0.0;
        double LfoFm             = 0.0;
        double LfoCutoff         = 0.0;
        double VoiceMode         = 0.0;
        double GlideLength       = 0.0;
        double MasterVolume      = 0.0;
        double Pan               = 0.5;
        double UnisonVoices      = 0.0;
        double PolyVoicesMax     = 0.0;
        double FilterDrive       = 0.5;
        double LfoPhase          = 0.0;
        double VolEnvTriggerMode   = 0.0;
        double ModEnvTriggerMode   = 0.0;
        double Lfo1RampTriggerMode = 0.0;
        double Lfo1TriggerMode     = 0.0;
        double Osc1PhaseResetMode  = 0.0;
        double Osc2PhaseResetMode  = 0.0;
        double MacroValues[8] = {};
    public:
        double* getProgramParameter(Parameters parameter) {
            switch (parameter) {
                case Parameters::VoiceMode: return &VoiceMode;
                case Parameters::GlideLength: return &GlideLength;
                case Parameters::FilterMode: return &FilterMode;
                case Parameters::FilterCutoff: return &FilterCutoff;
                case Parameters::FilterResonance: return &FilterResonance;
                case Parameters::FilterKeyTracking: return &FilterKeyTracking;
                case Parameters::VolEnvCutoff: return &VolEnvCutoff;
                case Parameters::ModEnvCutoff: return &ModEnvCutoff;
                case Parameters::OscMix: return &OscMix;
                case Parameters::Osc1Wave: return &Osc1Wave;
                case Parameters::Osc1Coarse: return &Osc1Coarse;
                case Parameters::Osc1Fine: return &Osc1Fine;
                case Parameters::Osc1Split: return &Osc1Split;
                case Parameters::Osc2Wave: return &Osc2Wave;
                case Parameters::Osc2Coarse: return &Osc2Coarse;
                case Parameters::Osc2Fine: return &Osc2Fine;
                case Parameters::Osc2Split: return &Osc2Split;
                case Parameters::LfoShape: return &LfoAmount;
                case Parameters::LfoFrequency: return &LfoFrequency;
                case Parameters::LfoDelay: return &LfoDelay;
                case Parameters::LfoCutoff: return &LfoCutoff;
                case Parameters::LfoWave: return &LfoWave;
                case Parameters::FmMode: return &FmMode;
                case Parameters::FmCoarse: return &FmCoarse;
                case Parameters::FmFine: return &FmFine;
                case Parameters::VolEnvFm: return &VolEnvFm;
                case Parameters::ModEnvFm: return &ModEnvFm;
                case Parameters::LfoFm: return &LfoFm;
                case Parameters::VolEnvA: return &VolEnvA;
                case Parameters::VolEnvD: return &VolEnvD;
                case Parameters::VolEnvS: return &VolEnvS;
                case Parameters::VolEnvR: return &VolEnvR;
                case Parameters::VolEnvV: return &VolEnvV;
                case Parameters::ModEnvA: return &ModEnvA;
                case Parameters::ModEnvD: return &ModEnvD;
                case Parameters::ModEnvS: return &ModEnvS;
                case Parameters::ModEnvR: return &ModEnvR;
                case Parameters::ModEnvV: return &ModEnvV;
                case Parameters::Panning: return &Pan;
                case Parameters::Voices: return &PolyVoicesMax;
                case Parameters::UnisonVoices: return &UnisonVoices;
                case Parameters::FilterDrive: return &FilterDrive;
                case Parameters::Macro01: return &MacroValues[0];
                case Parameters::Macro02: return &MacroValues[1];
                case Parameters::Macro03: return &MacroValues[2];
                case Parameters::Macro04: return &MacroValues[3];
                case Parameters::Macro05: return &MacroValues[4];
                case Parameters::Macro06: return &MacroValues[5];
                case Parameters::Macro07: return &MacroValues[6];
                case Parameters::Macro08: return &MacroValues[7];
                case Parameters::LfoPhase: return &LfoPhase;
                case Parameters::VolEnvTriggerMode: return &VolEnvTriggerMode;
                case Parameters::ModEnvTriggerMode: return &ModEnvTriggerMode;
                case Parameters::Lfo1RampTriggerMode: return &Lfo1RampTriggerMode;
                case Parameters::Lfo1TriggerMode: return &Lfo1TriggerMode;
                case Parameters::Osc1PhaseResetMode: return &Osc1PhaseResetMode;
                case Parameters::Osc2PhaseResetMode: return &Osc2PhaseResetMode;
                case Parameters::MasterVolume:
                case Parameters::kNumParams:
                    return nullptr;
            }
            dbgassert(0);
            return nullptr;
        }
    };

    struct snapshot_t;
    struct SynthParamBase;
    class SynthImpl;
    class PluginVST2_Synth : public BasePluginVST2 {
    public:
        using ThreadLock = std::lock_guard<std::recursive_mutex>;
        explicit PluginVST2_Synth(audioMasterCallback audioMaster);
        ~PluginVST2_Synth() override;
    
        // internal API
        std::shared_ptr<PluginViewContainers> createViewCtrVst2() override;
        param_converted_t convertParamValueDisplay(int32_t idx, const param_unit_t& displayValue) override;

        void addPropertiesParameterTooltip(Table::tbl& table, int idx) override;

        void notifyUiChanges();
        void onPresetLoaded();

        int32_t loadPreset(const String& path);
        SynthImpl* getSynth();

#ifdef DISPATCHER_DEBUG_TRACE
        VstIntPtr dispatcher(VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt);
#endif// DEBUG
        std::recursive_mutex& getMutex() {
            return mutex;
        }


        // VST2 API
        void setSampleRate(float sampleRate) override;
        VstInt32 processEvents(VstEvents* events) override;///< Called when new MIDI events come in
        void processReplacing(float** inputs, float** outputs, VstInt32 sampleFrames) override;
        VstInt32 getChunk (void** data, bool isPreset = false) override;
	    VstInt32 setChunk (void* data, VstInt32 byteSize, bool isPreset = false) override;

        bool beginSetProgram() override {
            this->issetprogram = true;
            return false;
        }///< Called before a program is loaded

        bool endSetProgram() override {
            this->issetprogram = false;
            return false;
        }///< Called after a program was loaded


        void setParameter(VstInt32 index, float value) override;
        float getParameter(VstInt32 index) override;
        void getParameterLabel(VstInt32 index, char* label) override;
        void getParameterDisplay(VstInt32 index, char* text) override;
        void getParameterName(VstInt32 index, char* text) override;

        bool getEffectName(char* name) override;
        bool getVendorString(char* text) override;
        bool getProductString(char* text) override;
        VstPlugCategory getPlugCategory() override {
            return kPlugCategEffect;
        }
        VstInt32 getVendorVersion() override;
        VstInt32 canDo(char* text) override;
        void getUiSnapshot(snapshot_t& snapshot);
        void setUiSnapshot(snapshot_t& snapshot);
    private:

        /* TODO: release lastProgramChunks after several seconds */
        std::vector<std::shared_ptr<std::vector<std::byte>>> lastProgramChunks;
        std::recursive_mutex mutex;
        SynthImpl* const impl;
        std::vector<SynthParamBase*>& vecParams;
    };
    AudioEffectX* createPlugin(audioMasterCallback audioMaster);
    const char* getName();
}// namespace PluginSynth

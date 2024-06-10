#include "host/plugin/plugin-lockable.h"
#include "synth-template.hpp"
#include "synth-types.hpp"
#include "synth-plugin.h"
#include "synth-modulations.hpp"
#include "synth-modulations-ui.hpp"
#include "assert_dbg.h"
#include "host/audiobuffer/audioblock.h"
#include "host/automation/automation.h"
#include "basectrl.h"
#include "color_util.h"
#include "compiler.h"
#include "config.h"
#include "dsp_util.h"
#include "fileio.h"
#include "gui/gui.h"
#include "gui/container/container.h"
#include "gui/container/scrollcontainer.h"
#include "gui/contextmenu/contextmenu_base.h"
#include "gui/contextmenu/contextmenu_daw.h"
#include "gui/contextmenu/contextmenu.h"
#include "gui/controls/button.h"
#include "gui/controls/inputfield.h"
#include "gui/controls/knob.h"
#include "gui/controls/knobpluginparam.h"
#include "gui/controls/list.h"
#include "gui/controls/textfield.h"
#include "gui/dropdown/dropdown_generic.h"
#include "gui/dropdown/dropdown_preset_tree.h"
#include "gui/dropdown/dropdown.h"
#include "gui/plugin/pluginviewcontainers.h"
#include "gui/shape/shapeeditor.h"
#include "gui/table/table.h"
#include "gui/views/notify.h"
#include "guicolors.h"
#include "guiconstant.h"
#include "guiglobals.h"
#include "host/plugin/internal/internal-plugin.h"
#include "host/project/projectcontroller.h"
#include "IPlugMidi.h"
#include "logging.h"
#include "math/seq_math.h"
#include "math/simd_math.h"
#include "midi-defs.h"
#include "platform.h"
#include "plugins/plugin-base.h"
#include "plugins/plugin-window.h"
#include "plugins/plugin.h"
#include "rand.h"
#include "seq_time.h"
#include "seq_util.h"
#include "host/shape/shape.h"
#include "sse.h"
#include "str_util.h"
#include "synth-snapshot.h"
#include "threads/playbackthread.h"
#include "threads/threadlock.h"
#include "tls.h"
#include "types.h"
#include "util/presetmanager.h"

#include <algorithm>
#include <functional>
#include <muParser.h>
#include <nanovg.h>
#include <optional>
#include <array>
#include <vector>
#include <map>
#include <memory>
#include <vstsdk-host-2.4/aeffectx.h>
#include <glm/gtx/fast_exponential.hpp>
#include <vstsdk-plugin-2.4/audioeffectx.h>
#include <dsp/rates.h>

namespace PluginSynth {

int32_t gDebugOverrides               = -1;
const char* const PLUGIN_EFFECT_NAME  = "Synth";
const uint32_t PLUGIN_UID = 1314080845; //"SYNT";
const char* const PLUGIN_PRODUCT_NAME = "Synth";


static constexpr uint16_t NUM_POLY_VOICES   = 64;
static constexpr uint16_t NUM_UNISON_VOICES = 16;
constexpr bool USE_THREADING = false;
constexpr uint16_t AUDIOPROCESSING_THREADS = USE_THREADING ? 32 : 0;
constexpr uint16_t AUDIOPROCESSING_TASKS = (USE_THREADING) ? NUM_POLY_VOICES * NUM_UNISON_VOICES : 0;


enum ModulationSourceType {
    VolEnv,
    ModEnv,
    Lfo1,
    Velocity,
    VoiceIndex,
    UnisonVoiceIndex,
    Pitch,
    Note,
    Lfo2,
    SrcMacro01,
    SrcMacro02,
    SrcMacro03,
    SrcMacro04,
    SrcMacro05,
    SrcMacro06,
    SrcMacro07,
    SrcMacro08,
    Lfo1Ramp,
    NumModulationSources,
};

const std::array<int32_t, 1 + ModulationSourceType::NumModulationSources + (ModulationType::NumModulationTypes-1)> modSrcTypesOrdered = {
    -1, // None
    Function,
    Constant,
    2 + VolEnv,
    2 + ModEnv,
    2 + Lfo1,
    2 + Lfo1Ramp,
    2 + Lfo2,
    2 + Velocity,
    2 + VoiceIndex,
    2 + UnisonVoiceIndex,
    2 + Pitch,
    2 + Note,
    2 + SrcMacro01,
    2 + SrcMacro02,
    2 + SrcMacro02,
    2 + SrcMacro04,
    2 + SrcMacro05,
    2 + SrcMacro06,
    2 + SrcMacro07,
    2 + SrcMacro08,
};

const std::array<const char*, 1 + ModulationSourceType::NumModulationSources + (ModulationType::NumModulationTypes-1)> stringsModSource = {
    "None",
    "Function",
    "Constant",
    "VolEnv",
    "ModEnv",
    "Lfo1",
    "Lfo1Ramp",
    "Lfo2",
    "Velocity",
    "VoiceIndex",
    "UnisonVoiceIndex",
    "Pitch",
    "Note",
    "SrcMacro01",
    "SrcMacro02",
    "SrcMacro03",
    "SrcMacro04",
    "SrcMacro05",
    "SrcMacro06",
    "SrcMacro07",
    "SrcMacro08",
};
static_assert(stringsModSource.size() == modSrcTypesOrdered.size(), "stringsModSource.size() does not match modSrcTypesOrdered.size()");

// static constexpr auto MathExprInputLen  = 1 + ModulationSourceType::NumModulationSources;
const std::array<const char*, MAX_MODULATION_INPUT_PARAMS> stringsShortSrcNames = {
    "x",
    "a",
    "m",
    "l",
    "v",
    "i",
    "u",
    "p",
    "n",
    "lfo2",
    "m1",
    "m2",
    "m3",
    "m4",
    "m5",
    "m6",
    "m7",
    "m8",
    "r1",
    ""
};


enum {
    // Global
    kNumOutputs  = 2,
    kNumInputs   = 2,
};

enum ParametersSynthUnison {
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
const ParametersSynthUnison parametersOrdered[] = {
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
    // LfoWave,
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
const ParametersSynthUnison parametersModulate[] = {
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
static_assert(kNumParams - 1 == sizeof(parametersOrdered) / sizeof(ParametersSynthUnison), "parametersOrdered is not the correct size");

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
    Oversampling,
    NumSettings,
};
extern const std::array<const char*, 14> stringsSettings;


class SynthState {
public:
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
    double* getProgramParameter(ParametersSynthUnison parameter) {
        using Parameters = ParametersSynthUnison;
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


struct Voice {
    std::array<double, 64> modValues{};
    std::array<float, 8> envelopeValuesCached{};

    Oscillator lfo1;
    Oscillator lfo2;
    double lfoValue     = 0.0;
    double prevLfoValue = 0.0;

    double velocity     = 0.0;
    int32_t indexUnison = 0;
    int noteNr        = 0;
    note_t noteT      = {};
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

    bool isVoiceActive() const {
        if (hint_likely(!bIsActive)) {
            return false;
        }
        return !this->volEnv.IsIdle();
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

    void SetNote(const note_t& n) {
        noteT = n;
        noteNr = n.pitch;
        targetFrequency = pitchToFrequency(noteNr);
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

class VoiceUnison {
public:
    std::array<Voice, NUM_UNISON_VOICES> voices;
    Voice* const first;
    Voice* last;
    int32_t indexPoly = 0;
    int noteNr        = 0;
    note_t noteT      = {};
    int32_t seqNr     = 0;
    seq_rand rand;
    double lfoValue         = 0.0;
    double driftVelocity    = 0.0;
    double driftPhase       = 0.0;
    double driftValue       = 0.0;
    int32_t numUnisonActive = 0;

public:
    VoiceUnison()
        : voices(), first(&voices.front()) {
        last = &voices.back() + 1;
    }
    void setUnisonVoiceCount(int32_t unisonVoiceCount) {
        unisonVoiceCount     = math::max(1, unisonVoiceCount);
        const auto maxVoices = CtrSize(voices);
        last                 = &voices[unisonVoiceCount > maxVoices ? maxVoices : unisonVoiceCount];
        dbgassert(getNumUnisonVoices() == unisonVoiceCount);
    }
    int32_t getNumUnisonVoices() const {
        return static_cast<int32_t>(last - first);
    }
    inline Voice& getVoice(int32_t i) noexcept { return voices[i]; }
    inline const Voice& getVoice(int32_t i) const noexcept { return voices[i]; }
    template<typename Functor>
    void visitVoices(Functor f) {
        std::for_each(first, last, f);
    }
    void init(int32_t indexPoly, uint64_t seed) {
        this->indexPoly = indexPoly;
        rand.rng_seed(seed);
        for (size_t j = 0; j < voices.size(); j++) {
            auto& uv       = voices[j];
            uv.indexUnison = j;
            uv.rand.rng_seed(static_cast<uint64_t>(rand.rng_rand()));
            std::fill(uv.envelopeValuesCached.begin(), uv.envelopeValuesCached.end(), -1.0);
        }
    }
    bool IsInactive() const {
        return std::all_of(first, last, [](auto& voice) { return !voice.bIsActive; });
    }
    bool isVoiceActive() const {
        return std::any_of(first, last, [](auto& voice) { return voice.bIsActive; });
    }

    bool isNotReleased() const {
        return !this->voices[0].IsReleased();
    }
    double GetVolume() const {
        auto voice = std::max_element(
                first, last,
                [](const Voice& a, const Voice& b) {
                    return a.GetVolume() < b.GetVolume();
                });
        return voice->GetVolume();
    }

    double getRandom() {
        double dRandPhase = rand.rng_bits(14) / static_cast<float>(1 << 14);
        return dRandPhase;
    }
    double getRandomPhase() {
        return getRandom() * 0.5;
    }
    void Release() {
        std::for_each(voices.begin(), voices.end(), [](Voice& voice) {
            voice.Release();
        });
    }

    void SetNote(const note_t& n) {
        noteT = n;
        noteNr = n.pitch;
        std::for_each(voices.begin(), voices.end(), [n](Voice& voice) {
            voice.SetNote(n);
        });
    }
    double GetFreqency() const {
        return voices[0].frequency;
    }

    void SetPitchBendFactor(double f) {
        std::for_each(voices.begin(), voices.end(), [f](Voice& voice) {
            voice.SetPitchBendFactor(f);
        });
    }

    void ResetPitch() {
        std::for_each(voices.begin(), voices.end(), [](Voice& voice) {
            voice.ResetPitch();
        });
    }

    void SetVelocity(double v) {
        std::for_each(voices.begin(), voices.end(), [v](Voice& voice) {
            voice.SetVelocity(v);
        });
    }

    void Start(bool bTriggerMono) {
        numUnisonActive = static_cast<int32_t>(last - first);
    }
    void UpdateVoiceDrift(double dt, const HostTempo& tempo) {
        driftVelocity += getRandom() * 1.0 * dt;
        driftVelocity -= driftVelocity * 2.0 * dt;
        driftPhase += driftVelocity * dt;
        driftValue = .0001 * sin(driftPhase);
    }
};

const Settings settingsOrdered[] = {
    Oversampling,
    ShowModulationRanges,
    FilterEnabled,
    FilterDriftEnabled,
    TuningDriftEnabled,
    LfoEnabled,
    Lfo1OneShotEnabled,
    Lfo1ResetByLfo2Enabled,
    LfoPhaseDriftEnabled,
    LfoShapeType,
    ModulationEnabled,
    ClearModulationEnabled,
    ExprEvaluationEnabled,
    DiagnosticOutputEnabled,
};

const std::array<const char*, 14> stringsSettings = {
    "FilterEnabled",
    "ModulationEnabled",
    "LfoEnabled",
    "ClearModulationEnabled",
    "ExprEvaluationEnabled",
    "Lfo1OneShotEnabled",
    "DiagnosticOutputEnabled",
    "LfoShapeType",
    "TuningDriftEnabled",
    "FilterDriftEnabled",
    "LfoPhaseDriftEnabled",
    "Lfo1ResetByLfo2Enabled",
    "ShowModulationRanges",
    "Oversampling",
};

struct MidiMessage {
    int32_t mOffset;
    int32_t StatusMsg() {
        return 0;
    }
    int32_t NoteNumber() {
        return 0;
    }
    int32_t Velocity() {
        return 0;
    }
};

class SynthImplUnison final : public SynthImpl<SynthImplUnison, ParametersSynthUnison>, public SynthState, public ModulationController {
public:
    bool isMathEvalEnabled() const override {
        return getSettingBool(Settings::ExprEvaluationEnabled);
    }
    bool isShowModulationRanges() const override{
        return getSettingBool(Settings::ShowModulationRanges);
    }
public:
    using UnisonVoiceList = std::array<int32_t, NUM_POLY_VOICES * NUM_UNISON_VOICES>;
    using PolyVoiceList   = std::array<int32_t, NUM_POLY_VOICES>;
    struct VoiceList {
        UnisonVoiceList unisonVoices;
        PolyVoiceList polyVoices;
        int32_t numUnisonVoices;
        int32_t maxUnisonVoices;
        int32_t numPolyVoices;
        int32_t polyVoiceIndexFirst;
        int32_t polyVoiceIndexLast;
    };
private:
    friend class PluginVST2_Synth;
    friend class module_synth_unison;
    module_synth_unison* const moduleSynthUnisonInstance;
    PluginVST2_Synth* const instanceVST2Plugin;
    std::array<VoiceUnison, NUM_POLY_VOICES> voices;
    std::array<float, Settings::NumSettings> settings{};
    std::vector<std::shared_ptr<PluginViewContainer>> views;
    Oscillator lfo2;
    SmoothSwitch osc1Wave;
    SmoothSwitch osc2Wave;
    // SmoothSwitch lfoWave;
    // SmoothSwitch filterMode;
    seq_rand synthRand;
    int32_t seq = 0;
    VoiceList prevVoiceList{};
    DAW::Shape::shape_t lfoShape;
    signalsmith::rates::Oversampler2xFIR<float> oversampler;
            
    class SynthVoicProcessTask final : public WorkerThread::ThreadTask {
        bool inUse = false;
        SynthImplUnison* m_impl = nullptr;
        double dt = 0.0;
        VoiceUnison* uv = nullptr;
        Voice* v = nullptr;
        FilterModes filterMode = FilterModes::Off;
        double voice = 0.0;
    public:
        ~SynthVoicProcessTask() {
        }
        struct process_task_stats_t {
            int64_t timeStart = 0;
            int64_t timeEnd = 0;
        };

        void init(SynthImplUnison* _impl) {
            this->m_impl = _impl;
        }

        process_task_stats_t stats;

        bool isInUse() const {
            return inUse;
        }

        void run() override {
            stats.timeStart = getTimeMicros();
            double vData = 0.0;
            voice = m_impl->GetVoiceImpl(dt, *uv, *v, filterMode, vData);
            stats.timeEnd = getTimeMicros();
        }

        void setTask(double dt, VoiceUnison& uv, Voice& voice, FilterModes filterMode) {
            reset();
            this->dt = dt;
            this->uv = &uv;
            this->v = &voice;
            this->filterMode = filterMode;
            inUse = true;
        }

        void resetTask() {
            inUse = false;
        }

        double getVoiceData() const {
            return voice;
        }
        VoiceUnison& getVoiceUnison() {
            return *uv;
        }
        Voice& getVoice() {
            return *v;
        }
    };
    std::array<WorkerThread, AUDIOPROCESSING_THREADS> threads;
    std::array<SynthVoicProcessTask, AUDIOPROCESSING_TASKS> tasks;
    uint32_t threadsRunningCount = 0;
    uint32_t threadCount = AUDIOPROCESSING_THREADS;

    void resetBlock() {
        for (auto i = threadsRunningCount; USE_THREADING && i < threadCount && i < AUDIOPROCESSING_THREADS; i++) {
            threads[i].setRealtimePriority(true);
            threads[i].setTls(daw_tls::getTls());
            threads[i].startThread(StringFormat("AudioProcessingThread %d", i), seqthreads::ThreadType::AudioThread);
            auto task = threads[i].call([]() {
                setSSEFlushDenormals();
            });
            task->wait();
            threadsRunningCount++;
        }
    }

    void startThreads() {
        uint32_t countStarted = 0;
        for (WorkerThread& thread : threads) {
            if (countStarted == this->threadCount) {
                break;
            }
            thread.setRealtimePriority(true);
            thread.setTls(daw_tls::getTls());
            thread.startThread(StringFormat("AudioProcessingThread %d", countStarted), seqthreads::ThreadType::AudioThread);
            auto task = thread.call([]() {
                setSSEFlushDenormals();
            });
            task->wait();
            countStarted++;
        }
        threadsRunningCount = countStarted;
        for (auto& task : tasks) {
            task.init(this);
        }
    }

    void stopThreads() {
        uint32_t countStopped = 0;
        for (WorkerThread& thread : threads) {
            if (countStopped == this->threadsRunningCount) {
                break;
            }
            thread.stopThread();
            countStopped++;
        }
        countStopped = 0;
        for (WorkerThread& thread : threads) {
            if (countStopped == this->threadsRunningCount) {
                break;
            }
            thread.joinThread();
            countStopped++;
        }
        threadsRunningCount = 0;
    }
public:
    int32_t activeVoiceCount  = 0;
    int32_t unisonVoiceCount  = 0;
    int32_t maxUnisonVoice    = 0;
    int32_t polyVoiceCount    = 0;
    int32_t minPolyVoiceIndex = 0;
    int32_t maxPolyVoiceIndex = 0;

    int32_t statsMaxVoiceCount = 0; // max voices seen during runtime

private:
    PresetManager::Preset currentPreset;
    PresetManager presetManager;
    void resetLfoShape() {
        lfoShape.pts.clear();
        lfoShape.pts.push_back({{ 0.5, 0.5 }, 0.5});
    }
    void initImpl() {
        lfoShape = DAW::Shape::GetShapeSaw(DAW::Shape::SHAPE_SHAPED|DAW::Shape::SHAPE_CYCLIC);
        for (auto& setting : settings) {
            setting = 1.0;
        }
        settings[Settings::Oversampling]            = true;
        settings[Settings::Lfo1OneShotEnabled]      = true;
        settings[Settings::DiagnosticOutputEnabled] = false;
        settings[Settings::LfoShapeType]            = false;
        // settings[Settings::ShowModulationRanges]    = false;
        if (gDebugOverrides != -1) {
            for (int i = 0; i < Settings::NumSettings; i++) {
                settings[i] = static_cast<float>((gDebugOverrides >> i) & 1);
            }
        }

        auto now = static_cast<uint64_t>(getTimeMillis());
        synthRand.rng_seed(now);
        resetLfoShape();
        auto pShape = &lfoShape;
        lfo2.setShape(pShape);
        for (size_t i = 0; i < voices.size(); i++) {
            auto& pv = voices[i];
            pv.init(static_cast<int32_t>(i), static_cast<uint64_t>(synthRand.rng_rand()));
            int32_t numVisited = 0;
            pv.visitVoices([&](Voice& v) {
                numVisited++;
                v.osc1a.setShape(pShape);
                v.osc1b.setShape(pShape);
                v.osc2a.setShape(pShape);
                v.osc2b.setShape(pShape);
                v.oscFm.setShape(pShape);
                v.lfo1.setShape(pShape);
                v.lfo2.setShape(pShape);
            });
            dbgassert(numVisited == NUM_UNISON_VOICES);
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
                        if (p->unit == "%")
                            p->format = "%.1f";
                        else
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
        addFloatParam(Parameters::FilterCutoff)->setRange(-22000.0, 22000.0)->setInitialValue(20.0);
        setParamName(getParam(Parameters::FilterCutoff), "Filter Cutoff", "Flt Cut", "Cutoff", "Hz", "%.0f");
        addFloatParam(Parameters::FilterResonance)->setRange(0.0, 1.0)->setInitialValue(0.0);
        setParamName(getParam(Parameters::FilterResonance), "Filter Resonance", "Flt Res", "Resonance");
        addFloatParam(Parameters::FilterDrive)->setRange(-1.0, 1.0)->setInitialValue(0.0);
        setParamName(getParam(Parameters::FilterDrive), "Filter Drive", "Flt Drv", "Drive", "", "%0.2f");
        addFloatParam(Parameters::FilterKeyTracking)->setRange(-24.0, 24.0)->setInitialValue(0.0);
        setParamName(getParam(Parameters::FilterKeyTracking), "Filter Keytracking", "Flt Trk", "Keytrack");
        for (Parameters p = Parameters::Macro01;
                p <= Parameters::Macro08;
                p = static_cast<Parameters>(static_cast<int>(p) + 1)) {
            addFloatParam(p)->setRange(0.0, 1.0)->setInitialValue(0.0);
            setParamName(getParam(p), "Macro " + std::to_string(static_cast<int>(p) - static_cast<int>(Parameters::Macro01) + 1));
        }

        addFloatParam(Parameters::FmFine)->setRange(-1.0, 1.0)->setInitialValue(0.0);
        setParamName(getParam(Parameters::FmFine), "FM fine", "FM fine", "Fine");
        addIntParam(Parameters::FmCoarse)->setRange(0, 48)->setInitialValue(0);
        setParamName(getParam(Parameters::FmCoarse), "FM Coarse", "FM Coarse", "Coarse");

        addFloatParam(Parameters::OscMix)->setRange(0.0, 1.0)->setInitialValue(0.5);
        setParamName(getParam(Parameters::OscMix), "Oscillator Mix", "OSC Mix", "Mix");
        addFloatParam(Parameters::Osc1Fine)->setRange(-1.0, 1.0)->setInitialValue(0.0);
        setParamName(getParam(Parameters::Osc1Fine), "Oscillator 1 fine", "OSC1 Fine", "Fine");
        addFloatParam(Parameters::Osc2Fine)->setRange(-1.0, 1.0)->setInitialValue(0.0);
        setParamName(getParam(Parameters::Osc2Fine), "Oscillator 2 fine", "OSC2 Fine", "Fine");
        addFloatParam(Parameters::Osc1Split)->setRange(-1.25, 1.25)->setInitialValue(0.0);
        setParamName(getParam(Parameters::Osc1Split), "Oscillator 1 split", "OSC1 Split", "Split");
        addFloatParam(Parameters::Osc2Split)->setRange(-1.25, 1.25)->setInitialValue(0.0);
        setParamName(getParam(Parameters::Osc2Split), "Oscillator 2 split", "OSC2 Split", "Split");
        addIntParam(Parameters::Osc1Coarse)->setRange(-24, 24)->setInitialValue(0);
        setParamName(getParam(Parameters::Osc1Coarse), "Oscillator 1 coarse", "OSC1 Semi", "Semi");
        addIntParam(Parameters::Osc2Coarse)->setRange(-24, 24)->setInitialValue(0);
        setParamName(getParam(Parameters::Osc2Coarse), "Oscillator 2 coarse", "OSC2 Semi", "Semi");

        addFloatParam(Parameters::VolEnvA)->setRange(0.0, 1.0)->setInitialValue(0.0);
        addFloatParam(Parameters::VolEnvD)->setRange(0.0, 1.0)->setInitialValue(0.5);
        addFloatParam(Parameters::VolEnvS)->setRange(0.0, 1.0)->setInitialValue(1.0);
        addFloatParam(Parameters::VolEnvR)->setRange(0.0, 1.0)->setInitialValue(0.25);
        addFloatParam(Parameters::VolEnvV)->setRange(0.0, 1.0)->setInitialValue(0.0);
        setParamName(getParam(Parameters::VolEnvA), "Volume envelope attack time", "EnvA Att", "Attack");
        setParamName(getParam(Parameters::VolEnvD), "Volume envelope decay time", "EnvA Dec", "Decay");
        setParamName(getParam(Parameters::VolEnvS), "Volume envelope sustain", "EnvA Sus", "Sustain");
        setParamName(getParam(Parameters::VolEnvR), "Volume envelope release time", "EnvA Rel", "Release");
        setParamName(getParam(Parameters::VolEnvV), "Volume envelope velocity sensitivity", "EnvA Vel", "Velocity");

        addFloatParam(Parameters::ModEnvA)->setRange(0.0, 1.0)->setInitialValue(0.0);
        addFloatParam(Parameters::ModEnvD)->setRange(0.0, 1.0)->setInitialValue(0.5);
        addFloatParam(Parameters::ModEnvS)->setRange(0.0, 1.0)->setInitialValue(0.5);
        addFloatParam(Parameters::ModEnvR)->setRange(0.0, 1.0)->setInitialValue(0.5);
        addFloatParam(Parameters::ModEnvV)->setRange(0.0, 1.0)->setInitialValue(0.0);
        setParamName(getParam(Parameters::ModEnvA), "Mod envelope attack time", "EnvM Att", "Attack");
        setParamName(getParam(Parameters::ModEnvD), "Mod envelope decay time", "EnvM Dec", "Decay");
        setParamName(getParam(Parameters::ModEnvS), "Mod envelope sustain", "EnvM Sus", "Sustain");
        setParamName(getParam(Parameters::ModEnvR), "Mod envelope release time", "EnvM Rel", "Release");
        setParamName(getParam(Parameters::ModEnvV), "Mod envelope velocity sensitivity", "EnvM Vel", "Velocity");

        addFloatParam(Parameters::LfoShape)->setRange(-1.0, 1.0)->setInitialValue(0.0);
        addFloatParam(Parameters::LfoFrequency)->setRange(1 / 64.0, 16.0)->setInitialValue(4.0);
        addFloatParam(Parameters::LfoDelay)->setRange(0.0, 1.0)->setInitialValue(0.05);
        addFloatParam(Parameters::LfoPhase)->setRange(-1.0, 1.0)->setInitialValue(0.0);
        setParamName(getParam(Parameters::LfoShape), "LFO shape", "LFO shape", "Shape");
        setParamName(getParam(Parameters::LfoFrequency), "LFO frequency", "LFO freq", "Freq");
        setParamName(getParam(Parameters::LfoDelay), "LFO ramp", "LFO ramp", "Ramp");
        setParamName(getParam(Parameters::LfoPhase), "LFO phase", "LFO phase", "Phase");

        addFloatParam(Parameters::VolEnvFm)->setRange(-24.0, 24.0)->setInitialValue(0.0);
        addFloatParam(Parameters::ModEnvFm)->setRange(-24.0, 24.0)->setInitialValue(0.0);
        addFloatParam(Parameters::LfoFm)->setRange(-24.0, 24.0)->setInitialValue(0.0);
        addFloatParam(Parameters::VolEnvCutoff)->setRange(-24000.0, 24000.0)->setInitialValue(0.0);
        addFloatParam(Parameters::ModEnvCutoff)->setRange(-24000.0, 24000.0)->setInitialValue(0.0);
        addFloatParam(Parameters::LfoCutoff)->setRange(-2 * 24000.0, 2 * 24000.0)->setInitialValue(0.0);
        addFloatParam(Parameters::GlideLength)->setRange(0.0, 1.0)->setInitialValue(0.0);
        addFloatParam(Parameters::MasterVolume)->setRange(0.0, 0.5)->setInitialValue(0.25);

        setParamName(getParam(Parameters::VolEnvFm), "Volume envelope to FM amount", "FM Amt EnvA", "Env Vol");
        setParamName(getParam(Parameters::ModEnvFm), "Modulation envelope to FM amount", "FM Amt EnvM", "Env Mod");
        setParamName(getParam(Parameters::LfoFm), "LFO to FM amount", "FM Amt LFO", "LFO1");
        setParamName(getParam(Parameters::VolEnvCutoff), "Volume envelope to filter cutoff", "Flt EnvA", "Env Vol", "Hz", "%.0f");
        setParamName(getParam(Parameters::ModEnvCutoff), "Modulation envelope to filter cutoff", "Flt EnvM", "Env Mod", "Hz", "%.0f");
        setParamName(getParam(Parameters::LfoCutoff), "LFO to Filter Cutoff", "LFO1 Amount", "LFO1", "Hz", "%.0f");
        setParamName(getParam(Parameters::GlideLength), "Glide length", "Glide");
        setParamName(getParam(Parameters::MasterVolume), "Volume", "Volume", "Volume", "%");


        addEnumParam(Parameters::Osc1Wave)->setStrings(stringsWaveform.begin(), stringsWaveform.end())->setInitialValue(static_cast<int32_t>(Waveforms::Saw));
        setParamName(getParam(Parameters::Osc1Wave), "Osc1 Waveform", "Osc1 Waveform", "Waveform");
        addEnumParam(Parameters::Osc2Wave)->setStrings(stringsWaveform.begin(), stringsWaveform.end())->setInitialValue(static_cast<int32_t>(Waveforms::Saw));
        setParamName(getParam(Parameters::Osc2Wave), "Osc2 Waveform", "Osc2 Waveform", "Waveform");
        // addEnumParam(Parameters::LfoWave)->setStrings(stringsWaveform.begin(), stringsWaveform.end())->setInitialValue(static_cast<int32_t>(Waveforms::));
        // setParamName(getParam(Parameters::LfoWave), "LFO1 Waveform", "LFO1 Waveform", "Waveform");
        addEnumParam(Parameters::VoiceMode)->setStrings(stringsVoiceMode.begin(), stringsVoiceMode.end())->setInitialValue(0);
        setParamName(getParam(Parameters::VoiceMode), "Voice Mode");
        addEnumParam(Parameters::FilterMode)->setStrings(stringsFilterMode.begin(), stringsFilterMode.end())->setInitialValue(0);
        setParamName(getParam(Parameters::FilterMode), "Filter Mode", "Flt Mode");
        addEnumParam(Parameters::FmMode)->setStrings(stringsFMMode.begin(), stringsFMMode.end())->setInitialValue(0);
        setParamName(getParam(Parameters::FmMode), "FM Mode");
        addEnumParam(Parameters::VolEnvTriggerMode)->setStrings(stringsReset.begin(), stringsReset.end())->setInitialValue(0);
        setParamName(getParam(Parameters::VolEnvTriggerMode), "Volume envelope reset mode", "Vol env reset", "Reset");
        addEnumParam(Parameters::ModEnvTriggerMode)->setStrings(stringsReset.begin(), stringsReset.end())->setInitialValue(0);
        setParamName(getParam(Parameters::ModEnvTriggerMode), "Modulation envelope reset mode", "Mod env reset", "Reset");
        addEnumParam(Parameters::Lfo1TriggerMode)->setStrings(stringsReset.begin(), stringsReset.end())->setInitialValue(0);
        setParamName(getParam(Parameters::Lfo1TriggerMode), "LFO1 phase reset mode", "LFO1 phase reset", "Phase Reset");
        addEnumParam(Parameters::Lfo1RampTriggerMode)->setStrings(stringsReset.begin(), stringsReset.end())->setInitialValue(0);
        setParamName(getParam(Parameters::Lfo1RampTriggerMode), "LFO1 ramp reset mode", "LFO1 ramp reset", "Ramp Reset");
        addEnumParam(Parameters::Osc1PhaseResetMode)->setStrings(stringsReset.begin(), stringsReset.end())->setInitialValue(0);
        setParamName(getParam(Parameters::Osc1PhaseResetMode), "OSC1 phase reset mode", "OSC1 phase reset", "OSC1 phase reset");
        addEnumParam(Parameters::Osc2PhaseResetMode)->setStrings(stringsReset.begin(), stringsReset.end())->setInitialValue(0);
        setParamName(getParam(Parameters::Osc2PhaseResetMode), "OSC2 phase reset mode", "OSC2 phase reset", "OSC2 phase reset");

        addIntParam(Parameters::Voices)->setRange(1, NUM_POLY_VOICES)->setInitialValue(32);
        addIntParam(Parameters::UnisonVoices)->setRange(1, NUM_UNISON_VOICES)->setInitialValue(3);

        setParamName(getParam(Parameters::Voices), "Polyphonic Voice Maximum", "Voices");
        setParamName(getParam(Parameters::UnisonVoices), "Unison Voices", "Unison");

        addFloatParam(Parameters::Panning)->setRange(-1.0, 1.0)->setInitialValue(0.0);
        setParamName(getParam(Parameters::Panning), "Stereo Panning", "Pan");
        for (size_t i = 0; i < stringsModSource.size(); ++i) {
            auto idx = -1 + i;
            modSourceDescs.emplace_back(idx, stringsModSource[i]);
        }
        int32_t dstIdx = 0;
        for (auto param : parametersModulate) {
            dbgassert(getParam(static_cast<Parameters>(param)));
            modDestDescs.emplace_back(dstIdx++, static_cast<int32_t>(param), getParam(static_cast<Parameters>(param))->name);
        }
        varNames = stringsShortSrcNames;
        String defaultPresetPath = App::Platform::toUserdataPath(String("presets/") + PLUGIN_EFFECT_NAME);
        CreateDirectoryIfNotExists(defaultPresetPath);
        presetManager.load(defaultPresetPath);
        setPreset(defaultPresetPath, "Untitled");
        startThreads();
    }

public:
    explicit SynthImplUnison(PluginVST2_Synth* vst2Plugin) : SynthImpl<SynthImplUnison, ParametersSynthUnison>(nullptr), moduleSynthUnisonInstance(nullptr), instanceVST2Plugin(vst2Plugin) {
        initImpl();
    }
    explicit SynthImplUnison(module_synth_unison* module);
    ~SynthImplUnison()
    {
        stopThreads();
        for (auto* ptr : vecParams) {
            delete ptr;
        }
    }
    PresetManager& getPresetManager() {
        return presetManager;
    }
    const VoiceList& getVoiceListPrev() const {
        return prevVoiceList;
    }
    bool getSetting(Settings setting) const {
        return settings[setting] > 0.5;
    }
    bool getSettingBool(Settings setting) const {
        return settings[setting] >= 0.5;
    }
    void setSetting(Settings setting, bool value);

    void init() override {
        for (auto param : this->vecParams) {
            if (!param) continue;
            OnParamChange(static_cast<Parameters>(param->enumParam));
        }
        setModulationType(0, 0, static_cast<int32_t>(ModulationType::ModulationSource) + static_cast<int32_t>(ModulationSourceType::Lfo1));
        setModulationDestination(0, 0, Parameters::FilterCutoff, 0.5);
    }

    samplecount_t getLatency() override {
        return getSetting(Settings::Oversampling) ? oversampler.latency()/2 : 0;
    }

    void initSampleRate() override {
    }

    void setLfoShape(const DAW::Shape::shape_t& shape) {
        lfoShape.pts = shape.pts;
        notifyUiChanges();
    }

    void setBlocksize(samplecount_t bs) override {
        this->oversampler.resize(2, bs);
    }

    int32_t getActiveVoiceCount() {
        return activeVoiceCount;
    }

    int32_t getStatsMaxVoiceCount() {
        return statsMaxVoiceCount;
    }

    bool getSnapshot(snapshot_t& snapshot) const {
        snapshot.version     = SYNTH_SNAPSHOT_VERSION;
        const auto numParams = CtrSize(vecParams);
        snapshot.params.reserve(numParams);
        for (int32_t i = 0; i < numParams; ++i) {
            if (!vecParams[i]) continue;
            // dbgassert(vecParams[i]->getAsDouble() >= 0.0 && vecParams[i]->getAsDouble() <= 1.0);
            snapshot.params.push_back({ i, vecParams[i]->getAsDouble() });
        }
        const auto numModulations = CtrSize(modulations);
        snapshot.modulations.reserve(numModulations);
        for (int32_t i = 0; i < numModulations; ++i) {
            const auto& modulation = modulations[i];
            modulation_snapshot_t modSnapshot;
            modSnapshot.slotIdx  = i;
            const auto numInputs = CtrSize(modulation.inputs);
            for (int32_t j = 0; j < numInputs; ++j) {
                const auto& input = modulation.inputs[j];
                if (input.type < 0) {
                    continue;
                }
                if (input.type >= ModulationType::NumModulationTypes) {
                    continue;
                }
                auto inputType    = math::clamp<int32_t>(input.type, 0, ModulationType::NumModulationTypes - 1);
                auto inputSrcType = math::clamp<int32_t>(input.src, 0, ModulationSourceType::NumModulationSources - 1);
                auto inputOpType  = math::clamp<int32_t>(input.op, 0, ModulationOperator::NumModulationOperators - 1);
                modSnapshot.inputs.push_back({ inputType,
                                                inputSrcType,
                                                inputOpType,
                                                input.value,
                                                input.function.str,
                                                static_cast<uint8_t>(input.range) });
            }
            const auto numDestinations = CtrSize(modulation.destinations);
            for (int32_t j = 0; j < numDestinations; ++j) {
                const auto& dest = modulation.destinations[j];
                modSnapshot.destinations.push_back({ static_cast<int32_t>(dest.parameter), dest.range });
            }
            snapshot.modulations.push_back(modSnapshot);
        }
        const auto numSettings = CtrSize(settings);
        snapshot.settings.reserve(numSettings);
        for (int32_t i = 0; i < numSettings; ++i) {
            snapshot.settings.push_back({ i, settings[i] });
        }
        snapshot.shapes.push_back(DAW::Shape::shape_snapshot_t{ 0, DAW::Shape::shape_preset_t{2, lfoShape} });
        return true;
    }

    bool setSnapshot(const snapshot_t& snapshot) {
        if (snapshot.version < 2) {
            dbgassert(0);
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
        modulations.clear();
        modulations.reserve(snapshot.modulations.size());
        for (auto& ms : snapshot.modulations) {
            auto msSlotIndex = ms.slotIdx;
            Modulation newModulation;
            const auto numInputs = CtrSize(ms.inputs);
            for (int32_t j = 0; j < numInputs; ++j) {
                const auto& input = ms.inputs[j];
                newModulation.inputs.push_back({ static_cast<ModulationType>(input.typeIdx),
                                                    static_cast<ModulationSourceType>(input.srcIdx),
                                                    static_cast<ModulationOperator>(input.opIdx),
                                                    input.value,
                                                    MathExpr{ input.function, nullptr },
                                                    static_cast<ModulationRange>(input.range) });
            }
            const auto numDestinations = CtrSize(ms.destinations);
            for (int32_t j = 0; j < numDestinations; ++j) {
                const auto& dest = ms.destinations[j];
                newModulation.destinations.push_back({ dest.paramIdx, dest.range });
            }
            while (CtrSize(modulations) <= msSlotIndex) {
                modulations.push_back({});
            }
            modulations[msSlotIndex] = std::move(newModulation);
        }

        for (auto& setting : snapshot.settings) {
            if (setting.paramIdx >= 0 && setting.paramIdx < CtrSize(settings)) {
                settings[setting.paramIdx] = setting.range;
            } else {
                dbgassert(0);
            }
        }
        resetLfoShape();
        for (auto& shape : snapshot.shapes) {
            if (shape.type == 0) {
                lfoShape = shape.shape.curve;
            } else {
                dbgassert(0);
            }
        }
        if (lfoShape.pts.size() < 2) {
            lfoShape = DAW::Shape::GetShapeSaw(DAW::Shape::SHAPE_SHAPED|DAW::Shape::SHAPE_CYCLIC);
        }

        for (auto& mod : modulations) {
            for (auto& input : mod.inputs) {
                try {
                    input.function = MathExpr::parse(input.function.str, getVarNames());
                } catch (mu::Parser::exception_type& e) {
                    input.function = MathExpr{};
                    log_lf(Log::L_ERROR, "Error in expression: %s\n", e.GetMsg().c_str());
                }
            }
        }
        for (auto& param : vecParams) {
            if (!param) continue;
            OnParamChange(static_cast<Parameters>(param->enumParam));
        }
        return true;
    }

    int32_t loadPreset(const String& presetPath);
    std::shared_ptr<PluginViewContainer> createViewCtrImpl() override;
private:

    float synthRandom() {
        uint32_t rnd32Bits = synthRand.rng_rand();
        return (rnd32Bits & 0xFFFF) / (float) 0xFFFF;
    }

    void UpdateParameters(double dt) {
        osc1Wave.Update(dt);
        osc1SplitMix += (targetOsc1SplitMix - osc1SplitMix) * 100.0 * dt;
        osc2Wave.Update(dt);
        osc2SplitMix += (targetOsc2SplitMix - osc2SplitMix) * 100.0 * dt;
        // lfoWave.Update(dt);
        oscMix += (targetOscMix - oscMix) * 100.0 * dt;
        masterVolume += (targetMasterVolume - masterVolume) * 100.0 * dt;
    }

    void UpdateVoiceEnvelopeModulations(VoiceUnison& vu, Voice& voice) {
        static constexpr auto LEN_USED = 6;
        static constexpr auto LEN_SIMD = 8;

        static const Parameters envParms[LEN_USED] = {
            Parameters::VolEnvA,
            Parameters::VolEnvD,
            Parameters::VolEnvR,
            Parameters::ModEnvA,
            Parameters::ModEnvD,
            Parameters::ModEnvR,
        };
        double* const envParamValsPtr[LEN_USED] = {
            &voice.volEnv.a,
            &voice.volEnv.d,
            &voice.volEnv.r,
            &voice.modEnv.a,
            &voice.modEnv.d,
            &voice.modEnv.r,
        };
#define OPTIMIZED_VERSION
#ifdef OPTIMIZED_VERSION
        using FPType = float;
        alignas(64) FPType envParamVals[LEN_SIMD]{};
        alignas(64) FPType envParamValsScaled[LEN_SIMD]{};
        for (int i = 0; i < LEN_USED; i++) {
            envParamVals[i] = math::clamp<FPType>(GetModulatedParamVoice(voice, envParms[i]), 1.0E-12, 1.0);
        }
        auto sizeofarr = sizeof(envParamVals);
        bool bAllEqual = std::memcmp(voice.envelopeValuesCached.data(), envParamVals, sizeofarr) == 0;
        if (!bAllEqual) {
            std::memcpy(voice.envelopeValuesCached.data(), envParamVals, sizeof(envParamVals));
            ShapeLogLikeSIMD<FPType, 8>(envParamVals, envParamValsScaled);
            for (int i = 0; i < LEN_USED; i++) {
                *envParamValsPtr[i] = Envelope::GetTimeBaseFromParam(envParamValsScaled[i]);
            }
        }
        voice.volEnv.s = GetModulatedParamVoice(voice, Parameters::VolEnvS);
        voice.modEnv.s = GetModulatedParamVoice(voice, Parameters::ModEnvS);
        auto p         = GetParamFloat(Parameters::LfoDelay);
        double rampVal = 1.0 - p->ValueModulated(voice.modValues[p->enumParam]);
        rampVal = (rampVal*rampVal);
        rampVal = (rampVal*rampVal);
        double rmpMin = 0.1;
        double rmpMax = 4000;
        double rampAtt = math::clamp(rmpMin+(rampVal)*(rmpMax-rmpMin), rmpMin, rmpMax);
        voice.lfoEnv.a = rampAtt;
        // log_printf("rampAtt %f\n", rampAtt);
        // voice.lfoEnv.a = p->GetMin() + p->GetMax() - (p->ValueModulated(voice.modValues[p->enumParam]));
#else
        double envParamVals[LEN_SIMD]{};
        double envParamValsScaled[LEN_SIMD]{};
        for (int i = 0; i < LEN_USED; i++) {
            envParamVals[i] = GetModulatedParamVoice(voice, envParms[i]);
        }
        for (int i = 0; i < LEN_USED; i++) {
            const auto valClamped = math::clamp(envParamVals[i], 1.0E-12, 1.0);
            const auto valPowed   = exp(log(valClamped) * 0.1);
            const auto valCosd    = cos(valPowed * M_PI);
            envParamValsScaled[i] = 1000.0 - 999.9 * (.5 - .5 * valCosd);
        }
        for (int i = 0; i < LEN_USED; i++) {
            *envParamValsPtr[i] = double(envParamValsScaled[i]);
        }
        voice.volEnv.s = GetModulatedParamVoice(voice, Parameters::VolEnvS);
        voice.modEnv.s = GetModulatedParamVoice(voice, Parameters::ModEnvS);
        auto p         = GetParamFloat(Parameters::LfoDelay);
        voice.lfoEnv.a = p->GetMin() + p->GetMax() - (p->ValueModulated(voice.modValues[p->enumParam]));
#endif
    }

    void UpdateVoiceEnvelopes(double dt, VoiceUnison& vu, Voice& voice) {
        voice.volEnv.Update(dt);
        voice.modEnv.Update(dt);
        voice.lfoEnv.Update(dt);
    }

    void UpdateAllVoiceStates(double dt, FilterModes filterMode, VoiceList& list) {
        dbgassert((list.numPolyVoices == 0) == (list.numUnisonVoices == 0));
        list.maxUnisonVoices = 0;
        list.polyVoiceIndexFirst = -1;
        list.polyVoiceIndexLast  = 0;
        for (int32_t polyIndex = 0; polyIndex < maxPolyVoiceIndex; ++polyIndex) {
            auto& uv      = voices[polyIndex];
            int32_t numAc = 0;
            for (int32_t unisonIndex = 0; unisonIndex < maxUnisonVoice; ++unisonIndex) {
                auto& v        = uv.getVoice(unisonIndex);
                bool bIsActive = v.isVoiceActive();
                dbgassert(!(!v.bIsActive && bIsActive));
                v.bIsActive = bIsActive;
                if (bIsActive) {
                    list.unisonVoices[list.numUnisonVoices++] = polyIndex * NUM_UNISON_VOICES + unisonIndex;
                    list.maxUnisonVoices = math::max(list.maxUnisonVoices, unisonIndex + 1);
                    numAc++;
                }
            }
            uv.numUnisonActive = numAc;
            if (uv.numUnisonActive) {
                list.polyVoiceIndexFirst = list.polyVoiceIndexFirst < 0 ? polyIndex : math::min(list.polyVoiceIndexFirst, polyIndex);
                list.polyVoiceIndexLast = math::max(list.polyVoiceIndexLast, polyIndex + 1);
                list.polyVoices[list.numPolyVoices++] = polyIndex;
            }
        }
        maxUnisonVoice    = list.maxUnisonVoices;
        maxPolyVoiceIndex = list.polyVoiceIndexLast;
        minPolyVoiceIndex = list.polyVoiceIndexFirst < 0 ? polyVoiceCount : list.polyVoiceIndexFirst;
        dbgassert((list.numPolyVoices > 0) == (list.numUnisonVoices > 0));
    }

    void UpdateAllVoiceLfos(double dt, const VoiceList& list) {
        auto floatParamFreq        = static_cast<SynthParam_Float*>(vecParams[Parameters::LfoFrequency]);
        auto floatParamShape       = static_cast<SynthParam_Float*>(vecParams[Parameters::LfoShape]);
        const auto lfo1OneShot     = getSettingBool(Settings::Lfo1OneShotEnabled);
        const auto lfo1ResetByLfo2 = getSettingBool(Settings::Lfo1ResetByLfo2Enabled);
        const auto lfoShapeMode    = getSettingBool(Settings::LfoShapeType);
        const auto bpmHz           = math::max(tempo.bpm, 1.0) / 60.0;
        for (int32_t p = 0; p < list.numPolyVoices; ++p) {
            auto& uv = voices[list.polyVoices[p]];
            uv.UpdateVoiceDrift(dt, tempo);
            for (int32_t unisonIndex = 0; unisonIndex < list.maxUnisonVoices; ++unisonIndex) {
                auto& v = uv.getVoice(unisonIndex);
                v.UpdateVoiceDrift(dt, tempo);

                if (v.bIsActive) {
                    if (v.lfo2.Update(dt, math::max(tempo.bpm / 4.0, 1.0) / 60.0) && lfo1ResetByLfo2) {
                        double phase = GetModulatedParamVoice(v, Parameters::LfoPhase);
                        v.lfo1.initPhase(phase + lfoPhaseDrift * this->driftValue * v.rand.rng_double(), false);
                    }
                    // dbgassert(v.lfo1.phase >= -1.0 && v.lfo1.phase <= 1.0);
                    double lfoFreqHz    = floatParamFreq->ValueModulated(v.modValues[Parameters::LfoFrequency]) * bpmHz;
                    double dVoiceLfoBi  = v.lfo1.GetLfo(dt, lfoFreqHz, lfo1OneShot);
                    v.lfo1.phaseFade -= (v.lfo1.phaseFade) * 1000 * dt;
                    double dVoiceLfoUni = 0.5 + 0.5 * dVoiceLfoBi;
                    dbgassert(dVoiceLfoUni >= 0.0 && dVoiceLfoUni <= 1.0);
                    double lfoAmount = floatParamShape->ValueModulated(v.modValues[Parameters::LfoShape]);
                    double dLfoShapeExp = 0.0;
                    if (lfoAmount < 0.0) {
                        dLfoShapeExp = 1.0 + dVoiceLfoUni * -lfoAmount * 16.;
                    } else {
                        dLfoShapeExp = 1.0 / (1.0 + dVoiceLfoUni * lfoAmount * 16.);
                    }
                    dbgassert(!fp_math::isNanOrInfd(dVoiceLfoUni));
                    dbgassert(!fp_math::isNanOrInfd(dLfoShapeExp));
                    double dVoiceLfoUniShaped = 0.0;
                    if (lfoShapeMode) {
                        dVoiceLfoUniShaped = exp(log(dVoiceLfoUni * dVoiceLfoUni) * dLfoShapeExp);
                    } else {
                        dVoiceLfoUniShaped = exp(log(abs(dVoiceLfoUni)) * dLfoShapeExp);
                    }
                    dbgassert(!fp_math::isNanOrInfd(dVoiceLfoUniShaped));
                    // v.lfoValue = v.lfoValue * 0.99 + dVoiceLfoUniShaped * 0.01;
                    v.lfoValue = dVoiceLfoUniShaped * (1.0 - v.lfo1.phaseFade) + v.lfo1.phaseFade * v.prevLfoValue;
                }
            }
        }
    }

    void UpdateVoiceModulations(VoiceUnison& vu, Voice& voice) {
        if (!getSetting(Settings::ModulationEnabled)) {
            return;
        }

        auto& voiceModulations = voice.modValues;
        if (getSetting(Settings::ClearModulationEnabled)) {
            std::memset(voiceModulations.data(), 0, voiceModulations.size() * sizeof(double));
        }

        ModulationSourceData modSrcData{};
        modSrcData[1 + ModulationSourceType::Lfo2] = lfo2Value;
        modSrcData[1 + ModulationSourceType::VolEnv]           = voice.volEnv.value;
        modSrcData[1 + ModulationSourceType::ModEnv]           = voice.modEnv.value;
        modSrcData[1 + ModulationSourceType::Lfo1]             = voice.lfoValue;
        modSrcData[1 + ModulationSourceType::Lfo1Ramp]         = voice.lfoEnv.value;
        modSrcData[1 + ModulationSourceType::Velocity]         = voice.velocity;
        modSrcData[1 + ModulationSourceType::VoiceIndex]       = this->polyVoiceCount < 2 ? 0.5 : vu.indexPoly / static_cast<double>(this->polyVoiceCount - 1);
        modSrcData[1 + ModulationSourceType::UnisonVoiceIndex] = this->unisonVoiceCount < 2 ? 0.5 : voice.indexUnison / static_cast<double>(this->unisonVoiceCount - 1);
        modSrcData[1 + ModulationSourceType::Pitch]            = noteToLinearScale(voice.noteNr);
        modSrcData[1 + ModulationSourceType::Note]             = voice.noteNr / 127.0;
        modSrcData[1 + ModulationSourceType::SrcMacro01] = GetParamFloat(Parameters::Macro01)->Value();
        modSrcData[1 + ModulationSourceType::SrcMacro02] = GetParamFloat(Parameters::Macro02)->Value();
        modSrcData[1 + ModulationSourceType::SrcMacro03] = GetParamFloat(Parameters::Macro03)->Value();
        modSrcData[1 + ModulationSourceType::SrcMacro04] = GetParamFloat(Parameters::Macro04)->Value();
        modSrcData[1 + ModulationSourceType::SrcMacro05] = GetParamFloat(Parameters::Macro05)->Value();
        modSrcData[1 + ModulationSourceType::SrcMacro06] = GetParamFloat(Parameters::Macro06)->Value();
        modSrcData[1 + ModulationSourceType::SrcMacro07] = GetParamFloat(Parameters::Macro07)->Value();
        modSrcData[1 + ModulationSourceType::SrcMacro08] = GetParamFloat(Parameters::Macro08)->Value();
        ProcessModulations(modSrcData, voiceModulations);
    }

    void UpdateDrift(double dt) {
        driftVelocity += synthRandom() * 4.0 * dt;
        driftVelocity -= driftVelocity * 2.0 * dt;
        driftPhase += driftVelocity * dt;
        driftValue = .001 * sin(driftPhase * 2.0 * M_PI);
    }

    double GetModulatedParamVoice(Voice& voice, Parameters param) const {
        dbgassert(param < vecParams.size());
        dbgassert(vecParams[param]->type == SynthParam::ParamType::FLOAT);
        return static_cast<SynthParam_Float*>(vecParams[param])->ValueModulated(voice.modValues[param]);
    }

    double GetModulatedIntParamVoice(Voice& voice, Parameters param) const {
        dbgassert(param < vecParams.size());
        dbgassert(vecParams[param]->type == SynthParam::ParamType::INT);
        return static_cast<SynthParam_Int*>(vecParams[param])->ValueModulated(voice.modValues[param]);
    }

    double GetModulatedParamVoiceRaw(const Voice& voice, Parameters param) const {
        return voice.modValues[param];
    }

    double GetVoiceImpl(double dt, VoiceUnison& uv, Voice& voice, FilterModes filtermode, double& data) {
        auto delayedLfoValue = voice.lfoValue * voice.lfoEnv.value;
        auto volEnvV         = GetModulatedParamVoice(voice, Parameters::VolEnvV);
        auto volEnvValueRaw  = voice.volEnv.value;
        auto volEnvValue     = (1.0 - volEnvV) * volEnvValueRaw + volEnvV * volEnvValueRaw * voice.velocity;
        auto modEnvV         = GetModulatedParamVoice(voice, Parameters::ModEnvV);
        auto modEnvValue     = (1.0 - modEnvV) * voice.modEnv.value + modEnvV * voice.modEnv.value * voice.velocity;


        auto baseFrequency = voice.frequency * voice.pitchBend;
        if (getSetting(Settings::TuningDriftEnabled)) {
            baseFrequency *= (1.0 + (voice.driftValue + driftValue) * tuningDrift);
        }

        {
            auto coarse = GetModulatedIntParamVoice(voice, Parameters::Osc1Coarse);
            auto fine   = GetModulatedParamVoice(voice, Parameters::Osc1Fine);
            osc1Tune    = pitchFactor(coarse + fine);
        }
        {
            auto coarse = GetModulatedIntParamVoice(voice, Parameters::Osc2Coarse);
            auto fine   = GetModulatedParamVoice(voice, Parameters::Osc2Fine);
            osc2Tune    = pitchFactor(coarse + fine);
        }

        auto osc1Frequency = osc1Tune * baseFrequency;
        auto osc2Frequency = osc2Tune * baseFrequency;

        auto fmMode = GetParamEnum(Parameters::FmMode)->getEnumValue<FmModes>();
        switch (fmMode) {
            case FmModes::Osc1:
            case FmModes::Osc2: {
                auto fmCoarse = GetModulatedIntParamVoice(voice, Parameters::FmCoarse);
                auto fmFine   = GetModulatedParamVoice(voice, Parameters::FmFine);
                baseFmAmount  = fmCoarse + fmFine;
                auto fmAmount = baseFmAmount;
                fmAmount += GetModulatedParamVoice(voice, Parameters::VolEnvFm) * volEnvValue;
                fmAmount += GetModulatedParamVoice(voice, Parameters::ModEnvFm) * modEnvValue;
                fmAmount += GetModulatedParamVoice(voice, Parameters::LfoFm) * delayedLfoValue;
                dbgassert(!fp_math::isNanOrInfd(fmAmount));
                dbgassert(!fp_math::isNanOrInfd(osc1Frequency));
                auto fmWaveform = voice.oscFm.GetWaveform(dt, osc1Frequency, Waveforms::Sine, true);
                dbgassert(!fp_math::isNanOrInfd(fmWaveform));
                double fm = fmWaveform * fmAmount;
                dbgassert(!fp_math::isNanOrInfd(fm));
                auto fmMultiplier = pitchFactor(fm);
                dbgassert(!fp_math::isNanOrInfd(fmMultiplier));
                switch (fmMode) {
                    case FmModes::Osc1:
                        osc1Frequency *= fmMultiplier;
                        dbgassert(!fp_math::isNanOrInfd(osc1Frequency));
                        break;
                    case FmModes::Osc2:
                        osc2Frequency *= fmMultiplier;
                        dbgassert(!fp_math::isNanOrInfd(osc2Frequency));
                        break;
                    default:
                        break;
                }
                break;
            }
            default:
                break;
        }

        auto out      = 0.0;
        double oscMix = GetModulatedParamVoice(voice, Parameters::OscMix);
        // if (oscMix < .999)
        {
            auto osc1Out             = 0.0;
            double osc1SplitFactorA  = pitchFactor(GetModulatedParamVoice(voice, Parameters::Osc1Split));
            double targetOscSplitMix = osc1SplitFactorA != 0.0 ? 1.0 : 0.0;
            osc1Out += voice.osc1a.Get(dt, osc1Wave, osc1Frequency * osc1SplitFactorA, true);
            dbgassert(!fp_math::isNanOrInfd(osc1Out));
            // if (osc1SplitMix > .001)
            osc1Out += targetOscSplitMix * voice.osc1b.Get(dt, osc1Wave, osc1Frequency * osc1SplitFactorB, true);
            dbgassert(!fp_math::isNanOrInfd(osc1Out));
            out += osc1Out * sqrt(1.0 - oscMix);
            dbgassert(!fp_math::isNanOrInfd(out));
        }
        // if (oscMix > .001)
        {
            double osc2SplitFactorA  = pitchFactor(GetModulatedParamVoice(voice, Parameters::Osc2Split));
            double targetOscSplitMix = osc2SplitFactorA != 0.0 ? 1.0 : 0.0;
            auto osc2Out             = 0.0;
            osc2Out += voice.osc2a.Get(dt, osc2Wave, osc2Frequency * osc2SplitFactorA, true);
            dbgassert(!fp_math::isNanOrInfd(osc2Out));
            // if (osc2SplitMix > .001)
            osc2Out += targetOscSplitMix * voice.osc2b.Get(dt, osc2Wave, osc2Frequency * osc2SplitFactorB, true);
            dbgassert(!fp_math::isNanOrInfd(osc2Out));
            out += osc2Out * sqrt(oscMix);
            dbgassert(!fp_math::isNanOrInfd(out));
        }
        double volEnvSmoothed = volEnvValue;
        if (voice.bTriggerSmoothing) {
            volEnvSmoothed = voice.prevVolEnv + dt * 8000.0 * (volEnvValue - voice.prevVolEnv);
        }
        voice.prevVolEnv = volEnvSmoothed;
        
        out *= volEnvSmoothed;

        auto filterDrive = GetModulatedParamVoice(voice, Parameters::FilterDrive);
        if (filterDrive < 0.0) {
            out *= 1.0 + filterDrive;
        } else {
            filterDrive *= 2.0;
            out *= 1.0 + filterDrive;
            if (out > filterDrive)
                out = filterDrive + (1 - filterDrive) * tanh((out - filterDrive) / (1 - filterDrive));
            else if (out < -filterDrive)
                out = -(filterDrive + (1 - filterDrive) * tanh((-out - filterDrive) / (1 - filterDrive)));
        }
        if (getSetting(Settings::FilterEnabled)) {
            // auto cutoff = filterCutoff;
            auto cutoff = GetModulatedParamVoice(voice, Parameters::FilterCutoff);
            cutoff += GetModulatedParamVoice(voice, Parameters::VolEnvCutoff) * volEnvValue;
            cutoff += GetModulatedParamVoice(voice, Parameters::ModEnvCutoff) * modEnvValue;
            cutoff += GetModulatedParamVoice(voice, Parameters::LfoCutoff) * delayedLfoValue;
            ;
            cutoff += pitchFactor(GetModulatedParamVoice(voice, Parameters::FilterKeyTracking)) * osc1Tune * baseFrequency;
            cutoff = math::clamp(cutoff, 20.0, 1.0 / dt * 0.7);
            if (getSetting(Settings::FilterDriftEnabled)) {
                cutoff *= 1.0 - filterDrift * (voice.driftValue + driftValue);
            }
            double cutoffSmooth = cutoff;
            if (voice.bTriggerSmoothing) {
                cutoffSmooth = voice.prevCutoff + dt * 8000.0 * (cutoff - voice.prevCutoff);
            }
            voice.prevCutoff = cutoffSmooth;
            // data = cutoff*dt;
            // auto res = filterResonance;
            auto res = static_cast<SynthParam_Float*>(vecParams[Parameters::FilterResonance])->ValueModulated(voice.modValues[Parameters::FilterResonance]);
            out      = voice.filter.Process(dt, out, filtermode, cutoffSmooth, res);
        }
        data = volEnvSmoothed;
        // out *= volEnvValue;

        return out;
    }

public:
    void onTransportChanged(bool bIsPlaying) override {
        seq              = 1;
        double lfo2Tempo = 1.0 / 4.0;
        lfo2.initPhase(fmod(tempo.ppqPos * lfo2Tempo, 1.0), true);

        if (getSetting(Settings::LfoPhaseDriftEnabled)) {
            lfoPhaseDrift = 0.8;
        } else {
            lfoPhaseDrift = 0.0;
        }
        for (auto& uv : voices) {
            uv.seqNr = 0;
            // uv.visitVoices([&](auto& voice) {
            //         voice.lfo1.initPhase(fmod(tempo.ppqPos * lfo1Tempo + lfoPhaseDrift*driftValue * voice.rand.rng_double(), 1.0));
            //         voice.lfo2.initPhase(fmod(tempo.ppqPos * lfo2Tempo + lfoPhaseDrift*uv.driftValue, 1.0));
            // });
        };
    }

    void StartVoice(VoiceUnison& voice, VoiceModes mode) {
        auto holdVolEnv = GetParamEnum(Parameters::VolEnvTriggerMode)->Value() == 1;
        auto holdModEnv = GetParamEnum(Parameters::ModEnvTriggerMode)->Value() == 1;
        auto holdLfo1 = GetParamEnum(Parameters::Lfo1TriggerMode)->Value() == 1;
        auto holdLfo1Ramp = GetParamEnum(Parameters::Lfo1RampTriggerMode)->Value() == 1;
        auto holdOsc1Phase = GetParamEnum(Parameters::Osc1PhaseResetMode)->Value() == 1;
        auto holdOsc2Phase = GetParamEnum(Parameters::Osc2PhaseResetMode)->Value() == 1;
        
        voice.Start(mode != VoiceModes::Poly);
        dbgassert(voice.numUnisonActive == unisonVoiceCount);
        voice.visitVoices([&](Voice& v) {
            bool isSilent = v.volEnv.stage >= EnvelopeStages::Idle || !v.bIsActive;
            UpdateVoiceEnvelopeModulations(voice, v);
            UpdateVoiceModulations(voice, v);
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
                v.prevLfoValue = v.lfoValue;
                double phase = GetModulatedParamVoice(v, Parameters::LfoPhase);
                v.lfo1.initPhase(phase + lfoPhaseDrift * this->driftValue * v.rand.rng_double(), bFadeLfo);
                // log_lf(Log::L_DEBUG, "voice %d:%d lfo phase %f\n", voice.indexPoly, v.indexUnison, v.lfo1.phase);
                v.lfo2.initPhase(lfoPhaseDrift * v.driftValue, bFadeLfo);
            }
        });
        maxUnisonVoice    = math::max(maxUnisonVoice, unisonVoiceCount);
        maxPolyVoiceIndex = math::max(maxPolyVoiceIndex, static_cast<int32_t>(&voice - &voices[0]) + 1);
    }

    void ProcessSynth(AudioBlock* in, float * const * outputs, int nFrames, const DAW::Host::Host* const host, double tick, playback_state state) override {
        // lockProcessing only locks VST2 versions of the plugin
        auto lock = this->lockProcessing();

        const auto bpmDiv4Hz                             = math::max(tempo.bpm / 4.0, 1.0) / 60.0;
        const auto mvInv                                 = sqrt(1.0 / math::max<double>(1.0, this->unisonVoiceCount));
        const auto voiceMode                             = GetParamEnum(Parameters::VoiceMode)->getEnumValue<VoiceModes>();
        const FilterModes filterMode                     = GetParamEnum(Parameters::FilterMode)->getEnumValue<FilterModes>();
        const bool bIsGlideEnabled                       = voiceMode != VoiceModes::Poly;
        int32_t numActiveVoices                          = 0;
        const auto dt                                    = getSetting(Settings::Oversampling) ? (oneOverSR * 0.5) : oneOverSR;
        const bool bDiagnostic                           = getSetting(Settings::DiagnosticOutputEnabled);
        if (isShowModulationRanges()) {
            std::memset(modulationValuesMax.data(), 0, modulationValuesMax.size()*sizeof(double));
            std::memset(modulationValuesMin.data(), 0, modulationValuesMin.size()*sizeof(double));
        }

        float* synthOutputs[2] = {};
        synthOutputs[0]        = outputs[0];
        synthOutputs[1]        = outputs[1];
        int nOversample        = 1;
        if (getSetting(Settings::Oversampling)) {
            nOversample = 2;
            if (in) {
                this->oversampler.up(in->buf, nFrames);
            }
            nFrames *= nOversample;
            synthOutputs[0] = this->oversampler[0];
            synthOutputs[1] = this->oversampler[1];
        }

        /**
        * framesPerAutomationUpdate 
        * 1 is highest precission, automation is updated every sample
        * this can be lowered to lower CPU load
        */
        int framesPerAutomationUpdate = state == playback_state::status_render ? 1 : 8;
        const auto bpm100 = host->prjGlobals.tempo100;
        for (int s = 0; s < nFrames; s++) {
            auto tickPos = tick + sampleToTickConvert<double, roundmode::none>(s, bpm100, host->m_sampleFormatInternal.sampleRate * nOversample);
            if (host && moduleSynthUnisonInstance && (s % framesPerAutomationUpdate) == 0) {
                this->moduleInstance->updateAutomatedParameters(host, math::floordS32(tickPos), state);
            }
            if (s % nOversample == 0) {
                ProcessMidiSample(*this, voices, voiceMode, s / nOversample, tickPos);
            }
            UpdateParameters(dt);
            UpdateDrift(dt);
            if (lfo2.Update(dt, bpmDiv4Hz)) {
            }
            lfo2Value = 0.5 + 0.5 * lfo2.GetWaveform(Waveforms::Saw, true);

            VoiceList list{};
            UpdateAllVoiceStates(dt, filterMode, list);
            if (getSetting(Settings::LfoEnabled)) {
                UpdateAllVoiceLfos(dt, list);
            }
            int32_t numUnisonVoices = list.numUnisonVoices;
            for (int32_t i = 0; i < numUnisonVoices; ++i) {
                auto& uv = voices[list.unisonVoices[i] / NUM_UNISON_VOICES];
                auto& v  = uv.voices[list.unisonVoices[i] % NUM_UNISON_VOICES];
                if (bIsGlideEnabled) {
                    v.frequency += (v.targetFrequency - v.frequency) * glideLength * dt;
                }
                        
                // TODO: executing a full modulation update each sample is really expensive
                // Make this adjustable
                UpdateVoiceModulations(uv, v);
                UpdateVoiceEnvelopeModulations(uv, v);
                UpdateVoiceEnvelopes(dt, uv, v);
                if (getSetting(Settings::ShowModulationRanges)) {
                    for (size_t j = 0; j < modulationValuesMax.size() && j < v.modValues.size(); j++) {
                        modulationValuesMax[j] = math::max(modulationValuesMax[j], v.modValues[j]);
                        modulationValuesMin[j] = math::min(modulationValuesMin[j], v.modValues[j]);
                    }
                }
            }
            // for (int32_t polyIndex = 0; polyIndex < polyVoiceCount; ++polyIndex) {
            //     auto& uv = voices[polyIndex];
            //     for (int32_t unisonIndex = 0; unisonIndex < maxUnisonVoice; ++unisonIndex) {
            //         auto& v = uv.getVoice(unisonIndex);
            //         UpdateVoiceEnvelopes(dt, uv, v);
            //     }
            // }
            auto outL = 0.0;
            if (bDiagnostic) {
                outL = -1.0;
            }
            auto outR = 0.0;
            if (USE_THREADING) {
                resetBlock();
            }
            int taskIndex = 0;
            for (int32_t polyIndex = list.polyVoiceIndexFirst; polyIndex >= 0 && polyIndex < list.polyVoiceIndexLast; ++polyIndex) {
                auto& uv = voices[polyIndex];
                for (int32_t unisonIndex = 0; unisonIndex < maxUnisonVoice; ++unisonIndex) {
                    auto& v = uv.getVoice(unisonIndex);
                    if (!v.bIsActive) {
                        continue;
                    }
                    if (USE_THREADING) {
                        auto& task = tasks[taskIndex];
                        task.setTask(dt, uv, v, filterMode);
                        threads[taskIndex%threadsRunningCount].pushTask(&task);
                        taskIndex++;
                    } else {
                        auto voiceVolume = GetModulatedParamVoice(v, Parameters::MasterVolume);
                        // auto noise = (synthRand.rng_double()*2-1)*0.002;
                        auto vData                = -1.0;
                        double vVal               = GetVoiceImpl(dt, uv, v, filterMode, vData);
                        auto voice                = vVal * mvInv * voiceVolume;
                        auto panningMinusOneToOne = GetModulatedParamVoice(v, Parameters::Panning);
                        auto panningUnipolar      = panningMinusOneToOne * 0.5 + 0.5;
                        constexpr bool autopan    = false;
                        double pan                = panningUnipolar;
                        if (autopan) {
                            pan += (unisonVoiceCount == 2) ? (unisonIndex & 1) : (unisonIndex / (unisonVoiceCount - 1.0));
                            pan *= 0.5;
                        }
                        outR += voice * sqrt(pan);
                        if (bDiagnostic) {
                            if (unisonIndex == 0/*  && uv.seqNr == 1 */) {
                                outL = vData;
                            }
                        } else {
                            outL += voice * sqrt(1.0 - pan);
                        }
                    }
                }
            }
            if (USE_THREADING) {
                for (int i = 0; i < taskIndex; i++) {
                    auto& task = tasks[i];
                    if (task.isInUse()) {
                        task.wait();
                        auto& v = task.getVoice();
                        double vVal = task.getVoiceData();
                        auto voiceVolume = GetModulatedParamVoice(v, Parameters::MasterVolume);
                        auto voice                = vVal * mvInv * voiceVolume;
                        auto panningMinusOneToOne = GetModulatedParamVoice(v, Parameters::Panning);
                        auto panningUnipolar      = panningMinusOneToOne * 0.5 + 0.5;
                        outR += voice * sqrt(panningUnipolar);
                        outL += voice * sqrt(1.0 - panningUnipolar);
                        task.resetTask();
                    }
                }
            }
            /* if (bDiagnostic) {
                // build saw wave from -1 to 1 over block length
                outL = fmod((s / double(nFrames) + 0.5), 1.0) * 2.0 - 1.0;
            } */
            synthOutputs[0][s] = fp_math::silenceNanInff(static_cast<float>(outL));
            synthOutputs[1][s] = fp_math::silenceNanInff(static_cast<float>(outR));

            dbgassert((list.numPolyVoices > 0) == (list.numUnisonVoices > 0));
            numActiveVoices = math::max(math::max(0, list.numUnisonVoices), numActiveVoices);
            if (s == nFrames - 1) {
                prevVoiceList = list;
            }
        }
        this->activeVoiceCount = numActiveVoices;
        this->statsMaxVoiceCount = math::max(this->statsMaxVoiceCount, numActiveVoices);

        if (getSetting(Settings::Oversampling)) {
            nFrames /= nOversample;
            this->oversampler.down(outputs, nFrames);
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
            case Parameters::UnisonVoices: {
                auto unisonVoicesCurrent = this->unisonVoiceCount;
                auto unisonVoicesTarget  = paramIntOptional->Value();
                if (unisonVoicesCurrent != unisonVoicesTarget) {
                    this->maxUnisonVoice   = math::max(maxUnisonVoice, math::max(unisonVoicesTarget, unisonVoicesCurrent));
                    this->unisonVoiceCount = unisonVoicesTarget;
                    for (auto& voice : voices) {
                        voice.setUnisonVoiceCount(this->unisonVoiceCount);
                        for (int i = this->unisonVoiceCount; i < this->maxUnisonVoice; ++i) {
                            voice.voices[i].Release();
                        }
                    }
                }
                break;
            }
            case Parameters::Voices: {
                auto polyVoicesCurrent = this->polyVoiceCount;
                auto polyVoicesTarget  = paramIntOptional->Value();
                if (polyVoicesCurrent != polyVoicesTarget) {
                    this->maxPolyVoiceIndex = math::max(maxPolyVoiceIndex, math::max(polyVoicesCurrent, polyVoicesTarget));
                    this->polyVoiceCount    = polyVoicesTarget;
                    for (int i = this->polyVoiceCount; i < this->maxPolyVoiceIndex; ++i) {
                        voices[i].Release();
                    }
                }
                break;
            }
            case Parameters::Osc1Wave:
                osc1Wave.Switch(value);
                break;
            case Parameters::Osc1Split:
                targetOsc1SplitMix = value != 0.0 ? 1.0 : 0.0;
                osc1SplitFactorA   = pitchFactor(value);
                osc1SplitFactorB   = 1.0;//pitchFactor(value);
                break;
            case Parameters::Osc2Wave:
                osc2Wave.Switch(value);
                break;
            case Parameters::Osc2Split:
                targetOsc2SplitMix = value != 0.0 ? 1.0 : 0.0;
                // osc2SplitFactorA   = pitchFactor(-value);
                // osc2SplitFactorB   = pitchFactor(value);
                osc2SplitFactorA = pitchFactor(value);
                osc2SplitFactorB = 1.0;//pitchFactor(value);
                break;
            case Parameters::OscMix:
                targetOscMix = 1.0 - value;
                break;
            case Parameters::FmCoarse:
            case Parameters::FmFine: {
                auto fmCoarse = GetParamInt(Parameters::FmCoarse)->Value();
                auto fmFine   = GetParamFloat(Parameters::FmFine)->Value();
                baseFmAmount  = fmCoarse + fmFine;
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
            // case Parameters::LfoWave:
            //     lfoWave.Switch(value);
                // break;
            default:
                break;
        }
    }

    void setPreset(const String& path, const String& name) {
        currentPreset.path = path;
        currentPreset.name = name;
    }

    const PresetManager::Preset& getPreset() const {
        return currentPreset;
    }

    DAW::Shape::shape_t& getShape(int idx) {
        return this->lfoShape;
    }

    void notifyUiChanges() {
        for (auto& pviewctr : this->views) {
            if (pviewctr->isInUse()) {
                pviewctr->onSetParameter(-1, 0.0f);
            }
        }
    }
};


class module_synth_unison final : public module_synth_template<SynthImplUnison> {
public:
    explicit module_synth_unison(int32_t _projectGlobalId, IHostCallback* _hostCallback)
        : module_synth_template<SynthImplUnison>(new SynthType(this), "Synth", _projectGlobalId, _hostCallback)
    {
        bCanReceiveMidi = true;
        isSynth = true;
        for (const auto& paramEntry : vecParams) {
            if (!paramEntry) {
                continue;
            }
            int idx = PARAM_OFFSET_IMPL + (&paramEntry - &vecParams.front());
            automatable_param_t* regparam = registerParam(idx);
            dbgassert(regparam && regparam->idx > 0);
            regparam->setInitial(paramEntry->getAsDouble());
            regparam->extensiveName  = paramEntry->name;
            regparam->name  = paramEntry->shortName;
            regparam->shortLabel  = paramEntry->hierarchicalName;
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
            using Parameters = ParametersSynthUnison;
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

    ~module_synth_unison() override {
        delete impl;
    }

    PluginType getPluginType() override { return PLUGIN_TYPE_SYNTH; };

    std::shared_ptr<PluginViewContainer> createViewCtrInternal() override {
        return this->impl->createViewCtrImpl();
    }
    void getUiSnapshot(snapshot_t& snapshot);
    void setUiSnapshot(snapshot_t& snapshot);

    void settingChanged(Settings setting, float value);

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
            auto param = getParam(PARAM_OFFSET_IMPL + idx);
            param->setAll(vecParams[idx]->getAsDouble());
        }
    }

    void addPropertiesParameterTooltip(Table::tbl& table, int idx) override {
        const auto idxInternal = idx - PARAM_OFFSET_IMPL;
        if (isValidParamIdx(idxInternal)) {
            SynthParamBase* param = vecParams[idxInternal];
            const auto strName    = param->name;
            const auto strDisplay = param->getValueDisplay(param->getAsDouble());
            table.colSizes.resize(2);
            table.colSizes[0] = table.strW->getStringWidth(strName);
            table.colSizes[1] = table.strW->getStringWidth(strDisplay);
            table.colSizes[1] = math::max(table.colSizes[1], table.strW->getStringWidth("12345"));
            table.tableWidth  = table.colSizes[0] + table.colSizes[1];
            table.rows.push_back({ { strName, strDisplay } });
            //TODO: move this block inside SynthImpl
#if 0
            for (auto& mod : impl->modulations) {
                auto modIndex = &mod - &impl->modulations.front();
                for (auto& dest : mod.destinations) {
                    if (dest.parameter == param->enumParam) {
                        for (int j = 0; j < impl->polyVoiceCount; j++) {
                            if (!impl->voices[j].IsInactive()) {
                                for (int i = 0; i < impl->unisonVoiceCount; i++) {
                                    bool bIsBipolar    = impl->IsBipolarModulation(mod);
                                    const auto strName = StringFormat("Mod %zd Voice %d, unison %d %s, bipolar: %d", modIndex, j, i, StringAsCStr(param->name), bIsBipolar);
                                    double& ref        = impl->voices[j].getVoice(i).modValues[dest.parameter];
                                    table.colSizes[0]  = math::max(table.colSizes[0], table.strW->getStringWidth(strName));
                                    table.rows.push_back({ { strName, Table::tbltyperef<double>{ ref, "%.2f" } } });
                                }
                                break;
                            }
                        }
                    }
                }
            }
#endif
        }
    }

    SynthParamBase* getSynthParam(ParametersSynthUnison enumParam) {
        return impl->getParam(enumParam);
    }
    void onPreUnload() override {
        impl->stopThreads();
        log_lf(Log::L_WARN, "getStatsMaxVoiceCount: %d\n", impl->getStatsMaxVoiceCount());
    }
};

PluginVST2_Synth::PluginVST2_Synth(audioMasterCallback audioMaster)
    : BasePluginVST2(audioMaster, PLUGIN_UID, 0, ParametersSynthUnison::kNumParams, kNumInputs, kNumOutputs),
        impl(new SynthImplUnison(this)),
        vecParams(impl->vecParams) {
    isSynth(true);
    programsAreChunks(true);
    impl->init();
    setInitialDelay(impl->getLatency());
}

PluginVST2_Synth::~PluginVST2_Synth() {
    delete impl;
}

void PluginVST2_Synth::onPresetLoaded() {
    updateDisplay();
}

SynthImplUnison* PluginVST2_Synth::getSynth() {
    return this->impl;
}

void PluginVST2_Synth::getParameterLabel(VstInt32 index, char* label) {
    if (label && index >= 0 && index < CtrSize(vecParams)) {
        SynthParamBase* param = vecParams[index];
        if (!param)
            return;
        vst_strncpy(label, StringAsCStr(param->unit), PLUGIN_PARAM_STR_MAX_LEN);
    }
}

void PluginVST2_Synth::getParameterDisplay(VstInt32 index, char* text) {
    if (text && index >= 0 && index < CtrSize(vecParams)) {
        SynthParamBase* param = vecParams[index];
        if (!param)
            return;
        String valDisplay     = param->getValueDisplay(param->getAsDouble());
        vst_strncpy(text, StringAsCStr(valDisplay), PLUGIN_PARAM_STR_MAX_LEN);
    }
}

void PluginVST2_Synth::getParameterName(VstInt32 index, char* label) {
    if (index >= 0 && index < CtrSize(vecParams)) {
        SynthParamBase* param = vecParams[index];
        if (!param)
            return;
        vst_strncpy(label, StringAsCStr(param->shortName), PLUGIN_PARAM_STR_MAX_LEN);
    }
}

void PluginVST2_Synth::setParameter(VstInt32 index, float value) {
    if (index >= 0 && index < CtrSize(vecParams)) {
        SynthParamBase* param = vecParams[index];
        if (!param)
            return;
        param->set(value, value);
        this->impl->OnParamChange(static_cast<ParametersSynthUnison>(param->enumParam));
    }
    for (auto& pviewctr : this->views) {
        if (pviewctr->isInUse()) {
            pviewctr->onSetParameter(index, value);
        }
    }
}

param_converted_t PluginVST2_Synth::convertParamValueDisplay(int32_t idx, const param_unit_t& displayValue) {
    if (idx >= 0 && idx < CtrSize(vecParams)) {
        SynthParamBase* param = vecParams[idx];
        if (param)
            return param->convertValueDisplay(displayValue);
    }
    return BasePluginVST2::convertParamValueDisplay(idx, displayValue);
}

void PluginVST2_Synth::addPropertiesParameterTooltip(Table::tbl& table, int idx) {
    if (idx >= 0 && idx < CtrSize(vecParams) && vecParams[idx]) {
        SynthParamBase* param = vecParams[idx];
        const auto strName    = param->name;
        const auto strDisplay = param->getValueDisplay(param->getAsDouble());
        table.colSizes.resize(2);
        table.colSizes[0] = table.strW->getStringWidth(strName);
        table.colSizes[1] = table.strW->getStringWidth(strDisplay);
        table.colSizes[1] = math::max(table.colSizes[1], table.strW->getStringWidth("12345"));
        table.tableWidth  = table.colSizes[0] + table.colSizes[1];
        table.rows.push_back({ { strName, strDisplay } });
#if 0
        for (auto& mod : impl->modulations) {
            auto modIndex = &mod - &impl->modulations.front();
            for (auto& dest : mod.destinations) {
                if (dest.parameter == param->enumParam) {
                    for (int j = 0; j < impl->polyVoiceCount; j++) {
                        if (!impl->voices[j].IsInactive()) {
                            for (int i = 0; i < impl->unisonVoiceCount; i++) {
                                bool bIsBipolar    = impl->IsBipolarModulation(mod);
                                const auto strName = StringFormat("Mod %zd Voice %d, unison %d %s, bipolar: %d", modIndex, j, i, StringAsCStr(param->name), bIsBipolar);
                                double& ref        = impl->voices[j].getVoice(i).modValues[dest.parameter];
                                table.colSizes[0]  = math::max(table.colSizes[0], table.strW->getStringWidth(strName));
                                table.rows.push_back({ { strName, Table::tbltyperef<double>{ ref, "%.2f" } } });
                            }
                            break;
                        }
                    }
                }
            }
        }
#endif
    }
}

float PluginVST2_Synth::getParameter(VstInt32 index) {
    if (index >= 0 && index < CtrSize(vecParams)) {
        SynthParamBase* param = vecParams[index];
        if (param)
            return static_cast<double>(param->getAsDouble());
    }
    return 0.0f;
}

bool PluginVST2_Synth::getEffectName(char* name) {
    vst_strncpy(name, PLUGIN_EFFECT_NAME, kVstMaxEffectNameLen);
    return true;
}

bool PluginVST2_Synth::getVendorString(char* text) {
    vst_strncpy(text, PLUGIN_VENDOR_NAME, kVstMaxVendorStrLen);
    return true;
}

bool PluginVST2_Synth::getProductString(char* text) {
    vst_strncpy(text, PLUGIN_PRODUCT_NAME, kVstMaxProductStrLen);
    return true;
}

VstInt32 PluginVST2_Synth::getVendorVersion() {
    return 1;
}

VstInt32 PluginVST2_Synth::canDo(char* text) {
    //if (!strcmp(text, PlugCanDos::canDoReceiveVstEvents))
    //    return 1;
    if (!strcmp(text, PlugCanDos::canDoReceiveVstMidiEvent))
        return 1;
    if (!strcmp(text, PlugCanDos::canDoReceiveVstTimeInfo))
        return 1;
    if (!strcmp(text, PlugCanDos::canDoReceiveVstEvents))
        return 1;
    return -1;// explicitly can't do; 0 => don't know
}
VstInt32 PluginVST2_Synth::getChunk(void** data, bool isPreset) {
    if (isPreset) {
        *data = nullptr;
        return 0;
    }

    snapshot_t snapshot;
    if (impl->getSnapshot(snapshot)) {
        getUiSnapshot(snapshot);
        auto pShrdHeapVec = serializeSnapshot(snapshot);
        lastProgramChunks.push_back(pShrdHeapVec);
        *data = pShrdHeapVec->data();
        return static_cast<VstInt32>(pShrdHeapVec->size());
    }
    *data = nullptr;
    return 0;
}

VstInt32 PluginVST2_Synth::setChunk(void* data, VstInt32 byteSize, bool isPreset) {
    std::shared_ptr<std::vector<std::byte>> buf;
    auto sizeData = static_cast<size_t>(byteSize);
    if (!isPreset && sizeData > 0) {
        buf = std::make_shared<std::vector<std::byte>>(sizeData);
        std::memcpy(buf->data(), data, sizeData);
        snapshot_t snapshotLoaded;
        if (deserializeSnapshot(buf, snapshotLoaded)) {
            impl->setSnapshot(snapshotLoaded);
            setUiSnapshot(snapshotLoaded);
            return 1;
        }
        return 0;
    }
    return 0;
}

void PluginVST2_Synth::notifyUiChanges() {
    for (auto& pviewctr : this->views) {
        if (pviewctr->isInUse()) {
            pviewctr->onSetParameter(-1, 0.0f);
        }
    }
    updateDisplay();
}

void PluginVST2_Synth::setSampleRate(float sampleRate) {
    AudioEffectX::setSampleRate(sampleRate);
    this->impl->setSamplerate(sampleRate);
}
void PluginVST2_Synth::setBlockSize(VstInt32 blockSize) {
    AudioEffectX::setBlockSize(blockSize);
    this->impl->setBlocksize(blockSize);
}

VstInt32 PluginVST2_Synth::processEvents(VstEvents* events) {
    dbgassert(events);
    if (events) {
        int32_t len = events->numEvents;
        for (int i = 0; i < len; i++) {
            auto pEvent = events->events[i];
            if (pEvent->type == VstEventTypes::kVstMidiType) {
                VstMidiEvent* pME = reinterpret_cast<VstMidiEvent*>(pEvent);
                IMidiMsg msg(pME->deltaFrames, pME->midiData[0], pME->midiData[1], pME->midiData[2]);
                impl->ProcessMidiMsg(msg);
            }
        }
    }
    return 1;
}

void PluginVST2_Synth::processReplacing(float** inputs, float** outputs, VstInt32 sampleFrames) {
    if (issetprogram)
        return;

    if (sampleFrames != blockSize) {
        return;
    }
    // ThreadLock lock(this->getMutex());
    if (this->getAeffect()->numOutputs == 1) {
        if (inputs)
            memset(inputs[0], 0, sizeof(float) * sampleFrames);
        memset(outputs[0], 0, sizeof(float) * sampleFrames);
    } else if (this->getAeffect()->numOutputs == 2) {
        VstTimeInfo* timeinfo = getTimeInfo(kVstBarsValid | kVstPpqPosValid | kVstTempoValid | kVstTransportChanged | kVstTimeSigValid);
        if (timeinfo && timeinfo->flags & kVstTempoValid) {
            this->impl->setTempo(timeinfo->tempo);
        }
        if (timeinfo && timeinfo->flags & kVstPpqPosValid) {
            this->impl->setPPQPos(timeinfo->ppqPos);
        }
        if (timeinfo && timeinfo->flags & kVstBarsValid) {
            this->impl->setBarPos(timeinfo->barStartPos);
        }
        if (timeinfo && timeinfo->flags & kVstTransportChanged) {
            this->impl->onTransportChanged(timeinfo->flags & kVstTransportPlaying);
        }
        auto state = playback_state::status_playback;
        if (timeinfo && timeinfo->flags & kVstTransportPlaying) {
            state = playback_state::status_playback;
        } else {
            state = playback_state::status_stop;
        }
        double tickPos = 0.0;
        if (timeinfo && timeinfo->flags & kVstPpqPosValid) {
            tickPos = timeinfo->ppqPos * TICKS_QUARTER;
        }
        if (inputs)
            dsp_util::fillChannels(inputs, this->getAeffect()->numInputs, sampleFrames, 0.0f);
        dsp_util::fillChannels(outputs, this->getAeffect()->numOutputs, sampleFrames, 0.0f);
        this->impl->ProcessSynth(nullptr, outputs, sampleFrames, nullptr, tickPos, state);
    }
}

}// namespace PluginSynth

template<>
effectbase* makeInstance<PluginSynth::module_synth_unison>(int32_t _projectGlobalId, IHostCallback* _hostCallback) {
    return new PluginSynth::module_synth_unison(_projectGlobalId, _hostCallback);
}

namespace PluginSynth {

float getLayoutHeight(guibase* gui) {
    return gui->theme->get(GuiConstant::CONST_ROW_HEIGHT) * 1.33f;
}

class gui_listsynthsettings final : public gui_list_entry {
    SynthImplUnison* const synth;
    const Settings setting;
    const String name;

public:
    explicit gui_listsynthsettings(SynthImplUnison* _synth, Settings _setting, String _settingName)
        : gui_list_entry(), synth(_synth), setting(_setting), name(std::move(_settingName)) {
        icon = -1;
    }

    String getText() override { return name; }
    void dragMoveOn(guibase* target, ivec2 mousepos) override {}
    void dragReleaseOn(guibase* target, ivec2 mousepos) override {}
    void handleDraggedBegin(MouseEvent& evt) override { toggle(); }
    bool enabled() { return synth->getSetting(setting); }

    bool toggle() {
        bool bEnbl = enabled();
        bEnbl      = !bEnbl;
        synth->setSetting(setting, bEnbl);
        if (parent && parent->parent) {
            parent->parent->buttonClicked(this);
        }
        return false;
    }

    void render(NVGcontext* vg) override {
        if (size.y < 5) {
            return;
        }
        BaseCtrl* ctrl  = parentCtrl;
        float spacing   = INSET_TITLE;
        float x         = spacing;
        float rowHeight = size.y;
        if (icon > -1) {
            x += rowHeight + spacing;
        }

        ivec2 inner = size;
        if (ctrl->isCtrOrChildFocused(this)) {
            nvgBeginPath(vg);
            nvgRect(vg, pos.x, pos.y, inner.x, inner.y);
            nvgFillColor(vg, theme->getColor(GuiColor::COL_BG_DRKER));
            nvgFill(vg);
        }
        nvgTranslate(vg, pos.x, pos.y);

        if (icon > -1) {
            RenderResources::NvgImageTexture& image = RenderResources::imgIcons[icon];
            drawIcon(vg, inner, &image);
        }

        renderTextLabel(vg,
                        vec2(x, rowHeight * 0.5f),
                        vec2(size),
                        getText(),
                        theme,
                        rowHeight,
                        THEMECOL_TEXT,
                        NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

        ivec2 sizeIcon = ivec2(inner.y - 4);
        ivec2 posIcon  = { inner.x - (int) spacing - sizeIcon.y, (inner.y - sizeIcon.y) / 2 };
        bool enbl      = enabled();

        renderTextLabel(vg,
                        vec2(posIcon.x - 4, rowHeight * 0.5f),
                        vec2(size),
                        enbl ? "On" : "Off",
                        theme,
                        rowHeight,
                        theme->getColor(enbl ? GuiColor::COL_ON : GuiColor::COL_OFF),
                        NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);


        RenderResources::NvgImageTexture& image = RenderResources::imgIcons[ICON_X];
        nvgTranslate(vg, posIcon.x, posIcon.y);
        nvgBeginPath(vg);
        nvgRect(vg, 0, 0, sizeIcon.x, sizeIcon.y);
        nvgFillColor(vg, theme->getColor(GuiColor::COL_BG_DRKER2));
        nvgFill(vg);
        nvgStrokeColor(vg, theme->getBgStrokeColor(parent->getFlags()));
        nvgStrokeWidth(vg, theme->getFloat(GuiConstant::CONST_GUI_FRAME_STROKE_WIDTH));
        nvgStroke(vg);
        if (enbl) {
            drawIcon(vg, sizeIcon, &image, 2);
        }
        nvgTranslate(vg, -posIcon.x, -posIcon.y);
        nvgTranslate(vg, -pos.x, -pos.y);
    }
};


class guicontainer_plugin_synth_editor final : public guictr_base, public splitter_cb {
    struct _synth_gui_param_knob {
        ParametersSynthUnison param;
        guiknob_pluginparam* knob;
        guiknob::knobtype type;
        guictr_base* parentContainer;
        ivec2 pos;
        ivec2 size;
    };
    PluginVST2_Synth* const vst2Instance;
    SynthImplUnison* const synth;
    effectbase* const moduleInstance;
    gui_textfield editfield;
    std::vector<_synth_gui_param_knob> vecParamUI;
    std::vector<guictr_synth_title*> containers;
    std::vector<std::vector<guictr_synth_title*>> moduleLayout;
    i_ctr_shape_editor* const shapeEditor;
    guicontainer_modulation modulation;
    gui_list list;
    gui_list list2;
    guictr_synth_param_container ctrOsc1;
    guictr_synth_param_container ctrOsc2;
    guictr_synth_param_container ctrFm;
    guictr_synth_param_container ctrFilter;
    guictr_synth_param_container ctrAmp;
    guictr_synth_param_container ctrEnvV;
    guictr_synth_param_container ctrEnvM;
    guictr_synth_param_container ctrLfo;
    guictr_synth_param_container ctrMacro;
    guictr_synth_param_container ctrShapeLfo;

    Splitter splitter;
    bool bGuiNeedsRefresh = true;
    class gui_synth_stats_list_entry final : public gui_list_entry {
    public:
        String string;
        gui_synth_stats_list_entry() {
            setBackgroundRendered(true);
            icon = -1;
        }
        void dragMoveOn(guibase* target, ivec2 mousepos) override {
        }
        void dragReleaseOn(guibase* target, ivec2 mousepos) override {
        }
        String getText() override {
            return string;
        }
    };

public:
    explicit guicontainer_plugin_synth_editor(effectbase* module, SynthImplUnison* synth, PluginVST2_Synth* plugin)
        : guictr_base(),
            vst2Instance(plugin),
            synth(synth),
            moduleInstance(module),
            shapeEditor(makeShapeEditor()),
            modulation(dynamic_cast<PluginLockable*>(synth), synth),
            ctrOsc1(synth),
            ctrOsc2(synth),
            ctrFm(synth),
            ctrFilter(synth),
            ctrAmp(synth),
            ctrEnvV(synth),
            ctrEnvM(synth),
            ctrLfo(synth),
            ctrMacro(synth),
            ctrShapeLfo(synth),
            splitter(1, 0.8f) {
        dbgassert(vst2Instance || moduleInstance);
        list.padding  = 4;
        list2.padding = 4;
        list2.margin  = 4;
        list.margin   = 4;
        splitter.setMinMax(0.3f, 0.85f);
        splitter.setCallback(this);
        setBackgroundRendered(false);
        editfield.setFlag(FLG_NO_LAYOUT, true);
        editfield.setVisible(false);
        editfield.setAlignment(gui_textfield::Alignment::Center);
        editfield.setReturnCommits(true);
        padding        = 2;
        int32_t ctrIdx = 1;
        moduleLayout = {
            {&ctrAmp, &ctrFilter, &ctrFm},
            {&ctrLfo, &ctrShapeLfo, &ctrOsc1, &ctrOsc2},
            {&ctrEnvV, &ctrEnvM, &ctrMacro},
        };
        for (auto& row : moduleLayout) {
            for (auto ctr : row) {
                ctr->id = ctrIdx++;
                add(ctr);
                containers.push_back(ctr);
            }
        }
        ctrAmp.setLabel("Main");
        ctrFilter.setLabel("Filter");
        ctrLfo.setLabel("LFO");
        ctrOsc1.setLabel("OSC 1");
        ctrOsc2.setLabel("OSC 2");
        ctrFm.setLabel("FM");
        ctrEnvV.setLabel("Envelpe Volume");
        ctrEnvM.setLabel("Envelope Modulation");
        ctrMacro.setLabel("Macros");
        ctrShapeLfo.setLabel("Shape");
        using Parameters = ParametersSynthUnison;
        vecParamUI.resize(Parameters::kNumParams);
        for (auto param : parametersOrdered) {
            auto type = guiknob::knobtype::SLIDER_LABELED;
            if (!stl_contains(parametersModulate, param)) {
                type = guiknob::knobtype::KNOB_LABELED;
            }
            auto synthParam = synth->getParam(param);
            dbgassert(synthParam);
            switch (synthParam->getType()) {
                case SynthParam::ParamType::ENUM:
                case SynthParam::ParamType::INT:
                    type = guiknob::knobtype::KNOB_LABELED;
                    break;
                default:
                    type = guiknob::knobtype::SLIDER_LABELED;
                    break;
            }
            guictr_synth_param_container* ctr = nullptr;
            switch (param) {
                case Parameters::Osc1Wave:
                case Parameters::Osc1Coarse:
                case Parameters::Osc1Fine:
                case Parameters::Osc1Split:
                case Parameters::Osc1PhaseResetMode:
                    ctr = &ctrOsc1;
                    break;
                case Parameters::Osc2Wave:
                case Parameters::Osc2Coarse:
                case Parameters::Osc2Fine:
                case Parameters::Osc2Split:
                case Parameters::Osc2PhaseResetMode:
                    ctr = &ctrOsc2;
                    break;
                case Parameters::LfoDelay:
                case Parameters::LfoFrequency:
                case Parameters::LfoPhase:
                case Parameters::LfoShape:
                // case Parameters::LfoWave:
                case Parameters::Lfo1TriggerMode:
                case Parameters::Lfo1RampTriggerMode:
                    ctr = &ctrLfo;
                    break;
                case Parameters::FmCoarse:
                case Parameters::FmFine:
                case Parameters::FmMode:
                case Parameters::LfoFm:
                case Parameters::ModEnvFm:
                case Parameters::VolEnvFm:
                    ctr = &ctrFm;
                    break;
                case Parameters::FilterMode:
                case Parameters::FilterCutoff:
                case Parameters::FilterResonance:
                case Parameters::FilterDrive:
                case Parameters::FilterKeyTracking:
                case Parameters::ModEnvCutoff:
                case Parameters::VolEnvCutoff:
                case Parameters::LfoCutoff:
                    ctr = &ctrFilter;
                    break;
                case Parameters::MasterVolume:
                case Parameters::Panning:
                case Parameters::VoiceMode:
                case Parameters::Voices:
                case Parameters::UnisonVoices:
                case Parameters::GlideLength:
                case Parameters::OscMix:
                    ctr = &ctrAmp;
                    break;
                case Parameters::ModEnvA:
                case Parameters::ModEnvD:
                case Parameters::ModEnvS:
                case Parameters::ModEnvR:
                case Parameters::ModEnvV:
                case Parameters::ModEnvTriggerMode:
                    ctr = &ctrEnvM;
                    break;
                case Parameters::VolEnvA:
                case Parameters::VolEnvD:
                case Parameters::VolEnvS:
                case Parameters::VolEnvR:
                case Parameters::VolEnvV:
                case Parameters::VolEnvTriggerMode:
                    ctr = &ctrEnvV;
                    break;
                case Parameters::Macro01:
                case Parameters::Macro02:
                case Parameters::Macro03:
                case Parameters::Macro04:
                case Parameters::Macro05:
                case Parameters::Macro06:
                case Parameters::Macro07:
                case Parameters::Macro08:
                    ctr = &ctrMacro;
                    break;
                default:
                    dbgassert(0);
                    unreachable();
                    break;
            }
            auto idx = static_cast<int32_t>(param);
            if (moduleInstance) idx += PARAM_OFFSET_IMPL;
            auto idxExternal = idx;
            if (vst2Instance) idxExternal += PARAM_OFFSET_EXTERNAL;
            auto knob = new guiknob_synthparam(idx, idxExternal, synth,
                                                param,
                                                type);
            if (ctr) {
                ctr->addParamKnob(knob);
                knob->id = type == guiknob::knobtype::KNOB_LABELED ? 1 : 0;
            }
            dbgassert(static_cast<size_t>(param) < vecParamUI.size());
            vecParamUI[param] = { param, knob, type, ctr, ivec2(0), ivec2(32, 32) };
        }
        shapeEditor->setShapeEditorShapeRef(&synth->getShape(0));
        shapeEditor->setShapeEditorCallback([synth=this->synth](const DAW::Shape::shape_t& shape, bool bIsDragMove) -> void {
            auto lock = synth->lock();
            auto& synthShape = synth->getShape(0);
            synthShape.pts = shape.pts;
            synthShape.eraseDuplicates();
        });
        auto shapeCtr = shapeEditor->getGuiContainer();
        shapeCtr->setBackgroundRendered(false);
        shapeCtr->setBackgroundRenderedInset(false);
        shapeCtr->setCanMouseHit(false);
        shapeCtr->id = 2;
        shapeCtr->margin = 0;
        shapeCtr->padding = 2;
        ctrShapeLfo.add(shapeCtr);
        add(&list);
        add(&list2);
        add(&modulation);
        // add(&modulation);
        add(&editfield);
        add(&splitter);
        // initialize settings list
        {
            std::vector<gui_list_entry*> _newListIn;
            for (auto setting : settingsOrdered) {
                _newListIn.push_back(new gui_listsynthsettings{ synth, setting, stringsSettings[setting] });
            }
            int idx = 0;
            for (auto* p : _newListIn) {
                p->id = 0x1f | (idx++ << 8);
            }
            list2.setList(_newListIn);
            // list2.layout();
        }
    }

    ~guicontainer_plugin_synth_editor() override {
        removeGuis();
    }

    float getSplitterPos(int32_t idx) {
        if (idx == 0)
            return splitter.getScale();
        return 0.0f;
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

    guiknob_pluginparam* getKnobFromParameter(int32_t index) {
        if (index >= 0 && index < (int32_t) vecParamUI.size() && vecParamUI[index].knob != nullptr) {
            return vecParamUI[index].knob;
        }
        return nullptr;
    }

    void onSetParameter(int32_t index, float value) {
        if (index == -1) {
            bGuiNeedsRefresh = true;
            for (auto& synthKnob : vecParamUI) {
                if (synthKnob.knob)
                    synthKnob.knob->setValueInit(synth->getParam(synthKnob.param)->getAsDouble());
            }
            return;
        }
        // if (moduleInstance) {
            // return;
        // }
        guiknob_pluginparam* knob = getKnobFromParameter(index);
        if (knob) {
            knob->setValueInit(value);
        }
    }

    void onGuiOpen() {
        for (auto& synthKnob : vecParamUI) {
            if (!synthKnob.knob)
                continue;
            if (!moduleInstance) {
                synthKnob.knob->setAudioEffect(vst2Instance);
            } else {
                synthKnob.knob->setEffectInstance(moduleInstance);
            }
            auto* param = synth->getParam(synthKnob.param);
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
            if (!moduleInstance) {
                synthKnob.knob->setAudioEffect(nullptr);
            } else {

                synthKnob.knob->setEffectInstance(nullptr);
            }
        }
    }

    void onTick(AppCtrl* ctrl) override {
        if (bGuiNeedsRefresh) {
            modulation.setFromSynth();
            layout();
            bGuiNeedsRefresh = false;
        }
        PluginVST2_Synth* thisImpl = this->vst2Instance;
        auto synthImpl = this->synth;
        auto heldNotes = synthImpl->getHeldNotes();//TODO: not threadsafe
        std::vector<String> strings;
        strings.reserve(8);
        strings.push_back(StringFormat("Drift Val %f", synthImpl->driftValue));
        strings.push_back(StringFormat("Drift Freq %f", (synthImpl->driftVelocity * synthImpl->getSamplerate())));
        strings.push_back(StringFormat("Voices %d", synthImpl->getActiveVoiceCount()));
        strings.push_back(StringFormat("minPolyVoiceIndex %d", synthImpl->minPolyVoiceIndex));
        strings.push_back(StringFormat("maxPolyVoiceIndex %d", synthImpl->maxPolyVoiceIndex));
        strings.push_back(StringFormat("maxUnisonVoice %d", synthImpl->maxUnisonVoice));
        strings.push_back(StringFormat("unisonVoiceCount %d", synthImpl->unisonVoiceCount));
        strings.push_back(StringFormat("polyVoiceCount %d", synthImpl->polyVoiceCount));
        String s = "Held notes: ";
        for (auto& note : heldNotes) {
            s += String(noteName(note.pitch)) + ",";
        }
        if (heldNotes.empty())
            s += "<empty>";
        strings.push_back(s);
        String str;
        if (thisImpl) {
            str = StringFormat("SR %.2f BS %d", thisImpl->getSampleRate(), thisImpl->getBlockSize());
        } else {
            str = StringFormat("SR %d BS %u", moduleInstance->getSampleFormat().sampleRate, moduleInstance->getSampleFormat().blockSize);
        }
        strings.push_back(str);
        int flags = 0;
        for (int i = 8; i < 16; i++) {
            flags |= (1 << i);
        }
        VstTimeInfo* timeinfo = thisImpl ? thisImpl->getTimeInfo(flags) : nullptr;
        if (timeinfo) {
            strings.push_back(StringFormat("samplePos %.3f", timeinfo->samplePos));
            strings.push_back(StringFormat("ppqPos %.3f", timeinfo->ppqPos));
            strings.push_back(StringFormat("tempo %.3f", timeinfo->tempo));
            strings.push_back(StringFormat("barStartPos %.3f", timeinfo->barStartPos));
        }

        auto& list = this->list.getListRef();

        while (list.size() < strings.size()) {
            list.push_back(new gui_synth_stats_list_entry());
            this->list.add(list.back());
        }
        while (list.size() > strings.size()) {
            this->list.remove(list.back());
            delete list.back();
            list.pop_back();
        }
        for (int i = 0; i < (int) strings.size(); i++) {
            static_cast<gui_synth_stats_list_entry*>(list[i])->string = strings[i];
        }
        this->list.layout();
        guictr_base::onTick(ctrl);
    }

    void render(NVGcontext* vg) override {
        if (isBackgroundRendered()) {
            renderBackground(vg);
        }
        if (!setScissorTransform(vg)) {
            return;
        }
        for (guibase* gui : guis) {
            if (gui->isVisible() && gui->size.x > 10 && gui->size.y > 10) {
                nvgSave(vg);
                gui->render(vg);
                nvgRestore(vg);
            }
        }
    }

    void handleSplitterChanged(Splitter& splitter, float scale, int clampedAt) override {
        onChildLayoutChanged(this);
    }

    ivec2 getContainerSize() override {
        return size;
    }

    ivec2 getContainerPos() override {
        return {0, 0};
    }

    void layout() override {
        auto cs = getSizeContent();
        const auto titleHeight = math::roundfS32(cs.y * 0.1f * 0.27f);
        for (auto& knob : vecParamUI) {
            if (!knob.knob)
                continue;
            if (knob.type == guiknob::knobtype::KNOB_LABELED) {
                knob.knob->setLabelsFontScale(0.9f, 0.9f);
                knob.knob->setLabelsScale(0.2f, 0.2f);
            }
            if (knob.type == guiknob::knobtype::SLIDER_LABELED) {
                knob.knob->setLabelsFontScale(0.9f, 0.9f);
                knob.knob->setLabelsScale(0.1f, 0.1f);
            }
        }
        modulation.setTitleHeight(titleHeight);
        const auto controlsWidth   = splitter.leftOrTop(cs.x) - padding / 2;
        const auto modulationWidth = splitter.rightOrBottom(cs.x) - padding / 2;
        splitter.pos               = ivec2(controlsWidth + padding / 2 - Splitter::SPLITTER_LAYOUT_THICKNESS / 2, 0);
        splitter.size              = ivec2(Splitter::SPLITTER_LAYOUT_THICKNESS, cs.y);
        modulation.size      = ivec2(modulationWidth, cs.y*0.75f);
        modulation.pos       = ivec2(cs.x - modulationWidth, 0);
        int scale            = 4;//!list.isVisible() && !list2.isVisible()?4:3;
        cs                   = ivec2(controlsWidth, cs.y);
        const auto numRows   = moduleLayout.size();
        const auto innerSize = vec2(cs.x, cs.y * scale / 4) - vec2(padding * 2);
        auto modulePos      = ivec2(0);
        auto knobSize = vec2(math::max(1.0, controlsWidth/20.0),0);
        for (auto& row : moduleLayout) {

            const auto numCols  = row.size();
            auto innerRowHeight = float(innerSize.y - (numRows - 1) * (padding)) / numRows;
            auto innerColWidth  = float(innerSize.x - (numCols - 1) * (padding)) / numCols;
            auto knobSizeF      = vec2(innerColWidth, innerRowHeight);
            auto moduleSize     = ivec2(math::roundfS32(knobSizeF.x), math::roundfS32(knobSizeF.y));
            for (auto* ctr : row) {
                ctr->pos  = modulePos;
                ctr->size = moduleSize;
                auto knobSizeModule = knobSize;
                if (ctr == &ctrLfo || ctr == &ctrMacro || ctr == &ctrEnvV || ctr == &ctrEnvM)
                    knobSizeModule.x *= 0.75f;
                ctr->layoutParameterGroup(ctr->size, knobSizeModule, titleHeight);
                modulePos.x = ctr->right() + padding;
            }
            modulePos.x = 0;
            modulePos.y += moduleSize.y + padding;
        }

        list2.pos   = moduleLayout[1].back()->getRightTop() + ivec2(padding, 0);
        list2.size  = ivec2(moduleLayout[0].back()->right() - list2.pos.x - padding, (moduleLayout[1].back()->size.y));
        list.pos  = vec2(modulation.left(), modulation.bottom() + padding);
        list.size = ivec2(modulation.size.x, size.y - (modulation.bottom()+padding));
        // auto listHeight = math::roundfS32(math::clamp<float>(cs.y*0.1f*0.33f, getLayoutHeight(this) / 2, getLayoutHeight(this)));
        list.setRowHeight(titleHeight);
        list2.setRowHeight(titleHeight);

        for (guibase* gui : guis) {
            gui->layout();
        }
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

        if (param && vst2Instance && vst2Instance->isExternalInstance()) {
            auto paramIdxInternal = param->getParamIdxInternal();
            char buf[PLUGIN_PARAM_STR_MAX_LEN+1]{};
            vst2Instance->getParameterDisplay(paramIdxInternal, buf);
            String paramValue = buf;
            editfield.mCallbackEnd = [this, param, paramValue, paramIdxInternal](const std::string& str) {
                auto paramConverted = vst2Instance->convertParamValueDisplay(paramIdxInternal, param_unit_t{ str, "" });
                if (paramConverted.success) {
                    vst2Instance->setParameter(paramIdxInternal, paramConverted.floatVal);
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
            editfield.setValue(paramValue);
            editfield.setSelectionRange(-1, -1);
            editfield.setFontSize(layout.valueHeight * layout.fontScaleValue);
            parentCtrl->focusGui(&editfield);
            return;
        }
        guictr_base::buttonClicked(button);
    }

    void setUiLayout(const ui_layout_t& layout) {
        splitter.setScale(layout.splitPos);
    }

    bool getUiLayout(ui_layout_t& layout) const {
        layout = ui_layout_t{ 0, splitter.getScale() };
        return true;
    }

    void onChildLayoutChanged(guibase* g) override {
        bGuiNeedsRefresh = true;
        if (this->parent) {
            this->parent->onChildLayoutChanged(this);
        }
    }
};

class guicontainer_plugin_synth_voicestates final : public guictr_base {
    SynthImplUnison* const synth;
    SynthImplUnison::VoiceList list{};
public:
    explicit guicontainer_plugin_synth_voicestates(SynthImplUnison* synth)
        : guictr_base(),
            synth(synth)
    {
        setBackgroundRendered(true);
        setCanMouseHit(true);
        padding = 2;
        margin  = 0;
    }

    void onTick(AppCtrl* ctrl) override {
        auto lock = synth->tryLock();
        if (lock.isLocked()) {
            list = synth->getVoiceListPrev();
        }
    }

    void render(NVGcontext* vg) override {
        if (!isRenderableSizeAndContext(vg))
            return;
        guictr_base::render(vg);
        auto cs = getSizeContent();
        auto voiceWidth = math::max(2.5f, float(cs.x) / NUM_POLY_VOICES);
        int inset = padding;
        auto voicePos = vec2(inset);
        auto voiceSize = vec2(voiceWidth, cs.y - inset * 2);
        {
            for (int polyIndex = 0; polyIndex < NUM_POLY_VOICES; ++polyIndex) {
                nvgBatchedRect(vg, voicePos.x, voicePos.y, voiceSize.x-2, voiceSize.y);
                voicePos.x += voiceSize.x;    
            }
            NVGpaint paint{};
            paint.image      = -1;
            paint.innerColor = theme->getColor(GuiColor::COL_NOTE_MUTE);
            paint.customPar = NVGBatchedShading::NVG_BATCHED_SHADED;
            nvgFillPaint(vg, paint);
            nvgBatchedRender(vg);
        }
        if (list.numPolyVoices > 0) {
            for (int i = 0; i < list.numPolyVoices; ++i) {
                int polyIndex = list.polyVoices[i];
                voicePos.x = inset + polyIndex * voiceSize.x;
                nvgBatchedRect(vg, voicePos.x, voicePos.y, voiceSize.x-2, voiceSize.y);
            }
            NVGpaint paint{};
            paint.image      = -1;
            paint.innerColor = theme->getColor(GuiColor::COL_NOTE_PLAYING);
            paint.customPar = NVGBatchedShading::NVG_BATCHED_SHADED;
            nvgFillPaint(vg, paint);
            nvgBatchedRender(vg);
        }
        /* nvgBeginPath(vg);
        for (int polyIndex = 0; polyIndex < NUM_POLY_VOICES; ++polyIndex) {
            nvgRect(vg, voicePos.x, voicePos.y, voiceSize.x-2, voiceSize.y);
            voicePos.x += voiceSize.x;    
        }
        nvgFillColor(vg, colBg);
        nvgFillCustomPar(vg, -2);
        nvgFill(vg);
        if (list.numPolyVoices > 0) {
            nvgBeginPath(vg);
            for (int i = 0; i < list.numPolyVoices; ++i) {
                int polyIndex = list.polyVoices[i];
                voicePos.x = inset + polyIndex * voiceSize.x;
                nvgRect(vg, voicePos.x, voicePos.y, voiceSize.x-2, voiceSize.y);
            }
            nvgFillColor(vg, theme->getColor(GuiColor::COL_NOTE_PLAYING));
            nvgFillCustomPar(vg, -2);
            nvgFill(vg);
        } */
    }
};

class guicontainer_plugin_synth_header final : public guictr_base {
    SynthImplUnison* const synth;
    guicontainer_plugin_synth_voicestates voiceStates;
    guidropdown_select_preset selectPreset;
    guibutton prev;
    guibutton next;

public:
    explicit guicontainer_plugin_synth_header(SynthImplUnison* _synth)
        : guictr_base(),
            synth(_synth),
            voiceStates(_synth),
            selectPreset()
    {
        padding = 0;
        margin  = 0;
        prev.setText("<");
        next.setText(">");
        add(&voiceStates);
        add(&selectPreset);
        add(&prev);
        add(&next);
        selectPreset.setPresetManager(synth->getPresetManager());
        selectPreset.setCallback([this](const PresetManager::Preset& preset) {
            ThreadLock lock = synth->lock();
            this->synth->loadPreset(preset.path);
            this->selectPreset.setString(synth->getPreset().name);
        });
    }

    void buttonClicked(guibase* button) override {
        int dir = 0;
        if (button == &prev) {
            dir = -1;
        }
        if (button == &next) {
            dir = 1;
        }
        if (dir) {
            auto& prMgr     = synth->getPresetManager();
            auto& presets   = prMgr.getPresets();
            if (presets.empty()) {
                return;
            }
            auto& curPreset = synth->getPreset();
            size_t i        = 0;
            for (; i < presets.size(); ++i) {
                if (presets[i].path == curPreset.path) {
                    break;
                }
            }
            int maxTries = CtrSize(presets);
            while (maxTries-- > 0) {
                size_t nextIdx = (i + dir + presets.size()) % presets.size();
                if (nextIdx < presets.size()) {
                    auto lock = synth->lock();
                    if (0 == synth->loadPreset(presets[nextIdx].path)) {
                        break;
                    }
                }
                i = nextIdx;
            }
        }
        this->selectPreset.setString(synth->getPreset().name);
    }

    ~guicontainer_plugin_synth_header() override {
        removeGuis();
    }

    void layout() override {
        this->selectPreset.setString(synth->getPreset().name);
        auto cs   = getSizeContent();
        prev.size = next.size = { cs.y / 2, cs.y };
        selectPreset.size     = { cs.x * 0.33f, cs.y };
        selectPreset.size.y   = cs.y;
        selectPreset.pos      = { (cs.x) / 2 - (prev.size.x + next.size.x + selectPreset.size.x) / 2, 0 };
        prev.pos              = selectPreset.getRightTop();
        next.pos              = prev.getRightTop();
        voiceStates.pos = { 0, 0 };
        voiceStates.size = { selectPreset.left(), cs.y };
        guictr_base::layout();
    }
};

class guicontainer_plugin_synth final : public guictr_base {
    guicontainer_plugin_synth_editor editor;
    guicontainer_plugin_synth_header header;

public:
    explicit guicontainer_plugin_synth(PluginVST2_Synth* plugin)
        : editor(nullptr, plugin->getSynth(), plugin), header(plugin->getSynth()) {
        padding = 0;
        margin  = 0;
        setBackgroundRendered(false);
        add(&header);
        add(&editor);
    }
    explicit guicontainer_plugin_synth(module_synth_unison* module)
        : editor(module, module->getSynth(), nullptr), header(module->getSynth()) {
        padding = 0;
        margin  = 0;
        setBackgroundRendered(false);
        add(&header);
        add(&editor);
    }

    ~guicontainer_plugin_synth() override {
        remove(&editor);
        remove(&header);
    }

    void layout() override {
        header.size.y = math::roundfS32(getLayoutHeight(this));
        editor.pos.y    = header.size.y;
        auto cs         = size;
        header.size.x = cs.x;
        cs.y -= header.size.y;
        editor.size  = cs;
        guictr_base::layout();
    }

    void onTick(AppCtrl* ctrl) override {
        guictr_base::onTick(ctrl);
    }

    void setUiLayout(const ui_layout_t& layout) {
        editor.setUiLayout(layout);
    }

    bool getUiLayout(ui_layout_t& layout) const {
        return editor.getUiLayout(layout);
    }

    void onSetParameter(int32_t index, float value) {
        editor.onSetParameter(index, value);
    }

    void getSizeScale(int& w, int& h) const {
        w = 1280*1.25;
        h = 720;
    }

    void onGuiOpen() {
        editor.onGuiOpen();
    }

    void onGuiClose() {
        editor.onGuiClose();
    }
};

const char* getName() {
    return PLUGIN_EFFECT_NAME;
}

AudioEffectX* createPlugin(audioMasterCallback audioMaster) {
    return new PluginVST2_Synth(audioMaster);
}

std::shared_ptr<PluginViewContainer> PluginVST2_Synth::createViewCtrVst2() {
    return this->impl->createViewCtrImpl();
}

class PluginViewContainerSynth final : public PluginViewContainer {
public:
    guicontainer_plugin_synth ctr_main;
    explicit PluginViewContainerSynth(module_synth_unison* eff)
        : ctr_main(eff) {
    }
    explicit PluginViewContainerSynth(PluginVST2_Synth* eff)
        : ctr_main(eff) {
    }
    ~PluginViewContainerSynth() override = default;
    guicontainer_plugin_synth& getPluginUI() {
        return ctr_main;
    }
    const guicontainer_plugin_synth& getPluginUI() const {
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

void module_synth_unison::getUiSnapshot(snapshot_t& snapshot) {
    for (auto& view : views) {
        auto implCtrType = dynamic_cast<PluginViewContainerSynth*>(view.get());
        ui_layout_t layout{};
        if (implCtrType && implCtrType->getPluginUI().getUiLayout(layout)) {
            layout.uiId = view->getUiId();
            snapshot.uiLayout.push_back(layout);
        }
    }
}

void module_synth_unison::setUiSnapshot(snapshot_t& snapshot) {
    for (auto& uis : snapshot.uiLayout) {
        std::vector<std::shared_ptr<PluginViewContainer>> views;
        getAllViewCtrs(uis.uiId, views);
        for (auto& view : views) {
            auto implCtrType = dynamic_cast<PluginViewContainerSynth*>(view.get());
            if (implCtrType) {
                implCtrType->getPluginUI().setUiLayout(uis);
            }
        }
    }
}

void PluginVST2_Synth::getUiSnapshot(snapshot_t& snapshot) {
    for (auto& view : views) {
        auto implCtrType = dynamic_cast<PluginViewContainerSynth*>(view.get());
        ui_layout_t layout{};
        if (implCtrType && implCtrType->getPluginUI().getUiLayout(layout)) {
            layout.uiId = view->getUiId();
            snapshot.uiLayout.push_back(layout);
        }
    }
}

void PluginVST2_Synth::setUiSnapshot(snapshot_t& snapshot) {
    for (auto& uis : snapshot.uiLayout) {
        auto view = getViewCtrVst2(uis.uiId);
        if (!view)
            continue;
        auto implCtrType = dynamic_cast<PluginViewContainerSynth*>(view.get());
        if (!implCtrType)
            continue;
        implCtrType->getPluginUI().setUiLayout(uis);
    }
}

int32_t SynthImplUnison::loadPreset(const String& presetPath) {
    std::shared_ptr<plugin_snapshot_t> pluginSnapshot = loadPluginSnapshot(presetPath);
    dbgassert(pluginSnapshot);
    if (pluginSnapshot) {
        std::shared_ptr<std::vector<std::byte>> buf;
        auto sizeData = static_cast<size_t>(pluginSnapshot->dataChunk.size());
        if (sizeData > 0) {
            buf = std::make_shared<std::vector<std::byte>>(sizeData);
            std::memcpy(buf->data(), pluginSnapshot->dataChunk.data(), sizeData);
            snapshot_t snapshotLoaded;
            if (deserializeSnapshot(buf, snapshotLoaded)) {
                setSnapshot(snapshotLoaded);
                String path;
                String name;
                String ext;
                SplitPath(presetPath, &path, &name, &ext);
                setPreset(presetPath, name);
                if (instanceVST2Plugin) {
                    instanceVST2Plugin->setUiSnapshot(snapshotLoaded);
                }
                if (moduleSynthUnisonInstance) {
                    moduleSynthUnisonInstance->setUiSnapshot(snapshotLoaded);
                    moduleSynthUnisonInstance->onPresetLoaded();
                }
                notifyUiChanges();
                if (instanceVST2Plugin) {
                    instanceVST2Plugin->onPresetLoaded();
                }
                return 0;
            }
            return -3;
        }
        return -2;
    }
    return -1;
}

std::shared_ptr<PluginViewContainer> SynthImplUnison::createViewCtrImpl() {
    if (this->moduleSynthUnisonInstance) {
        this->views.push_back(std::make_shared<PluginViewContainerSynth>(this->moduleSynthUnisonInstance));
        return this->views.back();
    }
    if (this->instanceVST2Plugin) {
        this->views.push_back(std::make_shared<PluginViewContainerSynth>(this->instanceVST2Plugin));
        return this->views.back();
    }
    return nullptr;
}

void SynthImplUnison::setSetting(Settings setting, bool value) {
    settings[setting] = static_cast<float>(value);
    auto lock = this->lock();
    if (setting == Settings::Oversampling) {
        initSampleRate();
    }
    if (this->instanceVST2Plugin) {
        this->instanceVST2Plugin->settingChanged(setting, settings[setting]);
    }
    if (this->moduleSynthUnisonInstance) {
        this->moduleSynthUnisonInstance->settingChanged(setting, settings[setting]);
    }
}

void module_synth_unison::settingChanged(Settings setting, float value) {
    log_lf(Log::L_DEBUG, "Setting changed: %s %f\n", stringsSettings[setting], value);
}

void PluginVST2_Synth::settingChanged(int32_t setting, float value) {
    log_lf(Log::L_DEBUG, "Setting changed: %s %f\n", stringsSettings[setting], value);
    if (setting == Settings::Oversampling) {
        setInitialDelay(getSynth()->getLatency());
    }
}

SynthImplUnison::SynthImplUnison(module_synth_unison* module)
    : SynthImpl<SynthImplUnison, ParametersSynthUnison>(module),
    moduleSynthUnisonInstance(module),
    instanceVST2Plugin(nullptr)
{
    initImpl();
}

}// namespace PluginSynth

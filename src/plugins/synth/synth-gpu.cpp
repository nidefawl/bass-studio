#include "assert_dbg.h"
#include "basectrl.h"
#include "config.h"
#include "dsp_util.h"
#include "event.h"
#include "gl/gl_util.h"
#include "gl/gl_vbo.h"
#include "gl/gl_context.hpp"
#include "glheaders.h"
#include "gui/container/container.h"
#include "gui/container/container_layout.h"
#include "gui/contextmenu/contextmenu_base.h"
#include "gui/controls/button.h"
#include "gui/controls/inputfield.h"
#include "gui/controls/knobpluginparam.h"
#include "gui/shape/shape-render.hpp"
#include "gui/shape/shapeeditor.h"
#include "hires_timer.h"
#include "host/daw/mainctrl.h"
#include "host/shape/shape.h"
#include "logging.h"
#include "math/seq_math.h"
#include "math/simd_math.h"
#include "note.h"
#include "platform.h"
#include "rand.h"
#include "renderresources.h"
#include "saferef.h"
#include "seq_time.h"
#include "seq_util.h"
#include "str_util.h"
#include "synth-gpu-gl.h"
#include "synth-modulations-ui.hpp"
#include "synth-modulations.hpp"
#include "synth-param.hpp"
#include "synth-plugin.h"
#include "synth-snapshot.h"
#include "synth-template.hpp"
#include "synth-types.hpp"
#include "plugins/lfo/lfo-types.hpp"
#include "plugins/lfo/lfo-ui.hpp"
#include "types.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <nanovg.h>
#include <nanovg_min.h>
#include <GLFW/glfw3.h>
#include <vector>

namespace PluginSynth::GPU {
uint32_t gDebugBenchmarkFlags = 0;
static constexpr uint16_t NUM_AUDIO_CHANNELS = 2;
static constexpr uint16_t NUM_POLY_VOICES   = 32;
static constexpr uint16_t MAX_UNISON_VOICES   = 32;

/* keep in sync with shader defines */
static constexpr size_t NUM_VOICE_INPUT_PARAMETERS = 3;
static constexpr size_t NUM_SYNTH_INPUT_PARAMETERS = 2;
/*

struct voice_state_input_t {
    float velocity[N_SAMPLES];
    float pitch[N_SAMPLES];
};

struct synth_state_input_t {
    float param_bleb[N_SAMPLES];
    float param_stereo[N_SAMPLES];
    float param_pw[N_SAMPLES];
    float param_filter_keytrack[N_SAMPLES];
    float param_detune_keytrack[N_SAMPLES];
    float param_width_keytrack[N_SAMPLES];
};

*/

static constexpr uint16_t NUM_ADSR = 2;
static constexpr uint16_t NUM_LFO = 3;

static constexpr uint16_t MAX_SYNTH_PARAMS = 256;
static constexpr uint16_t MAX_PARAMS_PER_ADSR = 32;
static constexpr uint16_t MAX_PARAMS_PER_LFO = 16;
static constexpr uint16_t MAX_ADSR_LFO = 8;


enum ModDestinations : int32_t {
    ModDest_Osc1Volume = 0,
    ModDest_Osc1Filter,
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
};
static constexpr uint16_t NUM_MODULATION_DESTINATIONS = 18;
// as name
const std::array<const char*, NUM_MODULATION_DESTINATIONS> modDestNames = {
    "OSC 1 Volume",
    "OSC 1 Filter",
    "ADSR 1 A Duration",
    "ADSR 1 H Duration",
    "ADSR 1 D Duration",
    "ADSR 1 S Amount",
    "ADSR 1 R Duration",
    "ADSR 1 A Shape",
    "ADSR 1 D Shape",
    "ADSR 1 R Shape",
    "ADSR 2 A Duration",
    "ADSR 2 H Duration",
    "ADSR 2 D Duration",
    "ADSR 2 S Amount",
    "ADSR 2 R Duration",
    "ADSR 2 A Shape",
    "ADSR 2 D Shape",
    "ADSR 2 R Shape",
};

struct LFOParameters  : public ::PluginLFO::LFOSyncParameters {
    double freq = 1.0;
    double freqHz = 1.0;
    double phaseOffset = 0.0;
    double rampDuration = 0.0;
    enum LFOTriggerMode {
        Note,
        OneShot,
        SongPosition,
    } trigger = Note;
    double bpm = 120.0;

    // state that has to be serialized
    int32_t randomModeId = 0;
    bool modeIsShape = true;
    DAW::Shape::shape_t shape{};

    double paramToFreqHz(double d) const {
        if (!syncFlags || syncRatios.empty()) {
            return math::max(pow(2.0, d * 21.0) * 0.01, 0.001);
        } else {
            auto index = math::clamp<int32_t>(math::floorfS32(d * CtrSize(syncRatios)), 0, CtrSize(syncRatios) - 1);
            return double(syncRatios[index].denominator * bpm) / double(syncRatios[index].numerator * 60.0 * 4.0);
        }
    }
};
class SynthLfo final : public PluginLFO::LFORateMinMaxAutomation {
    double phase          = 0.0;
    PluginLFO::lfo_automation_src_synced_t srcSync;
    std::shared_ptr<PluginLFO::lfo_automation_src_random_t> srcRand;
    LFOParameters* params{};
    Envelope envRamp;
    uint64_t customSeed = 0;
public:
    SynthLfo() {
        this->srcSync.rateMinMax = this;
        envRamp.a = 1.0 / 0.050;
        envRamp.h = 0.0;
        envRamp.d = 1.0 / 0.050;
        envRamp.s = 1.0;
        envRamp.r = 1.0 / 0.050;
        envRamp.shapes = {0.5, 0.5, 0.5};
    }
    LFOParameters& getParameters() { return *params; }
    const LFOParameters& getParameters() const { return *params; }
    void setPhase(double d) {
        this->phase = d;
    }
    void resetRamp() {
        envRamp.Reset();
        envRamp.Start();
    }
    const PluginLFO::lfo_automation_src_random_t* getSourceRand() const {
        return srcRand.get();
    }
    const PluginLFO::lfo_automation_src_synced_t* getSourceSync() const {
        return &srcSync;
    }
    void setRandomMode(int32_t mode) {
        if (mode != -1 && srcRand && srcRand->getModeId() == mode) {
            return;
        }
        using namespace PluginLFO;
        switch (mode) {
            case -1:
                if (srcRand) {
                    break;
                }
                [[fallthrough]];
            default:
            case 0:
                srcRand = std::make_shared<lfo_automation_src_random_smooth_t>();
                break;
            case 1:
                srcRand = std::make_shared<lfo_automation_src_random_linear_t>();
                break;
            case 2:
                srcRand = std::make_shared<lfo_automation_src_random_exp_t>();
                break;
            case 3:
                srcRand = std::make_shared<lfo_automation_src_random_sample_and_hold_t>();
                break;
        }
        srcRand->setSeed(customSeed);
        this->srcRand->rateMinMax = this;
        this->srcRand->sync = this->params;
    }
    void setParameters(LFOParameters* params, uint64_t customSeed) {
        this->params = params;
        this->customSeed = customSeed;
        this->srcSync.sync = this->params;
        this->srcSync.shape = &this->params->shape;
        if (!srcRand) {
            setRandomMode(params->randomModeId);
        } else {
            srcRand->setSeed(customSeed);
        }
    }
    void Update(double dt) {
        double p = fp_math::silenceNanInfd(this->phase + params->freqHz * dt);
        this->phase = p;
        envRamp.a = Envelope::GetTimeBaseFromParam(params->rampDuration);
        envRamp.Update(dt);
    }
    double GetRamp() const {
        if (params->rampDuration > 0.0) {
            return envRamp.value;
        }
        return 1.0;
    }
    double GetLfo() const {
        double p = this->phase;
        if (!params->modeIsShape) {
            return getSourceRand()->sampleCurve(p);
        }
        if (params->trigger == LFOParameters::OneShot) {
            p = math::min(1.0, p);
        } else {
            double _unused;
            p = std::modf(p, &_unused); 
        }
        return params->shape.sampleCurve(static_cast<float>(p), false);
    }
    double GetRampedLfo() const {
        double lfo = GetLfo();
        if (params->rampDuration > 0.0) {
            lfo *= envRamp.value;
        }
        return lfo;
    }
    std::pair<float, float> getMinMax(double dTick) const override {
        return { 0, 1 }; 
    }
    std::tuple<float, float, float> getRatePhase(double dTick) const override {
        return { params->freq, params->phaseOffset, 0 };
    }
    float getScaledRate(PluginLFO::LFOSyncParameters* sync, float rate) override { 
        return 1.0 / params->paramToFreqHz(rate);
    };
};
struct VoiceSynth {
    std::array<double, 64> modValues{};
    double velocity     = 0.0;
    note_t noteT = {};
    std::array<Envelope, NUM_ADSR> envelopes;
    std::array<SynthLfo, NUM_LFO> lfos;
    double frequency       = 0.0;
    double targetFrequency = 0.0;
    double pitchBend       = 1.0;
    bool bIsActive         = false;


    bool isVoiceActive() const {
        if (hint_likely(!bIsActive)) {
            return false;
        }
        return !GetVolumeEnvelope().IsIdle();
    }

    Envelope& GetVolumeEnvelope() { return envelopes[0]; }
    const Envelope& GetVolumeEnvelope() const { return envelopes[0]; }
    Envelope& GetFilterEnvelope() { return envelopes[1]; }
    const Envelope& GetFilterEnvelope() const { return envelopes[1]; }

    bool IsReleased() const { return GetVolumeEnvelope().IsReleased(); }
    double GetVolume() const { return GetVolumeEnvelope().value; }

    void ResetPhases(bool bRandomPhase) {
    }

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

    void Start(bool bTriggerMono) {
        bIsActive = true;
        if (!bTriggerMono) {
            ResetEnvelopes();
            for (auto& lfo : lfos) {
                lfo.setPhase(0.0);
                lfo.resetRamp();
            }
        }
        for (auto& env : envelopes) {
            env.Start();
        }
    }
};

enum ParametersSynthGPU : size_t {
    MasterVolume = 0,
    VoiceMode,
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
    ADSR_1_A_Duration = MAX_SYNTH_PARAMS,
    ADSR_1_H_Duration,
    ADSR_1_D_Duration,
    ADSR_1_S_Amount,
    ADSR_1_R_Duration,
    ADSR_1_A_Shape,
    ADSR_1_D_Shape,
    ADSR_1_R_Shape,
    ADSR_2_A_Duration = MAX_SYNTH_PARAMS + MAX_PARAMS_PER_ADSR,
    ADSR_2_H_Duration,
    ADSR_2_D_Duration,
    ADSR_2_S_Amount,
    ADSR_2_R_Duration,
    ADSR_2_A_Shape,
    ADSR_2_D_Shape,
    ADSR_2_R_Shape,
    LFO_1_Frequency = MAX_SYNTH_PARAMS + MAX_ADSR_LFO * MAX_PARAMS_PER_ADSR,
    LFO_1_TriggerMode, // 0 = note resets phase, 1 = one shot (note resets phase), 2 = song position
    LFO_1_Phase,
    LFO_1_RampDuration,
    LFO_2_Frequency = MAX_SYNTH_PARAMS + MAX_ADSR_LFO * MAX_PARAMS_PER_ADSR + MAX_PARAMS_PER_LFO,
    LFO_2_TriggerMode,
    LFO_2_Phase,
    LFO_2_RampDuration,
};

static constexpr std::array<const char*, 11> stringsModSource = {
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
    "Lfo3"
};
static constexpr size_t NUM_MODULATION_SOURCES = stringsModSource.size();

struct ui_layout_t {
    int32_t uiId = 0;
};
struct lfo_snapshot_t {
    DAW::Shape::shape_snapshot_t shape{};
    bool modeIsShape = true;
    int32_t randomModeId = 0;
    int32_t syncFlags;
};
struct snapshot_t {
    int32_t version = 0;
    std::vector<PluginSynth::param_float_snapshot_t> params;
    std::vector<modulation_snapshot_t> modulations;
    std::vector<lfo_snapshot_t> lfos;
    std::vector<ui_layout_t> uiLayout;
};

static constexpr int32_t SYNTH_GPU_SNAPSHOT_VERSION = 1;

std::shared_ptr<std::vector<std::byte>> serializeSnapshot(const snapshot_t& snapshot) {
    dbgassert(snapshot.version == SYNTH_GPU_SNAPSHOT_VERSION);
    auto shrdHeapVec = std::make_shared<std::vector<std::byte>>();
    shrdHeapVec->resize(256);
    DAW::ByteBuffer::stream_write<std::vector<std::byte>> out{*shrdHeapVec, 0};
    out.write(size_t(0));
    out.write(snapshot.version);
    out.write(size_t{snapshot.params.size()});
    out.write(size_t{snapshot.modulations.size()});
    out.write(size_t{snapshot.uiLayout.size()});
    out.write(size_t{snapshot.lfos.size()});
    for (const auto& p : snapshot.params) {
        out.write(p.paramIdx);
        out.write(p.value);
    }
    for (const auto& modulation : snapshot.modulations) {
        out.write(modulation.slotIdx);
        out.write(size_t{modulation.inputs.size()});
        out.write(size_t{modulation.destinations.size()});
        for (const auto& input : modulation.inputs) {
            out.write(input.typeIdx);
            out.write(input.srcIdx);
            out.write(input.opIdx);
            out.write(input.value);
            out.write(input.range);
            out.writeString(input.function);
        }
        for (const auto& dest : modulation.destinations) {
            out.write(dest.paramIdx);
            out.write(dest.range);
        }
    }
    for (const auto& uiLayout : snapshot.uiLayout) {
        out.write(uiLayout.uiId);
    }
    for (const auto& lfo : snapshot.lfos) {
        out.write(int32_t{1});
        DAW::Shape::writeShape(out, lfo.shape);
        out.write(lfo.modeIsShape);
        out.write(lfo.randomModeId);
        out.write(lfo.syncFlags);
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
    if (snapshot.version > SYNTH_GPU_SNAPSHOT_VERSION)
        return false;
    size_t numParams = 0;
    size_t numModulations = 0;
    size_t numUiLayouts = 0;
    size_t numLfos = 0;
    if (!in.read(numParams) || numParams > 1000)
        return false;
    if (!in.read(numModulations) || numModulations > 1000)
        return false;
    if (!in.read(numUiLayouts) || numUiLayouts > 1000)
        return false;
    if (!in.read(numLfos) || numLfos > 1000)
        return false;
    snapshot.params.resize(numParams);
    snapshot.uiLayout.resize(numUiLayouts);
    snapshot.modulations.resize(numModulations);
    snapshot.lfos.resize(numLfos);

    for (auto& p : snapshot.params) {
        if (!in.read(p.paramIdx))
            return false;
        if (!in.read(p.value))
            return false;
    }
    for (auto& modulation : snapshot.modulations) {
        if (!in.read(modulation.slotIdx))
            return false;
        size_t numInputs = 0;
        size_t numDestinations = 0;
        if (!in.read(numInputs))
            return false;
        if (!in.read(numDestinations))
            return false;
        modulation.inputs.resize(numInputs);
        modulation.destinations.resize(numDestinations);
        for (auto& input : modulation.inputs) {
            if (!in.read(input.typeIdx))
                return false;
            if (!in.read(input.srcIdx))
                return false;
            if (!in.read(input.opIdx))
                return false;
            if (!in.read(input.value))
                return false;
            if (!in.read(input.range))
                return false;
            if (!in.readString(input.function))
                return false;
        }
        for (auto& dest : modulation.destinations) {
            if (!in.read(dest.paramIdx))
                return false;
            if (!in.read(dest.range))
                return false;
        }
    }
    for (auto& layout : snapshot.uiLayout) {
        if (!in.read(layout.uiId))
            return false;
    }

    for (auto& lfo : snapshot.lfos) {
        int32_t version = 0;
        if (!in.read(version))
            return false;
        if (version < 1)
            return false;
        if (!DAW::Shape::readShape(in, lfo.shape)) {
            return false;
        }
        if (!in.read(lfo.modeIsShape))
            return false;
        if (!in.read(lfo.randomModeId))
            return false;
        if (!in.read(lfo.syncFlags))
            return false;
        // snapshot.shapes.push_back(std::move(shape));
    }
    snapshotOut = std::move(snapshot);
    return true;
}

class SynthImplGPU final : public SynthImpl<SynthImplGPU, ParametersSynthGPU>, public ModulationController, public DAW::GPU::GPUAudioProcessor {
private:
    friend class guicontainer_plugin_synth_gpu;
    friend class guicontainer_plugin_synth_adsr_shape;

private:
    PluginSynth::module_synth_template<SynthImplGPU>* const moduleSynthInstance;
    std::array<PluginSynth::GPU::VoiceSynth, NUM_POLY_VOICES> voices;
    std::array<SynthLfo, NUM_LFO> lfosSongPos;
    std::array<LFOParameters, NUM_LFO> lfoParameters;
    std::array<double, 1> otherParams{0.0f};
    seq_rand synthRand;
    std::vector<std::shared_ptr<PluginViewContainer>> views;

    int64_t timePerfLog = 0;
    int64_t timeCheckShader = 0;
    double timeComputeAvg = -2.0;
    hires_timer_t perfTimer;

private:
    void initImpl() {
        auto addParam = [this](SynthParamBase* param, size_t idx) {
            while (this->vecParams.size() <= idx) {
                this->vecParams.push_back(nullptr);
            }
            this->vecParams[idx] = param;
        };
        auto addFloatParam = [&addParam](size_t enumParam) -> SynthParam_Float* {
            SynthParam_Float* param = new SynthParam_Float(static_cast<Parameters>(enumParam));
            addParam(param, enumParam);
            return param;
        };
        auto addIntParam = [&addParam](size_t enumParam) -> SynthParam_Int* {
            SynthParam_Int* param = new SynthParam_Int(static_cast<Parameters>(enumParam));
            addParam(param, enumParam);
            return param;
        };
        auto addEnumParam = [&addParam](size_t enumParam) -> SynthParam_Enum* {
            SynthParam_Enum* param = new SynthParam_Enum(static_cast<Parameters>(enumParam));
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

        addFloatParam(Parameters::MasterVolume)->setRange(0.0, 1.0)->setInitialValue(dsp_util::gainToLinScale(0.25));
        setParamName(getParam(Parameters::MasterVolume), "Master Volume", "Volume", "Vol", "dB");

        const std::array<const char*, 2> stringsVoiceMode = {
            "Poly", "Mono"
        };
        addEnumParam(Parameters::VoiceMode)->setStrings(stringsVoiceMode.begin(), stringsVoiceMode.end())->setInitialValue(0);
        setParamName(getParam(Parameters::VoiceMode), "Voice Mode");

        addEnumParam(Parameters::Osc1Waveform)->setRange(0, 3)->setInitialValue(0);
        setParamName(getParam(Parameters::Osc1Waveform), "Oscillator 1 Waveform", "OSC1 Wave", "Wave");
        addIntParam(Parameters::Osc1UnisonVoiceCount)->setRange(1, MAX_UNISON_VOICES)->setInitialValue(3);
        setParamName(getParam(Parameters::Osc1UnisonVoiceCount), "Oscillator 1 Unison Voices", "OSC1 Unison", "Unison", "Voices");
        addFloatParam(Parameters::Osc1UnisonDetune)->setRange(-6.0, 6.0)->setInitialValue(-0.2);
        setParamName(getParam(Parameters::Osc1UnisonDetune), "Oscillator 1 Unison Detune", "OSC1 Detune", "Detune", "Semi");
        addFloatParam(Parameters::Osc1Filter)->setRange(0.0, 1.0)->setInitialValue(1.0 - 1.0 / 64.0);
        setParamName(getParam(Parameters::Osc1Filter), "Oscillator 1 Filter", "OSC1 Filter", "Filter", "");
        addFloatParam(Parameters::Osc1KeytrackFilter)->setRange(-100.0, 100.0)->setInitialValue(0.0);

        setParamName(getParam(Parameters::Osc1KeytrackFilter), "Oscillator 1 Keytrack Filter", "OSC1 Keytrack Filter", "Keytrack Filter", "%");

        addFloatParam(Parameters::Osc1KeytrackDetune)->setRange(-100.0, 100.0)->setInitialValue(0.0);
        setParamName(getParam(Parameters::Osc1KeytrackDetune), "Oscillator 1 Keytrack Detune", "OSC1 Keytrack Detune", "Keytrack Detune", "%");

        addFloatParam(Parameters::Osc1KeytrackStereoWidth)->setRange(-100.0, 100.0)->setInitialValue(0.0);
        setParamName(getParam(Parameters::Osc1KeytrackStereoWidth), "Oscillator 1 Keytrack Stereo Width", "OSC1 Keytrack Stereo Width", "Keytrack Stereo Width", "%");

        addIntParam(Parameters::Osc1Coarse)->setRange(-24, 24)->setInitialValue(0);
        setParamName(getParam(Parameters::Osc1Coarse), "Oscillator 1 coarse", "OSC1 Semi", "Semi");

        addFloatParam(Parameters::Osc1Fine)->setRange(-1.0, 1.0)->setInitialValue(0.0);
        setParamName(getParam(Parameters::Osc1Fine), "Oscillator 1 fine", "OSC1 Fine", "Fine");


        addFloatParam(Parameters::Osc1Stereo)->setRange(0.0, 100.0)->setInitialValue(75.0);
        setParamName(getParam(Parameters::Osc1Stereo), "Oscillator 1 Stereo", "OSC1 Stereo", "Stereo", "%");
        
        addFloatParam(Parameters::Osc1PulseWidth)->setRange(0.1, 99.9)->setInitialValue(50.0);
        setParamName(getParam(Parameters::Osc1PulseWidth), "Oscillator 1 Pulse Width", "OSC1 PW", "PW", "%");
        addFloatParam(Parameters::Osc1PulseWidthModRate)->setRange(0.0, 100.0)->setInitialValue(0.0);
        setParamName(getParam(Parameters::Osc1PulseWidthModRate), "Oscillator 1 Pulse Width Mod Rate", "OSC1 PW Mod Rate", "PW Mod Rate", "Hz");
        addFloatParam(Parameters::Osc1PulseWidthModDepth)->setRange(0.0, 100.0)->setInitialValue(0.0);
        setParamName(getParam(Parameters::Osc1PulseWidthModDepth), "Oscillator 1 Pulse Width Mod Depth", "OSC1 PW Mod Depth", "PW Mod Depth", "%");


        const int envBase[2] = {static_cast<int32_t>(Parameters::ADSR_1_A_Duration), static_cast<int32_t>(Parameters::ADSR_2_A_Duration)};
        const String parNames[2] = {"Volume", "Filter"};
        const String parNamesShort[2] = {"EnvV", "EnvF"};
        for (size_t i = 0; i < 2; i++) {
            String parName = i == 0 ? "Volume" : "Filter";
            String nameBase = parNames[i] + " " + "Envelope";
            String nameShort = parNamesShort[i];
            auto parAtt = addFloatParam(envBase[i]);
            parAtt->setInitialValue(UnshapeEnvTimeBaseParam(Envelope::GetParamFromTimeMillis(2.0)));
            setParamName(parAtt, nameBase + " Attack", nameShort + " Attack", "Att", "s");
            auto parHold = addFloatParam(envBase[i] + 1);
            parHold->setInitialValue(0.0);
            setParamName(parHold, nameBase + " Hold", nameShort + " Hold", "Hold", "s");
            auto parDec = addFloatParam(envBase[i] + 2);
            parDec->setInitialValue(UnshapeEnvTimeBaseParam(Envelope::GetParamFromTimeMillis(333.0)));
            setParamName(parDec, nameBase + " Decay", nameShort + " Decay", "Dec", "s");
            auto parSus = addFloatParam(envBase[i] + 3);
            parSus->setRange(0.0, 100.0)->setInitialValue(80.0);
            setParamName(parSus, nameBase + " Sustain", nameShort + " Sustain", "Sus", "%");
            auto parRel = addFloatParam(envBase[i] + 4);
            parRel->setInitialValue(UnshapeEnvTimeBaseParam(Envelope::GetParamFromTimeMillis(123.0)));
            setParamName(parRel, nameBase + " Release", nameShort + " Release", "Release", "s");
            auto parAttShape = addFloatParam(envBase[i] + 5);
            parAttShape->setRange(-100.0, 100.0)->setInitialValue(0.0);
            setParamName(parAttShape, nameBase + " Attack Shape", nameShort + " A Shape", "Shape", "%");
            auto parDecShape = addFloatParam(envBase[i] + 6);
            parDecShape->setRange(-100.0, 100.0)->setInitialValue(0.0);
            setParamName(parDecShape, nameBase + " Decay Shape", nameShort + " D Shape", "Shape", "%");
            auto parRelShape = addFloatParam(envBase[i] + 7);
            parRelShape->setRange(-100.0, 100.0)->setInitialValue(0.0);
            setParamName(parRelShape, nameBase + " Release Shape", nameShort + " R Shape", "Shape", "%");
        }

        for (size_t i = 0; i < NUM_LFO; i++) {
            String parName = "LFO " + std::to_string(i + 1);
            auto lfoFreq = addFloatParam(Parameters::LFO_1_Frequency + i * MAX_PARAMS_PER_LFO);
            lfoFreq->setRange(0.0, 1.0)->setInitialValue(0.5);
            setParamName(lfoFreq, parName + " Frequency", parName + " Freq", "Freq", "Hz");
            auto lfoTriggerMode = addEnumParam(Parameters::LFO_1_TriggerMode + i * MAX_PARAMS_PER_LFO);
            std::array triggerModeStrings = {"Note", "One Shot", "Song Position"};
            lfoTriggerMode->setStrings(triggerModeStrings.cbegin(), triggerModeStrings.cend())->setInitialValue(0);
            setParamName(lfoTriggerMode, parName + " Trigger Mode", parName + " Trigger", "Trigger");
            auto lfoPhase = addFloatParam(Parameters::LFO_1_Phase + i * MAX_PARAMS_PER_LFO);
            lfoPhase->setRange(0.0, 1.0)->setInitialValue(0.0);
            setParamName(lfoPhase, parName + " Phase", parName + " Phase", "Phase", "");
            // auto lfoShape = addFloatParam(Parameters::LFO_1_Shape + i * MAX_PARAMS_PER_LFO);
            // lfoShape->setRange(0.0, 1.0)->setInitialValue(0.5);
            // setParamName(lfoShape, parName + " Shape", parName + " Shape", "Shape", "");
            // auto lfoRampAmount = addFloatParam(Parameters::LFO_1_RampAmount + i * MAX_PARAMS_PER_LFO);
            // lfoRampAmount->setRange(0.0, 1.0)->setInitialValue(0.0);
            // setParamName(lfoRampAmount, parName + " Ramp Amount", parName + " Ramp", "Ramp", "");
            auto lfoRampDuration = addFloatParam(Parameters::LFO_1_RampDuration + i * MAX_PARAMS_PER_LFO);
            lfoRampDuration->setInitialValue(UnshapeEnvTimeBaseParam(Envelope::GetParamFromTimeMillis(5.0)));
            setParamName(lfoRampDuration, parName + " Ramp Duration", parName + " Ramp Dur", "Ramp Dur", "s");
        }

        for (size_t i = 0; i < stringsModSource.size(); ++i) {
            auto idx = -1 + i;
            modSourceDescs.emplace_back(idx, stringsModSource[i]);
        }

        modDestDescs.emplace_back(ModDest_Osc1Volume, MasterVolume, modDestNames[ModDest_Osc1Volume]);
        modDestDescs.emplace_back(ModDest_Osc1Filter, Osc1Filter, modDestNames[ModDest_Osc1Filter]);
        modDestDescs.emplace_back(ModDest_ADSR_1_A_Duration, ADSR_1_A_Duration, modDestNames[ModDest_ADSR_1_A_Duration]);
        modDestDescs.emplace_back(ModDest_ADSR_1_H_Duration, ADSR_1_H_Duration, modDestNames[ModDest_ADSR_1_H_Duration]);
        modDestDescs.emplace_back(ModDest_ADSR_1_D_Duration, ADSR_1_D_Duration, modDestNames[ModDest_ADSR_1_D_Duration]);
        modDestDescs.emplace_back(ModDest_ADSR_1_S_Amount, ADSR_1_S_Amount, modDestNames[ModDest_ADSR_1_S_Amount]);
        modDestDescs.emplace_back(ModDest_ADSR_1_R_Duration, ADSR_1_R_Duration, modDestNames[ModDest_ADSR_1_R_Duration]);
        modDestDescs.emplace_back(ModDest_ADSR_1_A_Shape, ADSR_1_A_Shape, modDestNames[ModDest_ADSR_1_A_Shape]);
        modDestDescs.emplace_back(ModDest_ADSR_1_D_Shape, ADSR_1_D_Shape, modDestNames[ModDest_ADSR_1_D_Shape]);
        modDestDescs.emplace_back(ModDest_ADSR_1_R_Shape, ADSR_1_R_Shape, modDestNames[ModDest_ADSR_1_R_Shape]);
        modDestDescs.emplace_back(ModDest_ADSR_2_A_Duration, ADSR_2_A_Duration, modDestNames[ModDest_ADSR_2_A_Duration]);
        modDestDescs.emplace_back(ModDest_ADSR_2_H_Duration, ADSR_2_H_Duration, modDestNames[ModDest_ADSR_2_H_Duration]);
        modDestDescs.emplace_back(ModDest_ADSR_2_D_Duration, ADSR_2_D_Duration, modDestNames[ModDest_ADSR_2_D_Duration]);
        modDestDescs.emplace_back(ModDest_ADSR_2_S_Amount, ADSR_2_S_Amount, modDestNames[ModDest_ADSR_2_S_Amount]);
        modDestDescs.emplace_back(ModDest_ADSR_2_R_Duration, ADSR_2_R_Duration, modDestNames[ModDest_ADSR_2_R_Duration]);
        modDestDescs.emplace_back(ModDest_ADSR_2_A_Shape, ADSR_2_A_Shape, modDestNames[ModDest_ADSR_2_A_Shape]);
        modDestDescs.emplace_back(ModDest_ADSR_2_D_Shape, ADSR_2_D_Shape, modDestNames[ModDest_ADSR_2_D_Shape]);
        modDestDescs.emplace_back(ModDest_ADSR_2_R_Shape, ADSR_2_R_Shape, modDestNames[ModDest_ADSR_2_R_Shape]);
        const std::array mathVars = {
            "x",
            "a",
            "m",
            "v",
            "p",
            "n"
        };
        for (size_t i = 0; i < this->varNames.size(); i++) {
            this->varNames[i] = i < mathVars.size() ? mathVars[i] : "";
        }
    }

    /**
    * UnshapeEnvTimeBaseParam
    * @param d: the shaped envelope parameter
    * @return the unshaped envelope parameter
    * WARNING: Slow! Only use this for initialization purposes! 
    */
    static double UnshapeEnvTimeBaseParam(double d) {
        /**
        * UGLY: we don't have the inverse function of ShapeLogLikeSIMD.
        * So we try to find p for ShapeLogLike(p) == d using a binary search
        */
        alignas(64) float envParamVals[8]{};
        alignas(64) float envParamValsScaled[8]{};
        for (int i = 0; i < 8; i++) {
            envParamVals[i] = static_cast<float>(i / (7.0));
        }
        auto maxIt = 25;
        double closestVal = 1e9;
        while (maxIt--) {
            ShapeLogLikeSIMD<float, 8>(envParamVals, envParamValsScaled);
            double closestDiff = 1e9;
            int closestIdx = -1;
            for (int i = 0; i < 8; i++) {
                double diff = (envParamValsScaled[i] - d);
                if (std::abs(diff) < std::abs(closestDiff)) {
                    closestDiff = diff;
                    closestIdx = i;
                }
            }
            if (closestIdx == -1) {
                break;
            }
            if (std::abs(closestDiff) < 1e-6) {
                closestVal = envParamVals[closestIdx];
                break;
            }
            closestVal = envParamVals[closestIdx];
            int32_t firstIdx = closestIdx == 0 ? 0 : (closestDiff > 0 ? closestIdx - 1 : closestIdx);
            int32_t lastIdx = closestIdx == 7 ? 7 : (closestDiff > 0 ? closestIdx : closestIdx + 1);
            double firstVal = envParamVals[firstIdx];
            double lastVal = envParamVals[lastIdx];
            double s = lastVal - firstVal;
            for (int i = 0; i < 8; i++) {
                envParamVals[i] = firstVal + (i / 7.0) * s;
            }
        }
        return closestVal;
    }
public:
    explicit SynthImplGPU(module_synth_template<SynthImplGPU>* module);

    ~SynthImplGPU()
    {
        for (auto* ptr : vecParams) {
            delete ptr;
        }
    }

    void updateProgramList() override {
        // adjust program param
        auto param = GetParamEnum(ParametersSynthGPU::Osc1Waveform);
        std::vector<String> programNames;
        for (int32_t i = 0; i < this->gpuProgram.numPrograms; i++) {
            programNames.push_back(this->gpuProgram.programDescs[i].name);
        }
        param->setStrings(programNames.begin(), programNames.end());
        auto regParam = moduleSynthInstance->getParam(PARAM_OFFSET_IMPL + static_cast<size_t>(Parameters::Osc1Waveform));
        auto intParam = dynamic_cast<SynthParam_Int*>(param);
        if (regParam && intParam) {
            regParam->quantizationSteps = intParam->iMax - intParam->iMin + 1;
        }
    }

    void setBlocksize(blocksize_t blockSize) override {
        SynthImpl::setBlocksize(blockSize);
        GlfwContextSwitch ctxSwitch(window);
        if (!gpuProgram.is_valid() || gpuProgram.blocksize != blockSize) {
            reloadShader({blockSize, NUM_AUDIO_CHANNELS, NUM_POLY_VOICES, MAX_UNISON_VOICES});
        }
        ssboInputSynthState.buffer.resize(blockSize * NUM_SYNTH_INPUT_PARAMETERS);
        ssboInputVoiceStates.buffer.resize(blockSize * NUM_POLY_VOICES * NUM_VOICE_INPUT_PARAMETERS);
        ssboOutput.buffer.resize(blockSize * gpuProgram.channels);
        ssboOutputWaveform.buffer.resize(blockSize);
        GPUAudioProcessor::reallocateSSBOs();
    }

    void init() override {
        for (size_t i = 0; i < lfoParameters.size(); i++) {
            auto& params = lfoParameters[i];
            using namespace PluginLFO;
            params.shape = {};
            params.shape.name = "LFO " + std::to_string(i + 1) + " Shape";
            params.shape.pts = DAW::Shape::GetShape(DAW::Shape::ShapeWaveform::SHAPE_TRIANGLE);
            params.shape.flags = DAW::Shape::ShapeFlags::SHAPE_CYCLIC | DAW::Shape::ShapeFlags::SHAPE_SHAPED;
            params.syncFlags = STRAIGHT | DOTTED | TRIPLET;
            params.syncFlags = 0;
            params.syncRatios = GetSyncRatios(params.syncFlags);
            params.modeIsShape = true;
            updateLFOParameters(params, i);
        }
        for (size_t i = 0; i < lfosSongPos.size(); i++) {
            lfosSongPos[i].setParameters(&lfoParameters[i], i);
        }
        for (size_t voiceIdx = 0; voiceIdx < voices.size(); voiceIdx++) {
            auto& v = this->voices[voiceIdx];
            for (size_t i = 0; i < v.lfos.size(); i++) {
                v.lfos[i].setParameters(&lfoParameters[i], voiceIdx * 256 + i);
            }
            updateEnvelopeParameters(v);
        }
        if (!assert_expr(initComputeContext())) {
            return;
        }
        // auto blockSize = moduleSynthInstance->getSampleFormat().blockSize;
        // if (blockSize == 0) {
        //     blockSize = 512;
        // }
        // reloadShader({blockSize, NUM_AUDIO_CHANNELS, NUM_POLY_VOICES, MAX_UNISON_VOICES});

        timeCheckShader = getTimeMillis();
        timePerfLog = getTimeMillis();

        auto sampleRate = moduleSynthInstance->getSampleFormat().sampleRate;
        if (!sampleRate) sampleRate = 44100;
        // print inputBuffer size in mega bytes
        const auto inputSizePerSample = (ssboInputSynthState.buffer.size() + ssboInputVoiceStates.buffer.size()) / gpuProgram.blocksize;
        log_lf(Log::L_INFO, "inputBuffer size: %f MB\n", inputSizePerSample * gpuProgram.blocksize * sizeof(float) / 1024.0 / 1024.0);
        const auto inputPerSec = inputSizePerSample * sampleRate * sizeof(float) / 1024.0 / 1024.0;
        // print required input bandwith
        log_lf(Log::L_INFO, "inputBuffer bandwidth: %f MB/s\n", inputPerSec);

        // also print output buffer size and bandwidth
        const auto outputSizePerSample = 3;
        log_lf(Log::L_INFO, "outputBuffer size: %f MB\n", outputSizePerSample * gpuProgram.blocksize * sizeof(float) / 1024.0 / 1024.0);
        const auto outputPerSec = outputSizePerSample * sampleRate * sizeof(float) / 1024.0 / 1024.0;
        log_lf(Log::L_INFO, "outputBuffer bandwidth: %f MB/s\n", outputPerSec);

        for (auto param : vecParams) {
            if (!param) continue;
            OnParamChange(static_cast<Parameters>(param->enumParam));
        }
    }

    std::array<double, 1>& getOtherParams() {
        return otherParams;
    }

    LFOParameters& getLFOParams(int32_t lfoIdx) {
        return lfoParameters[lfoIdx];
    }

    SynthLfo& getGlobalLFO(int32_t lfoIdx) {
        return lfosSongPos[lfoIdx];
    }

    int32_t getSyncFlags(int32_t chIdx) const {
        dbgassert(chIdx >= 0 && chIdx < CtrSize(lfoParameters));
        return lfoParameters[chIdx].syncFlags;
    }

    void setSyncFlags(int32_t chIdx, int32_t flags) {
        dbgassert(chIdx >= 0 && chIdx < CtrSize(lfoParameters));
        lfoParameters[chIdx].syncFlags = flags;
        lfoParameters[chIdx].syncRatios = PluginLFO::GetSyncRatios(flags);
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
            case Parameters::Osc1Waveform:
                currentProgramId = static_cast<int32_t>(value);
                break;
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
            default:
                break;
        }
    }

    void StartVoice(VoiceSynth& v, bool bTriggerMono) {
        v.Start(bTriggerMono);
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
            bool bHasNoteDaw = message.note.has_value();
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
            static int32_t maxPolyCountSeen = 0;
            int32_t polyCount = std::count_if(std::cbegin(voices), std::cend(voices), [](auto& v) { return v.bIsActive; });
            if (polyCount > maxPolyCountSeen) {
                log_lf(Log::L_WARN, "Max poly count seen: %d\n", polyCount);
                maxPolyCountSeen = polyCount;
            }
            if (!polyCount) {
                auto hostInfo = moduleSynthInstance->getHostCallback();
                gpuContext.time_sample_phase_reset = hostInfo->m_vstTimeInfo.samplePos;
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
                                    if (voice.noteT.pitch == noteDaw.pitch
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
                            voice->SetNote(noteDaw);
                            voice->SetVelocity(velocity);
                            voice->ResetPitch();
                            voice->Start(false);
                            // voice->seqNr = seq++;
                            break;
                        }
                        default:
                        case VoiceModes::Mono:
                            voices[0].SetNote(noteDaw);
                            voices[0].SetVelocity(velocity);
                            voices[0].Start(true);
                            // voices[0].seqNr = 1;
                            break;
                        case VoiceModes::Legato:
                            voices[0].SetNote(noteDaw);
                            if (heldNotes.empty()) {
                                voices[0].SetVelocity(velocity);
                                voices[0].ResetPitch();
                                voices[0].Start(true);
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

    samplecount_t getLatency() override { return gpuProgram.blocksize * (ssboOutput.ssbo.size() - 1); }

    void updateEnvelopeParameters(VoiceSynth& v) {
        alignas(64) float envParamVals[8]{};
        alignas(64) float envParamValsScaled[8]{};
        envParamVals[0] = GetParamFloat(Parameters::ADSR_1_A_Duration)->getAsDoubleModulated(v.modValues[ModDestinations::ModDest_ADSR_1_A_Duration]);
        envParamVals[1] = GetParamFloat(Parameters::ADSR_1_H_Duration)->getAsDoubleModulated(v.modValues[ModDestinations::ModDest_ADSR_1_H_Duration]);
        envParamVals[2] = GetParamFloat(Parameters::ADSR_1_D_Duration)->getAsDoubleModulated(v.modValues[ModDestinations::ModDest_ADSR_1_D_Duration]);
        envParamVals[3] = GetParamFloat(Parameters::ADSR_1_R_Duration)->getAsDoubleModulated(v.modValues[ModDestinations::ModDest_ADSR_1_R_Duration]);
        envParamVals[4] = GetParamFloat(Parameters::ADSR_2_A_Duration)->getAsDoubleModulated(v.modValues[ModDestinations::ModDest_ADSR_2_A_Duration]);
        envParamVals[5] = GetParamFloat(Parameters::ADSR_2_H_Duration)->getAsDoubleModulated(v.modValues[ModDestinations::ModDest_ADSR_2_H_Duration]);
        envParamVals[6] = GetParamFloat(Parameters::ADSR_2_D_Duration)->getAsDoubleModulated(v.modValues[ModDestinations::ModDest_ADSR_2_D_Duration]);
        envParamVals[7] = GetParamFloat(Parameters::ADSR_2_R_Duration)->getAsDoubleModulated(v.modValues[ModDestinations::ModDest_ADSR_2_R_Duration]);
        ShapeLogLikeSIMD<float, 8>(envParamVals, envParamValsScaled);
        for (auto& f : envParamValsScaled) {
            f = Envelope::GetTimeBaseFromParam(f);
        }
        float envSusShape[8]{};
        envSusShape[0] = GetParamFloat(Parameters::ADSR_1_S_Amount)->getAsDoubleModulated(v.modValues[ModDestinations::ModDest_ADSR_1_S_Amount]);
        envSusShape[1] = GetParamFloat(Parameters::ADSR_1_A_Shape)->getAsDoubleModulated(v.modValues[ModDestinations::ModDest_ADSR_1_A_Shape]);
        envSusShape[2] = GetParamFloat(Parameters::ADSR_1_D_Shape)->getAsDoubleModulated(v.modValues[ModDestinations::ModDest_ADSR_1_D_Shape]);
        envSusShape[3] = GetParamFloat(Parameters::ADSR_1_R_Shape)->getAsDoubleModulated(v.modValues[ModDestinations::ModDest_ADSR_1_R_Shape]);
        envSusShape[4] = GetParamFloat(Parameters::ADSR_2_S_Amount)->getAsDoubleModulated(v.modValues[ModDestinations::ModDest_ADSR_2_S_Amount]);
        envSusShape[5] = GetParamFloat(Parameters::ADSR_2_A_Shape)->getAsDoubleModulated(v.modValues[ModDestinations::ModDest_ADSR_2_A_Shape]);
        envSusShape[6] = GetParamFloat(Parameters::ADSR_2_D_Shape)->getAsDoubleModulated(v.modValues[ModDestinations::ModDest_ADSR_2_D_Shape]);
        envSusShape[7] = GetParamFloat(Parameters::ADSR_2_R_Shape)->getAsDoubleModulated(v.modValues[ModDestinations::ModDest_ADSR_2_R_Shape]);

        for (size_t i = 0; i < 2; ++i) {
            v.envelopes[i].a = envParamValsScaled[i * 4 + 0];
            v.envelopes[i].h = envParamValsScaled[i * 4 + 1];
            v.envelopes[i].d = envParamValsScaled[i * 4 + 2];
            v.envelopes[i].r = envParamValsScaled[i * 4 + 3];
            v.envelopes[i].s = envSusShape[i * 4 + 0];
            v.envelopes[i].shapes[0] = envSusShape[i * 4 + 1];
            v.envelopes[i].shapes[1] = envSusShape[i * 4 + 2];
            v.envelopes[i].shapes[2] = envSusShape[i * 4 + 3];
        }
    }
    void updateLFOParameters(LFOParameters& p, int32_t lfoIdx) {
        using P = Parameters;
        const double lfoFreq = vecParams[P::LFO_1_Frequency + lfoIdx * MAX_PARAMS_PER_LFO]->getAsDoubleModulated();
        p.bpm = gpuContext.bpm;
        p.freq = lfoFreq;
        p.freqHz = p.paramToFreqHz(lfoFreq);
        p.phaseOffset = vecParams[P::LFO_1_Phase + lfoIdx * MAX_PARAMS_PER_LFO]->getAsDoubleModulated();
        p.rampDuration = vecParams[P::LFO_1_RampDuration + lfoIdx * MAX_PARAMS_PER_LFO]->getAsDoubleModulated();
        p.trigger = GetParamEnum(static_cast<P>(P::LFO_1_TriggerMode + lfoIdx * MAX_PARAMS_PER_LFO))->getEnumValue<LFOParameters::LFOTriggerMode>();
    }

    void updateVoiceModulations(VoiceSynth& v) {

        auto& voiceModulations = v.modValues;
        std::memset(voiceModulations.data(), 0, voiceModulations.size() * sizeof(double));
        ModulationSourceData modSrcData{};
        modSrcData[0] = 0.0; // input value
        modSrcData[1] = v.GetVolumeEnvelope().value;
        modSrcData[2] = v.GetFilterEnvelope().value;
        modSrcData[3] = v.velocity;
        modSrcData[4] = noteToLinearScale(v.noteT.pitch);
        modSrcData[5] = v.noteT.pitch / 127.0;
        modSrcData[6] = getVoiceLfoValue(v, 0) * v.lfos[0].GetRamp();
        modSrcData[7] = getVoiceLfoValue(v, 1) * v.lfos[1].GetRamp();
        modSrcData[8] = getVoiceLfoValue(v, 2) * v.lfos[2].GetRamp();
        ProcessModulations(modSrcData, voiceModulations);
    }
    double getVoiceLfoValue(const VoiceSynth& v, int32_t lfoIdx) {
        auto bIsSongSync = lfoParameters[lfoIdx].trigger == LFOParameters::LFOTriggerMode::SongPosition;
        if (bIsSongSync) {
            return lfosSongPos[lfoIdx].GetLfo();
        }
        return v.lfos[lfoIdx].GetLfo();
    }

    void processGpuSynth(float * const * outputs, int nFrames, const DAW::Host::Host* const host, double tick, playback_state state) {
        const bool bIsBenchmark = gDebugBenchmarkFlags & 1;
        const bool bDbgSkipBufferBuild = gDebugBenchmarkFlags & 2;
        const bool bDbgSkipGPUDispatch = gDebugBenchmarkFlags & 4;
        const bool bDbgSkipAll = gDebugBenchmarkFlags & 8;
        if (bDbgSkipAll) {
            return;
        }
        if (!glad_glDispatchCompute) {
            return;
        }
        const auto blockSize = moduleSynthInstance->getSampleFormat().blockSize;
        if (nFrames != blockSize) {
            return;
        }
        GlfwContextSwitch ctxSwitch(window);
        if (!gpuProgram.is_valid()) {
            gpuProgram.destroy();
            gpuProgram = {};
        }
        auto tmNow_ms = getTimeMillis();
        if (!bIsBenchmark && (!gpuProgram.is_valid() || tmNow_ms - timeCheckShader > 1000)) {
            if (tmNow_ms - timeLastShaderError > 2000) {
                timeCheckShader = tmNow_ms;
                reloadShader({blockSize, NUM_AUDIO_CHANNELS, NUM_POLY_VOICES, MAX_UNISON_VOICES});
            }
        }

        const int nOversample = 1;
        const auto bpm100 = host->prjGlobals.tempo100;
        int framesPerAutomationUpdate = state == playback_state::status_render ? 1 : 8;

        const int32_t programId = currentProgramId % gpuProgram.programs.size();
        const auto sampleRate = moduleSynthInstance->format.sampleRate;

        const auto hostInfo = moduleSynthInstance->getHostCallback();
        const auto samplePos = hostInfo->m_vstTimeInfo.samplePos;
        gpuContext.bpm = host->prjGlobals.tempo100 / 100.0;
        gpuContext.one_over_samplerate = 1.0 / sampleRate;
        gpuContext.time_samples = samplePos;
        gpuContext.time_seconds = samplePos * gpuContext.one_over_samplerate;
        gpuContext.time_beats = hostInfo->m_vstTimeInfo.ppqPos;
        gpuContext.osc1_unison_voice_count = GetParamInt(Parameters::Osc1UnisonVoiceCount)->Value();
        gpuContext.osc1_unison_detune = GetParamFloat(Parameters::Osc1UnisonDetune)->Value();
        gpuContext.osc1_filter = GetParamFloat(Parameters::Osc1Filter)->getAsDoubleModulated();
        gpuContext.osc1_stereo = GetParamFloat(Parameters::Osc1Stereo)->getAsDoubleModulated();
        gpuContext.osc1_pw = GetParamFloat(Parameters::Osc1PulseWidth)->getAsDoubleModulated();
        gpuContext.osc1_pw_mod_rate = GetParamFloat(Parameters::Osc1PulseWidthModRate)->getAsDoubleModulated();
        gpuContext.osc1_pw_mod_depth = GetParamFloat(Parameters::Osc1PulseWidthModDepth)->getAsDoubleModulated();
        gpuContext.osc1_filter_keytrack = GetParamFloat(Parameters::Osc1KeytrackFilter)->getAsDoubleModulated();
        gpuContext.osc1_detune_keytrack = GetParamFloat(Parameters::Osc1KeytrackDetune)->getAsDoubleModulated();
        gpuContext.osc1_width_keytrack = GetParamFloat(Parameters::Osc1KeytrackStereoWidth)->getAsDoubleModulated();

        double osc1_filter = 0.0;
        double osc1_filter_keytrack = 0.0;
        double osc1_stereo = 0.0;

        auto& inputBufferSynthState = ssboInputSynthState.buffer;
        auto& inputBufferVoiceStates = ssboInputVoiceStates.buffer;
        ssboInputSynthState.clearBuffer();
        ssboInputVoiceStates.clearBuffer();
        const bool bHasAutomationOrModulation = true; // TODO: implement
        if (bDbgSkipBufferBuild) {
            midiQueue.Clear();
        }
        for (size_t i = 0; i < lfosSongPos.size(); i++) {
            auto& lfoSongPos = lfosSongPos[i];
            lfoSongPos.setPhase(0.0);
            lfoSongPos.Update(oneOverSR*samplePos);
        }
        for (int s = 0; !bDbgSkipBufferBuild && s < gpuProgram.blocksize; s++) {
            if (s % nOversample == 0) {
                FlushMidi(s / nOversample);
            }
            if (host && moduleSynthInstance && (s % framesPerAutomationUpdate) == 0) {
                ReadAutomation(host, tick, state, s, nFrames, nOversample);
            }
            for (size_t i = 0; i < lfoParameters.size(); i++) {
                updateLFOParameters(lfoParameters[i], i);
            }
            if (bHasAutomationOrModulation || s == 0) {
                // osc1_filter = GetParamFloat(Parameters::Osc1Filter)->getAsDoubleModulated();
                osc1_stereo = GetParamFloat(Parameters::Osc1Stereo)->getAsDoubleModulated();
                osc1_filter_keytrack = GetParamFloat(Parameters::Osc1KeytrackFilter)->getAsDoubleModulated();
            }
            const float masterVolume = GetParamFloat(Parameters::MasterVolume)->getAsDoubleModulated();
            float masterGain = 0.0;
            dsp_util::getGainLvl(masterVolume, masterGain);

            inputBufferSynthState[s + gpuProgram.blocksize * 0] = osc1_filter_keytrack;
            inputBufferSynthState[s + gpuProgram.blocksize * 1] = osc1_stereo;

            const auto coarse = GetParamInt(Parameters::Osc1Coarse)->Value();
            const auto fine   = GetParamFloat(Parameters::Osc1Fine)->Value();
            const auto tickPos = tick + sampleToTickConvert<double, roundmode::none>(s, bpm100, host->m_sampleFormatInternal.sampleRate * nOversample);
            for (size_t i = 0; i < lfosSongPos.size(); i++) {
                lfosSongPos[i].Update(oneOverSR);
            }
            for (size_t i = 0; i < NUM_POLY_VOICES; i++) {
                const auto idx_base = i * (NUM_VOICE_INPUT_PARAMETERS * gpuProgram.blocksize);
                const auto idx_velocity = idx_base + s;
                auto& v = voices[i];
                float velocity = -1.0;
                if (v.bIsActive) {
                    for (size_t l = 0; l < v.lfos.size(); l++) {
                        auto& lfo = v.lfos[l];
                        lfo.Update(oneOverSR);
                    }
                    updateVoiceModulations(v);
                    updateEnvelopeParameters(v);
                    // bool bIsFirst = v.GetVolumeEnvelope().stage == EnvelopeStages::Triggered;
                    for (auto& env : v.envelopes) {
                        env.Update(oneOverSR);
                    }
                    osc1_filter = GetParamFloat(Parameters::Osc1Filter)->getAsDoubleModulated() + v.modValues[ModDestinations::ModDest_Osc1Filter];
                    const auto osc1Tune = pitchFactor(coarse + fine);
                    const auto baseFrequency = v.frequency * v.pitchBend;
                    const auto osc1Frequency = osc1Tune * baseFrequency;
                    if (v.bIsActive) {
                        float volEnv = v.GetVolumeEnvelope().value;
                        if (v.noteT.len > 0 && otherParams[0] > 0.0f) {
                            const float noteProgress = v.noteT.end() - tickPos;
                            const float fNoteFadeDurationTicks = 64.0f;
                            float fFadeIn = math::smoothstep(math::clamp(noteProgress / fNoteFadeDurationTicks, 0.0f, 1.0f));
                            float fFadeOut = math::smoothstep(math::clamp((v.noteT.len - noteProgress) / fNoteFadeDurationTicks, 0.0f, 1.0f));
                            float noteFade = 1.0 + (fFadeIn * fFadeOut - 1.0) * otherParams[0];
                            volEnv *= noteFade;
                        }
                        velocity = masterGain * (v.velocity + v.modValues[ModDestinations::ModDest_Osc1Volume]) * volEnv;
                    }
                    const auto idx_pitch    = idx_base + gpuProgram.blocksize * 1 + s;
                    const auto idx_filter   = idx_base + gpuProgram.blocksize * 2 + s;
                    inputBufferVoiceStates[idx_pitch]    = osc1Frequency;
                    inputBufferVoiceStates[idx_filter]   = 1.0-osc1_filter;
                    v.bIsActive = v.isVoiceActive();
                    // if (bIsFirst) {
                    //     double firstSample = velocity;
                    //     dbgassert(firstSample == 0.0);
                    // }
                    // if (!v.bIsActive) {
                    //     double lastSample = velocity;
                    //     dbgassert(lastSample == 0.0);
                    // }
                }
                inputBufferVoiceStates[idx_velocity] = velocity;
            }
        }

        perfTimer.reset();
        if (!bDbgSkipGPUDispatch) {
            ssboInputSynthState.uploadBuffer();
            ssboInputVoiceStates.uploadBuffer();
            checkGLError("glBufferData");

            glBindBuffer(GL_UNIFORM_BUFFER, ubo);
            checkGLError("glBindBuffer");
            glBufferData(GL_UNIFORM_BUFFER, sizeof(gpuContext), &gpuContext, GL_STREAM_DRAW);
            checkGLError("glBufferData");
            glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssboInputSynthState.ssbo.current());
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssboInputVoiceStates.ssbo.current());
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ssboOutput.ssbo.current());
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, ssboOutputWaveform.ssbo.current());
            glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
            checkGLError("glBindBufferBase");
        }

        if (gpuProgram.programs[programId] && !bDbgSkipGPUDispatch) {
            glUseProgram(gpuProgram.programs[programId]);
            checkGLError("glUseProgram");
            glDispatchCompute(1, 1, 1);
            checkGLError("glDispatchCompute");
        }

        if (gpuProgram.programsWaveform[programId] && !bDbgSkipGPUDispatch) {
            glUseProgram(gpuProgram.programsWaveform[programId]);
            checkGLError("glBufferData");
            glDispatchCompute(1, 1, 1);
        }
        if (!bDbgSkipGPUDispatch) {
            glFinish();
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT|GL_BUFFER_UPDATE_BARRIER_BIT);
        }
        if (gpuProgram.programs[programId] && !bDbgSkipGPUDispatch) {
            ssboOutput.downloadBuffer();
        } else {
            ssboOutput.clearBuffer();
        }
        if (gpuProgram.programsWaveform[programId] && !bDbgSkipGPUDispatch) {
            ssboOutputWaveform.downloadBuffer();
        } else {
            ssboOutputWaveform.clearBuffer();
        }

        for (auto* buffer : hostBuffers) {
            buffer->incrementFrame();
        }

        const auto& outputBuffer = ssboOutput.buffer;
        // copy nFrames to outputs
        for (samplecount_t ch = 0; ch < NUM_AUDIO_CHANNELS; ch++) {
            for (samplecount_t sampleIdx = 0; sampleIdx < nFrames; sampleIdx++) {
                float val = outputBuffer[sampleIdx + ch * nFrames];
                float hardClipAt = 2.5;
                if (fp_math::isNanOrInfd(val)) {
                    val = 0;
                } else if (val < -hardClipAt) {
                    val = -hardClipAt;
                } else if (val > hardClipAt) {
                    val = hardClipAt;
                }
                outputs[ch][sampleIdx] = val;
            }
        }
        if (bIsBenchmark) {
            return;
        }
        tmNow_ms = getTimeMillis();
        auto tmTotal_ms = perfTimer.getTimeDoubleReset() * 1000.0;
        if (tmNow_ms - timePerfLog >= 10000 || tmTotal_ms > timeComputeAvg * 10.0) {
            if (timeComputeAvg < 0.0) {
                if (tmNow_ms - timePerfLog > 1500)
                    timeComputeAvg = tmTotal_ms;
            } else {
                log_lf(Log::L_WARN, "gpu_compute_test: %f ms (avg: %f ms)\n", tmTotal_ms, timeComputeAvg);
                // print sample 0
                auto sample0 = outputBuffer[0];
                log_lf(Log::L_WARN, "sample 0: %f\n", sample0);
            }
            timePerfLog = tmNow_ms;
        }
        timeComputeAvg = 0.95 * timeComputeAvg + 0.05 * tmTotal_ms;
    }

    void ProcessSynth(AudioBlock* in, float * const * outputs, int nFrames, const DAW::Host::Host* const host, double tick, playback_state state) override {
        auto lock = this->lockProcessing();
        processGpuSynth(outputs, nFrames, host, tick, state);
    }

    std::shared_ptr<PluginViewContainer> createViewCtrImpl() override;

    bool getSnapshot(snapshot_t& snapshot) const {
        snapshot.version     = SYNTH_GPU_SNAPSHOT_VERSION;
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
                auto inputSrcType = math::clamp<int32_t>(input.src, 0, NUM_MODULATION_SOURCES - 1);
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
        const auto numLfos = CtrSize(lfoParameters);
        snapshot.modulations.reserve(numLfos);
        for (int32_t i = 0; i < numLfos; ++i) {
            const auto& lfo = lfoParameters[i];
            auto shapeSnapshot = DAW::Shape::shape_snapshot_t{ 0, DAW::Shape::shape_preset_t{2, lfo.shape} };
            snapshot.lfos.emplace_back(std::move(shapeSnapshot), lfo.modeIsShape, lfo.randomModeId, lfo.syncFlags);
        }
        return true;
    }

    bool setSnapshot(const snapshot_t& snapshot) {
        if (snapshot.version != SYNTH_GPU_SNAPSHOT_VERSION) {
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
                log_lf(Log::L_WARN, "Invalid param index %d\n", ps.paramIdx);
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
                                                    input.srcIdx,
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

        for (size_t i = 0; i < this->lfoParameters.size() && i < snapshot.lfos.size(); i++) {
            auto& lfoParams = lfoParameters[i];
            const auto& lfoSnapshot = snapshot.lfos[i];
            lfoParams.modeIsShape = lfoSnapshot.modeIsShape;
            lfoParams.randomModeId = lfoSnapshot.randomModeId;
            lfoParams.syncFlags = lfoSnapshot.syncFlags;
            lfoParams.shape = lfoSnapshot.shape.shape.curve;
            lfoParams.shape.flags = DAW::Shape::SHAPE_SHAPED|DAW::Shape::SHAPE_CYCLIC;
        }

        for (auto& param : vecParams) {
            if (!param) continue;
            OnParamChange(static_cast<Parameters>(param->enumParam));
        }
        for (auto& v : voices) {
            updateEnvelopeParameters(v);
        }
        return true;
    }

    void onPresetLoaded() {
        for (size_t i = 0; i < lfoParameters.size(); i++) {
            auto& lfoParams = lfoParameters[i];
            lfoParams.syncRatios = PluginLFO::GetSyncRatios(lfoParams.syncFlags);
            getGlobalLFO(i).setRandomMode(lfoParams.randomModeId);
            for (auto& v : voices) {
                v.lfos[i].setRandomMode(lfoParams.randomModeId);
            }
        }
    }
};

class module_synth_gpu final : public module_synth_template<SynthImplGPU> {
public:
    explicit module_synth_gpu(int32_t _projectGlobalId, IHostCallback* _hostCallback)
        : module_synth_template<SynthImplGPU>(new SynthType(this), "Synth GPU", _projectGlobalId, _hostCallback)
    {
        bCanReceiveMidi = true;
        isSynth = true;
        for (const auto& paramEntry : vecParams) {
            if (!paramEntry)
                continue;
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
            using P = ParametersSynthGPU;
            switch (paramEntry->enumParam) {
                case P::Osc1Fine:
                case P::Osc1Coarse:
                case P::Osc1UnisonDetune:
                case P::Osc1PulseWidth:
                    regparam->isBiPolar = true;
                    break;
                default:
                    break;
            }
        }
        impl->init();
    }

    ~module_synth_gpu() override {
        delete impl;
    }

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
                param->setAll(vecParams[idx]->getAsDouble());
            }
        }
        getSynth()->onPresetLoaded();
    }
    void getUiSnapshot(snapshot_t& snapshot);
    void setUiSnapshot(snapshot_t& snapshot);
    
    param_unit_t convertParamValueToDisplay(int32_t idx, float value) override {
        const auto idxInternal = idx - PARAM_OFFSET_IMPL;
        if (isValidParamIdx(idxInternal)) {
            SynthParamBase* param = vecParams[idxInternal];
            if (param) {
                static constexpr auto enumDurList = {
                    ADSR_1_A_Duration,
                    ADSR_1_H_Duration,
                    ADSR_1_D_Duration,
                    ADSR_1_R_Duration,
                    ADSR_2_A_Duration,
                    ADSR_2_H_Duration,
                    ADSR_2_D_Duration,
                    ADSR_2_R_Duration,
                    LFO_1_RampDuration,
                    LFO_2_RampDuration
                };
                if (std::find(enumDurList.begin(), enumDurList.end(), param->enumParam) != enumDurList.end()) {
                    alignas(64) float envParamVals[4]{};
                    alignas(64) float envParamValsScaled[4]{};
                    envParamVals[0] = value;
                    ShapeLogLikeSIMD<float, 4>(envParamVals, envParamValsScaled);
                    auto ms = Envelope::GetSecondsFromParam(envParamValsScaled[0]) * 1000.0;
                    auto fmt = ms < 10.0 ? "%.3f" : (ms < 100.0 ? "%.2f" : "%.1f");
                    auto v = StringFormat(fmt, ms);
                    return { v, "ms" };
                }
                if (param->enumParam == ParametersSynthGPU::Osc1PulseWidthModRate) {
                    auto v = StringFormat("%.3f", pow(2.0, value * 21.0) * 0.01);
                    return { v, "Hz" };
                }
                if ((param->enumParam == ParametersSynthGPU::LFO_1_Frequency || param->enumParam == ParametersSynthGPU::LFO_2_Frequency)) {
                    auto& lfoParams = impl->getLFOParams(param->enumParam == ParametersSynthGPU::LFO_1_Frequency ? 0 : 1);
                    if (lfoParams.syncFlags == 0) {
                        auto v = StringFormat("%.3f", pow(2.0, value * 21.0) * 0.01);
                        return { v, "Hz" };
                    }
                    auto lfoRateStr = FormatSyncRate(lfoParams.syncRatios, lfoParams.syncFlags, value);
                    return {lfoRateStr, lfoParams.syncFlags != 0 ? "" : param->unit};
                }
            }
        }
        return module_synth_template<SynthImplGPU>::convertParamValueToDisplay(idx, value);
    }

    param_converted_t convertParamValueDisplay(int32_t idx, const param_unit_t& displayValue) override {
        const auto idxInternal = idx - PARAM_OFFSET_IMPL;
        if (isValidParamIdx(idxInternal)) {
            SynthParamBase* param = vecParams[idxInternal];
            if (param->enumParam == ParametersSynthGPU::Osc1PulseWidthModRate) {
                auto fTextFieldVal = static_cast<float>(atof(StringAsCStr(displayValue.value)));
                double parsed = math::clamp(log2(fTextFieldVal * 100.0) / 21.0, 0.0, 1.0);
                return {float(parsed), true};
            }
            if (param && (param->enumParam == ParametersSynthGPU::LFO_1_Frequency || param->enumParam == ParametersSynthGPU::LFO_2_Frequency)) {
                auto& lfoParams = impl->getLFOParams(param->enumParam == ParametersSynthGPU::LFO_1_Frequency ? 0 : 1);
                if (lfoParams.syncFlags) {
                    auto numSyncRatios = CtrSize(lfoParams.syncRatios);
                    for (int32_t i = 0; i < numSyncRatios; ++i) {
                        if (lfoParams.syncRatios[i].text == displayValue.value) {
                            return {((i)/float(numSyncRatios-1)), true};
                        }
                        if (lfoParams.syncRatios[i].text == displayValue.value + "/1") {
                            return {((i)/float(numSyncRatios-1)), true};
                        }
                    }
                } else {
                    auto fTextFieldVal = static_cast<float>(atof(StringAsCStr(displayValue.value)));
                    return {math::clamp(PluginLFO::RateToParam(fTextFieldVal), 0.0f, 1.0f), true};
                }
            }
        }
        return internalplugin::convertParamValueDisplay(idx, displayValue);
    }
};

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
        const int envBase[2] = {static_cast<int32_t>(SynthImplGPU::Parameters::ADSR_1_A_Duration), static_cast<int32_t>(SynthImplGPU::Parameters::ADSR_2_A_Duration)};
        for (size_t i = 0; i < knobs.size(); ++i) {
            auto& knob = knobs[i];
            knob.setSynthParam(moduleInstance->getSynth(), envBase[idx] + i);
            knob.setFontScale(0.75f);
            knob.setAutomationRef(moduleInstance, PARAM_OFFSET_IMPL + envBase[idx] + i);
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
        auto shapeW = cs.x / 4;

        knobs[0].pos = {0, 0};
        knobs[0].size = {cs.x - shapeW, btnH};
        knobs[5].pos = {cs.x - shapeW + padding, 0};
        knobs[5].size = {cs.x - knobs[5].pos.x, btnH};
    
        knobs[1].pos = {0, knobs[0].bottom()+padding};
        knobs[1].size = {cs.x, btnH};
    
        knobs[2].pos = {0, knobs[1].bottom()+padding};
        knobs[2].size = {cs.x - shapeW, btnH};
        knobs[6].pos = {cs.x - shapeW + padding, knobs[2].top()};
        knobs[6].size = {cs.x - knobs[6].pos.x, btnH};

        knobs[3].pos = {0, knobs[2].bottom()+padding};
        knobs[3].size = {cs.x, btnH};

        knobs[4].pos = {0, knobs[3].bottom()+padding};
        knobs[4].size = {cs.x - shapeW, btnH};
        knobs[7].pos = {cs.x - shapeW + padding, knobs[4].top()};
        knobs[7].size = {cs.x - knobs[7].pos.x, btnH};
        guictr_base::layout();
    }
};

class guicontainer_plugin_synth_other_parameters final : public guictr_base {
    module_synth_gpu* const moduleInstance;
    std::array<gui_numberinput_double, 1> knobs;
    gui_numberinput_u32 debugFlags;
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
                    nrInput.setRef(&synth->getOtherParams()[0]);
                    nrInput.setLabel("Fade Note Ends");
                    add(&nrInput);
                    break;
                default:
                    dbgassert(0);
                    break;
            }
        }
        debugFlags.setLabel("Debug Flags");
        debugFlags.setRef(&gDebugBenchmarkFlags);
        add(&debugFlags);
    }
    ~guicontainer_plugin_synth_other_parameters() override {
        removeGuis();
    }
};

struct sampled_pt_t {
    vec2 pos;
};
struct sampled_curved_t {
    std::vector<sampled_pt_t> pts;
    int32_t flags = DAW::Shape::SHAPE_FLAGS_NONE;
    inline float shapeSegmentPt(float t, const sampled_pt_t& pt) const {
        return t;
    }
};
class guictr_sampled_curve_shape final : public guictr_base, public DAW::Shape::RenderShape<sampled_curved_t> {
    sampled_curved_t curveInternal;
public:
    guictr_sampled_curve_shape()
    {
        curveInternal.pts.push_back({ { 0, 0 } });
        padding = 4;
        margin = 4;
        setBackgroundRendered(true);
        setCanMouseHit(true);
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
        DAW::Shape::DrawGrid(vg, theme, {}, cs, 4, 2);
        renderShapeView(vg, theme, &curveInternal, {}, cs);
    }
    sampled_curved_t& getShape() {
        return curveInternal;
    }
    const sampled_curved_t& getShape() const {
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
            float stepPos = s / static_cast<float>(numSamples);
            std::fill(std::begin(envParamVals), std::end(envParamVals), stepPos);
            ShapeLogLikeSIMD<float, 8>(envParamVals, envParamValsScaled);
            shapeAdsrSampled.pts.push_back({{ stepPos, envParamValsScaled[0] }});
        }
        shapeAdsrSampled.flags = DAW::Shape::SHAPE_UNCLAMPPED | DAW::Shape::SHAPE_LOCK_POINTS;
        shapeAdsrControls.flags = DAW::Shape::SHAPE_UNCLAMPPED | DAW::Shape::SHAPE_SHOW_ONLY_CONTROL_POINTS;
    }
    void setShapeFromAdsr() {
        // convert synth impls outputBufferWaveform to shape
        const auto& sampleFormat = moduleInstance->getSampleFormat();
        // sample ADSR and show in second shape editor
        auto& shapeAdsrSampled = this->shapeAdsr.getShape();
        auto& shapeAdsrControls = this->shapeAdsrControls->getShape();
        shapeAdsrSampled.pts.clear();
        

        VoiceSynth voiceTmp;
        moduleInstance->getSynth()->updateEnvelopeParameters(voiceTmp);
        Envelope& envelope = voiceTmp.envelopes[idx];
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
        std::array<double, 4> durationPhaseSeconds{};
        while (maxIterations-- > 0 && !bFinished) {
            lastSample = pos + stepsize;
            for (samplecount_t s = 0; s < stepsize && !bFinished; s++) {
                auto envState = envelope.stage;
                envelope.Update(oneOverSr);
                auto envStateNew = envelope.stage;
                bool bAddPoint = s == 0;
                if (envState != envStateNew && envState == EnvelopeStages::Attack) {
                    durationPhaseSeconds[0] = (pos + s) * oneOverSr;
                    shapeAdsrControls.pts[1].pos = { (pos + s), envelope.value };
                    bAddPoint = true;
                }
                if (envState != envStateNew && envState == EnvelopeStages::Hold) {
                    durationPhaseSeconds[1] = (pos + s) * oneOverSr;
                    shapeAdsrControls.pts[2].pos = { (pos + s), envelope.value };
                    bAddPoint = true;
                }
                if (envelope.IsSustain()) {
                    durationPhaseSeconds[2] = (pos + s) * oneOverSr;
                    shapeAdsrControls.pts[3].pos = { (pos + s), envelope.value };
                    envelope.Release();
                    bAddPoint = true;
                } else if(envelope.IsIdle()) {
                    durationPhaseSeconds[4] = (pos + s) * oneOverSr;
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
            auto waveform = static_cast<ShapeWaveform>(shapeIdx);
            auto lock = synth->lock();
            auto& params = synth->getLFOParams(channel);
            params.shape.pts = GetShape(waveform);
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

class guictr_module_synth_lfo_container final : public guictr_base {
    module_synth_gpu* const moduleInstance;
    int32_t lfoIdx;
    guictr_sampled_curve_shape sampledShaped;
    i_ctr_shape_editor* const shapeEditor;
    guictr_base* lfoShapeCtr;
public:
    explicit guictr_module_synth_lfo_container(module_synth_gpu* module, int32_t _idx) 
        : moduleInstance(module), lfoIdx(_idx),
        shapeEditor(makeShapeEditor())
    {
        padding = 0;
        margin = 0;
        setLayoutMode(autolayout_mode::LAYOUT_STACK);
        setBackgroundRendered(false);
        setBackgroundRenderedInset(false);
        setCanMouseHit(false);
        auto synth = moduleInstance->getSynth();
        shapeEditor->setShapeEditorShapeRef(&synth->getLFOParams(lfoIdx).shape);
        shapeEditor->setShapeEditorCallback([synth, _idx](const DAW::Shape::shape_t& shape, bool bIsDragMove) -> void {
            auto lock = synth->lock();
            auto& synthShape = synth->getLFOParams(_idx).shape;
            synthShape.pts = shape.pts;
            synthShape.eraseDuplicates();
        });
        lfoShapeCtr = shapeEditor->getGuiContainer();
        lfoShapeCtr->setBackgroundRendered(false);
        lfoShapeCtr->setBackgroundRenderedInset(false);
        lfoShapeCtr->setCanMouseHit(false);
        add(lfoShapeCtr);
        add(&sampledShaped);
    }
    ~guictr_module_synth_lfo_container() override {
        removeGuis();
        delete shapeEditor->getGuiContainer();
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
    void layout() override {
        lfoShapeCtr->setVisible(true);
        sampledShaped.setVisible(true);
        guictr_base::layout();
        setVisibleCtr(moduleInstance->getSynth()->getLFOParams(lfoIdx).modeIsShape);
    }
    void setVisibleCtr(bool bIsEditor) {
        lfoShapeCtr->setVisible(bIsEditor);
        sampledShaped.setVisible(!bIsEditor);
    }
    void prerender(NVGcontext* vg) override {
        guictr_base::prerender(vg);
        auto synth = moduleInstance->getSynth();
        auto& params = synth->getLFOParams(lfoIdx);
        bool bIsShowingEditor = lfoShapeCtr->isVisible();
        if (params.modeIsShape != bIsShowingEditor) {
            setVisibleCtr(params.modeIsShape);
        }
        if (!params.modeIsShape) {
            auto& shape = sampledShaped.getShape();
            auto& lfo = synth->getGlobalLFO(lfoIdx);
            auto* srcRand = lfo.getSourceRand();
            if (!srcRand) {
                shape.pts.clear();
                return;
            }
            auto numPoints = samplecount_t(math::clamp(size.x, 16, 1024));
            shape.pts.resize(numPoints);
            shape.flags |= DAW::Shape::ShapeFlags::SHAPE_LOCK_POINTS;
            auto* hostInfo = moduleInstance->getHostCallback();
            auto timeSeconds = hostInfo->m_vstTimeInfo.samplePos / hostInfo->m_vstTimeInfo.sampleRate;
            double barDurationInSeconds = toSecondsDD(TICKS_BAR, 1.0 / (hostInfo->m_vstTimeInfo.tempo * 100.0));
            double range = barDurationInSeconds * 4.0;
            double begin = timeSeconds - range;
            for (samplecount_t j = 0; j < samplecount_t(numPoints); ++j) {
                auto t = begin + double(j * range) / numPoints;
                auto v = lfo.getSourceRand()->sampleCurve(t);
                auto normalizedT = (t - begin) / float(range);
                auto& pt = shape.pts[j];
                pt.pos.x = normalizedT;
                pt.pos.y = v;
            }
        }
    }
};
class guicontainer_plugin_synth_gpu final : public guictr_base {
    module_synth_gpu* const moduleInstance;
    guictr_sampled_curve_shape shapeOscWaveform;
    guictr_module_synth_lfo_container ctrLfo1Shape;
    guictr_module_synth_lfo_container ctrLfo2Shape;
    guicontainer_plugin_synth_adsr adsr1;
    guicontainer_plugin_synth_adsr adsr2;
    guicontainer_plugin_synth_other_parameters otherParams;
    guicontainer_modulation ctrModulation;
    guictr_stacked ctrHorizontal;
    guictr_stacked ctrStackedOSC;
    guictr_stacked ctrStackedADSR;
    guictr_stacked ctrStackedLFO1;
    guictr_stacked ctrStackedLFO2;
    guictr_stacked ctrStackedBothLFOs;
    guictr_synth_param_container ctrAmpParams;
    guictr_synth_param_container ctrOscParams;
    guictr_synth_param_container ctrLfo1Params;
    guictr_synth_param_container ctrLfo2Params;
    struct _synth_gui_param_knob {
        ParametersSynthGPU param;
        guiknob_pluginparam* knob;
    };
    gui_textfield editfield;
    std::vector<_synth_gui_param_knob> vecParamUI;
    std::vector<guictr_synth_title*> containers;
    seq_rand synthRandUI;
    bool bGuiNeedsRefresh = true;
public:

    explicit guicontainer_plugin_synth_gpu(module_synth_gpu* module) 
        : moduleInstance(module),
        ctrLfo1Shape(module, 0),
        ctrLfo2Shape(module, 1),
        adsr1(module, 0),
        adsr2(module, 1),
        otherParams(module),
        ctrModulation(dynamic_cast<PluginLockable*>(module->getSynth()), module->getSynth()),
        ctrAmpParams(module->getSynth()),
        ctrOscParams(module->getSynth()),
        ctrLfo1Params(module->getSynth()),
        ctrLfo2Params(module->getSynth())
    {
        padding = 0;
        ctrStackedADSR.setVerticalLayout(true);
        ctrStackedLFO1.setVerticalLayout(true);
        ctrStackedLFO2.setVerticalLayout(true);
        ctrStackedOSC.setVerticalLayout(true);
        ctrStackedBothLFOs.setVerticalLayout(true);
        auto const synth = module->getSynth();
        auto makeParamKnob = [synth](auto p, auto knobType) {
            return new guiknob_synthparam(PARAM_OFFSET_IMPL + p, PARAM_OFFSET_IMPL + p, synth, p, knobType);
        };
        for (auto p : {
            LFO_1_Frequency, 
            LFO_1_TriggerMode,
            LFO_1_Phase,
            LFO_1_RampDuration,
        }) {
            auto knob = makeParamKnob(p, guiknob::knobtype::KNOB_LABELED);
            ctrLfo1Params.addParamKnob(knob);
            vecParamUI.push_back({ p, knob });
        }
        for (auto p : {
            LFO_2_Frequency, 
            LFO_2_TriggerMode,
            LFO_1_Phase,
            LFO_2_RampDuration,
        }) {
            auto knob = makeParamKnob(p, guiknob::knobtype::KNOB_LABELED);
            ctrLfo2Params.addParamKnob(knob);
            vecParamUI.push_back({ p, knob });
        }
        for (auto p : {
            MasterVolume, 
            VoiceMode,
        }) {
            auto knobType = p == MasterVolume ? guiknob::knobtype::SLIDER_LABELED : guiknob::knobtype::KNOB_LABELED;
            auto knob = makeParamKnob(p, knobType);
            ctrAmpParams.addParamKnob(knob);
            vecParamUI.push_back({ p, knob });
        }
        for (auto p : {
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
            ctrOscParams.addParamKnob(knob);
            vecParamUI.push_back({ p, knob });
        }
        ctrAmpParams.setLayoutMode(autolayout_mode::LAYOUT_VERTICAL);
        ctrOscParams.setLayoutMode(autolayout_mode::LAYOUT_GRID);
        ctrLfo1Params.setLayoutMode(autolayout_mode::LAYOUT_HORIZONTAL);
        ctrLfo2Params.setLayoutMode(autolayout_mode::LAYOUT_HORIZONTAL);
        auto padding = 2;
        auto margin = 2;
        ctrAmpParams.padding         = padding;
        ctrAmpParams.margin          = margin;
        ctrOscParams.padding         = padding;
        ctrOscParams.margin          = margin;
        ctrLfo1Params.padding         = padding;
        ctrLfo1Params.margin          = margin;
        ctrLfo2Params.padding         = padding;
        ctrLfo2Params.margin          = margin;
        ctrHorizontal.padding  = padding;
        ctrHorizontal.margin   = margin;
        ctrStackedOSC.padding  = padding;
        ctrStackedOSC.margin   = margin;
        ctrStackedADSR.padding = 0;
        ctrStackedADSR.margin  = 0;
        adsr2.padding     = padding;
        adsr2.margin      = margin;
        adsr1.padding     = padding;
        adsr1.margin      = margin;
        ctrStackedLFO1.padding  = padding;
        ctrStackedLFO1.margin   = margin;
        ctrStackedLFO2.padding  = padding;
        ctrStackedLFO2.margin   = margin;
        ctrModulation.padding  = padding;
        ctrModulation.margin   = margin;
        ctrStackedBothLFOs.padding  = 0;
        ctrStackedBothLFOs.margin   = 0;
        ctrAmpParams.setLabel("Amp");
        ctrAmpParams.setBackgroundRendered(true);
        ctrHorizontal.addEntry(&ctrAmpParams);
            ctrStackedOSC.addEntry(&shapeOscWaveform);
            ctrStackedOSC.addEntry(&ctrOscParams);
            ctrStackedOSC.setLabel("OSC 1");
            shapeOscWaveform.setBackgroundRendered(false);
            ctrOscParams.setBackgroundRendered(false);
            ctrStackedOSC.setBackgroundRendered(true);
        ctrHorizontal.addEntry(&ctrStackedOSC);
            adsr1.setLabel("ADSR 1");
            adsr2.setLabel("ADSR 2");
            adsr1.setBackgroundRendered(true);
            adsr2.setBackgroundRendered(true);
            otherParams.setBackgroundRendered(true);
            ctrStackedADSR.addEntry(&adsr1);
            ctrStackedADSR.addEntry(&adsr2);
            ctrStackedADSR.addEntry(&otherParams);
            ctrStackedADSR.setBackgroundRendered(false);
        ctrHorizontal.addEntry(&ctrStackedADSR);
                ctrLfo1Shape.setBackgroundRendered(false);
                ctrStackedLFO1.setBackgroundRendered(false);
                ctrStackedLFO1.addEntry(&ctrLfo1Shape);
                ctrStackedLFO1.addEntry(&ctrLfo1Params);
                ctrStackedLFO1.setLabel("LFO 1");
                ctrStackedLFO1.setBackgroundRendered(true);
            ctrStackedBothLFOs.addEntry(&ctrStackedLFO1);
                ctrLfo2Shape.setBackgroundRendered(false);
                ctrStackedLFO2.setBackgroundRendered(false);
                ctrStackedLFO2.addEntry(&ctrLfo2Shape);
                ctrStackedLFO2.addEntry(&ctrLfo2Params);
                ctrStackedLFO2.setLabel("LFO 2");
                ctrStackedLFO2.setBackgroundRendered(true);
            ctrStackedBothLFOs.addEntry(&ctrStackedLFO2);
        ctrHorizontal.addEntry(&ctrStackedBothLFOs);
            ctrModulation.setLabel("Modulation");
            ctrModulation.setBackgroundRendered(true);
        ctrHorizontal.addEntry(&ctrModulation);
        setLayoutMode(autolayout_mode::LAYOUT_HORIZONTAL);
        add(&ctrHorizontal);
        std::vector<float> splitterPositions(5);
        splitterPositions[0] = 0.05f;
        for (size_t i = 1; i < splitterPositions.size(); ++i) {
            splitterPositions[i] = (i) * 0.25f;
        }
        ctrStackedOSC.setSplitters({ 0.25f });
        ctrHorizontal.setSplitters(splitterPositions);
        ctrStackedADSR.setSplitters({ 0.45f, 0.9f });
        ctrStackedLFO1.setSplitters({ 0.6f });
        ctrStackedLFO2.setSplitters({ 0.6f });
        ctrStackedBothLFOs.setSplitters({ 0.5f });
        editfield.setFlag(FLG_NO_LAYOUT, true);
        editfield.setVisible(false);
        editfield.setAlignment(gui_textfield::Alignment::Center);
        editfield.setReturnCommits(true);
        add(&editfield);
    }
    ~guicontainer_plugin_synth_gpu() override {
        removeGuis();
        ctrHorizontal.removeEntries();
        ctrStackedOSC.removeEntries();
        ctrStackedADSR.removeEntries();
        ctrStackedLFO1.removeEntries();
        ctrStackedLFO2.removeEntries();
    }

    void layout() override {
        const auto cs = getSizeContent();
        const auto titleHeight = math::clamp(math::roundfS32(cs.y * 0.05f), 14, 32);
        for (auto& knob : vecParamUI) {
            if (knob.knob->getKnobType() == guiknob::knobtype::KNOB_LABELED) {
                knob.knob->setLabelsFontScale(1.2f, 1.2f);
                knob.knob->setLabelsScale(0.2f, 0.2f);
            }
            if (knob.knob->getKnobType() == guiknob::knobtype::SLIDER_LABELED) {
                knob.knob->setLabelsFontScale(1.2f, 1.2f);
                knob.knob->setLabelsScale(0.2f, 0.2f);
            }
        }
        ctrAmpParams.setTitleHeight(titleHeight);
        ctrOscParams.setTitleHeight(titleHeight);
        ctrLfo1Params.setTitleHeight(titleHeight);
        ctrLfo2Params.setTitleHeight(titleHeight);
        ctrModulation.setTitleHeight(titleHeight);
        ctrHorizontal.setTitleHeight(titleHeight);
        ctrStackedOSC.setTitleHeight(titleHeight);
        adsr1.setTitleHeight(titleHeight);
        adsr2.setTitleHeight(titleHeight);
        ctrStackedLFO1.setTitleHeight(titleHeight);
        ctrStackedLFO2.setTitleHeight(titleHeight);
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
        if (bGuiNeedsRefresh) {
            ctrModulation.setFromSynth();
            layout();
            bGuiNeedsRefresh = false;
        }
    }

    void onSetParameter(int32_t index, float value) {
        if (index == -1) {
            bGuiNeedsRefresh = true;
            for (auto& synthKnob : vecParamUI) {
                if (synthKnob.knob)
                    synthKnob.knob->setValueInit(moduleInstance->getSynth()->getParam(synthKnob.param)->getAsDouble());
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
                if (synthKnob.param == static_cast<ParametersSynthGPU>(internalIdx)) {
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
            int32_t idx = -1;
            if (what == &this->ctrStackedLFO1) {
                idx = 0;
            } else if (what == &this->ctrStackedLFO2) {
                idx = 1;
            }
            if (idx > -1) {
                parentCtrl->openContextMenu(new guictr_module_lfo_context_menu(moduleInstance, idx), evt.mousepos);
            }
            what = what->parent;
        }
    }

    void getSizeScale(int& w, int& h) {
        auto size = ivec2(128);
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
            float x = i / static_cast<float>(waveform.size());
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
        *w = *h = 128;
        *w = math::roundfS32(*h * 2.5f);
    }
    bool isViewSupported(int32_t uiId) const override {
        return uiId != UID_VIEW_CTR_NODES;
    }
};

std::shared_ptr<PluginViewContainer> SynthImplGPU::createViewCtrImpl() {
    if (this->moduleSynthInstance) {
        this->views.push_back(std::make_shared<PluginViewContainerSynthGPU>(static_cast<module_synth_gpu*>(this->moduleSynthInstance)));
        return this->views.back();
    }
    return nullptr;
}

SynthImplGPU::SynthImplGPU(module_synth_template<SynthImplGPU>* module)
    : SynthImpl<SynthImplGPU, ParametersSynthGPU>(module),
    moduleSynthInstance(module)
{
    initImpl();
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

template<>
effectbase* makeInstance<PluginSynth::GPU::module_synth_gpu>(int32_t _projectGlobalId, IHostCallback* _hostCallback) {
    return new PluginSynth::GPU::module_synth_gpu(_projectGlobalId, _hostCallback);
}

#include "assert_dbg.h"
#include "dsp_util.h"
#include "event.h"
#include "gl/gl_util.h"
#include "gl/gl_vbo.h"
#include "glheaders.h"
#include "gui/container/container.h"
#include "gui/contextmenu/contextmenu_base.h"
#include "gui/controls/button.h"
#include "gui/shape/shapeeditor.h"
#include "hires_timer.h"
#include "host/shape/shape.h"
#include "math/seq_math.h"
#include "note.h"
#include "platform.h"
#include "rand.h"
#include "renderresources.h"
#include "saferef.h"
#include "seq_time.h"
#include "str_util.h"
#include "synth-gpu-gl.h"
#include "synth-plugin.h"
#include "synth-snapshot.h"
#include "synth-template.hpp"
#include "synth-types.hpp"
#include "types.h"
#include <cstddef>
#include <nanovg.h>
#include <nanovg_min.h>
#include <GLFW/glfw3.h>

namespace PluginSynth::GPU {
    
struct VoiceSynth {
    std::array<double, 64> modValues{};
    std::array<float, 8> envelopeValuesCached{};

    double velocity     = 0.0;
    int32_t indexUnison = 0;
    note_t noteT = {};
    Envelope volEnv;
    double frequency       = 0.0;
    double targetFrequency = 0.0;
    double pitchBend       = 1.0;
    bool bIsActive         = false;


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

    void Start(bool bTriggerMono) {
        bIsActive = true;
        if (!bTriggerMono) {
            volEnv.Reset();
        }
        volEnv.Start();
    }
};

enum ParametersSynthGPU {
    MasterVolume = 0,
    VoiceMode,
    Keytrack,
    Osc1UnisonVoiceCount,
    Osc1UnisonDetune,
    Osc1Waveform,
    Osc1BlebDuration,
    Osc1Coarse,
    Osc1Fine,
    Osc1Stereo,
    Osc1PulseWidth,
    Osc1PulseWidthModDepth,
    Osc1PulseWidthModRate,
};

struct ui_layout_t {
    int32_t uiId = 0;
};

struct snapshot_t {
    int32_t version = 0;
    std::vector<PluginSynth::param_float_snapshot_t> params;
    std::vector<DAW::Shape::shape_snapshot_t> shapes;
    std::vector<ui_layout_t> uiLayout;
};

static constexpr int32_t SYNTH_GPU_SNAPSHOT_VERSION = 2;

std::shared_ptr<std::vector<std::byte>> serializeSnapshot(const snapshot_t& snapshot) {
    dbgassert(snapshot.version == SYNTH_GPU_SNAPSHOT_VERSION);
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
    if (snapshot.version > SYNTH_GPU_SNAPSHOT_VERSION)
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

class SynthImplGPU final : public SynthImpl<SynthImplGPU, ParametersSynthGPU> {
private:
    friend class guicontainer_plugin_synth_gpu;
    static constexpr uint16_t NUM_POLY_VOICES   = 32;
    static constexpr uint16_t MAX_UNISON_VOICES   = 1024*16;

private:
    PluginSynth::module_synth_template<SynthImplGPU>* const moduleSynthInstance;
    std::array<PluginSynth::GPU::VoiceSynth, NUM_POLY_VOICES> voices;
    seq_rand synthRand;
    DAW::Shape::shape_t oscShape;
    std::vector<std::shared_ptr<PluginViewContainer>> views;


    int32_t currentProgramId = 0;
    int32_t currentProgram() const { return currentProgramId; }
    GLFWwindow* window = nullptr;
    gpu_compute_context_t gpuContext{};
    GLuint ubo = 0;
    struct host_buffer_t {
        ssbo_ringbuffer_t<16> ssbo{};
        std::vector<float> buffer;
        void downloadBuffer() {
            ssbo.downloadBufferDelayed(buffer.data(), buffer.size() * sizeof(float));
        }
        void uploadBuffer() {
            ssbo.uploadBuffer(buffer.data(), buffer.size() * sizeof(float));
        }
        void clearBuffer() {
            std::fill(std::begin(buffer), std::end(buffer), 0.0f);
        }
        void incrementFrame() {
            ssbo.incrementFrame();
        }
    };
    host_buffer_t ssboInputSynthState{};
    host_buffer_t ssboInputVoiceStates{};
    host_buffer_t ssboOutput{};
    host_buffer_t ssboOutputWaveform{};
    std::array<host_buffer_t*, 4> hostBuffers{&ssboInputSynthState, &ssboInputVoiceStates, &ssboOutput, &ssboOutputWaveform};
    PluginSynth::GPU::shader_gpu_compute gpuProgram{};

    int64_t timePerfLog = 0;
    int64_t timeCheckShader = 0;
    int64_t timeLastShaderError = 0;
    double timeComputeAvg = -2.0;
    hires_timer_t perfTimer;

private:
    void initImpl() {
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
    // MasterVolume = 0,
    // VoiceMode,
    // Osc1Waveform,
    // Osc1Coarse,
    // Osc1Fine,
    // Osc1UnisonVoiceCount,
    // Osc1UnisonDetune,
    // Osc1BlebDuration,
    // Osc1Stereo,
    // Osc1PulseWidth,
    // Osc1PulseWidthModDepth,
    // Osc1PulseWidthModRate,

        addFloatParam(Parameters::MasterVolume)->setRange(0.0, 1.0)->setInitialValue(dsp_util::gainToLinScale(0.25));
        setParamName(getParam(Parameters::MasterVolume), "Master Volume", "Volume", "Vol", "dB");

        const std::array<const char*, 2> stringsVoiceMode = {
            "Poly", "Mono"
        };
        addEnumParam(Parameters::VoiceMode)->setStrings(stringsVoiceMode.begin(), stringsVoiceMode.end())->setInitialValue(0);
        setParamName(getParam(Parameters::VoiceMode), "Voice Mode");

        addFloatParam(Parameters::Keytrack)->setRange(0.0, 100.0)->setInitialValue(0.0);
        setParamName(getParam(Parameters::Keytrack), "Keytrack", "Keytrack", "Keytrack", "%");

        addEnumParam(Parameters::Osc1Waveform)->setRange(0, 3)->setInitialValue(0);
        setParamName(getParam(Parameters::Osc1Waveform), "Oscillator 1 Waveform", "OSC1 Wave", "Wave");

        addFloatParam(Parameters::Osc1Fine)->setRange(-1.0, 1.0)->setInitialValue(0.0);
        setParamName(getParam(Parameters::Osc1Fine), "Oscillator 1 fine", "OSC1 Fine", "Fine");

        addIntParam(Parameters::Osc1Coarse)->setRange(-24, 24)->setInitialValue(0);
        setParamName(getParam(Parameters::Osc1Coarse), "Oscillator 1 coarse", "OSC1 Semi", "Semi");

        addIntParam(Parameters::Osc1UnisonVoiceCount)->setRange(1, MAX_UNISON_VOICES)->setInitialValue(3);
        setParamName(getParam(Parameters::Osc1UnisonVoiceCount), "Oscillator 1 Unison Voices", "OSC1 Unison", "Unison", "Voices");
        addFloatParam(Parameters::Osc1UnisonDetune)->setRange(-6.0, 6.0)->setInitialValue(-0.2);
        setParamName(getParam(Parameters::Osc1UnisonDetune), "Oscillator 1 Unison Detune", "OSC1 Detune", "Detune", "Semi");
        addFloatParam(Parameters::Osc1BlebDuration)->setRange(0.0, 128.0)->setInitialValue(2.0);
        setParamName(getParam(Parameters::Osc1BlebDuration), "Oscillator 1 Bleb Duration", "OSC1 Bleb", "Bleb");


        addFloatParam(Parameters::Osc1Stereo)->setRange(0.0, 100.0)->setInitialValue(75.0);
        setParamName(getParam(Parameters::Osc1Stereo), "Oscillator 1 Stereo", "OSC1 Stereo", "Stereo", "%");
        
        addFloatParam(Parameters::Osc1PulseWidth)->setRange(0.1, 99.9)->setInitialValue(50.0);
        setParamName(getParam(Parameters::Osc1PulseWidth), "Oscillator 1 Pulse Width", "OSC1 PW", "PW", "%");
        addFloatParam(Parameters::Osc1PulseWidthModRate)->setRange(0.0, 100.0)->setInitialValue(0.0);
        setParamName(getParam(Parameters::Osc1PulseWidthModRate), "Oscillator 1 Pulse Width Mod Rate", "OSC1 PW Mod Rate", "PW Mod Rate", "Hz");
        addFloatParam(Parameters::Osc1PulseWidthModDepth)->setRange(0.0, 100.0)->setInitialValue(0.0);
        setParamName(getParam(Parameters::Osc1PulseWidthModDepth), "Oscillator 1 Pulse Width Mod Depth", "OSC1 PW Mod Depth", "PW Mod Depth", "%");
    }
public:
    explicit SynthImplGPU(module_synth_template<SynthImplGPU>* module);

    ~SynthImplGPU()
    {
        if (window) {
            auto curContext = glfwGetCurrentContext();
            bool bRestoreContext = false;
            if (curContext != window) {
                glfwMakeContextCurrent(window);
                bRestoreContext = true;
            }
            releaseGlResources();
            if (bRestoreContext) {
                glfwMakeContextCurrent(curContext);
            }
        }
        for (auto* ptr : vecParams) {
            delete ptr;
        }
        if (window)
            glfwDestroyWindow(window);
    }
    void init() override {
        for (auto param : vecParams) {
            if (!param) continue;
            OnParamChange(static_cast<Parameters>(param->enumParam));
        }
        if (!glad_glDispatchCompute) {
            return;
        }
        auto curContext = glfwGetCurrentContext();
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

        window = glfwCreateWindow(512, 512, "GPU Synth", NULL, NULL);
        glfwMakeContextCurrent(window);
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

        int work_grp_cnt[3];

        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &work_grp_cnt[0]);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &work_grp_cnt[1]);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &work_grp_cnt[2]);

        log_lf(Log::L_INFO, "max global (total) work group counts x:%i y:%i z:%i\n",
        work_grp_cnt[0], work_grp_cnt[1], work_grp_cnt[2]);
        int work_grp_size[3];

        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &work_grp_size[0]);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &work_grp_size[1]);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &work_grp_size[2]);

        log_lf(Log::L_INFO, "max local (in one shader) work group sizes x:%i y:%i z:%i\n",
        work_grp_size[0], work_grp_size[1], work_grp_size[2]);
        int work_grp_inv;
        glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &work_grp_inv);
        log_lf(Log::L_INFO, "max local work group invocations %i\n", work_grp_inv);
    
        initGlResources();
        glfwMakeContextCurrent(curContext);
        for (auto& voice : voices) {
            voice.volEnv.a = 1000.0;
            voice.volEnv.d = 500.0;
            voice.volEnv.r = 1220.0;
        }
    }
    void reloadShader(shader_gpu_compute_defs_t defs) {
        auto res = loadshader_synth(defs, this->gpuProgram);
        if (std::holds_alternative<String>(res)) {
            log_lf(Log::L_ERROR, "%s\n", std::get<String>(res).c_str());
            timeLastShaderError = getTimeMillis();
        } else {
            this->gpuProgram = std::get<shader_gpu_compute>(res);
            // adjust program param
            auto param = GetParamEnum(Parameters::Osc1Waveform);
            std::vector<String> programNames;
            for (int32_t i = 0; i < this->gpuProgram.numPrograms; i++) {
                programNames.push_back(this->gpuProgram.programDescs[i].name);
            }
            param->setStrings(programNames.begin(), programNames.end());
        }
        checkGLError("loadshader_synth");
    }
    void initGlResources() {
        if (!glad_glDispatchCompute) {
            return;
        }
        glGenBuffers(1, &ubo);
        glBindBuffer(GL_UNIFORM_BUFFER, ubo);
        checkGLError("glBindBuffer");
        glBufferData(GL_UNIFORM_BUFFER, sizeof(gpu_compute_context_t), nullptr, GL_STREAM_DRAW);
        checkGLError("glBufferData");
        for (auto* buffer : hostBuffers) {
            buffer->ssbo.genBuffers();
        }
        checkGLError("genBuffers");

        auto blockSize = moduleSynthInstance->getSampleFormat().blockSize;
        if (blockSize == 0) {
            blockSize = 512;
        }
        reloadShader({blockSize, 2, NUM_POLY_VOICES, MAX_UNISON_VOICES});
        allocateForBlockSize(gpuProgram.blocksize);

        timeCheckShader = getTimeMillis();
        timePerfLog = getTimeMillis();
    }

    void allocateForBlockSize(samplecount_t blockSize) {
        ssboInputSynthState.buffer.resize(blockSize * NUM_SYNTH_INPUT_PARAMETERS);
        ssboInputVoiceStates.buffer.resize(blockSize * NUM_POLY_VOICES * NUM_VOICE_INPUT_PARAMETERS);
        ssboOutput.buffer.resize(blockSize * gpuProgram.channels);
        ssboOutputWaveform.buffer.resize(blockSize);
        for (auto* buffer : hostBuffers) {
            buffer->clearBuffer();
            for (size_t i = 0; i < buffer->ssbo.size(); i++) {
                buffer->ssbo.allocate(buffer->buffer.size() * sizeof(float), GL_DYNAMIC_DRAW);
                buffer->uploadBuffer();
                buffer->incrementFrame();
            }
        }
        checkGLError("allocateForBlockSize");
    }

    void releaseGlResources() {
        for (auto* buffer : hostBuffers) {
            buffer->ssbo.destroy();
        }
        glDeleteBuffers(1, &ubo);
        gpuProgram.destroy();
        gpuProgram = {};
        ubo = 0;
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
            case Parameters::MasterVolume:
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
                    if (!std::count_if(std::cbegin(voices), std::cend(voices), [](auto& v) { return v.bIsActive; })) {
                        auto hostInfo = moduleSynthInstance->getHostCallback();
                        // gpuContext.time_sample_phase_reset = hostInfo->m_vstTimeInfo.samplePos;
                    }
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

    static bool checkGLError(const char* s) {
#ifndef NDEBUG
        auto i = isGLContextPresent() ? glGetError() : 0;
        if (i != 0) {
            log_lf(Log::L_ERROR, "%s: %s\n", s, getGlErrorString(i));
            dbgassert(0);
            return true;
        }
#endif
        return false;
    }
    samplecount_t getLatency() override { return gpuProgram.blocksize * (ssboOutput.ssbo.size() - 1); }
    void processGpuSynth(AudioBlock* audioblock, float * const * outputs, int nFrames, const DAW::Host::Host* const host, double tick, playback_state state) {
        if (!glad_glDispatchCompute) {
            return;
        }
        auto curContext = glfwGetCurrentContext();
        if (curContext != window)
            glfwMakeContextCurrent(window);
        if (gpuProgram.blocksize != audioblock->samples 
                || !gpuProgram.is_valid() 
                || gpuProgram.channels != audioblock->channels) {
            gpuProgram.destroy();
            gpuProgram = {};
        }
        checkGLError("glfwMakeContextCurrent");
        auto tmNow_ms = getTimeMillis();
        if (!gpuProgram.is_valid() || tmNow_ms - timeCheckShader > 1000) {
            if (tmNow_ms - timeLastShaderError > 2000) {
                timeCheckShader = tmNow_ms;
                auto prevBlocksize = gpuProgram.blocksize;
                reloadShader({audioblock->samples, audioblock->channels, NUM_POLY_VOICES, MAX_UNISON_VOICES});
                if (prevBlocksize != gpuProgram.blocksize) {
                    allocateForBlockSize(gpuProgram.blocksize);
                }
            }
        }

        const auto channels = audioblock->channels;
        const int nOversample = 1;
        const auto bpm100 = host->prjGlobals.tempo100;
        int framesPerAutomationUpdate = state == playback_state::status_render ? 8 : 8;
        if (state == playback_state::status_render) {
            framesPerAutomationUpdate = 8;
        }
        double osc1_bleb_duration = 0.0;
        double osc1_stereo = 0.0;
        double osc1_pw = 0.0;
        double osc1_keytrack = 0.0;
        auto& inputBufferSynthState = ssboInputSynthState.buffer;
        auto& inputBufferVoiceStates = ssboInputVoiceStates.buffer;
        ssboInputSynthState.clearBuffer();
        ssboInputVoiceStates.clearBuffer();
        const bool bHasAutomationOrModulation = true; // TODO: implement
        for (int s = 0; s < gpuProgram.blocksize; s++) {
            if (host && moduleSynthInstance && (s % framesPerAutomationUpdate) == 0) {
                ReadAutomation(host, tick, state, s, nFrames, nOversample);
            }
            if (s % nOversample == 0) {
                FlushMidi(s / nOversample);
            }
            for (auto& v : voices) {
                v.volEnv.Update(oneOverSR);
            }
            for (auto& v : voices) {
                if (v.bIsActive) {
                    v.bIsActive = v.isVoiceActive(FilterModes::Off);
                }
            }
            if (bHasAutomationOrModulation || s == 0) {
                osc1_bleb_duration = GetParamFloat(Parameters::Osc1BlebDuration)->Value();
                osc1_stereo = GetParamFloat(Parameters::Osc1Stereo)->getAsDoubleModulated();
                osc1_pw = GetParamFloat(Parameters::Osc1PulseWidth)->getAsDoubleModulated();
                osc1_keytrack = GetParamFloat(Parameters::Keytrack)->getAsDoubleModulated();
            }

            auto idx_synth_param_bleb = s;
            inputBufferSynthState[idx_synth_param_bleb] = osc1_bleb_duration;
            auto idx_synth_param_stereo = s + gpuProgram.blocksize;
            inputBufferSynthState[idx_synth_param_stereo] = osc1_stereo;
            auto idx_synth_param_pw = s + gpuProgram.blocksize * 2;
            inputBufferSynthState[idx_synth_param_pw] = osc1_pw;
            auto idx_synth_param_keytrack = s + gpuProgram.blocksize * 3;
            inputBufferSynthState[idx_synth_param_keytrack] = osc1_keytrack;

            const auto coarse = GetParamInt(Parameters::Osc1Coarse)->Value();
            const auto fine   = GetParamFloat(Parameters::Osc1Fine)->Value();
            const float masterVolume = GetParamFloat(Parameters::MasterVolume)->getAsDoubleModulated();
            float masterGain = 0.0;
            dsp_util::getGainLvl(masterVolume, masterGain);
            const auto tickPos = tick + sampleToTickConvert<double, roundmode::none>(s, bpm100, host->m_sampleFormatInternal.sampleRate * nOversample);
            for (size_t i = 0; i < NUM_POLY_VOICES; i++) {
                auto& v = voices[i];
                auto idx_base = i * (NUM_VOICE_INPUT_PARAMETERS * gpuProgram.blocksize);
                auto idx_velocity = idx_base + s;
                auto idx_pitch = idx_base + gpuProgram.blocksize * 1 + s;
                auto osc1Tune = pitchFactor(coarse + fine);
                auto baseFrequency = v.frequency * v.pitchBend;
                auto osc1Frequency = osc1Tune * baseFrequency;
                float velocity = 0.0;
                if (v.bIsActive) {
                    float volEnv = v.volEnv.value;
                    if (v.noteT.len > 0) {
                        const float noteProgress = v.noteT.end() - tickPos;
                        const float fFadeLen = 64.0f;
                        float fFadeIn = math::smoothstep(math::clamp(noteProgress / fFadeLen, 0.0f, 1.0f));
                        float fFadeOut = math::smoothstep(math::clamp((v.noteT.len - noteProgress) / fFadeLen, 0.0f, 1.0f));
                        volEnv = volEnv * fFadeIn * fFadeOut;
                    }
                    velocity = masterGain * v.velocity * volEnv;
                }
                inputBufferVoiceStates[idx_velocity] = v.bIsActive ? velocity : -1.0;
                inputBufferVoiceStates[idx_pitch]    = osc1Frequency;
            }
        }
        const int32_t programId = currentProgramId % gpuProgram.programs.size();
        const auto sampleRate = moduleSynthInstance->format.sampleRate;
    

        auto hostInfo = moduleSynthInstance->getHostCallback();
        gpuContext.bpm = host->prjGlobals.tempo100 / 100.0;
        gpuContext.one_over_samplerate = 1.0 / sampleRate;
        gpuContext.time_samples = hostInfo->m_vstTimeInfo.samplePos;
        gpuContext.time_seconds = hostInfo->m_vstTimeInfo.samplePos * gpuContext.one_over_samplerate;
        gpuContext.time_beats = hostInfo->m_vstTimeInfo.ppqPos;
        gpuContext.osc1_unison_voice_count = GetParamInt(Parameters::Osc1UnisonVoiceCount)->Value();
        gpuContext.osc1_unison_detune = GetParamFloat(Parameters::Osc1UnisonDetune)->Value();
        gpuContext.osc1_bleb_duration = GetParamFloat(Parameters::Osc1BlebDuration)->Value();
        gpuContext.osc1_stereo = GetParamFloat(Parameters::Osc1Stereo)->getAsDoubleModulated();
        gpuContext.osc1_pw = GetParamFloat(Parameters::Osc1PulseWidth)->getAsDoubleModulated();
        gpuContext.osc1_pw_mod_rate = GetParamFloat(Parameters::Osc1PulseWidthModRate)->getAsDoubleModulated();
        gpuContext.osc1_pw_mod_depth = GetParamFloat(Parameters::Osc1PulseWidthModDepth)->getAsDoubleModulated();

        audioblock->clear();
        perfTimer.reset();
        ssboInputSynthState.uploadBuffer();
        ssboInputVoiceStates.uploadBuffer();
        checkGLError("glBufferData");

        glBindBuffer(GL_UNIFORM_BUFFER, ubo);
        checkGLError("glBindBuffer");
        glBufferData(GL_UNIFORM_BUFFER, sizeof(gpu_compute_context_t), &gpuContext, GL_STREAM_DRAW);
        checkGLError("glBufferData");
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssboInputSynthState.ssbo.current());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssboInputVoiceStates.ssbo.current());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ssboOutput.ssbo.current());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, ssboOutputWaveform.ssbo.current());
        glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);

        checkGLError("glBindBufferBase");
        if (gpuProgram.programs[programId]) {
            glUseProgram(gpuProgram.programs[programId]);
            checkGLError("glUseProgram");
            glDispatchCompute(1, 1, 1);
            checkGLError("glDispatchCompute");
        }

        if (gpuProgram.programsWaveform[programId]) {
            glUseProgram(gpuProgram.programsWaveform[programId]);
            checkGLError("glBufferData");
            glDispatchCompute(1, 1, 1);
        }

        glFinish();
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT|GL_BUFFER_UPDATE_BARRIER_BIT);
        if (gpuProgram.programs[programId]) {
            ssboOutput.downloadBuffer();
        } else {
            ssboOutput.clearBuffer();
        }
        if (gpuProgram.programsWaveform[programId]) {
            ssboOutputWaveform.downloadBuffer();
        } else {
            ssboOutputWaveform.clearBuffer();
        }

        for (auto* buffer : hostBuffers) {
            buffer->incrementFrame();
        }

        const auto& outputBuffer = ssboOutput.buffer;
        // copy nFrames to outputs
        for (samplecount_t ch = 0; ch < channels; ch++) {
            for (samplecount_t sampleIdx = 0; sampleIdx < audioblock->samples; sampleIdx++) {
                float val = outputBuffer[sampleIdx + ch * audioblock->samples];
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
        // if (state == playback_state::status_render) {
        //     // print tick, sample, block size, notes active
        //     log_lf(Log::L_WARN, "tick: %f, sample: %f, block size: %lld, notes active: %d\n", tick, hostInfo->m_vstTimeInfo.samplePos, audioblock->samples, int32_t(std::count_if(std::cbegin(voices), std::cend(voices), [](auto& v) { return v.bIsActive; })));
        // }
        timeComputeAvg = 0.95 * timeComputeAvg + 0.05 * tmTotal_ms;
        if (curContext != window)
            glfwMakeContextCurrent(curContext);
    }

    void ProcessSynth(AudioBlock* in, float * const * outputs, int nFrames, const DAW::Host::Host* const host, double tick, playback_state state) override {
        auto lock = this->lockProcessing();
        processGpuSynth(in, outputs, nFrames, host, tick, state);
    }

    std::shared_ptr<PluginViewContainer> createViewCtrImpl() override;

    DAW::Shape::shape_t& getShape() {
        return oscShape;
    }

    bool getSnapshot(snapshot_t& snapshot) const {
        snapshot.version     = SYNTH_GPU_SNAPSHOT_VERSION;
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
    
    param_unit_t convertParamValueToDisplay(int32_t idx, float value) override {
        if (idx > 0 && idx - 1 < CtrSize(vecParams)) {
            SynthParamBase* param = vecParams[idx-1];
            if (param && param->enumParam == ParametersSynthGPU::Osc1PulseWidthModRate) {
                auto v = StringFormat("%.3f", pow(2.0, value*12.0));
                return { v, "Hz" };
            }
        }
        return module_synth_template<SynthImplGPU>::convertParamValueToDisplay(idx, value);
    }
};

class guicontainer_plugin_synth_gpu final : public guictr_base {
    module_synth_gpu* const moduleInstance;
    DAW::Shape::guictr_curve_shape* const shapeEditorCtr;
    seq_rand synthRandUI;
public:

    explicit guicontainer_plugin_synth_gpu(module_synth_gpu* module) : moduleInstance(module), shapeEditorCtr(DAW::Shape::makeShapeCurveView())
    {
        padding = 0;
        margin  = 0;
        setBackgroundRendered(false);
        shapeEditorCtr->setBackgroundRendered(false);
        shapeEditorCtr->setBackgroundRenderedInset(false);
        shapeEditorCtr->setCanMouseHit(false);
        shapeEditorCtr->id = 2;
        shapeEditorCtr->margin = 0;
        shapeEditorCtr->padding = 2;
        add(shapeEditorCtr);
        setCanMouseHit(true);
    }
    class guictr_module_synth_gpu_context_menu final : public guictxtmenu {
        module_synth_gpu* const moduleInstance;
        SafeRef<guibase> refGui;
    public:
        explicit guictr_module_synth_gpu_context_menu(module_synth_gpu* _module, SafeRef<guibase> ref)
            : guictxtmenu(), moduleInstance(_module), refGui(std::move(ref))
        {
            this->size.x   = 220;
            maxHeight = 0;
            this->fontSize = FONT_SIZE_CTXT_SMALL;
            this->paddingV = 0;
            addEntry(new DAW::Shape::ctxtmenu_lfo_shape_select("Shape", 1));
        }
        bool clickedElement(ctxtmenu_entry* e, int _id) override {
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
        parentCtrl->openContextMenu(new guictr_module_synth_gpu_context_menu(moduleInstance, safeRef), evt.mousepos);
    }

    ~guicontainer_plugin_synth_gpu() override {
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

    void prerender(NVGcontext* vg) override {
        guictr_base::prerender(vg);
        // convert synth impls outputBufferWaveform to shape
        auto synth = moduleInstance->getSynth();
        auto& shape = this->shapeEditorCtr->getShape();
        auto& waveform = synth->ssboOutputWaveform.buffer;
        shape.pts.clear();
        shape.pts.reserve(waveform.size());
        for (size_t i = 0; i < waveform.size(); i++) {
            float x = i / static_cast<float>(waveform.size());
            shape.pts.push_back({{ x, waveform[i] * 0.5 + 0.5 }, 0.5});
        }
        shape.flags = DAW::Shape::SHAPE_CYCLIC | DAW::Shape::SHAPE_LOCK_POINTS;
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

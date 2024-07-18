#include "assert_dbg.h"
#include "host/audiobuffer/audioblock.h"
#include "host/host_pluginmanager.h"
#include "logging.h"
#include "seq_time.h"
#include "synth-gpu-parameters.h"
#include "synth-gpu-snapshot.hpp"
#include "synth-gpu-impl.hpp"
#include "dsp_util.h"
#include "gl/gl_util.h"
#include "gl/gl_vbo.h"
#include "gl/gl_context.hpp"
#include "host/shape/shape.h"
#include "hires_timer.h"
#include "math/simd_math.h"
#include "platform.h"
#include "synth-types.hpp"
#include "types.h"
#include <algorithm>
#include <cstdint>
#include "host/track/track_impl.h"

namespace PluginSynth::GPU {

uint32_t gDebugBenchmarkFlags = 0;

SynthImplGPU::SynthImplGPU(module_synth_template<SynthImplGPU>* module)
    : SynthImpl<SynthImplGPU, ParametersSynthGPU>(module),
    moduleSynthInstance(module),
    audioOutputBuffer(NUM_AUDIO_CHANNELS, 0)
{
    initImpl();
}

void SynthImplGPU::init() {
    for (size_t i = 0; i < lfoParameters.size(); i++) {
        auto& params = lfoParameters[i];
        using namespace DAW::LFO;
        params.shape       = {};
        params.shape.name  = "LFO " + std::to_string(i + 1) + " Shape";
        params.shape.pts   = DAW::Shape::GetShape(DAW::Shape::ShapeWaveform::SHAPE_TRIANGLE);
        params.shape.flags = DAW::Shape::ShapeFlags::SHAPE_CYCLIC | DAW::Shape::ShapeFlags::SHAPE_SHAPED;
        params.syncFlags   = STRAIGHT | DOTTED | TRIPLET;
        params.syncRatios  = GetSyncRatios(params.syncFlags);
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
        log_lf(Log::L_ERROR, "Could not initialize OpenGL context for audio processing.\n");
        return;
    }

    reloadProgram();
    timeCheckShader = getTimeMillis();
    timePerfLog     = getTimeMillis();

    auto sampleRate = moduleSynthInstance->getSampleFormat().sampleRate;
    if (!sampleRate) sampleRate = 44100;
    auto blocksize1024Fixed = gpuProgram.blocksize1024Fixed;
    // print inputBuffer size in mega bytes
    const auto inputSizePerSample = (ssboInputSynthState.buffer.size() + ssboInputVoiceStates.buffer.size()) / blocksize1024Fixed;
    log_lf(Log::L_INFO, "inputBuffer size: %f MB\n", inputSizePerSample * blocksize1024Fixed * sizeof(float) / 1024.0 / 1024.0);
    const auto inputPerSec = inputSizePerSample * sampleRate * sizeof(float) / 1024.0 / 1024.0;
    // print required input bandwith
    log_lf(Log::L_INFO, "inputBuffer bandwidth: %f MB/s\n", inputPerSec);

    // also print output buffer size and bandwidth
    const auto outputSizePerSample = size_t(3);
    log_lf(Log::L_INFO, "outputBuffer size: %f MB\n", outputSizePerSample * blocksize1024Fixed * sizeof(float) / 1024.0 / 1024.0);
    const auto outputPerSec = outputSizePerSample * sampleRate * sizeof(float) / 1024.0 / 1024.0;
    log_lf(Log::L_INFO, "outputBuffer bandwidth: %f MB/s\n", outputPerSec);

    for (auto param : vecParams) {
        if (!param) continue;
        OnParamChange(Parameters(param->enumParam));
    }
}

void SynthImplGPU::initImpl() {
    auto addParam = [this](SynthParamBase* param, size_t idx) {
        while (this->vecParams.size() <= idx) {
            this->vecParams.push_back(nullptr);
        }
        this->vecParams[idx] = param;
    };
    auto addFloatParam = [&addParam](size_t enumParam) -> SynthParam_Float* {
        SynthParam_Float* param = new SynthParam_Float(Parameters(enumParam));
        addParam(param, enumParam);
        return param;
    };
    auto addIntParam = [&addParam](size_t enumParam) -> SynthParam_Int* {
        SynthParam_Int* param = new SynthParam_Int(Parameters(enumParam));
        addParam(param, enumParam);
        return param;
    };
    auto addEnumParam = [&addParam](size_t enumParam) -> SynthParam_Enum* {
        SynthParam_Enum* param = new SynthParam_Enum(Parameters(enumParam));
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

    addFloatParam(Parameters::MasterVolume)->setRange(0.0, 1.0)->setInitialValue(dsp_util::gainToLinScale(0.5));
    setParamName(getParam(Parameters::MasterVolume), "Master Volume", "Volume", "Vol", "dB");

    const std::array<const char*, 2> stringsVoiceMode = {
        "Poly", "Mono"
    };
    addEnumParam(Parameters::VoiceMode)->setStrings(stringsVoiceMode.begin(), stringsVoiceMode.end())->setInitialValue(0);
    setParamName(getParam(Parameters::VoiceMode), "Voice Mode");

    addFloatParam(Parameters::Osc1Gain)->setRange(0.0, 1.0)->setInitialValue(dsp_util::gainToLinScale(1.0));
    setParamName(getParam(Parameters::Osc1Gain), "Oscillator 1 Gain", "OSC1 Gain", "Gain", "dB");
    addEnumParam(Parameters::Osc1Waveform)->setRange(0, 3)->setInitialValue(0);
    setParamName(getParam(Parameters::Osc1Waveform), "Oscillator 1 Waveform", "OSC1 Wave", "Wave");
    addIntParam(Parameters::Osc1UnisonVoiceCount)->setRange(1, userLimitUnisonVoices)->setInitialValue(3);
    setParamName(getParam(Parameters::Osc1UnisonVoiceCount), "Oscillator 1 Unison Voices", "OSC1 Unison", "Unison", "Voices");
    addFloatParam(Parameters::Osc1UnisonDetune)->setRange(-6.0, 6.0)->setInitialValue(0.0);
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

    const int envBase[2]          = { int32_t(Parameters::ADSR_1_A_Duration), int32_t(Parameters::ADSR_2_A_Duration) };
    const String parNames[2]      = { "Volume", "Mod" };
    const String parNamesShort[2] = { "EnvV", "EnvM" };
    for (size_t i = 0; i < 2; i++) {
        String parName   = i == 0 ? "Volume" : "Mod";
        String nameBase  = parNames[i] + " " + "Envelope";
        String nameShort = parNamesShort[i];
        auto parAtt      = addFloatParam(envBase[i]);
        parAtt->setInitialValue(UnshapeEnvTimeBaseParam(Envelope::GetParamFromTimeMillis(2.0, envTimeRanges[0])));
        setParamName(parAtt, nameBase + " Attack", nameShort + " Attack", "Attack", "s");
        auto parHold = addFloatParam(envBase[i] + 1);
        parHold->setInitialValue(0.0);
        setParamName(parHold, nameBase + " Hold", nameShort + " Hold", "Hold", "s");
        auto parDec = addFloatParam(envBase[i] + 2);
        parDec->setInitialValue(UnshapeEnvTimeBaseParam(Envelope::GetParamFromTimeMillis(333.0, envTimeRanges[2])));
        setParamName(parDec, nameBase + " Decay", nameShort + " Decay", "Decay", "s");
        auto parSus = addFloatParam(envBase[i] + 3);
        parSus->setRange(0.0, 100.0)->setInitialValue(80.0);
        setParamName(parSus, nameBase + " Sustain", nameShort + " Sustain", "Sustain", "%");
        auto parRel = addFloatParam(envBase[i] + 4);
        parRel->setInitialValue(UnshapeEnvTimeBaseParam(Envelope::GetParamFromTimeMillis(123.0, envTimeRanges[3])));
        setParamName(parRel, nameBase + " Release", nameShort + " Release", "Release", "s");
        auto parAttShape = addFloatParam(envBase[i] + 5);
        parAttShape->setRange(-100.0, 100.0)->setInitialValue(-10.0);
        setParamName(parAttShape, nameBase + " Attack Shape", nameShort + " A Shape", "Shape", "%", "%.0f");
        auto parDecShape = addFloatParam(envBase[i] + 6);
        parDecShape->setRange(-100.0, 100.0)->setInitialValue(-10.0);
        setParamName(parDecShape, nameBase + " Decay Shape", nameShort + " D Shape", "Shape", "%", "%.0f");
        auto parRelShape = addFloatParam(envBase[i] + 7);
        parRelShape->setRange(-100.0, 100.0)->setInitialValue(-10.0);
        setParamName(parRelShape, nameBase + " Release Shape", nameShort + " R Shape", "Shape", "%", "%.0f");
    }

    for (size_t i = 0; i < NUM_LFO; i++) {
        String parName = "LFO " + std::to_string(i + 1);
        auto lfoFreq   = addFloatParam(Parameters::LFO_1_Frequency + i * MAX_PARAMS_PER_LFO);
        lfoFreq->setRange(0.0, 1.0)->setInitialValue(0.5);
        setParamName(lfoFreq, parName + " Frequency", parName + " Freq", "Freq", "Hz");
        auto lfoTriggerMode           = addEnumParam(Parameters::LFO_1_TriggerMode + i * MAX_PARAMS_PER_LFO);
        std::array triggerModeStrings = { "Note On", "One Shot", "Free" };
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
        lfoRampDuration->setInitialValue(UnshapeEnvTimeBaseParam(Envelope::GetParamFromTimeMillis(5.0, getGlobalLFO(0).getRampRange())));
        setParamName(lfoRampDuration, parName + " Ramp Duration", parName + " Ramp Dur", "Ramp Dur", "s");
    }

    addFloatParam(Parameters::Macro_1)->setRange(0.0, 1.0)->setInitialValue(0.0);
    setParamName(getParam(Parameters::Macro_1), "Macro 1", "Macro 1", "Macro 1", "");
    addFloatParam(Parameters::Macro_2)->setRange(0.0, 1.0)->setInitialValue(0.0);
    setParamName(getParam(Parameters::Macro_2), "Macro 2", "Macro 2", "Macro 2", "");
    addFloatParam(Parameters::Macro_3)->setRange(0.0, 1.0)->setInitialValue(0.0);
    setParamName(getParam(Parameters::Macro_3), "Macro 3", "Macro 3", "Macro 3", "");
    addFloatParam(Parameters::Macro_4)->setRange(0.0, 1.0)->setInitialValue(0.0);
    setParamName(getParam(Parameters::Macro_4), "Macro 4", "Macro 4", "Macro 4", "");

    for (size_t i = 0; i < stringsModSource.size(); ++i) {
        auto idx = -1 + i;
        modSourceDescs.emplace_back(idx, stringsModSource[i]);
    }

    for (size_t i = 0; i < this->varNames.size(); i++) {
        this->varNames[i] = i < modSourceVarNames.size() ? modSourceVarNames[i] : "";
    }
    static constexpr std::array modDestParamMapping = {
        std::pair{ ModDest_MasterVolume, MasterVolume },
        std::pair{ ModDest_Osc1Gain, Osc1Gain },
        std::pair{ ModDest_Osc1Filter, Osc1Filter },
        std::pair{ ModDest_Osc1PulseWidth, Osc1PulseWidth },
        std::pair{ ModDest_ADSR_1_A_Duration, ADSR_1_A_Duration },
        std::pair{ ModDest_ADSR_1_H_Duration, ADSR_1_H_Duration },
        std::pair{ ModDest_ADSR_1_D_Duration, ADSR_1_D_Duration },
        std::pair{ ModDest_ADSR_1_S_Amount, ADSR_1_S_Amount },
        std::pair{ ModDest_ADSR_1_R_Duration, ADSR_1_R_Duration },
        std::pair{ ModDest_ADSR_1_A_Shape, ADSR_1_A_Shape },
        std::pair{ ModDest_ADSR_1_D_Shape, ADSR_1_D_Shape },
        std::pair{ ModDest_ADSR_1_R_Shape, ADSR_1_R_Shape },
        std::pair{ ModDest_ADSR_2_A_Duration, ADSR_2_A_Duration },
        std::pair{ ModDest_ADSR_2_H_Duration, ADSR_2_H_Duration },
        std::pair{ ModDest_ADSR_2_D_Duration, ADSR_2_D_Duration },
        std::pair{ ModDest_ADSR_2_S_Amount, ADSR_2_S_Amount },
        std::pair{ ModDest_ADSR_2_R_Duration, ADSR_2_R_Duration },
        std::pair{ ModDest_ADSR_2_A_Shape, ADSR_2_A_Shape },
        std::pair{ ModDest_ADSR_2_D_Shape, ADSR_2_D_Shape },
        std::pair{ ModDest_ADSR_2_R_Shape, ADSR_2_R_Shape },
        std::pair{ ModDest_LFO_1_Frequency, LFO_1_Frequency },
        std::pair{ ModDest_LFO_1_TriggerMode, LFO_1_TriggerMode },
        std::pair{ ModDest_LFO_1_Phase, LFO_1_Phase },
        std::pair{ ModDest_LFO_1_RampDuration, LFO_1_RampDuration },
        std::pair{ ModDest_LFO_2_Frequency, LFO_2_Frequency },
        std::pair{ ModDest_LFO_2_TriggerMode, LFO_2_TriggerMode },
        std::pair{ ModDest_LFO_2_Phase, LFO_2_Phase },
        std::pair{ ModDest_LFO_2_RampDuration, LFO_2_RampDuration },
    };
    for (const auto& mapping : modDestParamMapping) {
        auto param = getParam(mapping.second);
        if (assert_expr(param)) {
            modDestDescs.emplace_back(mapping.first, mapping.second, param->getShortName());
        }
    }
}

bool SynthImplGPU::getSnapshot(snapshot_t& snapshot) const {
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
            modSnapshot.destinations.push_back({ int32_t(dest.parameter), dest.range });
        }
        snapshot.modulations.push_back(modSnapshot);
    }
    const auto numLfos = CtrSize(lfoParameters);
    snapshot.modulations.reserve(numLfos);
    for (int32_t i = 0; i < numLfos; ++i) {
        const auto& lfo    = lfoParameters[i];
        auto shapeSnapshot = DAW::Shape::shape_snapshot_t{ 0, DAW::Shape::shape_preset_t{ 2, lfo.shape } };
        snapshot.lfos.emplace_back(std::move(shapeSnapshot), lfo.modeIsShape, lfo.randomModeId, lfo.syncFlags);
    }
    const auto numEnvelopes = CtrSize(tmpVoice.envelopes);
    snapshot.adsrs.reserve(numEnvelopes);
    for (int32_t i = 0; i < numEnvelopes; ++i) {
        snapshot.adsrs.push_back({ int32_t(tmpVoice.envelopes[i].shaping) });
    }
    return true;
}

bool SynthImplGPU::setSnapshot(const snapshot_t& snapshot) {
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
        onModulationsChanged();
    }

    for (size_t i = 0; i < this->lfoParameters.size() && i < snapshot.lfos.size(); i++) {
        auto& lfoParams         = lfoParameters[i];
        const auto& lfoSnapshot = snapshot.lfos[i];
        lfoParams.modeIsShape   = lfoSnapshot.modeIsShape;
        lfoParams.randomModeId  = lfoSnapshot.randomModeId;
        lfoParams.syncFlags     = lfoSnapshot.syncFlags;
        lfoParams.shape         = lfoSnapshot.shape.shape.curve;
        lfoParams.shape.flags   = DAW::Shape::SHAPE_SHAPED | DAW::Shape::SHAPE_CYCLIC;
    }

    for (size_t i = 0; i < this->tmpVoice.envelopes.size() && i < snapshot.adsrs.size(); i++) {
        auto& adsr   = this->tmpVoice.envelopes[i];
        adsr.shaping = DAW::CurveShapingFunction(snapshot.adsrs[i].shapingMode);
        for (auto& v : voices) {
            v.envelopes[i].shaping = adsr.shaping;
        }
    }

    for (auto& param : vecParams) {
        if (!param) continue;
        OnParamChange(Parameters(param->enumParam));
    }
    for (auto& v : voices) {
        updateEnvelopeParameters(v);
    }
    return true;
}

void SynthImplGPU::setBlocksize(samplecount_t blocksize) {
    tmpVoice.Kill();
    for (auto& voice : voices) {
        voice.Kill();
    }
    SynthImpl::setBlocksize(blocksize);
    audioOutputBuffer.clear();
    sampleOffsetSubBlock = 0;
    readOffsetSubBlock = 0;
    midiQueue.Clear();
    heldNotes.clear();
}
void SynthImplGPU::reloadProgram() {
    numActiveVoicesMax = 0;
    GlfwContextSwitch ctxSwitch(window);
    if (!gpuProgram.is_valid()) {
        reloadShader({ GPU_BLOCK_SIZE, NUM_AUDIO_CHANNELS, userLimitPolyVoices, userLimitUnisonVoices });
        timeCheckShader = getTimeMillis();
    }
    allocatedVoiceCount = userLimitPolyVoices;
    audioOutputBuffer.realloc(gpuProgram.blocksize1024Fixed);
    ssboInputSynthState.buffer.resize(gpuProgram.blocksize1024Fixed * size_t(NUM_SYNTH_INPUT_PARAMETERS));
    ssboInputVoiceStates.buffer.resize(gpuProgram.blocksize1024Fixed * size_t(userLimitPolyVoices) * size_t(NUM_VOICE_INPUT_PARAMETERS));
    ssboOutput.buffer.resize(gpuProgram.blocksize1024Fixed * gpuProgram.channels);
    ssboOutputWaveform.buffer.resize(gpuProgram.blocksize1024Fixed);
    GPUAudioProcessor::reallocateSSBOs();
}

void SynthImplGPU::updateProgramList() {
    // adjust program param
    auto param = GetParamEnum(ParametersSynthGPU::Osc1Waveform);
    std::vector<String> programNames;
    programNames.reserve(this->gpuProgram.numPrograms);
    for (int32_t i = 0; i < this->gpuProgram.numPrograms; i++) {
        programNames.push_back(this->gpuProgram.programDescs[i].name);
    }
    param->setStrings(programNames.begin(), programNames.end());
    auto regParam = moduleSynthInstance->getParam(PARAM_OFFSET_IMPL + int32_t(Parameters::Osc1Waveform));
    auto intParam = dynamic_cast<SynthParam_Int*>(param);
    if (regParam && intParam) {
        regParam->quantizationSteps = intParam->iMax - intParam->iMin + 1;
    }
}

void SynthImplGPU::StartVoice(VoiceSynth& voice, VoiceModes mode, double velocity) {
    std::fill(voice.modValues.begin(), voice.modValues.end(), 0.0);
    voice.Start(mode == VoiceModes::Mono, velocity);
    voice.unisonDetune         = GetParamFloat(Parameters::Osc1UnisonDetune)->Value();
    voice.unisonDetuneKeytrack = GetParamFloat(Parameters::Osc1KeytrackDetune)->getAsDoubleModulated();
}

double SynthImplGPU::UnshapeEnvTimeBaseParam(double d) {
    /**
        * UGLY: we don't have the inverse function of ShapeLogLikeSIMD.
        * So we try to find p for ShapeLogLike(p) == d using a binary search
        */
    alignas(64) float envParamVals[8]{};
    alignas(64) float envParamValsScaled[8]{};
    for (int i = 0; i < 8; i++) {
        envParamVals[i] = float(i / (7.0));
    }
    auto maxIt        = 25;
    double closestVal = 1e9;
    while (maxIt--) {
        ShapeLogLikeSIMD<float>(envParamVals, envParamValsScaled);
        double closestDiff = 1e9;
        int closestIdx     = -1;
        for (int i = 0; i < 8; i++) {
            double diff = (envParamValsScaled[i] - d);
            if (std::abs(diff) < std::abs(closestDiff)) {
                closestDiff = diff;
                closestIdx  = i;
            }
        }
        if (closestIdx == -1) {
            break;
        }
        if (std::abs(closestDiff) < 1e-6) {
            closestVal = envParamVals[closestIdx];
            break;
        }
        closestVal       = envParamVals[closestIdx];
        int32_t firstIdx = closestIdx == 0 ? 0 : (closestDiff > 0 ? closestIdx - 1 : closestIdx);
        int32_t lastIdx  = closestIdx == 7 ? 7 : (closestDiff > 0 ? closestIdx : closestIdx + 1);
        double firstVal  = envParamVals[firstIdx];
        double lastVal   = envParamVals[lastIdx];
        double s         = lastVal - firstVal;
        for (int i = 0; i < 8; i++) {
            envParamVals[i] = float(firstVal + (i / 7.0) * s);
        }
    }
    return closestVal;
}

void SynthImplGPU::OnParamChange(Parameters parameter) {
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
            currentProgramId = int32_t(value);
            break;
        case Parameters::VoiceMode:
            numActiveVoicesMax = 0;
            switch (GetParamEnum(parameter)->getEnumValue<VoiceModes>()) {
                case VoiceModes::Mono:
                case VoiceModes::Legato:
                    for (auto& voice : voices) {
                        voice.Release();
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

void SynthImplGPU::updateEnvelopeParameters(VoiceSynth& v) {
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
    ShapeLogLikeSIMD<float>(envParamVals, envParamValsScaled);
    for (auto& f : envParamValsScaled) {
        auto idx = &f - &envParamValsScaled[0];
        f = Envelope::GetTimeBaseFromParam(f, envTimeRanges[idx % 4]);
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

void SynthImplGPU::updateLFOParameters(DAW::LFO::LFOParameters& p, size_t lfoIdx) {
    using P              = Parameters;
    const double lfoFreq = vecParams[P::LFO_1_Frequency + lfoIdx * MAX_PARAMS_PER_LFO]->getAsDoubleModulated();

    p.bpm          = gpuContext.bpm;
    p.freq         = lfoFreq;
    p.freqHz       = p.paramToFreqHz(lfoFreq);
    p.phaseOffset  = vecParams[P::LFO_1_Phase + lfoIdx * MAX_PARAMS_PER_LFO]->getAsDoubleModulated();
    p.rampDuration = vecParams[P::LFO_1_RampDuration + lfoIdx * MAX_PARAMS_PER_LFO]->getAsDoubleModulated();
    p.trigger      = GetParamEnum(Parameters(P::LFO_1_TriggerMode + lfoIdx * MAX_PARAMS_PER_LFO))->getEnumValue<DAW::LFO::LFOTriggerMode>();
    p.shape.flags  = DAW::Shape::ShapeFlags::SHAPE_SHAPED;
    if (p.trigger != DAW::LFO::LFOTriggerMode::OneShot) {
        p.shape.flags |= DAW::Shape::ShapeFlags::SHAPE_CYCLIC;
    }
}

void SynthImplGPU::updateVoiceModulations(ModulationSourceData& modSrcData, VoiceSynth& v, double tickPos) {
    auto itOut = modSrcData.begin();
    auto itIsInUse = this->isModulationInputInUse.begin();
    itIsInUse++;
    *itOut++ = 0.0;// input value
    *itOut++ = !*itIsInUse++ ? 0.0 : v.GetVolumeEnvelope().value;
    *itOut++ = !*itIsInUse++ ? 0.0 : v.GetFilterEnvelope().value;
    *itOut++ = !*itIsInUse++ ? 0.0 : v.velocity;
    *itOut++ = !*itIsInUse++ ? 0.0 : noteToLinearScale(v.noteT.pitch);
    *itOut++ = !*itIsInUse++ ? 0.0 : v.noteT.pitch / 127.0;
    *itOut++ = !*itIsInUse++ ? 0.0 : getVoiceLfoValue(v, 0) * v.lfos[0].GetRamp();
    *itOut++ = !*itIsInUse++ ? 0.0 : getVoiceLfoValue(v, 1) * v.lfos[1].GetRamp();
    *itOut++ = !*itIsInUse++ ? 0.0 : getVoiceLfoValue(v, 2) * v.lfos[2].GetRamp();
    *itOut++ = !*itIsInUse++ ? 0.0 : gpuContext.osc1_filter;
    *itOut++ = !*itIsInUse++ ? 0.0 : v.seqNr % 2;
    *itOut++ = !*itIsInUse++ ? 0.0 : v.randoms[0];
    *itOut++ = !*itIsInUse++ ? 0.0 : v.randoms[1];
    bool bHasNotePhase = *itIsInUse++;
    bool bHasNoteFade  = *itIsInUse++;
    if (bHasNotePhase || bHasNoteFade){
        const auto noteStart    = double(v.noteT.start());
        const auto noteLen      = double(math::max(1, v.noteT.len));
        const auto notePhase    = math::clamp(double(tickPos - noteStart) / noteLen, 0.0, 1.0);
        const auto noteProgress = tickPos - noteStart;

        const auto fNoteFadeDurationTicks = 64.0;
        const auto fFadeIn  = math::smoothstep(math::clamp(noteProgress / fNoteFadeDurationTicks, 0.0, 1.0));
        const auto fFadeOut = math::smoothstep(math::clamp((noteStart + noteLen - tickPos) / fNoteFadeDurationTicks, 0.0, 1.0));
        const auto noteFade = math::clamp(fFadeIn * fFadeOut, 0.0, 1.0);
        *itOut++ = notePhase;
        *itOut++ = noteFade;
    } else {
        *itOut++ = 0.0;
        *itOut++ = 0.0;
    }
    *itOut++ = !*itIsInUse++ ? 0.0 : vecParams[Parameters::Macro_1]->getAsDoubleModulated();
    *itOut++ = !*itIsInUse++ ? 0.0 : vecParams[Parameters::Macro_2]->getAsDoubleModulated();
    *itOut++ = !*itIsInUse++ ? 0.0 : vecParams[Parameters::Macro_3]->getAsDoubleModulated();
    *itOut++ = !*itIsInUse++ ? 0.0 : vecParams[Parameters::Macro_4]->getAsDoubleModulated();
    auto& voiceModulations = v.modValues;
    std::memset(voiceModulations.data(), 0, voiceModulations.size() * sizeof(double));
    ProcessModulations(modSrcData, voiceModulations);
}
template<typename VoiceType, typename VoiceListType>
VoiceType& getLatestVoice(VoiceListType& voices, size_t polyVoiceLimit) {
    // get the quietest voice, prioritizing voices that are released
    auto voiceEnd = std::begin(voices) + polyVoiceLimit;
    auto voice    = std::min_element(
            std::begin(voices),
            voiceEnd,
            [](auto& a, auto& b) {
                return a.seqNr < b.seqNr;
            });
    if (voice != voiceEnd) return *voice;
    return voices[0];
}

void SynthImplGPU::ProcessSynth(AudioBlock* in, AudioBlock* out, int nFrames, const DAW::Host::Host* const host, double tick, double samplePos, playback_state state) {
    if (!glad_glDispatchCompute) {
        return;
    }
    const bool bIsBenchmark = gDebugBenchmarkFlags & 1;
    const bool bDbgSkipBufferBuild = gDebugBenchmarkFlags & 2;
    const bool bDbgSkipAll = gDebugBenchmarkFlags & 8;
    if (bDbgSkipAll) {
        return;
    }
    const auto blockSizeExternal  = samplecount_t(moduleSynthInstance->format.blockSize);
    const auto blockSizeInternal = samplecount_t(gpuProgram.blocksize1024Fixed);
    const auto sampleRate = moduleSynthInstance->format.sampleRate;
    if (blockSizeExternal != nFrames) {
        //TODO: handle this case
        return;
    }
    if (bDbgSkipBufferBuild) {
        midiQueue.Clear();
    }

    // TODO: dispatch is not called if the external blocksize is smaller than the GPU blocksize. don't switch context unless we have to:
    GlfwContextSwitch ctxSwitch(window);
    auto tmNow_ms = getTimeMillis();
    if (gpuProgram.polyVoices != userLimitPolyVoices
        || gpuProgram.unisonVoices != userLimitUnisonVoices) {
        GetParamInt(Parameters::Osc1UnisonVoiceCount)->setRange(1, userLimitUnisonVoices);
        for (size_t i = userLimitPolyVoices; i < voices.size(); i++) {
            voices[i].Release();
        }
        gpuProgram.destroy();
        reloadProgram();
    }
    if (!bIsBenchmark && (!gpuProgram.is_valid() || tmNow_ms - timeCheckShader > 1000) && tmNow_ms - timeLastShaderError > 2000) {
        timeCheckShader = tmNow_ms;
        reloadShader({ GPU_BLOCK_SIZE, NUM_AUDIO_CHANNELS, userLimitPolyVoices, userLimitUnisonVoices });
    }
    samplecount_t numBlocksInternal = 1;
    samplecount_t numBlocksExternal = 1;
    if (blockSizeInternal != blockSizeExternal) {
        if (blockSizeExternal > 1024) {
            // we need to produce multiple internal blocks
            dbgassert(blockSizeExternal % 1024 == 0);
            numBlocksInternal = blockSizeExternal / 1024; 
            numBlocksExternal = 1;
        } else {
            // we need to split a single internal block into multiple external blocks
            // for each dispatchGpuSynth we need to wait numBlocksExternal invocations of ProcessSynth
            numBlocksInternal = 1;
            numBlocksExternal = blockSizeInternal / blockSizeExternal;
        }
    }

    this->minVoiceIdx = -1;
    this->maxVoiceIdx = -1;
    out->clear();
    for (samplecount_t block = 0; block < numBlocksInternal; block++) {
        auto s = gpuProgram.blocksize1024Fixed * block;
        double offsetSamplePos = samplePos + s;
        double offsetTickPos   = tick      + sampleToTickConvert<double, roundmode::none>(s, tempo.bpm100, sampleRate);
        auto latestVoice = getLatestVoice<VoiceSynth>(voices, userLimitPolyVoices);
        auto& modVals = latestVoice.modValues;

        gpuContext.bpm = tempo.bpm;
        gpuContext.one_over_samplerate = 1.0 / sampleRate;
        gpuContext.time_samples        = offsetSamplePos;
        gpuContext.time_seconds        = offsetSamplePos * gpuContext.one_over_samplerate;
        gpuContext.time_beats          = offsetTickPos / double(TICKS_QUARTER);

        gpuContext.osc1_unison_voice_count = GetParamInt(Parameters::Osc1UnisonVoiceCount)->Value();
        gpuContext.osc1_filter             = GetParamFloat(Parameters::Osc1Filter)->getAsDoubleModulated(modVals[ModDestinations::ModDest_Osc1Filter]);
        gpuContext.osc1_pw                 = GetParamFloat(Parameters::Osc1PulseWidth)->getAsDoubleModulated(modVals[ModDestinations::ModDest_Osc1PulseWidth]);
        gpuContext.osc1_pw_mod_rate        = GetParamFloat(Parameters::Osc1PulseWidthModRate)->getAsDoubleModulated();
        gpuContext.osc1_pw_mod_depth       = GetParamFloat(Parameters::Osc1PulseWidthModDepth)->getAsDoubleModulated();
        gpuContext.osc1_width_keytrack     = GetParamFloat(Parameters::Osc1KeytrackStereoWidth)->getAsDoubleModulated();

        auto q = state == playback_state::status_render ? DAW::Host::ProcessingQuality::Q_RENDER : DAW::Host::ProcessingQuality::Q_PLAYBACK;
        dbgassert(1024 == gpuProgram.blocksize1024Fixed);
        dbgassert(1024 == audioOutputBuffer.samples);
        auto samplesExternal = math::min(blockSizeExternal, gpuProgram.blocksize1024Fixed);
        if (sampleOffsetSubBlock == 0) {
            ssboInputSynthState.clearBuffer();
            ssboInputVoiceStates.clearBuffer();
            std::memset(modulationValuesMax.data(), 0, modulationValuesMax.size() * sizeof(double));
            std::memset(modulationValuesMin.data(), 0, modulationValuesMin.size() * sizeof(double));
        }
        processGpuSynthInput(host, offsetTickPos, offsetSamplePos, s, sampleOffsetSubBlock, samplesExternal, q, state);
        sampleOffsetSubBlock += samplesExternal;
        if (sampleOffsetSubBlock >= gpuProgram.blocksize1024Fixed) {
            dispatchGpuSynth();
            sampleOffsetSubBlock = 0;
            readOffsetSubBlock = 0;
        }
        if (numBlocksExternal > 1) {
            dbgassert(out->samples == blockSizeExternal);
            dbgassert(readOffsetSubBlock + samplesExternal <= audioOutputBuffer.samples);
            auto subBlockSrc = audioOutputBuffer.SubSamplesBlock(readOffsetSubBlock, samplesExternal);
            // out->clear();
            // out->fillNoise(synthRand, 0.2);
            // out->addFromOp(&subBlockSrc, mix_op::ADD, 1.0);
            out->copyFrom(&subBlockSrc);
            readOffsetSubBlock += samplesExternal;
            if (readOffsetSubBlock >= audioOutputBuffer.samples) {
                readOffsetSubBlock = 0;
            }
        } else {
            auto subBlock = out->SubSamplesBlock(block * audioOutputBuffer.samples, audioOutputBuffer.samples);
            dbgassert(subBlock.samples > 0 && subBlock.samples >= samplesExternal);
            subBlock.copyFrom(&audioOutputBuffer);
        }
    }
}
void SynthImplGPU::processGpuSynthInput(const DAW::Host::Host* const host, double tick, double samplePos, samplecount_t sampleOffsetInt, samplecount_t sampleOffsetExt, samplecount_t numSamples, DAW::Host::ProcessingQuality quality, playback_state state) {
    const bool bIsBenchmark        = gDebugBenchmarkFlags & 1;
    const bool bDbgSkipBufferBuild = gDebugBenchmarkFlags & 2;

    const int nOversample         = 1;
    int framesPerAutomationUpdate = state == playback_state::status_render ? 1 : 8;

    const auto voiceMode     = GetParamEnum(Parameters::VoiceMode)->getEnumValue<VoiceModes>();

    double osc1_filter          = 0.0;
    double osc1_filter_keytrack = 0.1;
    double osc1_stereo          = 0.0;

    auto& inputBufferSynthState  = ssboInputSynthState.buffer;
    auto& inputBufferVoiceStates = ssboInputVoiceStates.buffer;
    const bool bHasAutomationOrModulation = true;// TODO: implement
    const double songPosSeconds = oneOverSR * samplePos;
    for (size_t i = 0; i < lfosSongPos.size(); i++) {
        auto& lfoSongPos = lfosSongPos[i];
        lfoSongPos.setPhase(lfoSongPos.getParameters().phaseOffset);
        lfoSongPos.Update(songPosSeconds);
    }
    size_t numActiveVoicesMax = 0;
    for (samplecount_t s = 0; !bDbgSkipBufferBuild && s < numSamples; s++) {
        auto polyCount = size_t(std::count_if(std::cbegin(voices), std::cend(voices), [](auto& v) { return v.bIsActive; }));
        if (polyCount > numActiveVoicesMax) {
            numActiveVoicesMax = polyCount;
        }
        if (!polyCount) {
            // gpuContext.time_sample_phase_reset = samplePos + s;
        }

        const auto tickPos = tick + sampleToTickConvert<double, roundmode::none>(s, tempo.bpm100, host->m_sampleFormatInternal.sampleRate * nOversample);
        if (s % nOversample == 0) {
            ProcessMidiSample(*this, voices, voiceMode, sampleOffsetExt + sampleOffsetInt + s, tickPos, userLimitPolyVoices);
        }
        if (host && moduleSynthInstance && (s % framesPerAutomationUpdate) == 0) {
            this->moduleInstance->updateAutomatedParameters(host, tick, state);
        }
        for (size_t i = 0; i < lfoParameters.size(); i++) {
            updateLFOParameters(lfoParameters[i], i);
        }
        if (bHasAutomationOrModulation || s == 0) {
            // osc1_filter = GetParamFloat(Parameters::Osc1Filter)->getAsDoubleModulated();
            osc1_stereo          = GetParamFloat(Parameters::Osc1Stereo)->getAsDoubleModulated();
            osc1_filter_keytrack = GetParamFloat(Parameters::Osc1KeytrackFilter)->getAsDoubleModulated();
        }

        inputBufferSynthState[sampleOffsetExt + s + gpuProgram.blocksize1024Fixed * 0] = float(osc1_filter_keytrack);
        inputBufferSynthState[sampleOffsetExt + s + gpuProgram.blocksize1024Fixed * 1] = float(osc1_stereo);

        const auto coarse = GetParamInt(Parameters::Osc1Coarse)->Value();
        const auto fine   = GetParamFloat(Parameters::Osc1Fine)->Value();
        for (size_t i = 0; i < lfosSongPos.size(); i++) {
            lfosSongPos[i].Update(oneOverSR);
        }
        for (int64_t i = 0; i < userLimitPolyVoices && i < allocatedVoiceCount; i++) {
            auto& v = voices[i];
            if (v.bIsActive) {
                if (minVoiceIdx < 0 || i < minVoiceIdx) {
                    minVoiceIdx = i;
                }
                if (maxVoiceIdx < 0 || i > maxVoiceIdx) {
                    maxVoiceIdx = i;
                }
                for (size_t l = 0; l < v.lfos.size(); l++) {
                    auto& lfo = v.lfos[l];
                    lfo.Update(oneOverSR);
                }
                ModulationSourceData modSrcData{};
                updateVoiceModulations(modSrcData, v, tickPos);
                updateEnvelopeParameters(v);
                auto& modVals = v.modValues;
                for (size_t j = 0; j < modulationValuesMax.size() && j < modVals.size(); j++) {
                    modulationValuesMax[j] = math::max(modulationValuesMax[j], modVals[j]);
                    modulationValuesMin[j] = math::min(modulationValuesMin[j], modVals[j]);
                }
                // bool bIsFirst = v.GetVolumeEnvelope().stage == EnvelopeStages::Triggered;
                for (auto& env : v.envelopes) {
                    env.Update(oneOverSR);
                }
                osc1_filter              = GetParamFloat(Parameters::Osc1Filter)->getAsDoubleModulated() + v.modValues[ModDestinations::ModDest_Osc1Filter];
                const auto osc1Tune      = pitchFactor(coarse + fine);
                const auto baseFrequency = v.frequency * v.pitchBend;
                const auto osc1Frequency = osc1Tune * baseFrequency;
                double volEnv            = v.GetVolumeEnvelope().value;
                if (v.noteT.len > 0 && otherParams[0] > 0.0f) {
                    double noteFade = 1.0 + (modSrcData[14] - 1.0) * otherParams[0];
                    volEnv *= noteFade;
                }
                const double masterVolume = GetParamFloat(Parameters::MasterVolume)->getAsDoubleModulated(v.modValues[ModDestinations::ModDest_MasterVolume]);
                float masterGain = 0.0;
                dsp_util::getGainLvl(float(masterVolume), masterGain);
                const double osc1GainParam = GetParamFloat(Parameters::Osc1Gain)->getAsDoubleModulated(v.modValues[ModDestinations::ModDest_Osc1Gain]);
                float osc1Gain = 0.0;
                dsp_util::getGainLvl(float(osc1GainParam), osc1Gain);
                auto velocity = (osc1Gain+v.modValues[ModDestinations::ModDest_Osc1Gain]);
                velocity *= v.velocity;
                velocity *= volEnv;
                velocity *= masterGain;

                const auto idx_base     = sampleOffsetExt + i * (NUM_VOICE_INPUT_PARAMETERS * gpuProgram.blocksize1024Fixed);
                const auto idx_velocity = idx_base + s;
                const auto idx_pitch    = idx_base + gpuProgram.blocksize1024Fixed * 1 + s;
                const auto idx_filter   = idx_base + gpuProgram.blocksize1024Fixed * 2 + s;
                const auto idx_detune   = idx_base + gpuProgram.blocksize1024Fixed * 3 + s;
                const auto idx_detune_keytrack       = idx_base + gpuProgram.blocksize1024Fixed * 4 + s;
                inputBufferVoiceStates[idx_velocity] = float(velocity);
                inputBufferVoiceStates[idx_pitch]    = float(osc1Frequency);
                inputBufferVoiceStates[idx_filter]   = float(1.0 - osc1_filter);
                inputBufferVoiceStates[idx_detune]   = float(v.unisonDetune);
                inputBufferVoiceStates[idx_detune_keytrack] = float(v.unisonDetuneKeytrack);
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
        }
    }
    // dbgassert(midiQueue.Empty());

    if (numActiveVoicesMax > this->numActiveVoicesMax) {
        this->numActiveVoicesMax = numActiveVoicesMax;
#ifndef NDEBUG
        if (!bIsBenchmark) {
            log_lf(Log::L_WARN, "Max poly count seen: %zu\n", numActiveVoicesMax);
        }
#endif
    }

    // move this up and find out max across all subblocks
    this->numActiveVoicesBlock = numActiveVoicesMax;

}
void SynthImplGPU::dispatchGpuSynth() {
    const bool bIsBenchmark = gDebugBenchmarkFlags & 1;
    const bool bDbgSkipGPUDispatch = gDebugBenchmarkFlags & 4;
    const int32_t programMax = math::max(1, CtrSize(gpuProgram.programs));
    const int32_t programId  = currentProgramId % programMax;
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
        if (bUseMemoryBarriers) {
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        }
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
        if (bUseGlFinish) {
            glFinish();
        }
        if (bUseMemoryBarriers) {
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT|GL_BUFFER_UPDATE_BARRIER_BIT);
        }
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

    AudioBlock* output = &this->audioOutputBuffer;
    if (!assert_expr(output->samples == gpuProgram.blocksize1024Fixed)) {
        return;
    }
    const auto& outputBuffer = ssboOutput.buffer;
    const samplecount_t numsamples = math::min(output->samples, samplecount_t(outputBuffer.size()));
    for (samplecount_t ch = 0; ch < NUM_AUDIO_CHANNELS; ch++) {
        auto bufChannel = output->buf[ch];
        for (samplecount_t sampleIdx = 0; sampleIdx < numsamples; sampleIdx++) {
            float val        = outputBuffer[sampleIdx + ch * gpuProgram.blocksize1024Fixed];
            float hardClipAt = 2.5;
            if (fp_math::isNanOrInfd(val)) {
                val = 0;
            } else if (val < -hardClipAt) {
                val = -hardClipAt;
            } else if (val > hardClipAt) {
                val = hardClipAt;
            }
            bufChannel[sampleIdx] = val;
        }
    }
    if (bIsBenchmark) {
        return;
    }
    auto tmNow_ms = getTimeMillis();
    auto tmTotal_ms = perfTimer.getTimeDoubleReset() * 1000.0;
#ifndef NDEBUG
    if (tmNow_ms - timePerfLog >= 10000 || tmTotal_ms > timeComputeAvg * 10.0) {
        if (timeComputeAvg < 0.0) {
            if (tmNow_ms - timePerfLog > 1500)
                timeComputeAvg = tmTotal_ms;
        } else {
            log_lf(Log::L_WARN, "gpu_compute_test: %f ms avg: %f ms - voices: %zu max %zu [IDX %zd - %zd]\n", tmTotal_ms, timeComputeAvg, numActiveVoicesBlock, numActiveVoicesMax, minVoiceIdx, maxVoiceIdx);
            // print sample 0
            auto sample0 = outputBuffer[0];
            log_lf(Log::L_WARN, "sample 0: %f\n", sample0);
        }
        timePerfLog = tmNow_ms;
    }
#endif
    timeComputeAvg = 0.95 * timeComputeAvg + 0.05 * tmTotal_ms;
}

}// namespace PluginSynth::GPU

namespace PluginSynth::GPU {

module_synth_gpu::module_synth_gpu(int32_t _projectGlobalId, IHostCallback* _hostCallback)
    : module_synth_template<SynthImplGPU>(new SynthType(this), "Synth GPU", _projectGlobalId, _hostCallback) {
    bCanReceiveMidi = true;
    isSynth         = true;
    for (const auto& paramEntry : vecParams) {
        if (!paramEntry)
            continue;
        int idx                       = PARAM_OFFSET_IMPL + (&paramEntry - &vecParams.front());
        automatable_param_t* regparam = registerParam(idx);
        dbgassert(regparam && regparam->idx > 0);
        regparam->setInitial(float(paramEntry->getAsDouble()));
        regparam->extensiveName = paramEntry->name;
        regparam->name          = paramEntry->shortName;
        regparam->shortLabel    = paramEntry->hierarchicalName;
        regparam->unit          = paramEntry->unit;
        switch (paramEntry->type) {
            case SynthParam::ParamType::FLOAT:
                break;
            case SynthParam::ParamType::INT:
            case SynthParam::ParamType::ENUM:
                auto paramInt = dynamic_cast<SynthParam_Int*>(paramEntry);
                dbgassert(paramInt);
                auto params                 = paramInt->iMax - paramInt->iMin;
                regparam->quantizationSteps = params;
                break;
        }
        using P = ParametersSynthGPU;
        switch (paramEntry->enumParam) {
            case P::Osc1Fine:
            case P::Osc1Coarse:
            case P::Osc1UnisonDetune:
            case P::Osc1PulseWidth:
            case P::ADSR_1_A_Shape:
            case P::ADSR_1_D_Shape:
            case P::ADSR_1_R_Shape:
            case P::ADSR_2_A_Shape:
            case P::ADSR_2_D_Shape:
            case P::ADSR_2_R_Shape:
                regparam->isBiPolar = true;
                break;
            default:
                break;
        }
    }
    impl->init();
}

param_unit_t module_synth_gpu::convertParamValueToDisplay(int32_t idx, float value) {
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
            };
            auto it = std::find(enumDurList.begin(), enumDurList.end(), param->enumParam);
            if (it != enumDurList.end()) {
                auto idx = int32_t(it - enumDurList.begin());
                alignas(64) float envParamVals[8]{};
                alignas(64) float envParamValsScaled[8]{};
                envParamVals[0] = value;
                ShapeLogLikeSIMD<float>(envParamVals, envParamValsScaled);
                auto& envTimeRanges = impl->getEnvTimeRanges();
                auto ms  = Envelope::GetSecondsFromParam(envParamValsScaled[0], envTimeRanges[idx%4]) * 1000.0;
                auto fmt = ms < 10.0 ? "%.3f" : (ms < 100.0 ? "%.2f" : "%.1f");
                auto v   = StringFormat(fmt, ms);
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
                return { lfoRateStr, lfoParams.syncFlags != 0 ? "" : param->unit };
            }
        }
    }
    return module_synth_template<SynthImplGPU>::convertParamValueToDisplay(idx, value);
}

param_converted_t module_synth_gpu::convertParamValueDisplay(int32_t idx, const param_unit_t& displayValue) {
    const auto idxInternal = idx - PARAM_OFFSET_IMPL;
    if (isValidParamIdx(idxInternal)) {
        SynthParamBase* param = vecParams[idxInternal];
        if (param->enumParam == ParametersSynthGPU::Osc1PulseWidthModRate) {
            auto fTextFieldVal = float(atof(StringAsCStr(displayValue.value)));
            double parsed      = math::clamp(log2(fTextFieldVal * 100.0) / 21.0, 0.0, 1.0);
            return { float(parsed), true };
        }
        if (param && (param->enumParam == ParametersSynthGPU::LFO_1_Frequency || param->enumParam == ParametersSynthGPU::LFO_2_Frequency)) {
            auto& lfoParams = impl->getLFOParams(param->enumParam == ParametersSynthGPU::LFO_1_Frequency ? 0 : 1);
            if (lfoParams.syncFlags) {
                auto numSyncRatios = CtrSize(lfoParams.syncRatios);
                for (int32_t i = 0; i < numSyncRatios; ++i) {
                    if (lfoParams.syncRatios[i].text == displayValue.value) {
                        return { ((i) / float(numSyncRatios - 1)), true };
                    }
                    if (lfoParams.syncRatios[i].text == displayValue.value + "/1") {
                        return { ((i) / float(numSyncRatios - 1)), true };
                    }
                }
            } else {
                auto fTextFieldVal = float(atof(StringAsCStr(displayValue.value)));
                return { math::clamp(DAW::LFO::RateToParam(fTextFieldVal), 0.0f, 1.0f), true };
            }
        }
    }
    return module_synth_template<SynthImplGPU>::convertParamValueDisplay(idx, displayValue);
}

void module_synth_gpu::processMidi(midi_data_processing_t& midiEvents) {
    const double tickToSamples = tickToSampleConvert<double, roundmode::none>(1.0, midiEvents.bpm100, format.sampleRate);
    std::vector<IMidiMsg> messages; // TODO: get rid of heap allocation
    messages.reserve(midiEvents.noteEvents->size());
    auto synth = getSynth();
    auto currentSampleOffset = int32_t(synth->getProcessingSubBlockSampleMidiWriteOffset());
    for (auto& evt : *midiEvents.noteEvents) {
        auto deltaFrames = math::floordS32(evt.tickOffsetInBlock * tickToSamples) + currentSampleOffset;
        dbgassert(deltaFrames >= currentSampleOffset && deltaFrames < currentSampleOffset + format.blockSize);
        bool bContained = std::binary_search(std::begin(heldNotes), std::end(heldNotes), evt.pitch);
        if (evt.isNoteOn && !bContained) {
            insertSorted(heldNotes, evt.pitch);
        } else if (!evt.isNoteOn && bContained) {
            removeEntry(heldNotes, evt.pitch);
        }

        messages.emplace_back();
        IMidiMsg& msg = messages.back();
        if (evt.isNoteOn) {
            msg.MakeNoteOnMsg(evt.pitch, evt.velocity, deltaFrames, evt.channel);
        } else {
            msg.MakeNoteOffMsg(evt.pitch, deltaFrames, evt.channel);
        }
        msg.note = evt.note;
    }
    for (auto& evt : *midiEvents.ctrlEvents) {
        auto offsetInBlock = math::floordS32((evt.tick - midiEvents.tickLatencyCompensated) * tickToSamples) + currentSampleOffset;
        if (offsetInBlock < 0 || offsetInBlock >= format.blockSize) {
            log_lf(Log::L_WARN, "ctrl event out of range: %d\n", offsetInBlock);
            continue;
        }
        messages.push_back(IMidiMsg::FromU32AndTick(evt.message, offsetInBlock));
    }
    if (!messages.empty()) {
        std::sort(std::begin(messages), std::end(messages), [](const IMidiMsg& a, const IMidiMsg& b) {
            return a.mOffset < b.mOffset;
        });
    }
    processMidiMessages(messages);
    this->midiEventsDispatched += CtrSize(messages);
}

}// namespace PluginSynth::GPU

template<>
effectbase* makeInstance<PluginSynth::GPU::module_synth_gpu>(int32_t _projectGlobalId, IHostCallback* _hostCallback) {
    return new PluginSynth::GPU::module_synth_gpu(_projectGlobalId, _hostCallback);
}

#include "assert_dbg.h"
#include "config.hpp"
#include "event.hpp"
#include "gui/container/container.hpp"
#include "gui/contextmenu/contextmenu_base.hpp"
#include "host/plugin/modules.hpp"
#include "math/seq_math.hpp"
#include "note.hpp"
#include "rand.hpp"
#include "seq_time.hpp"
#include "plugins/synth/synth-snapshot.hpp"
#include "plugins/synth/synth-template.hpp"
#include "plugins/synth/synth-types.hpp"
#include "types.hpp"
#include <cstdint>
#include <cstring>
#include <nanovg.h>
#include <nanovg_min.h>
#include <numbers>
#include <utility>


namespace PluginSynth::KickXP {
    static const std::vector<float>& GetThumpData() {
        static std::vector<float> thumpdata1;
        if (thumpdata1.empty()) {
            thumpdata1.resize(1024);
            for (int i = 0; i < 1024; i++)
                thumpdata1[i] = float(sin(1.37*i + 0.1337*(1024 - i)*sin(1.1*i))*pow(1.0 / 256.0, i / 1024.0));
        }
        return thumpdata1;
    }
    enum VoiceState : uint8_t {
        VOICE_IDLE = 0,
        VOICE_HOLD = 1,
        VOICE_DECAY = 2,
        VOICE_KILLED = 3,
    };
    struct VoiceSynth {
        VoiceState state = VOICE_IDLE;
        double velocity = 0.0;
        int32_t seqNr = 0;
        note_t noteT = {};
        double frequency       = 0.0;
        double targetFrequency = 0.0;
        double pitchBend       = 1.0;
        bool bIsActive         = false;

        float PitchLimit = 0.0f;
        float ThisPitchLimit = 0.0f;
        float StartFrq = 0.0f;
        float ThisStartFrq = 0.0f;
        float EndFrq = 0.0f;
        float ThisEndFrq = 0.0f;
        float TDecay = 0.0f;
        float ThisTDecay = 0.0f;
        float TShape = 0.0f;
        float ThisTShape = 0.0f;
        float DSlope = 0.0f;
        float ThisDSlope = 0.0f;
        float DTime = 0.0f;
        float ThisDTime = 0.0f;
        float RSlope = 0.0f;
        float ThisRSlope = 0.0f;
        float BDecay = 0.0f;
        float ThisBDecay = 0.0f;
        float CDecay = 0.0f;
        float ThisCDecay = 0.0f;
        float CurVolume = 0.0f;
        float ThisCurVolume = 0.0f;
        float ClickAmt = 0.0f;
        float PunchAmt = 0.0f;
        float BuzzAmt = 0.0f;
        float Amp = 0.0f;
        float DecAmp = 0.0f;
        float BAmp = 0.0f;
        float MulBAmp = 0.0f;
        float CAmp = 0.0f;
        float MulCAmp = 0.0f;
        float Frequency = 0.0f;

        double xSin = 0.0;
        double xCos = 0.0;
        double dxSin = 0.0;
        double dxCos = 0.0;

        samplecount_t EnvPhase = 0;
        samplecount_t Age = 0;
        double OscPhase = 0.0;

        bool isVoiceActive() const {
            return bIsActive && state > VOICE_IDLE;
        }

        bool isNotReleased() const {
            return state == VOICE_HOLD;
        }

        bool IsReleased() const { return state > VOICE_HOLD; }
        double GetVolume() const { return Amp; }

        void SetNote(const note_t& n) {
            noteT           = n;
            targetFrequency = pitchToFrequency(noteT.pitch);
        }

        void SetPitchBendFactor(double f) { pitchBend = f; }
        void ResetPitch() { frequency = targetFrequency; }
        void SetVelocity(double v) { velocity = v; }
        void Release() {
            if (state != VOICE_IDLE && state < VOICE_DECAY)
                state = VOICE_DECAY;
        }

        void Start(bool bTriggerMono, double velocity) {
            SetVelocity(velocity);
            bIsActive = true;
            this->state = VOICE_HOLD;
            this->EnvPhase = 0;
            this->OscPhase = this->ClickAmt;
            this->Age = 0;
            this->Amp = 32;
            this->CurVolume = this->velocity;
            this->ThisPitchLimit = this->PitchLimit;
            this->ThisDTime = this->DTime;
            this->ThisDSlope = this->DSlope;
            this->ThisRSlope = this->RSlope;
            this->ThisBDecay = this->BDecay;
            this->ThisCDecay = this->CDecay;
            this->ThisTDecay = this->TDecay;
            this->ThisTShape = this->TShape;
            this->ThisStartFrq = this->StartFrq;
            this->ThisEndFrq = this->EndFrq;
            this->ThisCurVolume = this->CurVolume;
        }
    };
}// namespace PluginSynth::KickXP
namespace PluginSynth::KickXP {
    enum ParametersSynthKickXP {
        MasterVolume = 0,
        kStartFrq,
        kEndFrq,
        kBuzzAmt,

        kClickAmt,
        kPunchAmt,
        kToneDecay,
        kToneShape,
        kBDecay,

        kCDecay,
        kDecSlope,
        kDecTime,
        kRelSlope,
    };

    struct ui_layout_t {
        int32_t uiId = 0;
    };
    struct snapshot_t {
        int32_t version = 0;
        std::vector<param_float_snapshot_t> params;
        std::vector<ui_layout_t> uiLayout;
    };
    static constexpr int32_t SYNTH_KICKXP_SNAPSHOT_VERSION = 1;

    std::shared_ptr<std::vector<std::byte>> serializeSnapshot(const snapshot_t& snapshot) {
        dbgassert(snapshot.version == SYNTH_KICKXP_SNAPSHOT_VERSION);
        auto shrdHeapVec = std::make_shared<std::vector<std::byte>>();
        shrdHeapVec->resize(256);
        DAW::ByteBuffer::stream_write<std::vector<std::byte>> out{ *shrdHeapVec, 0 };
        out.write(size_t(0));
        out.write(snapshot.version);
        out.write(size_t{ snapshot.params.size() });
        out.write(size_t{ snapshot.uiLayout.size() });
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
        size_t dataSize    = data->size();
        size_t dataSizeHdr = 0;
        if (!in.read(dataSizeHdr))
            return false;
        if (dataSizeHdr > dataSize)
            return false;
        in.read(snapshot.version);
        if (snapshot.version > SYNTH_KICKXP_SNAPSHOT_VERSION)
            return false;
        size_t numParams    = 0;
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

    class
            SynthImplKickXP final : public ::PluginSynth::SynthImpl<PluginSynth::KickXP::SynthImplKickXP, PluginSynth::KickXP::ParametersSynthKickXP> {
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
            addFloatParam(Parameters::MasterVolume)->setRange(0.0, 1.0)->setInitialValue(0.9);
            setParamName(getParam(Parameters::MasterVolume), "Master Volume", "MasterVol", "Volume", "", "%.2f");
            addFloatParam(Parameters::kStartFrq)->setRange(1.0, 240.0)->setInitialValue(99.0);
            setParamName(getParam(Parameters::kStartFrq), "Start Frequency", "StartFrq", "Freq" "Hz");
            addFloatParam(Parameters::kEndFrq)->setRange(1.0, 240.0)->setInitialValue(66.0);
            setParamName(getParam(Parameters::kEndFrq), "End Frequency", "EndFrq", "Freq" "Hz");
            addFloatParam(Parameters::kBuzzAmt)->setRange(0.0, 100.0)->setInitialValue(0.0);
            setParamName(getParam(Parameters::kBuzzAmt), "Buzz", "BuzzAmt", "Buzz", "%");
            addFloatParam(Parameters::kClickAmt)->setRange(0.0, 100.0)->setInitialValue(0.0);
            setParamName(getParam(Parameters::kClickAmt), "Click", "ClickAmt", "Click", "%");
            addFloatParam(Parameters::kPunchAmt)->setRange(0.0, 100.0)->setInitialValue(17.0);
            setParamName(getParam(Parameters::kPunchAmt), "Punch", "PunchAmt", "Punch", "%");
            addFloatParam(Parameters::kToneDecay)->setRange(1.0, 240.0)->setInitialValue(104.0);
            setParamName(getParam(Parameters::kToneDecay), "Tone decay rate", "ToneDecR", "ToneDecay", "");
            addFloatParam(Parameters::kToneShape)->setRange(1.0, 240.0)->setInitialValue(17.0);
            setParamName(getParam(Parameters::kToneShape), "Tone decay shape", "ToneShape", "ToneDecay", "");
            addFloatParam(Parameters::kBDecay)->setRange(1.0, 240.0)->setInitialValue(69.0);
            setParamName(getParam(Parameters::kBDecay), "Buzz decay rate", "BDecay", "BuzzDecay", "");
            addFloatParam(Parameters::kCDecay)->setRange(1.0, 240.0)->setInitialValue(164.0);
            setParamName(getParam(Parameters::kCDecay), "Click+Punch decay rate", "CDecay", "C+PDecay", "");
            addFloatParam(Parameters::kDecSlope)->setRange(1.0, 240.0)->setInitialValue(94.0);
            setParamName(getParam(Parameters::kDecSlope), "Amplitude decay slope", "DecSlope", "DecaySlope", "");
            addFloatParam(Parameters::kDecTime)->setRange(1.0, 240.0)->setInitialValue(41.0);
            setParamName(getParam(Parameters::kDecTime), "Amplitude decay time", "DecTime", "DecayTime", "");
            addFloatParam(Parameters::kRelSlope)->setRange(1.0, 240.0)->setInitialValue(139.0);
            setParamName(getParam(Parameters::kRelSlope), "Amplitude release slope", "RelSlope", "ReleaseSlope", "");
        }

        module_synth_template<SynthImplKickXP>* const moduleSynthInstance;
        std::array<VoiceSynth, 32> voices;
        std::vector<std::shared_ptr<PluginViewContainer>> views;

    public:
        explicit SynthImplKickXP(module_synth_template<SynthImplKickXP>* module);

        ~SynthImplKickXP() {
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
        }

        void StartVoice(VoiceSynth& v, VoiceModes mode, double velocity) {
            bool bIsMonoTrigger = false;
            if (mode == VoiceModes::Mono) {
                bIsMonoTrigger = true;
            } else if (mode == VoiceModes::Legato) {
                bIsMonoTrigger = v.isNotReleased();
            }
            v.StartFrq = (float)(33.0*pow(128, GetParamFloat(Parameters::kStartFrq)->Value() / 240.0));
            v.EndFrq = (float)(33.0*pow(16, GetParamFloat(Parameters::kEndFrq)->Value() / 240.0));
            v.TDecay = (float)((GetParamFloat(Parameters::kToneDecay)->Value() / 240.0)*(1.0 / 400.0)*(44100.0 / 44100.0));
            v.TShape = (float)(GetParamFloat(Parameters::kToneShape)->Value() / 240.0);
            v.DSlope = (float)pow(20, GetParamFloat(Parameters::kDecSlope)->Value() / 240.0 - 1) * 25 / 44100.0;
            v.DTime = (float)(GetParamFloat(Parameters::kDecTime)->Value()*44100.0 / 240.0);
            v.RSlope = (float)pow(20, GetParamFloat(Parameters::kRelSlope)->Value() / 240.0 - 1) * 25 / 44100.0;
            v.BDecay = (float)(GetParamFloat(Parameters::kBDecay)->Value() / 240.0);
            v.CDecay = (float)(GetParamFloat(Parameters::kCDecay)->Value() / 2240.0);
            v.ClickAmt = (float)(GetParamFloat(Parameters::kClickAmt)->Value() / 100.0);
            v.BuzzAmt = 3 * (float)(GetParamFloat(Parameters::kBuzzAmt)->Value() / 100.0);
            v.PunchAmt = (float)(GetParamFloat(Parameters::kPunchAmt)->Value() / 100.0);
            v.PitchLimit = (float)(440.0*pow(2, (v.noteT.pitch - 69) / 12.0));
            v.Start(bIsMonoTrigger, velocity);
        }

        bool ProcessVoice(VoiceSynth *trk, float *pout, samplecount_t offset, float gain)
        {
            trk->OscPhase = fmod(trk->OscPhase, 1.0);
            float Ratio = trk->ThisEndFrq / trk->ThisStartFrq;
            
            double xSin = trk->xSin, xCos = trk->xCos;
            double dxSin = trk->dxSin, dxCos = trk->dxCos;
            float Amp = trk->Amp;
            float DecAmp = trk->DecAmp;
            float BAmp = trk->BAmp;
            float MulBAmp = trk->MulBAmp;
            float CAmp = trk->CAmp;
            float MulCAmp = trk->MulCAmp;
            float Vol = 0.5f*trk->ThisCurVolume*gain;
            bool amphigh = Amp >= 16;
            auto Age = trk->Age;
            auto sr = getSamplerate();
            auto odsr = oneOverSR;
            auto& ThumpData = GetThumpData();
            auto ThDataSize = samplecount_t(ThumpData.size());
            
            double EnvPoint = trk->EnvPhase*trk->ThisTDecay;
            double ShapedPoint = pow(EnvPoint, trk->ThisTShape*2.0);
            trk->Frequency = (float)(trk->ThisStartFrq*pow((double)Ratio, ShapedPoint));
            if (trk->Frequency>10000.f) trk->EnvPhase = 6553600;
            if (trk->EnvPhase<trk->ThisDTime)
            {
                trk->DecAmp = DecAmp = trk->ThisDSlope;
                trk->Amp = Amp = (float)(1 - DecAmp*trk->EnvPhase);
            }
            else
            {
                DecAmp = trk->ThisDSlope;
                Amp = (float)(1 - DecAmp*trk->ThisDTime);
                if (Amp>0)
                {
                    trk->DecAmp = DecAmp = trk->ThisRSlope;
                    trk->Amp = Amp = Amp - DecAmp*(trk->EnvPhase - trk->ThisDTime);
                }
            }
            if (trk->Amp <= 0)
            {
                trk->Amp = 0;
                trk->DecAmp = 0;
                return amphigh;
            }

            trk->BAmp = BAmp = trk->BuzzAmt*(float)(pow(1.0f / 256.0f, trk->ThisBDecay*trk->EnvPhase*(odsr * 10)));
            float CVal = (float)(pow(1.0f / 256.0f, trk->ThisCDecay*trk->EnvPhase*(odsr * 20)));
            trk->CAmp = CAmp = trk->ClickAmt*CVal;
            trk->Frequency *= (1 + 2 * trk->PunchAmt*CVal*CVal*CVal);
            if (trk->Frequency>10000) trk->Frequency = 10000;
            if (trk->Frequency<trk->ThisPitchLimit) trk->Frequency = trk->ThisPitchLimit;

            trk->MulBAmp = MulBAmp = (float)pow(1.0f / 256.0f, trk->ThisBDecay*(10 * odsr));
            trk->MulCAmp = MulCAmp = (float)pow(1.0f / 256.0f, trk->ThisCDecay*(10 * odsr));
            xSin = (float)sin(2.0*std::numbers::pi*trk->OscPhase);
            xCos = (float)cos(2.0*std::numbers::pi*trk->OscPhase);
            dxSin = (float)sin(2.0*std::numbers::pi*trk->Frequency / sr);
            dxCos = (float)cos(2.0*std::numbers::pi*trk->Frequency / sr);
            trk->dxSin = dxSin, trk->dxCos = dxCos;
            
            if (Amp>0.00001f && Vol>0)
            {
                amphigh = true;
                float OldAmp = Amp;
                if (BAmp>0.01f)
                {
                    pout[offset] += float(float(Amp*Vol*xSin));
                    if (xSin>0)
                    {
                        float D = (float)(Amp*Vol*BAmp*xSin*xCos);
                        pout[offset] -= D;
                    }
                    double xSin2 = double(xSin*dxCos + xCos*dxSin);
                    double xCos2 = double(xCos*dxCos - xSin*dxSin);
                    xSin = xSin2; xCos = xCos2;
                    Amp -= DecAmp;
                    BAmp *= MulBAmp;
                } else {
                    pout[offset] += float(float(Amp*Vol*xSin));
                    double xSin2 = double(xSin*dxCos + xCos*dxSin);
                    double xCos2 = double(xCos*dxCos - xSin*dxSin);
                    xSin = xSin2; xCos = xCos2;
                    Amp -= DecAmp;
                }
                if (OldAmp>0.1f && CAmp>0.001f)
                {
                    auto ThDataIdx = Age >= ThDataSize ? ThDataSize - 1 : Age;
                    float LVal2 = OldAmp*Vol*CAmp*ThumpData[ThDataIdx];
                    {
                        pout[offset] += LVal2;
                        OldAmp -= DecAmp;
                        CAmp *= MulCAmp;
                        Age++;
                    }
                }
            }
            if (Amp != 0.0f)
            {
                trk->OscPhase += trk->Frequency / sr;
                trk->EnvPhase += 1;
            }

            trk->xSin = xSin, trk->xCos = xCos;
            trk->Amp = Amp;
            trk->BAmp = BAmp;
            trk->CAmp = CAmp;
            trk->Age = Age;
            return amphigh;
        }

        void ProcessSynth(AudioBlock* in, AudioBlock* out, int nFrames, const DAW::Host::Host* const host, double tick, double samplePos, playback_state state) override {
            // lockProcessing only locks VST2 versions of the plugin
            auto lock = this->lockProcessing();

            float* synthOutputs[2]   = {};
            synthOutputs[0]          = out->buf[0];
            synthOutputs[1]          = out->buf[1];
            samplerate_t nOversample = 1;
            auto bpm100              = host->prjGlobals.tempo100;

            /**
            * framesPerAutomationUpdate 
            * 1 is highest precission, automation is updated every sample
            * this can be lowered to lower CPU load
            */
            int framesPerAutomationUpdate = state == playback_state::status_render ? 1 : 8;
            const auto voiceMode          = VoiceModes::Poly;//GetParamEnum(Parameters::VoiceMode)->getEnumValue<VoiceModes>();
            memset(synthOutputs[0], 0, nFrames * sizeof(float));
            memset(synthOutputs[1], 0, nFrames * sizeof(float));
            for (samplecount_t s = 0; s < nFrames; s++) {
                auto tickPos = tick + sampleToTickConvert<double, roundmode::none>(s, bpm100, host->m_sampleFormatInternal.sampleRate * nOversample);
                if (host && moduleInstance && (s % framesPerAutomationUpdate) == 0) {
                    this->moduleInstance->updateAutomatedParameters(host, tickPos, state);
                }

                if (s % nOversample == 0) {
                    ProcessMidiSample(*this, voices, voiceMode, s / nOversample, tickPos, voices.size());
                }

                for (auto& v : voices) {
                    if (v.isVoiceActive()) {
                        bool bActive = ProcessVoice(&v, synthOutputs[0], s, 1.0f);
                        float fGain = GetParamFloat(Parameters::MasterVolume)->Value();
                        synthOutputs[0][s] = fp_math::silenceNanInff(synthOutputs[0][s] * fGain);
                        // sum in second channel
                        synthOutputs[1][s] += synthOutputs[0][s];
                        if (!bActive) {
                            v.state = VOICE_IDLE;
                            v.bIsActive = false;
                        }
                    }
                }
            }
            // copy second channel to first channel
            for (int i = 0; i < nFrames; i++) {
                synthOutputs[0][i] = synthOutputs[1][i];
            }
        }
        std::shared_ptr<PluginViewContainer> createViewCtrImpl() override;

        bool getSnapshot(snapshot_t& snapshot) const {
            snapshot.version     = SYNTH_KICKXP_SNAPSHOT_VERSION;
            const auto numParams = CtrSize(vecParams);
            snapshot.params.reserve(numParams);
            for (int32_t i = 0; i < numParams; ++i) {
                if (!vecParams[i]) continue;
                snapshot.params.push_back({ i, vecParams[i]->getAsDouble() });
            }
            return true;
        }

        bool setSnapshot(const snapshot_t& snapshot) {
            if (snapshot.version != SYNTH_KICKXP_SNAPSHOT_VERSION) {
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
                OnParamChange(static_cast<SynthImplKickXP::Parameters>(param->enumParam));
            }
            return true;
        }
    };


    class module_synth_kickxp final : public module_synth_template<SynthImplKickXP> {
    public:
        explicit module_synth_kickxp(int32_t _projectGlobalId, IHostCallback* _hostCallback)
            : module_synth_template<SynthImplKickXP>(new SynthType(this), "Kick XP", _projectGlobalId, _hostCallback) {
            bCanReceiveMidi = true;
            isSynth         = true;
            for (const auto& paramEntry : vecParams) {
                if (!paramEntry)
                    continue;
                int idx                       = PARAM_OFFSET_IMPL + (&paramEntry - &vecParams.front());
                automatable_param_t* regparam = registerParam(idx);
                dbgassert(regparam && regparam->idx > 0);
                regparam->setInitial(float(paramEntry->getAsDouble()));
                regparam->name = paramEntry->shortName;
                regparam->unit = paramEntry->unit;
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
                /* using P = ParametersSynthMono;
            switch (paramEntry->enumParam) {
                case P::Osc1Fine:
                case P::Osc1Coarse:
                case P::Panning:
                    regparam->isBiPolar = true;
                    break;
                default:
                    break;
            } */
            }
            impl->init();
        }

        ~module_synth_kickxp() override {
            delete impl;
        }

        PluginType getPluginType() override { return PLUGIN_TYPE_SYNTH_KICKXP; };

        param_converted_t convertParamValueDisplay(int32_t idx, const param_unit_t& displayValue) override {
            const auto idxInternal = idx - PARAM_OFFSET_IMPL;
            if (isValidParamIdx(idxInternal)) {
                SynthParamBase* param = vecParams[idxInternal];
                if (param->enumParam == ParametersSynthKickXP::MasterVolume) {
                    auto fTextFieldVal = static_cast<float>(atof(StringAsCStr(displayValue.value)));
                    return { math::clamp(fTextFieldVal / 100.0f, 0.0f, 1.0f), true };
                }
            }
            return module_synth_template<SynthImplKickXP>::convertParamValueDisplay(idx, displayValue);
        }
        param_unit_t convertParamValueToDisplay(int32_t idx, float value) override {
            const auto idxInternal = idx - PARAM_OFFSET_IMPL;
            if (isValidParamIdx(idxInternal)) {
                SynthParamBase* param = vecParams[idxInternal];
                if (param->enumParam == ParametersSynthKickXP::MasterVolume) {
                    return { StringFormat("%.1f", value * 100.0f), "%" };
                }
            }
            return module_synth_template<SynthImplKickXP>::convertParamValueToDisplay(idx, value);
        }
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
                auto param = getParam(PARAM_OFFSET_IMPL + idx);
                if (assert_expr(param)) {
                    param->setAll(float(vecParams[idx]->getAsDouble()));
                }
            }
        }
        void getUiSnapshot(snapshot_t& snapshot);
        void setUiSnapshot(snapshot_t& snapshot);
    };

    class guicontainer_plugin_kickxp final : public guictr_base {
        module_synth_kickxp* const moduleInstance;
        seq_rand synthRandUI;
        const int buttonScale = 10;

    public:
        explicit guicontainer_plugin_kickxp(module_synth_kickxp* module)
            : moduleInstance(module)
        {
            padding = 0;
            margin  = 0;
            setBackgroundRendered(false);
            setCanMouseHit(true);
        }
        class guictr_module_synth_basic_context_menu final : public guictxtmenu {
            module_synth_kickxp* const moduleInstance;

        public:
            explicit guictr_module_synth_basic_context_menu(module_synth_kickxp* _module)
                : guictxtmenu(), moduleInstance(_module) {
                (void) moduleInstance;
                this->size.x   = 220;
                maxHeight      = 0;
                this->fontSize = FONT_SIZE_CTXT_SMALL;
                this->paddingV = 0;
            }
            bool clickedElement(ctxtmenu_entry* e, int _id) override {
                closeContextMenu();
                return true;
            }
        };

        ~guicontainer_plugin_kickxp() override {
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
            auto shapeEditorSize   = ivec2(math::clamp(size.y, 0, math::max(16, size.x - 64)), size.y);
            auto bHeight           = math::max(64, size.y / buttonScale);
            auto bWidth            = size.x;
            bWidth                 = math::clamp(bWidth, 64, math::min(256, bHeight * 3));
            bWidth                 = math::min(size.x, bWidth);
        }
        void layout() override {
            layoutImpl(size);
            guictr_base::layout();
        }

        void onGuiOpen() {
        }

        void onGuiClose() {
        }
        void setUiLayout(const ui_layout_t& layout) {
        }

        bool getUiLayout(ui_layout_t& layout) const {
            return true;
        }
    };

    class PluginViewContainerSynthKickXP final : public PluginViewContainer {
    public:
        guicontainer_plugin_kickxp ctr_main;
        explicit PluginViewContainerSynthKickXP(module_synth_kickxp* eff)
            : ctr_main(eff) {
        }
        ~PluginViewContainerSynthKickXP() override = default;
        guicontainer_plugin_kickxp& getPluginUI() {
            return ctr_main;
        }
        const guicontainer_plugin_kickxp& getPluginUI() const {
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

    std::shared_ptr<PluginViewContainer> SynthImplKickXP::createViewCtrImpl() {
        if (this->moduleSynthInstance) {
            this->views.push_back(std::make_shared<PluginViewContainerSynthKickXP>(static_cast<module_synth_kickxp*>(this->moduleSynthInstance)));
            return this->views.back();
        }
        return nullptr;
    }

    SynthImplKickXP::SynthImplKickXP(module_synth_template<SynthImplKickXP>* module)
        : SynthImpl<SynthImplKickXP, ParametersSynthKickXP>(module),
          moduleSynthInstance(module) {
        initImpl();
    }


    void module_synth_kickxp::getUiSnapshot(snapshot_t& snapshot) {
        for (auto& view : views) {
            auto implCtrType = dynamic_cast<PluginViewContainerSynthKickXP*>(view.get());
            ui_layout_t layout{};
            if (implCtrType && implCtrType->getPluginUI().getUiLayout(layout)) {
                layout.uiId = view->getUiId();
                snapshot.uiLayout.push_back(layout);
            }
        }
    }

    void module_synth_kickxp::setUiSnapshot(snapshot_t& snapshot) {
        for (auto& uis : snapshot.uiLayout) {
            std::vector<std::shared_ptr<PluginViewContainer>> views;
            getAllViewCtrs(uis.uiId, views);
            for (auto& view : views) {
                auto implCtrType = dynamic_cast<PluginViewContainerSynthKickXP*>(view.get());
                if (implCtrType) {
                    implCtrType->getPluginUI().setUiLayout(uis);
                }
            }
        }
    }
}// namespace PluginSynth::KickXP

template<>
effectbase* makeInstance<PluginSynth::KickXP::module_synth_kickxp>(int32_t _projectGlobalId, IHostCallback* _hostCallback) {
    return new PluginSynth::KickXP::module_synth_kickxp(_projectGlobalId, _hostCallback);
}

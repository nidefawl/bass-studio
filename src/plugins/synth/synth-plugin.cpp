#include <cmath>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <vector>
#include <map>
#include <deque>
#include <memory>
#include <vstsdk-host-2.4/aeffectx.h>
#include "config.h"
#include "gui/controls/list.h"
#include "logging.h"
#include "math/seq_math.h"
#include "rand.h"
#include "seq_util.h"
#include "str_util.h"
#include "dsp_util.h"
#include "color_util.h"

#include "gui/gui.h"
#include "gui/container/container.h"
#include "gui/plugin/pluginviewcontainers.h"
#include "gui/controls/button.h"
#include "gui/controls/knob.h"
#include "gui/controls/inputfield.h"
#include "gui/controls/knobpluginparam.h"
#include "gui/container/container.h"
#include "gui/contextmenu/contextmenu_daw.h"
#include "gui/table/table.h"

#include "basectrl.h"

#include "platform.h"

#include "../plugin.h"
#include "synth-plugin.h"
#include "plugins/plugin.h"
#include "plugins/plugin-base.h"
#include "plugins/plugin-window.h"
#include "vstsdk-plugin-2.4/audioeffect.h"
#include "vstsdk-plugin-2.4/audioeffectx.h"
#include "audioblock.h"
#include "midi-defs.h"
#include "IPlugMidi.h"

#if BUILD_EXTERNAL_PLUGIN
AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {
    return PluginSynth::createPlugin(audioMaster);
}
#endif

namespace PluginSynth {
    const char* const PLUGIN_EFFECT_NAME = "Synth";
    const char* const PLUGIN_UID = "SYNT";
    const char* const PLUGIN_PRODUCT_NAME = "Synth VST2.4";
    enum class Waveforms : int32_t {
        Sine = 0,
        Triangle,
        Saw,
        Square,
        Pulse,
        Noise,
        NumWaveforms
    };
    std::vector<String> stringsWaveform = {
        "Sine", "Triangle", "Saw", "Square", "Pulse", "Noise"
    };
    static constexpr int NUM_POLY_VOICES = 32;
    static constexpr int NUM_UNISON_VOICES = 3;
    
    struct HostTempo {
        double barPos;
        double bpm;
        double ppqPos;
    };

    struct SmoothSwitch {
        double current  = 0.0;
        double previous = 0.0;
        double mix      = 1.0;
        bool switching  = false;

        void Update(double dt) {
            if (switching) {
                mix += (1.0 - mix) * 100.0 * dt;
                if (mix >= .99999) {
                    mix       = 1.0;
                    switching = false;
                }
            }
        }

        void Switch(double value) {
            if (current == value) return;
            previous  = current;
            current   = value;
            mix       = 0.0;
            switching = true;
        }
    };
    enum class EnvelopeStages : int32_t {
        Attack = 0,
        Decay,
        Release,
        Idle,
    };

    struct Envelope {
        // these defaults are used for the lfo delay envelope
        double a = 0.0;
        double d = 0.5;
        double s = 1.0;
        double r = 0.5;

        EnvelopeStages stage = EnvelopeStages::Idle;
        double value         = 0.0;

        bool IsReleased() const { return stage == EnvelopeStages::Release || stage == EnvelopeStages::Idle; }

        void Reset() { value = 0.0; }
        void Start() { stage = EnvelopeStages::Attack; }
        void Release() { stage = EnvelopeStages::Release; }

        void Update(double dt) {
            switch (stage) {
                case EnvelopeStages::Attack:
                    value += (1.1 - value) * a * dt;
                    if (value >= 1.0) {
                        value = 1.0;
                        stage = EnvelopeStages::Decay;
                    }
                    break;
                case EnvelopeStages::Decay:
                    value += (s - value) * d * dt;
                    break;
                case EnvelopeStages::Release:
                    value += (-.1 - value) * r * dt;
                    if (value <= 0.0) {
                        value = 0.0;
                        stage = EnvelopeStages::Idle;
                    }
                    break;
                default:
                    break;
            }
        }
    };
    struct Oscillator {
        double phase          = 0.0;
        double phaseIncrement = 0.0;
        double triCurrent     = 0.0;
        double triLast        = 0.0;
        double noiseValue     = 19.1919191919191919191919191919191919191919;

        /* waveform generation */

        // http://www.kvraudio.com/forum/viewtopic.php?t=375517
        inline double Blep(double _phase, double _phaseIncrement) {
            if (_phase < _phaseIncrement) {
                _phase /= _phaseIncrement;
                return _phase + _phase - _phase * _phase - 1.0;
            } else if (_phase > 1.0 - _phaseIncrement) {
                _phase = (_phase - 1.0) / _phaseIncrement;
                return _phase * _phase + _phase + _phase + 1.0;
            }
            return 0.0;
        }

        inline double GeneratePulse(double _phase, double _phaseIncrement, double width) {
            double v = _phase < width ? 1.0 : -1.0;
            v += Blep(_phase, _phaseIncrement);
            v -= Blep(fmod(_phase + (1.0 - width), 1.0), _phaseIncrement);
            return v;
        }
        inline double GeneratePulseRaw(double _phase, double width) {
            double v = _phase < width ? 1.0 : -1.0;
            return v;
        }
        double GetWaveform(Waveforms waveform, bool bleb) {
            switch (waveform) {
                case Waveforms::Sine:
                    return sin(phase * M_PI * 2.0);
                case Waveforms::Triangle:
                    triLast    = triCurrent;
                    if (!bleb) {
                        triCurrent = phaseIncrement * GeneratePulseRaw(phase, .5) + (1.0 - phaseIncrement) * triLast;
                    } else {
                        triCurrent = phaseIncrement * GeneratePulse(phase, phaseIncrement, .5) + (1.0 - phaseIncrement) * triLast;
                    }
                    return triCurrent * 5.0;
                case Waveforms::Saw:
                    if (!bleb) {
                        return 1.0 - 2.0 * phase;
                    } else {
                        return 1.0 - 2.0 * phase + Blep(phase, phaseIncrement);
                    }
                case Waveforms::Square:
                    if (!bleb) {
                        return GeneratePulseRaw(phase, .5);
                    }
                    return GeneratePulse(phase, phaseIncrement, .5);
                case Waveforms::Pulse:
                    if (!bleb) {
                        return GeneratePulseRaw(phase, .75);
                    }
                    return GeneratePulse(phase, phaseIncrement, .75);
                case Waveforms::Noise:
                    // Ove Karlsen's noise algorithm
                    // http://musicdsp.org/showArchiveComment.php?ArchiveID=217
                    noiseValue += 19.0;
                    noiseValue *= noiseValue;
                    noiseValue -= (int) noiseValue;
                    return noiseValue - .5;
                default:
                    break;
            }
            return 0;
        }

        double GetWaveform(double dt, double frequency, Waveforms waveform, bool bleb) {
            phaseIncrement = frequency * dt;
            phase += phaseIncrement;
            while (phase > 1.0) phase -= 1.0;
            return GetWaveform(waveform, bleb);
        }
        bool Update(double dt, double frequency) {
            phaseIncrement = frequency * dt;
            phase += phaseIncrement;
            bool b = false;
            while (phase > 1.0) {
                phase -= 1.0;
                b = true;
            }
            return b;
        }
        double Get(double dt, SmoothSwitch& waveform, double frequency, bool bleb) {
            phaseIncrement = frequency * dt;
            phase += phaseIncrement;
            while (phase > 1.0) phase -= 1.0;

            if (waveform.switching) {
                auto out = 0.0;
                out += (1.0 - waveform.mix) * GetWaveform((Waveforms) (int) waveform.previous, bleb);
                out += waveform.mix * GetWaveform((Waveforms) (int) waveform.current, bleb);
                return out;
            }
            return GetWaveform((Waveforms) (int) waveform.current, bleb);
        }
        void initPhase(double phase) {
            this->phase = phase;
            if (0.0 == phase) {
                triCurrent = triLast = 0.0;
            }
        }
    };

    enum class FilterModes : int32_t {
        Off = 0,
        TwoPole,
        Svf,
        FourPole,
        NumFilterModes
    };
    std::vector<String> stringsFilterMode = {
        "Off", "TwoPole", "Svf", "FourPole"
    };

    /* fast trigonometry */
    inline double fastAtan(double x) { return x / (1.0 + .28 * (x * x)); }
    struct TwoPoleFilter {
        double a = 0.0;
        double b = 0.0;

        void Reset() {
            a = 0.0;
            b = 0.0;
        }

        bool IsSilent() { return b == 0.0; }

        double Process(double dt, double input, double cutoff, double resonance) {
            // f calculation
            auto f = 2 * sin(M_PI * cutoff * dt);
            f      = f > .99 ? .99 : f < .01 ? .01
                                             : f;

            // feedback calculation
            auto feedback = resonance + resonance / (1.0 - f);
            feedback      = fastAtan(feedback * .1) * 10.0;

            // main processing
            a += f * (input - a + feedback * (a - b));
            a = fastAtan(a * .1) * 10.0;
            b += f * (a - b);
            b = fastAtan(b * .1) * 10.0;

            return b;
        }
    };

    struct StateVariableFilter {
        double band = 0.0;
        double low  = 0.0;

        void Reset() {
            band = 0.0;
            low  = 0.0;
        }

        bool IsSilent() { return low == 0.0; }

        double Process(double dt, double input, double cutoff, double resonance) {
            // f calculation
            auto f = 2 * sin(M_PI * cutoff * dt);
            f      = f > 1.0 ? 1.0 : f < .01 ? .01
                                             : f;

            // resonance rolloff
            auto maxResonance = 1.0 - f * f * f * f * f;
            resonance         = resonance > maxResonance ? maxResonance : resonance;

            // main processing
            auto high = input - (low + band * (1.0 - resonance));
            band += f * high;
            low += f * band;
            low = fastAtan(low * .1) * 10.0;

            return low;
        }
    };

    struct FourPoleFilter {
        double a = 0.0;
        double b = 0.0;
        double c = 0.0;
        double d = 0.0;

        void Reset() {
            a = 0.0;
            b = 0.0;
            c = 0.0;
            d = 0.0;
        }

        bool IsSilent() { return d == 0.0; }

        double Process(double dt, double input, double cutoff, double resonance) {
            // f calculation
            auto f = 2 * sin(M_PI * cutoff * dt);
            f      = f > .99 ? .99 : f < .01 ? .01
                                             : f;

            // feedback calculation
            auto feedback = resonance + resonance / (1.0 - f);
            feedback      = fastAtan(feedback * .1) * 10.0;

            // main processing
            a += f * (input - a + feedback * (a - b));
            a = fastAtan(a * .1) * 10.0;
            b += f * (a - b);
            b = fastAtan(b * .1) * 10.0;
            c += f * (b - c);
            c = fastAtan(c * .1) * 10.0;
            d += f * (c - d);
            d = fastAtan(d * .1) * 10.0;

            return d;
        }
    };

    struct Filter {
        TwoPoleFilter twoPoleFilter;
        StateVariableFilter stateVariableFilter;
        FourPoleFilter fourPoleFilter;

        void Reset() {
            twoPoleFilter.Reset();
            stateVariableFilter.Reset();
            fourPoleFilter.Reset();
        }

        bool IsSilentIndividual(FilterModes mode) {
            switch (mode) {
                case FilterModes::Off:
                    return true;
                case FilterModes::TwoPole:
                    return twoPoleFilter.IsSilent();
                case FilterModes::Svf:
                    return stateVariableFilter.IsSilent();
                case FilterModes::FourPole:
                    return fourPoleFilter.IsSilent();
                default:
                    return true;
            }
        }

        bool IsSilent(SmoothSwitch mode) {
            if (mode.switching) {
                return IsSilentIndividual((FilterModes) (int) mode.previous) && IsSilentIndividual((FilterModes) (int) mode.current);
            }
            return IsSilentIndividual((FilterModes) (int) mode.current);
        }

        double ProcessIndividual(double dt, double input, FilterModes mode, double cutoff, double resonance) {
            switch (mode) {
                case FilterModes::Off:
                    return input;
                case FilterModes::TwoPole:
                    return twoPoleFilter.Process(dt, input, cutoff, resonance);
                case FilterModes::Svf:
                    return stateVariableFilter.Process(dt, input, cutoff, resonance);
                case FilterModes::FourPole:
                    return fourPoleFilter.Process(dt, input, cutoff, resonance);
                default:
                    return 0;
            }
        }

        double Process(double dt, double input, SmoothSwitch mode, double cutoff, double resonance) {
            if (mode.switching) {
                auto out = 0.0;
                out += (1.0 - mode.mix) * ProcessIndividual(dt, input, (FilterModes) (int) mode.previous, cutoff, resonance);
                out += mode.mix * ProcessIndividual(dt, input, (FilterModes) (int) mode.current, cutoff, resonance);
                return out;
            }
            return ProcessIndividual(dt, input, (FilterModes) (int) mode.current, cutoff, resonance);
        }
    };
    enum class VoiceModes : int32_t {
        Poly = 0,
        Mono,
        Legato,
        NumVoiceModes
    };
    std::vector<String> stringsVoiceMode = {
        "Poly", "Mono", "Legato"
    };

    // pitch calculation //
    inline double pitchFactor(double p) { return pow(1.0595, p); }
    inline double pitchToFrequency(double p) { return 440.0 * pitchFactor(p - 69); }
    struct Voice {
        Envelope volEnv;
        Envelope modEnv;
        Envelope lfoEnv;
        int note               = 0;
        double targetFrequency = 0.0;
        double frequency       = 0.0;
        double velocity        = 0.0;
        double pitchBend       = 1.0;
        Oscillator oscFm;
        Oscillator osc1a;
        Oscillator osc1b;
        Oscillator osc2a;
        Oscillator osc2b;
        Oscillator lfo1;
        Oscillator lfo2;
        Filter filter;
        seq_rand rand;
        double lfoValue      = 0.0;
        double driftVelocity = 0.0;
        double driftPhase    = 0.0;
        double driftValue    = 0.0;

        double getRandom() {
            return rand.rng_double();
        }
        double getRandomPhase() {
            return rand.rng_double()*0.5;
        }

        bool IsReleased() const { return volEnv.IsReleased(); }
        double GetVolume() const { return volEnv.value; }

        bool bInitial = true;
        void Reset(double phase, bool osc1OutOfPhase, bool osc2OutOfPhase) {
            // if (bInitial) {
                oscFm.phase = rand.rng_double();
                osc1a.phase = rand.rng_double();
                osc1b.phase = rand.rng_double();
                // osc1b.phase = osc1a.phase + (osc1OutOfPhase ? (rand.rng_double()*2.0-1.0)*0.166+0.3333 : 0.0);
                osc2a.phase = rand.rng_double();
                osc2b.phase = rand.rng_double();
                // osc2b.phase = osc2a.phase + (osc1OutOfPhase ? (rand.rng_double()*2.0-1.0)*0.166+0.3333 : 0.0);
            // }
            // oscFm.phase = phase;
            // osc1a.phase = phase;
            // osc1b.phase = phase + (osc1OutOfPhase ? .33 : 0.0);
            // osc2a.phase = phase;
            // osc2b.phase = phase + (osc2OutOfPhase ? .33 : 0.0);
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

        void Start(double phase, bool osc1OutOfPhase, bool osc2OutOfPhase) {
            if (volEnv.stage == EnvelopeStages::Idle)
                Reset(phase, osc1OutOfPhase, osc2OutOfPhase);
            volEnv.Start();
            modEnv.Start();
            lfoEnv.Start();
        }

        void UpdateVoice(const HostTempo& tempo, double dt) {
            driftVelocity += getRandom() * 1.0 * dt;
            driftVelocity -= driftVelocity * 2.0 * dt;
            driftPhase += driftVelocity * dt;
            driftValue = .00001 * sin(driftPhase);
            if (lfo2.Update(dt, math::max(tempo.bpm/4.0, 1.0) / 60.0)) {
                lfo1.initPhase(0.0);
            }
        }
    };

    struct VoiceUnison {
        std::array<Voice, NUM_UNISON_VOICES> voices;
        int32_t index = 0;
        seq_rand rand;
        double lfoValue      = 0.0;
        double driftVelocity = 0.0;
        double driftPhase    = 0.0;
        double driftValue    = 0.0;

        bool IsReleased() const { return std::all_of(std::cbegin(voices), std::cend(voices), [](auto& voice) { return voice.volEnv.IsReleased() && voice.modEnv.IsReleased(); }); };
        double GetVolume() const { 
            auto voice = std::max_element(
                                        std::cbegin(voices),
                                        std::cend(voices),
                                        [](const Voice& a, const Voice& b) {
                                            return a.IsReleased() == b.IsReleased() ? a.GetVolume() < b.GetVolume() : a.IsReleased();
                                        });
            return voice->GetVolume();
        }

        double getRandom() {
            double dRandPhase = rand.rng_bits(14)/static_cast<float>(1<<14);
            return dRandPhase;
        }
        double getRandomPhase() {
            return getRandom()*0.5;
        }

        void Release() {
            std::for_each(std::begin(voices), std::end(voices), [](Voice& voice) {
                voice.Release();
            });
        }

        void SetNote(int n) {
            std::for_each(std::begin(voices), std::end(voices), [n](Voice& voice) {
                voice.SetNote(n);
            });
        }

        void SetPitchBendFactor(double f) { 
            std::for_each(std::begin(voices), std::end(voices), [f](Voice& voice) {
                voice.SetPitchBendFactor(f);
            });
        }

        void ResetPitch() { 
            std::for_each(std::begin(voices), std::end(voices), [](Voice& voice) {
                voice.ResetPitch();
            });
        }

        void SetVelocity(double v) { 
            std::for_each(std::begin(voices), std::end(voices), [v](Voice& voice) {
                voice.SetVelocity(v);
            });
        }

        void Start(HostTempo& tempo, bool osc1OutOfPhase, bool osc2OutOfPhase) {
            std::for_each(std::begin(voices), std::end(voices), [=](Voice& voice) {
                bool reset = voice.volEnv.stage >= EnvelopeStages::Release && voice.modEnv.stage >= EnvelopeStages::Decay;
                voice.Start(getRandomPhase(), osc1OutOfPhase, osc2OutOfPhase);
                if (reset) {
                    voice.lfo1.initPhase(fmod(this->driftValue * voice.rand.rng_double(), 1.0));
                    voice.lfo2.initPhase(fmod(voice.driftValue, 1.0));

                }
            });
        }
        void UpdateVoice(const HostTempo& tempo, double dt) {
            driftVelocity += getRandom() * 1.0 * dt;
            driftVelocity -= driftVelocity * 2.0 * dt;
            driftPhase += driftVelocity * dt;
            driftValue = .0001 * sin(driftPhase);
        }
    };

    enum class FmModes : int32_t {
        Off = 0,
        Osc1,
        Osc2,
        NumFmModes
    };
    std::vector<String> stringsFMMode{
        "Off", "Osc1", "Osc2"
    };
    enum class ParamType {
        FLOAT,
        INT,
        ENUM,
    };
    struct SynthParam {
        virtual ~SynthParam() = default;
    };
    struct SynthParamBase : public SynthParam {
        ParamType type;
        Parameters enumParam;
        String name;
        String shortName;
        String format;
        String label;
        SynthParamBase(ParamType _type, Parameters _enumParam) : type(_type), enumParam(_enumParam) {
        }
        ParamType getType() {
            return this->type;
        }
        ~SynthParamBase() override = default;
        virtual void set(float f) {
        }
        virtual float getAsFloat() {
            return 0.0f;
        }
        virtual String getValueDisplay() = 0;
        virtual param_converted_t convertValueDisplay(const param_unit_t& displayValue) const = 0;
    };
    struct SynthParam_Float : public SynthParamBase {
        SynthParam_Float(Parameters _enumParam) : SynthParamBase(ParamType::FLOAT, _enumParam) {
        }
        double valDouble = 0.0;
        double fmin      = 0.0;
        double fmax      = 1.0;
        SynthParam_Float* setRange(float _fmin, float _fmax) {
            fmin = _fmin;
            fmax = _fmax;
            return this;
        }
        double Value() {
            return math::max(fmin, math::min(fmax, (valDouble) * (fmax - fmin) + fmin));
        }
        void setRangedValue(double f) {
            double fVal = math::max(0.0, math::min(1.0, (f - fmin) / (fmax - fmin)));
            valDouble   = fVal;
        }
        float GetMin() {
            return fmin;
        }
        float GetMax() {
            return fmax;
        }
        void set(float f) override {
            valDouble = f;
        }
        float getAsFloat() override {
            return valDouble;
        }
        String getValueDisplay() override {
            return StringFormat(StringAsCStr(format), Value());
        }
        param_converted_t convertValueDisplay(const param_unit_t& displayValue) const override {
            auto f = static_cast<float>(atof(StringAsCStr(displayValue.value)));
            float fVal = math::max(0.0, math::min(1.0, (f - fmin) / (fmax - fmin)));
            return {fVal, true};
        }
    };
    struct SynthParam_Int : public SynthParamBase {
        SynthParam_Int(Parameters _enumParam) : SynthParamBase(ParamType::INT, _enumParam) {
        }
        SynthParam_Int(ParamType _paramType, Parameters _enumParam) : SynthParamBase(_paramType, _enumParam) {
        }
        float valFloat = 0.0f;
        int32_t iValue = 0;
        int32_t iMin   = 0;
        int32_t iMax   = 1;
        SynthParam_Int* setRange(int32_t _iMin, int32_t _iMax) {
            iMin = _iMin;
            iMax = _iMax;
            return this;
        }
        int32_t Value() {
            return math::max(iMin, math::min(iMax, this->iValue));
        }
        float getAsFloat() override {
            return valFloat;
        }
        void set(float f) override {
            iValue = math::max(iMin, math::min(iMax, (int32_t) math::froundf(f * (iMax - iMin) + iMin)));
            setRangedValue(iValue);
        }
        void setRangedValue(int32_t i) {
            iValue         = math::max(iMin, math::min(iMax, i));
            this->valFloat = math::max(0.0, math::min(1.0, (iValue - iMin) / (double) (iMax - iMin)));
        }
        String getValueDisplay() override {
            return StringFormat(StringAsCStr(format), Value());
        }
        param_converted_t convertValueDisplay(const param_unit_t& displayValue) const override {
            auto f = static_cast<float>(atof(StringAsCStr(displayValue.value)));
            auto iVal = math::max(iMin, math::min(iMax, (int32_t) math::froundf(f * (iMax - iMin) + iMin)));
            return {math::max(0.0f, math::min(1.0f, static_cast<float>((iVal - iMin) / (double) (iMax - iMin)))), true};
        }
    };
    struct SynthParam_Enum : public SynthParam_Int {
        SynthParam_Enum(Parameters _enumParam) : SynthParam_Int(ParamType::ENUM, _enumParam) {
        }
        std::vector<String> strings;
        SynthParam_Enum* setStrings(std::vector<String> _strings) {
            this->strings = _strings;
            this->iMax    = _strings.size() - 1;
            return this;
        }
        String getValueDisplay() override {
            int val = this->Value();
            if (val >= 0 && val < CtrSize(strings)) {
                return strings[val];
            }
            return StringFormat("%d", val);
        }
        template<typename T>
        T getEnumValue() {
            return (T) Value();
        }
    };
    void setParamName(SynthParamBase* p, String name, String shortName, String format) {
        p->name      = name;
        p->shortName = shortName;
        if (format == "%f") {
            p->format = "%0.3f";
        } else {
            p->format = format;
        }
    }
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
    class SynthImpl : public SynthState {
        std::array<VoiceUnison, NUM_POLY_VOICES> voices;
        Oscillator lfo;
        Oscillator lfo2;
        SmoothSwitch osc1Wave;
        SmoothSwitch osc2Wave;
        SmoothSwitch lfoWave;
        SmoothSwitch filterMode;
        std::vector<int> heldNotes;
        IMidiQueue midiQueue;
        double dt = 1.0 / 44100.0;
        seq_rand synthRand;
        PluginVST2_Synth* instanceVst2 = nullptr;
        HostTempo tempo{};
    public:
        SynthImpl() : SynthState() {
            auto now = static_cast<uint64_t>(getTimeMillis());
            synthRand.rng_seed(now);
            for (size_t i = 0; i < voices.size(); i++) {
                auto& mv = voices[i];
                mv.index = static_cast<int32_t>(i);
                mv.rand.rng_seed(static_cast<uint64_t>(synthRand.rng_rand()));
                for (auto& v : mv.voices) {
                    v.rand.rng_seed(static_cast<uint64_t>(mv.rand.rng_rand()));
                }
            }
        }
        void setInstance(PluginVST2_Synth* instance) {
            this->instanceVst2 = instance;
        }
        void setSamplerate(float sr) {
            if (sr < 1) sr = 1;
            dt = 1.0 / sr;
        }
        void ProcessMidiMsg(IMidiMsg& msg) {
            midiQueue.Add(msg);
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
    private:
        float synthRandom() {
            uint32_t rnd32Bits = synthRand.rng_rand();
            return (rnd32Bits & 0xFFFF) / (float) 0xFFFF;
        }
        SynthParamBase* GetParam(Parameters param) {
            dbgassert(instanceVst2);
            return instanceVst2->getParam(param);
        }
        SynthParam_Float* GetParamFloat(Parameters param) {
            dbgassert(instanceVst2);
            return static_cast<SynthParam_Float*>(instanceVst2->getParam(param));
        }
        SynthParam_Int* GetParamInt(Parameters param) {
            dbgassert(instanceVst2);
            return static_cast<SynthParam_Int*>(instanceVst2->getParam(param));
        }
        SynthParam_Enum* GetParamEnum(Parameters param) {
            dbgassert(instanceVst2);
            return static_cast<SynthParam_Enum*>(instanceVst2->getParam(param));
        }
        void FlushMidi(int sample) {
            while (!midiQueue.Empty()) {
                auto message = midiQueue.Peek();
                if (message.mOffset > sample) break;

                auto voiceMode      = GetParamEnum(Parameters::VoiceMode)->getEnumValue<VoiceModes>();
                auto status         = message.StatusMsg();
                auto ctrl           = message.ControlChangeIdx();
                auto note           = message.NoteNumber();
                auto velocity       = pow(message.Velocity() * .0078125, 1.25);
                auto osc1OutOfPhase = osc1SplitFactorA > 1.0;
                auto osc2OutOfPhase = osc2SplitFactorA > 1.0;

                if (status == IMidiMsg::kNoteOn && velocity == 0) status = IMidiMsg::kNoteOff;

                switch (status) {
                    case IMidiMsg::kNoteOff:
                        heldNotes.erase(
                                std::remove(
                                        std::begin(heldNotes),
                                        std::end(heldNotes),
                                        note),
                                std::end(heldNotes));

                        switch (voiceMode) {
                            case VoiceModes::Poly:
                                for (auto& voice : voices)
                                    if (voice.voices[0].note == note) voice.Release();
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
                                auto voice = std::min_element(
                                        std::begin(voices),
                                        std::end(voices),
                                        [](auto& a, auto& b) {
                                            return a.IsReleased() == b.IsReleased() ? a.GetVolume() < b.GetVolume() : a.IsReleased();
                                        });
                                log_printf("Starting voice[%d] %s vel %.2f\n", voice->index, noteName(note), velocity);
                                voice->SetNote(note);
                                voice->SetVelocity(velocity);
                                voice->ResetPitch();
                                voice->Start(tempo, osc1OutOfPhase, osc2OutOfPhase);
                                break;
                            }
                            default:
                            case VoiceModes::Mono:
                                voices[0].SetNote(note);
                                voices[0].SetVelocity(velocity);
                                voices[0].Start(tempo, osc1OutOfPhase, osc2OutOfPhase);
                                break;
                            case VoiceModes::Legato:
                                voices[0].SetNote(note);
                                if (heldNotes.empty()) {
                                    voices[0].SetVelocity(velocity);
                                    voices[0].ResetPitch();
                                    voices[0].Start(tempo, osc1OutOfPhase, osc2OutOfPhase);
                                }
                                break;
                        }

                        heldNotes.push_back(note);
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
                        log_printf("Unhandled midi msg %d\n", (int32_t) status);
                        break;
                }
                midiQueue.Remove();
            }
        }

        void UpdateParameters() {
            osc1Wave.Update(dt);
            osc1SplitMix += (targetOsc1SplitMix - osc1SplitMix) * 100.0 * dt;
            osc2Wave.Update(dt);
            osc2SplitMix += (targetOsc2SplitMix - osc2SplitMix) * 100.0 * dt;
            lfoWave.Update(dt);
            oscMix += (targetOscMix - oscMix) * 100.0 * dt;
            filterMode.Update(dt);
            filterCutoff += (targetFilterCutoff - filterCutoff) * 100.0 * dt;
            filterResonance += (targetFilterResonance - filterResonance) * 100.0 * dt;
            filterKeyTracking += (targetFilterKeyTracking - filterKeyTracking) * 100.0 * dt;
            masterVolume += (targetMasterVolume - masterVolume) * 100.0 * dt;
        }

        void UpdateDrift() {
            driftVelocity += synthRandom() * 10000.0 * dt;
            driftVelocity -= driftVelocity * 2.0 * dt;
            driftPhase += driftVelocity * dt;
            driftValue = .001 * sin(driftPhase);
        }

        double GetVoice(Voice& voice) {
            voice.volEnv.Update(dt);
            if (voice.volEnv.stage == EnvelopeStages::Idle && voice.filter.IsSilent(filterMode)) return 0.0;
            voice.modEnv.Update(dt);
            voice.lfoEnv.Update(dt);
            auto volEnvV         = GetParamFloat(Parameters::VolEnvV)->Value();
            auto volEnvValue     = (1.0 - volEnvV) * voice.volEnv.value + volEnvV * voice.volEnv.value * voice.velocity;
            auto modEnvV         = GetParamFloat(Parameters::ModEnvV)->Value();
            auto modEnvValue     = (1.0 - modEnvV) * voice.modEnv.value + modEnvV * voice.modEnv.value * voice.velocity;
            double bpmHz = math::max(tempo.bpm, 1.0) / 60.0;
            double lfoFreqHz = GetParamFloat(Parameters::LfoFrequency)->Value() * bpmHz;
            double dVoiceLfoBi = voice.lfo1.Get(dt, lfoWave, lfoFreqHz, false);
            auto lfoAmount = GetParamFloat(Parameters::LfoAmount)->Value();
            double dVoiceLfoUni = 0.5+0.5*dVoiceLfoBi;
            if (lfoAmount < 0.0) {
                dVoiceLfoUni = pow(dVoiceLfoUni, 1.0+dVoiceLfoUni*-lfoAmount*4.);
            } else {
                dVoiceLfoUni = pow(dVoiceLfoUni, 1.0/(1.0+dVoiceLfoUni*lfoAmount*4.));
            }
            voice.lfoValue = -1.0 + 2.0 * dVoiceLfoUni;
            auto delayedLfoValue = dVoiceLfoUni * voice.lfoEnv.value;

            voice.frequency += (voice.targetFrequency - voice.frequency) * glideLength * dt;

            auto baseFrequency = voice.frequency * voice.pitchBend * (1.0 + driftValue);
            auto osc1Frequency = osc1Tune * baseFrequency;
            auto osc2Frequency = osc2Tune * baseFrequency;

            auto fmMode = GetParamEnum(Parameters::FmMode)->getEnumValue<FmModes>();
            switch (fmMode) {
                case FmModes::Osc1:
                case FmModes::Osc2: {
                    auto fmAmount = baseFmAmount;
                    fmAmount += GetParamFloat(Parameters::VolEnvFm)->Value() * volEnvValue;
                    fmAmount += GetParamFloat(Parameters::ModEnvFm)->Value() * modEnvValue;
                    fmAmount += GetParamFloat(Parameters::LfoFm)->Value() * delayedLfoValue;

                    auto fmMultiplier = pitchFactor(voice.oscFm.GetWaveform(dt, osc1Frequency, Waveforms::Sine, true) * fmAmount);
                    switch (fmMode) {
                        case FmModes::Osc1:
                            osc1Frequency *= fmMultiplier;
                            break;
                        case FmModes::Osc2:
                            osc2Frequency *= fmMultiplier;
                            break;
                        default:
                            break;
                    }
                    break;
                }
                default:
                    break;
            }

            auto out = 0.0;
            if (oscMix < .999) {
                auto osc1Out = 0.0;
                osc1Out += voice.osc1a.Get(dt, osc1Wave, osc1Frequency * osc1SplitFactorA, true);
                if (osc1SplitMix > .001)
                    osc1Out += osc1SplitMix * voice.osc1b.Get(dt, osc1Wave, osc1Frequency * osc1SplitFactorB, true);
                out += osc1Out * sqrt(1.0 - oscMix);
            }
            if (oscMix > .001) {
                auto osc2Out = 0.0;
                osc2Out += voice.osc2a.Get(dt, osc2Wave, osc2Frequency * osc2SplitFactorA, true);
                if (osc2SplitMix > .001)
                    osc2Out += osc2SplitMix * voice.osc2b.Get(dt, osc2Wave, osc2Frequency * osc2SplitFactorB, true);
                out += osc2Out * sqrt(oscMix);
            }

            out *= volEnvValue;

            auto cutoff = filterCutoff;
            cutoff += GetParamFloat(Parameters::VolEnvCutoff)->Value() * volEnvValue;
            cutoff += GetParamFloat(Parameters::ModEnvCutoff)->Value() * modEnvValue;
            cutoff += GetParamFloat(Parameters::LfoCutoff)->Value() * delayedLfoValue;
            cutoff += pitchFactor(GetParamFloat(Parameters::FilterKeyTracking)->Value()) * osc1Tune * baseFrequency;
            cutoff *= 1.0 - driftValue;
            cutoff = math::clamp(cutoff, -1.0/dt*0.7, 1.0/dt*0.7);
            out = voice.filter.Process(dt, out, filterMode, cutoff, filterResonance);

            return out;
        }

    public:
        void onTransportChanged(bool bIsPlaying) {
            double lfo1Tempo = 1.0;
            double lfo2Tempo = 1.0 / 4.0;
            lfo.initPhase(fmod(tempo.ppqPos * lfo1Tempo, 1.0));
            lfo2.initPhase(fmod(tempo.ppqPos * lfo2Tempo, 1.0));

            log_lf(Log::L_DEBUG, "Reset LFO phases: at ppq/4 %f\n", fmod(tempo.ppqPos/4.0, 1.0));
            // for (auto& voice : voices) voice.initLfoPhases(tempo);
            for (auto& uv : voices) {
                for (auto& voice : uv.voices) {
                    voice.lfo1.initPhase(fmod(tempo.ppqPos * lfo1Tempo + driftValue * voice.rand.rng_double(), 1.0));
                    voice.lfo2.initPhase(fmod(tempo.ppqPos * lfo2Tempo + uv.driftValue, 1.0));
                }
            };
        }
        void ProcessReplacing(float** inputs, float** outputs, int nFrames) {
            // double dPosPPQ = tempo.ppqPos - tempo.barPos;
            double bpmHz = math::max(tempo.bpm, 1.0) / 60.0;
            // double sampleToPPQ = ppqPeriod * dt;
            //     lfo.initPhase(dPosPPQAtSample);
            const auto mvInv = 1.0/voices[0].voices.size();
            for (int s = 0; s < nFrames; s++) {
                FlushMidi(s);
                UpdateParameters();
                UpdateDrift();
                for (auto& uv : voices) {
                    uv.UpdateVoice(tempo, dt);
                    for (auto& v : uv.voices) {
                        v.UpdateVoice(tempo, dt);
                    }
                }
                if (lfo2.Update(dt, math::max(tempo.bpm/4.0, 1.0) / 60.0)) {
                    lfo.initPhase(0.0);
                    log_lf(Log::L_DEBUG, "Init phase: at sample %d ppq/4 %f\n", s, fmod(tempo.ppqPos/4.0, 1.0));
                }
                // lfoValue

                // double dPosPPQAtSample = dPosPPQ + s * sampleToPPQ;
                // double lfoFreqHz = GetParamFloat(Parameters::LfoFrequency)->Value();
                // lfo.initPhase(dPosPPQAtSample);
                // calculate lfo freqency in Hz based on tempo
                double lfoFreqHz = GetParamFloat(Parameters::LfoFrequency)->Value() * bpmHz;
                lfoValue = lfo.Get(dt, lfoWave, lfoFreqHz, false);
                auto outL = 0.0;
                auto outR = 0.0;
                for (auto& mv : voices) {
                    for (size_t vIdx = 0; vIdx < mv.voices.size(); ++vIdx) {
                        auto voice = GetVoice(mv.voices[vIdx]) * mvInv;
                        if constexpr(NUM_UNISON_VOICES % 2 == 0) {
                            double leftRight = vIdx & 1;
                            outL += voice * leftRight;
                            outR += voice * (1.0 - leftRight);
                        } else {
                            double pan = vIdx / (NUM_UNISON_VOICES - 1.0);
                            outL += voice * pan;
                            outR += voice * (1.0 - pan);
                        }
                    }
                }
                outputs[0][s] = static_cast<float>(outL * masterVolume);
                outputs[1][s] = static_cast<float>(outR * masterVolume);
            }
        }

        void Reset() {
            //IMutexLock lock(this);
            //dt = 1.0 / GetSampleRate();
        }

        void GrayOutControls() {
            //auto osc1Enabled = GetParam(Parameters::OscMix)->Value() > 0.0;
            //auto osc2Enabled = GetParam(Parameters::OscMix)->Value() < 1.0;
            //auto osc1Noise   = (Waveforms) (int) GetParam(Parameters::Osc1Wave)->Value() == Waveforms::Noise;
            //auto osc2Noise   = (Waveforms) (int) GetParam(Parameters::Osc2Wave)->Value() == Waveforms::Noise;
            //auto fmEnabled   = (GetParam(Parameters::FmMode)->Value() == 1 && osc1Enabled && !osc1Noise) ||
            //                 (GetParam(Parameters::FmMode)->Value() == 2 && osc2Enabled && !osc2Noise);
            //auto filterEnabled  = GetParam(Parameters::FilterMode)->Value();
            //auto modEnvEnabled  = GetParam(Parameters::ModEnvFm)->Value() != 0.0 || GetParam(Parameters::ModEnvCutoff)->Value() != 0.0;
            //auto vibratoEnabled = GetParam(Parameters::LfoFm)->Value() != 0.0 || GetParam(Parameters::LfoCutoff)->Value() != 0.0 ||
            //                      GetParam(Parameters::LfoAmount)->Value() < 0.0 || (GetParam(Parameters::LfoAmount)->Value() > 0.0 && osc2Enabled);
            //
            //// oscillator 1
            //pGraphics->GetControl(1)->GrayOut(!osc1Enabled);
            //pGraphics->GetControl(2)->GrayOut(!((osc1Enabled && !osc1Noise) || fmEnabled));
            //pGraphics->GetControl(3)->GrayOut(!((osc1Enabled && !osc1Noise) || fmEnabled));
            //pGraphics->GetControl(4)->GrayOut(!(osc1Enabled && !osc1Noise));
            //
            //// oscillator 2
            //pGraphics->GetControl(5)->GrayOut(!osc2Enabled);
            //for (int i = 6; i < 9; i++) pGraphics->GetControl(i)->GrayOut(!(osc2Enabled && !osc2Noise));
            //
            //// fm
            //for (int i = 12; i < 14; i++) pGraphics->GetControl(i)->GrayOut(!fmEnabled);
            //for (int i = 41; i < 44; i++) pGraphics->GetControl(i)->GrayOut(!fmEnabled);
            //
            //// filter
            //for (int i = 15; i < 18; i++) pGraphics->GetControl(i)->GrayOut(!filterEnabled);
            //for (int i = 44; i < 47; i++) pGraphics->GetControl(i)->GrayOut(!filterEnabled);
            //
            //// mod sources
            //for (int i = 28; i < 38; i++) pGraphics->GetControl(i)->GrayOut(!modEnvEnabled);
            //for (int i = 39; i < 41; i++) pGraphics->GetControl(i)->GrayOut(!vibratoEnabled);
            //
            //// glide
            //pGraphics->GetControl(48)->GrayOut(!GetParam(Parameters::VoiceMode)->Value());
        }

        void OnParamChange(Parameters parameter) {
            //IMutexLock lock(this);
            //auto value = GetParam(parameter)->Value();
            float value = 0.0f;
            if (GetParam(parameter)->getType() == ParamType::FLOAT) {
                value = GetParamFloat(parameter)->Value();
            }
            if (GetParam(parameter)->getType() == ParamType::INT) {
                value = GetParamInt(parameter)->Value();
            }
            if (GetParam(parameter)->getType() == ParamType::ENUM) {
                value = GetParamEnum(parameter)->Value();
            }
            switch (parameter) {
                case Parameters::Osc1Wave:
                    osc1Wave.Switch(value);
                    break;
                case Parameters::Osc1Coarse:
                case Parameters::Osc1Fine: {
                    auto coarse = GetParamInt(Parameters::Osc1Coarse)->Value();
                    auto fine   = GetParamFloat(Parameters::Osc1Fine)->Value();
                    osc1Tune    = pitchFactor(coarse + fine);
                    break;
                }
                case Parameters::Osc1Split:
                    targetOsc1SplitMix = value != 0.0 ? 1.0 : 0.0;
                    osc1SplitFactorA   = pitchFactor(value);
                    osc1SplitFactorB   = 1.0;//pitchFactor(value);
                    break;
                case Parameters::Osc2Wave:
                    osc2Wave.Switch(value);
                    break;
                case Parameters::Osc2Coarse:
                case Parameters::Osc2Fine: {
                    auto coarse = GetParamInt(Parameters::Osc2Coarse)->Value();
                    auto fine   = GetParamFloat(Parameters::Osc2Fine)->Value();
                    osc2Tune    = pitchFactor(coarse + fine);
                    break;
                }
                case Parameters::Osc2Split:
                    targetOsc2SplitMix = value != 0.0 ? 1.0 : 0.0;
                    // osc2SplitFactorA   = pitchFactor(-value);
                    // osc2SplitFactorB   = pitchFactor(value);
                    osc2SplitFactorA   = pitchFactor(value);
                    osc2SplitFactorB   = 1.0;//pitchFactor(value);
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
                case Parameters::FilterMode:
                    filterMode.Switch(value);
                    break;
                case Parameters::FilterCutoff:
                    targetFilterCutoff = value;
                    break;
                case Parameters::FilterResonance:
                    targetFilterResonance = value;
                    break;
                case Parameters::FilterKeyTracking:
                    targetFilterKeyTracking = value;
                    break;
                case Parameters::VolEnvA: {
                    auto volEnvA = 1000 - 999.9 * (.5 - .5 * cos(pow(value, .1) * M_PI));
                    for (auto& mv : voices)
                        for (auto& v : mv.voices)
                            v.volEnv.a = volEnvA;
                    break;
                }
                case Parameters::VolEnvD: {
                    auto volEnvD = 1000 - 999.9 * (.5 - .5 * cos(pow(value, .1) * M_PI));
                    
                    for (auto& mv : voices)
                        for (auto& v : mv.voices)
                            v.volEnv.d = volEnvD;
                    break;
                }
                case Parameters::VolEnvS:
                    
                    for (auto& mv : voices)
                        for (auto& v : mv.voices)
                            v.volEnv.s = value;
                    break;
                case Parameters::VolEnvR: {
                    auto volEnvR = 1000 - 999.9 * (.5 - .5 * cos(pow(value, .1) * M_PI));
                    
                    for (auto& mv : voices)
                        for (auto& v : mv.voices)
                            v.volEnv.r = volEnvR;
                    break;
                }
                case Parameters::ModEnvA: {
                    auto modEnvA = 1000 - 999.9 * (.5 - .5 * cos(pow(value, .1) * M_PI));
                    
                    for (auto& mv : voices)
                        for (auto& v : mv.voices)
                            v.modEnv.a = modEnvA;
                    break;
                }
                case Parameters::ModEnvD: {
                    auto modEnvD = 1000 - 999.9 * (.5 - .5 * cos(pow(value, .1) * M_PI));
                    
                    for (auto& mv : voices)
                        for (auto& v : mv.voices)
                            v.modEnv.d = modEnvD;
                    break;
                }
                case Parameters::ModEnvS:
                    
                    for (auto& mv : voices)
                        for (auto& v : mv.voices)
                            v.modEnv.s = value;
                    break;
                case Parameters::ModEnvR: {
                    auto modEnvR = 1000 - 999.9 * (.5 - .5 * cos(pow(value, .1) * M_PI));
                    
                    for (auto& mv : voices)
                        for (auto& v : mv.voices)
                            v.modEnv.r = modEnvR;
                    break;
                }
                case Parameters::LfoDelay: {
                    auto p        = static_cast<SynthParam_Float*>(GetParam(parameter));
                    auto lfoDelay = p->GetMin() + p->GetMax() - p->Value();
                    
                    for (auto& mv : voices)
                        for (auto& v : mv.voices)
                            v.lfoEnv.a = lfoDelay;
                    break;
                }
                case Parameters::LfoCutoff:
                    lfoToCutoff = copysign((value * .000125) * (value * .000125) * 8000.0, value);
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
                case Parameters::GlideLength:
                    glideLength = 1000 - 999.0 * (.5 - .5 * cos(pow(value, .1) * M_PI));
                    break;
                case Parameters::MasterVolume:
                    targetMasterVolume = value;
                    break;
                case Parameters::LfoWave:
                    lfoWave.Switch(value);
                    break;
                default:
                    break;
            }

            GrayOutControls();
        }
    };
    void PluginVST2_Synth::initPrograms() {
        for (SynthProgram& program : staticPrograms) {
            program = {};
            program.VoiceMode = 0.000000;
            program.GlideLength = 0.000000;
            program.FilterMode = 1.000000;
            program.FilterCutoff = 0.100000;
            program.FilterResonance = 0.020000;
            program.FilterKeyTracking = 0.500000;
            program.VolEnvCutoff = 0.750000;
            program.ModEnvCutoff = 0.740000;
            program.OscMix = 0.500000;
            program.Osc1Wave = 0.400000;
            program.Osc1Coarse = 0.5;
            program.Osc1Fine = 0.500000;
            program.Osc1Split = 0.554000;
            program.Osc2Wave = 0.400000;
            program.Osc2Coarse = 0.745;
            program.Osc2Fine = 0.505000;
            program.Osc2Split = 0.560000;
            program.LfoAmount = 0.500000;
            program.LfoFrequency = 0.5;
            program.LfoDelay = 0.000000;
            program.LfoCutoff = 0.500000;
            program.FmMode = 0.000000;
            program.FmCoarse = 0.000000;
            program.FmFine = 0.500000;
            program.VolEnvFm = 0.500000;
            program.ModEnvFm = 0.500000;
            program.LfoFm = 0.500000;
            program.VolEnvA = 0.075000;
            program.VolEnvD = 0.500000;
            program.VolEnvS = 1.000000;
            program.VolEnvR = 0.650000;
            program.VolEnvV = 0.800000;
            program.ModEnvA = 0.220000;
            program.ModEnvD = 0.500000;
            program.ModEnvS = 0.500000;
            program.ModEnvR = 0.700000;
            program.ModEnvV = 0.600000;      
            // if (index >= 0 && index < CtrSize(vecParams)) {
        //     SynthParamBase* param = vecParams[index];
        }
        {
        auto& prog        = staticPrograms[0];
        prog.FilterCutoff = 1.0f;
        prog.FilterMode   = 1.0;// TODO: add butto to write out presets in source code format so we can add them here

        }
        {
            auto& prog = staticPrograms[1];
            prog.VoiceMode=0.000000;
            prog.GlideLength=0.000000;
            prog.FilterMode=0.666667;
            prog.FilterCutoff=0.135000;
            prog.FilterResonance=0.050000;
            prog.FilterKeyTracking=0.875000;
            prog.VolEnvCutoff=0.590000;
            prog.ModEnvCutoff=0.655000;
            prog.OscMix=1.000000;
            prog.Osc1Wave=0.400000;
            prog.Osc1Coarse=0.500000;
            prog.Osc1Fine=0.500000;
            prog.Osc1Split=0.554000;
            prog.Osc2Wave=0.000000;
            prog.Osc2Coarse=0.500000;
            prog.Osc2Fine=0.500000;
            prog.Osc2Split=0.715000;
            prog.LfoAmount=0.500000;
            prog.LfoFrequency=0.393939;
            prog.LfoDelay=0.000000;
            prog.LfoCutoff=0.500000;
            prog.FmMode=0.500000;
            prog.FmCoarse=0.000000;
            prog.FmFine=0.500000;
            prog.VolEnvFm=0.500000;
            prog.ModEnvFm=0.500000;
            prog.LfoFm=0.495000;
            prog.VolEnvA=0.075000;
            prog.VolEnvD=0.400000;
            prog.VolEnvS=0.545000;
            prog.VolEnvR=0.700000;
            prog.VolEnvV=0.770000;
            prog.ModEnvA=0.025000;
            prog.ModEnvD=0.375000;
            prog.ModEnvS=0.165000;
            prog.ModEnvR=0.735000;
            prog.ModEnvV=0.865000;
        }


    }
    PluginVST2_Synth::PluginVST2_Synth(audioMasterCallback audioMaster)
        : BasePluginVST2(audioMaster, PLUGIN_UID, kNumPrograms, Parameters::kNumParams, kNumInputs, kNumOutputs) {
        isSynth(true);
        impl = new SynthImpl();
        impl->setInstance(this);
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
        addFloatParam(Parameters::FilterCutoff)->setRange(-22000.0, 22000.0)->setRangedValue(20.0);
        setParamName(getParam(Parameters::FilterCutoff), "Filter Cutoff", "Flt Cut", "%f");
        addFloatParam(Parameters::FilterResonance)->setRange(0.0, 1.0)->setRangedValue(0.0);
        setParamName(getParam(Parameters::FilterResonance), "Filter Resonance", "Flt Res", "%f");
        addFloatParam(Parameters::FilterKeyTracking)->setRange(-24.0, 24.0)->setRangedValue(0.0);
        setParamName(getParam(Parameters::FilterKeyTracking), "Filter Keytracking", "Flt Trk", "%f");

        addFloatParam(Parameters::FmFine)->setRange(-1.0, 1.0)->setRangedValue(0.0);
        setParamName(getParam(Parameters::FmFine), "Fm fine", "Fm fine", "%f");
        addIntParam(Parameters::FmCoarse)->setRange(0, 48)->setRangedValue(0);
        setParamName(getParam(Parameters::FmCoarse), "Fm Coarse", "Fm Coarse", "%d");

        addFloatParam(Parameters::OscMix)->setRange(0.0, 1.0)->setRangedValue(1.0);
        setParamName(getParam(Parameters::OscMix), "Oscillator Mix", "OSC Mix", "%f");
        addFloatParam(Parameters::Osc1Fine)->setRange(-1.0, 1.0)->setRangedValue(0.0);
        setParamName(getParam(Parameters::Osc1Fine), "Oscillator 1 fine", "OSC1 Fine", "%f");
        addFloatParam(Parameters::Osc2Fine)->setRange(-1.0, 1.0)->setRangedValue(0.0);
        setParamName(getParam(Parameters::Osc2Fine), "Oscillator 2 fine", "OSC2 Fine", "%f");
        addFloatParam(Parameters::Osc1Split)->setRange(-1.25, 1.25)->setRangedValue(0.0);
        setParamName(getParam(Parameters::Osc1Split), "Oscillator 1 split", "OSC1 Split", "%f");
        addFloatParam(Parameters::Osc2Split)->setRange(-1.25, 1.25)->setRangedValue(0.0);
        setParamName(getParam(Parameters::Osc2Split), "Oscillator 2 split", "OSC2 Split", "%f");
        addIntParam(Parameters::Osc1Coarse)->setRange(-24, 24)->setRangedValue(0);
        setParamName(getParam(Parameters::Osc1Coarse), "Oscillator 1 coarse", "OSC1 Semi", "%d");
        addIntParam(Parameters::Osc2Coarse)->setRange(-24, 24)->setRangedValue(0);
        setParamName(getParam(Parameters::Osc2Coarse), "Oscillator 2 coarse", "OSC2 Semi", "%d");

        addFloatParam(Parameters::VolEnvA)->setRange(0.0, 1.0)->setRangedValue(0.0);
        addFloatParam(Parameters::VolEnvD)->setRange(0.0, 1.0)->setRangedValue(0.5);
        addFloatParam(Parameters::VolEnvS)->setRange(0.0, 1.0)->setRangedValue(1.0);
        addFloatParam(Parameters::VolEnvR)->setRange(0.0, 1.0)->setRangedValue(0.25);
        addFloatParam(Parameters::VolEnvV)->setRange(0.0, 1.0)->setRangedValue(0.0);
        setParamName(getParam(Parameters::VolEnvA), "Volume envelope attack time", "EnvA Att", "%f");
        setParamName(getParam(Parameters::VolEnvD), "Volume envelope decay time", "EnvA Dec", "%f");
        setParamName(getParam(Parameters::VolEnvS), "Volume envelope sustain", "EnvA Sus", "%f");
        setParamName(getParam(Parameters::VolEnvR), "Volume envelope release time", "EnvA Rel", "%f");
        setParamName(getParam(Parameters::VolEnvV), "Volume envelope velocity sensitivity", "EnvA Vel", "%f");

        addFloatParam(Parameters::ModEnvA)->setRange(0.0, 1.0)->setRangedValue(0.0);
        addFloatParam(Parameters::ModEnvD)->setRange(0.0, 1.0)->setRangedValue(0.5);
        addFloatParam(Parameters::ModEnvS)->setRange(0.0, 1.0)->setRangedValue(0.5);
        addFloatParam(Parameters::ModEnvR)->setRange(0.0, 1.0)->setRangedValue(0.5);
        addFloatParam(Parameters::ModEnvV)->setRange(0.0, 1.0)->setRangedValue(0.0);
        setParamName(getParam(Parameters::ModEnvA), "Mod envelope attack time", "EnvM Att", "%f");
        setParamName(getParam(Parameters::ModEnvD), "Mod envelope decay time", "EnvM Dec", "%f");
        setParamName(getParam(Parameters::ModEnvS), "Mod envelope sustain", "EnvM Sus", "%f");
        setParamName(getParam(Parameters::ModEnvR), "Mod envelope release time", "EnvM Rel", "%f");
        setParamName(getParam(Parameters::ModEnvV), "Mod envelope velocity sensitivity", "EnvM Vel", "%f");

        addFloatParam(Parameters::LfoAmount)->setRange(-1.0f, 1.0f)->setRangedValue(0.0);
        addFloatParam(Parameters::LfoFrequency)->setRange(1/64.0, 16.0)->setRangedValue(4.0);
        addFloatParam(Parameters::LfoDelay)->setRange(0.001f, 1000.0)->setRangedValue(0.1);
        setParamName(getParam(Parameters::LfoAmount), "LFO amount", "LFO amt", "%f");
        setParamName(getParam(Parameters::LfoFrequency), "LFO frequency", "LFO freq", "%f");
        setParamName(getParam(Parameters::LfoDelay), "LFO ramp", "LFO ramp", "%f");

        addFloatParam(Parameters::VolEnvFm)->setRange(-24.0, 24.0)->setRangedValue(0.0);
        addFloatParam(Parameters::ModEnvFm)->setRange(-24.0, 24.0)->setRangedValue(0.0);
        addFloatParam(Parameters::LfoFm)->setRange(-24.0, 24.0)->setRangedValue(0.0);
        addFloatParam(Parameters::VolEnvCutoff)->setRange(-24000.0, 24000.0)->setRangedValue(0.0);
        addFloatParam(Parameters::ModEnvCutoff)->setRange(-24000.0, 24000.0)->setRangedValue(0.0);
        addFloatParam(Parameters::LfoCutoff)->setRange(-2*24000.0, 2*24000.0)->setRangedValue(0.0);
        addFloatParam(Parameters::GlideLength)->setRange(0.0, 1.0)->setRangedValue(0.0);
        addFloatParam(Parameters::MasterVolume)->setRange(0.0, 0.5)->setRangedValue(0.25);

        setParamName(getParam(Parameters::VolEnvFm), "Volume envelope to FM amount", "FM Amt EnvA", "%f");
        setParamName(getParam(Parameters::ModEnvFm), "Modulation envelope to FM amount", "FM Amt EnvM", "%f");
        setParamName(getParam(Parameters::LfoFm), "LFO to FM amount", "FM Amt LFO", "%f");
        setParamName(getParam(Parameters::VolEnvCutoff), "Volume envelope to filter cutoff", "Flt EnvA", "%f");
        setParamName(getParam(Parameters::ModEnvCutoff), "Modulation envelope to filter cutoff", "Flt EnvM", "%f");
        setParamName(getParam(Parameters::LfoCutoff), "Modulation LFO to filter cutoff", "Flt LFO", "%f");
        setParamName(getParam(Parameters::GlideLength), "Glide length", "Glide", "%f");
        setParamName(getParam(Parameters::MasterVolume), "Volume", "Volume", "%f");


        addEnumParam(Parameters::Osc1Wave)->setStrings(stringsWaveform)->setRangedValue(0);
        setParamName(getParam(Parameters::Osc1Wave), "Osc1 Waveform", "Osc1 Waveform", "%d");
        addEnumParam(Parameters::Osc2Wave)->setStrings(stringsWaveform)->setRangedValue(0);
        setParamName(getParam(Parameters::Osc2Wave), "Osc2 Waveform", "Osc2 Waveform", "%d");
        addEnumParam(Parameters::LfoWave)->setStrings(stringsWaveform)->setRangedValue(0);
        setParamName(getParam(Parameters::LfoWave), "Lfo Waveform", "Lfo Waveform", "%d");
        addEnumParam(Parameters::VoiceMode)->setStrings(stringsVoiceMode)->setRangedValue(0);
        setParamName(getParam(Parameters::VoiceMode), "Voice Mode", "Voice Mode", "%d");
        addEnumParam(Parameters::FilterMode)->setStrings(stringsFilterMode)->setRangedValue(0);
        setParamName(getParam(Parameters::FilterMode), "Filter Mode", "Flt Mode", "%d");
        addEnumParam(Parameters::FmMode)->setStrings(stringsFMMode)->setRangedValue(0);
        setParamName(getParam(Parameters::FmMode), "Fm Mode", "Fm Mode", "%d");
        initPrograms();

        for (auto param : this->vecParams) {
            this->impl->OnParamChange(param->enumParam);
        }
    }

    void logParam(const char* szParamName, const float val) {
    }

    void PluginVST2_Synth::writeCurrentProgram() {
        for (auto param : this->vecParams) {
            switch (param->enumParam) {
                case Parameters::MasterVolume:
                case Parameters::kNumParams:
                    break;
                default:
                    log_printf("%s = %f\n", StringAsCStr(param->shortName), param->getAsFloat());
                    break;
            }
        }
    }
    void PluginVST2_Synth::setFromSynthProgram(SynthProgram* program) {
        for (auto param : this->vecParams) {
            auto* pParDouble = program->getProgramParameter(param->enumParam);
            if (pParDouble) {
                param->set(static_cast<float>(*pParDouble));
            }
        }
        for (auto param : this->vecParams) {
            this->impl->OnParamChange(param->enumParam);
        }
    }
    SynthParamBase* PluginVST2_Synth::getParam(Parameters enumParam) {
        if (enumParam >= 0 && enumParam < vecParams.size()) {
            return vecParams[enumParam];
        }
        return nullptr;
    }
    SynthImpl* PluginVST2_Synth::getSynth() {
        return this->impl;
    }

    void PluginVST2_Synth::setProgram(VstInt32 programIdx) {
        if (programIdx < 0 || programIdx >= kNumPrograms)
            return;

        curProgram = programIdx;
        if (curProgram >= 0 && curProgram < CtrSize(staticPrograms)) {
            setFromSynthProgram(&staticPrograms[curProgram]);
        }
    }

    void PluginVST2_Synth::setProgramName(char* name) {
        if (name && curProgram >= 0 && curProgram < CtrSize(staticPrograms)) {
            staticPrograms[curProgram].setName(name);
        }
    }

    void PluginVST2_Synth::getProgramName(char* name) {
        if (name) name[0] = 0;
        if (name && curProgram >= 0 && curProgram < CtrSize(staticPrograms)) {
            vst_strncpy(name, StringAsCStr(staticPrograms[curProgram].getName()), PLUGIN_PROGRAM_STR_MAX_LEN);
        }
    }

    void PluginVST2_Synth::getParameterLabel(VstInt32 index, char* label) {
        if (label && index >= 0 && index < CtrSize(vecParams)) {
            SynthParamBase* param = vecParams[index];
            vst_strncpy(label, StringAsCStr(param->label), PLUGIN_PARAM_STR_MAX_LEN);
        }
    }

    void PluginVST2_Synth::getParameterDisplay(VstInt32 index, char* text) {
        if (text && index >= 0 && index < CtrSize(vecParams)) {
            SynthParamBase* param = vecParams[index];
            String valDisplay = param->getValueDisplay();
            vst_strncpy(text, StringAsCStr(valDisplay), PLUGIN_PARAM_STR_MAX_LEN);
        }
    }

    void PluginVST2_Synth::getParameterName(VstInt32 index, char* label) {
        if (index >= 0 && index < CtrSize(vecParams)) {
            SynthParamBase* param = vecParams[index];
            vst_strncpy(label, StringAsCStr(param->shortName), PLUGIN_PARAM_STR_MAX_LEN);
        }
    }

    void PluginVST2_Synth::setParameter(VstInt32 index, float value) {
        if (index >= 0 && index < CtrSize(vecParams)) {
            SynthParamBase* param = vecParams[index];
            if (issetprogram && curProgram >= 0 && curProgram < CtrSize(staticPrograms)) {
                auto pParamDouble = staticPrograms[curProgram].getProgramParameter(param->enumParam);
                if (pParamDouble) *pParamDouble = value;
            }
            param->set(value);
            this->impl->OnParamChange(param->enumParam);
        }
#if BUILD_VSTHOST
        for (auto& pviewctr : this->views) {
            if (pviewctr->isInUse()) {
                pviewctr->onSetParameter(index, value);
            }
        }
#else
        if (this->editor) {
            static_cast<pluginwindow*>(this->editor)->onSetParameter(index, value);
        }
#endif
    }

    param_converted_t PluginVST2_Synth::convertParamValueDisplay(int32_t idx, const param_unit_t& displayValue) {
        if (idx >= 0 && idx < CtrSize(vecParams)) {
            SynthParamBase* param = vecParams[idx];
            return param->convertValueDisplay(displayValue);
        }
        return BasePluginVST2::convertParamValueDisplay(idx, displayValue);
    }
    void PluginVST2_Synth::addPropertiesParameterTooltip(Table::tbl& table, int idx) {
        if (idx >= 0 && idx < CtrSize(vecParams)) {
            SynthParamBase* param = vecParams[idx];
            const auto strName = param->name;
            const auto strDisplay = param->getValueDisplay();
            table.colSizes.resize(2);
            table.colSizes[0] = table.strW->getStringWidth(strName);
            table.colSizes[1] = table.strW->getStringWidth(strDisplay);
            table.tableWidth = table.colSizes[0] + table.colSizes[1];
            table.rows.push_back({ { strName, strDisplay } });
        }
    }

    float PluginVST2_Synth::getParameter(VstInt32 index) {
        float value = 0;
        if (index >= 0 && index < CtrSize(vecParams)) {
            SynthParamBase* param = vecParams[index];
            value                 = param->getAsFloat();
        }
        return value;
    }

    bool PluginVST2_Synth::getProgramNameIndexed(VstInt32 category, VstInt32 index, char* text) {
        if (index >= 0 && index < kNumPrograms) {
            String progName = StringFormat("Program %d", index);
            vst_strncpy(text, progName.c_str(), PLUGIN_PROGRAM_STR_MAX_LEN);
            return true;
        }
        return false;
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

    void PluginVST2_Synth::setSampleRate(float sampleRate) {
        AudioEffectX::setSampleRate(sampleRate);
        this->impl->setSamplerate(sampleRate);
    }
    void PluginVST2_Synth::setBlockSize(VstInt32 blockSize) {
        AudioEffectX::setBlockSize(blockSize);
    }
    VstInt32 PluginVST2_Synth::processEvents(VstEvents* events) {
        dbgassert(events);
        if (events) {
            // ThreadLock lock(this->getMutex());
            int32_t len = events->numEvents;
            //if (events->numEvents)
            //    log_printf("events->numEvents %d\n", events->numEvents);
            for (int i = 0; i < len; i++) {
                auto pEvent = events->events[i];
                if (pEvent->type == VstEventTypes::kVstMidiType) {
                    VstMidiEvent* pME = (VstMidiEvent*) pEvent;
                    IMidiMsg msg(pME->deltaFrames, pME->midiData[0], pME->midiData[1], pME->midiData[2]);
                    impl->ProcessMidiMsg(msg);
                    //log_printf("event[%d].type %d\n", i, pME->type);
                    //log_printf("event[%d].byteSize %d\n", i, pME->byteSize);
                    //log_printf("event[%d].deltaFrames %d\n", i, pME->deltaFrames);
                    //log_printf("event[%d].flags %d\n", i, pME->flags);
                    //log_printf("event[%d].noteLength %d\n", i, pME->noteLength);
                    //log_printf("event[%d].noteOffset %d\n", i, pME->noteOffset);
                    //log_printf("event[%d].midiData %02X%02X%02X%02X\n", i,
                    //           (unsigned) pME->midiData[0], (unsigned) pME->midiData[1], (unsigned) pME->midiData[2], (unsigned) pME->midiData[3]);
                    //log_printf("event[%d].detune %d\n", i, (unsigned) pME->detune);
                    //log_printf("event[%d].noteOffVelocity %d\n", i, (unsigned) pME->noteOffVelocity);
                    //log_printf("event[%d].reserved1 %d\n", i, (unsigned) pME->reserved1);
                    //log_printf("event[%d].reserved2 %d\n", i, (unsigned)pME->reserved2);
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
            VstTimeInfo* timeinfo = getTimeInfo(kVstBarsValid|kVstPpqPosValid|kVstTempoValid|kVstTransportChanged|kVstTimeSigValid);
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
            if (inputs)
                dsp_util::fillChannels(inputs, this->getAeffect()->numInputs, sampleFrames, 0.0f);
            dsp_util::fillChannels(outputs, this->getAeffect()->numOutputs, sampleFrames, 0.0f);
            this->impl->ProcessReplacing(inputs, outputs, sampleFrames);
        }
    }


    SynthProgram::SynthProgram() : SynthProgramParameters() {
        setName("Init");
    }

}

namespace PluginSynth {


    class guicontainer_plugin_synth : public guictr_base {
        struct _synth_gui_param_knob {
            guiknob_pluginparam* knob;
            Parameters param;
        };
        PluginVST2_Synth* const plugin;
        effectbase* const module;
        gui_textfield editfield;
        std::vector<_synth_gui_param_knob> knobs;
        std::map<Parameters, guiknob_pluginparam*> mapKnobs;
        gui_list list;

    public:
        explicit guicontainer_plugin_synth(PluginVST2_Synth* plugin)
            : guictr_base(),
            plugin(plugin),
            module(plugin->getHostSideHandle())
        {
            setBackgroundRendered(true);
            editfield.setFlag(FLG_NO_LAYOUT, true);
            editfield.setVisible(false);
            editfield.setAlignment(gui_textfield::Alignment::Center);
            editfield.setReturnCommits(true);
            padding = 4;
            // margin  = 4;
            knobs.reserve(Parameters::kNumParams);
            for (int i = 0; i < (int) Parameters::kNumParams; i++) {
                knobs.push_back(_synth_gui_param_knob{new guiknob_pluginparam(PARAM_OFFSET_EXTERNAL+i, i, guiknob::knobtype::SLIDER_LABELED), static_cast<Parameters>(i)});
                mapKnobs[knobs.back().param] = knobs.back().knob;
                add(knobs.back().knob);
            }
            add(&list);
            add(&editfield);
        }
        ~guicontainer_plugin_synth() override {
            removeGuis();
            for (auto& synthKnob : knobs) {
                delete synthKnob.knob;
            }
        }

        guiknob_pluginparam* getKnobFromParameter(int32_t index) {
            if (index >= 0 && index < (int32_t) knobs.size()) {
                return knobs[index].knob;
            }
            return nullptr;
        }
        void onSetParameter(int32_t index, float value) {
#if BUILD_EXTERNAL_PLUGIN
            guiknob_pluginparam* knob = getKnobFromParameter(index);
            if (knob) {
                knob->setValueInit(value);
            }
#endif
        }
        void onGuiOpen() {
            for (auto& synthKnob : knobs) {
#if BUILD_VSTHOST
                synthKnob.knob->setEffectInstance(plugin->getHostSideHandle());
#endif
#if BUILD_EXTERNAL_PLUGIN
                synthKnob.knob->setAudioEffect(plugin);
#endif
            }
        }
        void onGuiClose() {
            for (auto& synthKnob : knobs) {
#if BUILD_VSTHOST
                synthKnob.knob->setEffectInstance(nullptr);
#endif
#if BUILD_EXTERNAL_PLUGIN
                synthKnob.knob->setAudioEffect(nullptr);
#endif
            }
        }

        void onTick(AppCtrl* ctrl) override {
            PluginVST2_Synth* thisImpl = this->plugin;
            std::vector<int> heldNotes = thisImpl->getSynth()->getHeldNotes();//TODO: not threadsafe
            std::vector<String> strings;
            String s                   = "Held notes: ";
            for (int i : heldNotes) {
                s += String(noteName(i)) + ",";
            }
            if (heldNotes.empty())
                s += "<empty>";
            strings.push_back(s);
            String str;
            str = StringFormat("SR %.2f BS %d", this->plugin->getSampleRate(), this->plugin->getBlockSize());
            strings.push_back(str);
            int flags = 0;
            for (int i = 8; i < 16; i++) {
                flags |= (1 << i);
            }
            VstTimeInfo* timeinfo = thisImpl->getTimeInfo(flags);
            dbgassert(timeinfo);
            strings.push_back(StringFormat("samplePos %.3f", timeinfo->samplePos));
            strings.push_back(StringFormat("ppqPos %.3f", timeinfo->ppqPos));
            strings.push_back(StringFormat("tempo %.3f", timeinfo->tempo));
            strings.push_back(StringFormat("barStartPos %.3f", timeinfo->barStartPos));
            auto& list = this->list.getListRef();

            class gui_synth_stats_list_entry : public gui_list_entry {
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

            // PluginVST2_Synth::ThreadLock lock(thisImpl->getMutex());
            ////stress test thread safety
            //for (int i = 0; i < 10000; i++) {
            //    std::vector<int> heldNotes = thisImpl->getSynth()->getHeldNotes();//TODO: not threadsafe
            //}
            for (guibase* gui : guis) {
                if (gui->isVisible()) {
                    nvgSave(vg);
                    gui->render(vg);
                    nvgRestore(vg);
                }
            }
        }
        void layout() override {
            auto knobSize    = ivec2(64, 128+64);
            auto knobPos     = ivec2(padding);
            for (auto& synthKnob : knobs) {
                synthKnob.knob->pos = knobPos;
                synthKnob.knob->size = knobSize;
                synthKnob.knob->setLabelsFontScale(0.7f, 0.8f);
                knobPos.x += knobSize.x + INSET_CTR_SPACING;
                if (knobPos.x + knobSize.x > size.x - padding) {
                    knobPos.x = padding;
                    knobPos.y += knobSize.y + INSET_CTR_SPACING;
                }
            }
            if (knobPos.x + knobSize.x > size.x - padding) {
                knobPos.x = padding;
                knobPos.y += knobSize.y + INSET_CTR_SPACING;
            }
            list.pos = knobPos;
            list.size = size - list.pos - ivec2(padding, padding);
            for (guibase* gui : guis) {
                gui->layout();
            }
        }
        bool handleKeyInput(KeyEvent& event) override {
            if (event.type != KeyEventType::K_RELEASE) {
                if (event.keyCode == KEY_ENTER) {
                    this->plugin->writeCurrentProgram();
                }
            }
            return false;
        }
        void buttonClicked(guibase* button) override {
            auto param = dynamic_cast<guiknob_pluginparam*>(button);
            if (param && module) {
                auto paramIdx = param->getParamIdx();
                auto paramValue = module->getParamValueDisplay(paramIdx);
                editfield.mCallbackEnd = [this, param, paramValue, paramIdx](const std::string& str) {
                    auto paramConverted = module->convertParamValueDisplay(param->getParamIdx(), param_unit_t{str, paramValue.unit});
                    if (paramConverted.success) {
                        module->setParamValue(paramIdx, paramConverted.floatVal, FLG_PAR_UPDATE_USER);
                        if (param->fnValueEditChanged)
                            param->fnValueEditChanged(param->getValue(), paramConverted.floatVal);
                    }
                    editfield.setVisible(false);
                    return true;
                };
                auto layout = param->getLayout();
                editfield.pos = layout.pValue;
                editfield.size = layout.sValue;
                editfield.setVisible(true);
                editfield.layout();
                editfield.setValue(paramValue.value);
                editfield.setSelectionRange(-1, -1);
                editfield.setFontSize(layout.valueHeight * theme->getFloat(GuiConstant::CONST_FONT_SCALE));
                parentCtrl->focusGui(&editfield);
                return;
            }
            guictr_base::buttonClicked(button);
        }
    };


    const char* getName() {
        return PLUGIN_EFFECT_NAME;
    }
    AudioEffectX* createPlugin(audioMasterCallback audioMaster) {
        return new PluginVST2_Synth(audioMaster);
    }
    std::shared_ptr<PluginViewContainers> PluginVST2_Synth::createView() {
        auto view = std::make_shared<SinglePluginViewContainers<guicontainer_plugin_synth, PluginVST2_Synth>>(this, 1280, 720);
        this->views.push_back(view);
        return view;
    }
}

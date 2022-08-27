#include <cmath>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <limits>
#include <muParser.h>
#include <optional>
#include <vector>
#include <map>
#include <deque>
#include <memory>
#include <vstsdk-host-2.4/aeffectx.h>
#include "assert_dbg.h"
#include "automation.h"
#include "compiler.h"
#include "config.h"
#include "gui/controls/list.h"
#include "gui/controls/textfield.h"
#include "gui/dropdown/dropdown_generic.h"
#include "guicolors.h"
#include "guiglobals.h"
#include "logging.h"
#include "math/seq_math.h"
#include "math/simd_math.h"
#include "plugins/synth/synth-plugin.h"
#include "rand.h"
#include "seq_util.h"
#include "str_util.h"
#include "dsp_util.h"
#include "color_util.h"

#include "gui/gui.h"
#include "gui/container/scrollcontainer.h"
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
#include "synth-snapshot.h"
#include "plugins/plugin.h"
#include "plugins/plugin-base.h"
#include "plugins/plugin-window.h"
#include "vstsdk-plugin-2.4/audioeffect.h"
#include "vstsdk-plugin-2.4/audioeffectx.h"
#include "audioblock.h"
#include "midi-defs.h"
#include "IPlugMidi.h"
#include <glm/gtx/fast_exponential.hpp>

#if BUILD_EXTERNAL_PLUGIN
AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {
    return PluginSynth::createPlugin(audioMaster);
}
#endif

namespace PluginSynth {
    int32_t gDebugOverrides = -1;
    const char* const PLUGIN_EFFECT_NAME  = "Synth";
    const char* const PLUGIN_UID          = "SYNT";
    const char* const PLUGIN_PRODUCT_NAME = "Synth VST2.4";
    enum class ParamType {
        FLOAT,
        INT,
        ENUM,
    };
    enum class Waveforms : int32_t {
        Sine = 0,
        Triangle,
        Saw,
        Square,
        Pulse,
        Noise,
        NumWaveforms
    };
    enum class FilterModes : int32_t {
        Off = 0,
        TwoPole,
        Svf,
        FourPole,
        NumFilterModes
    };
    enum class VoiceModes : int32_t {
        Poly = 0,
        Mono,
        Legato,
        NumVoiceModes
    };
    enum class FmModes : int32_t {
        Off = 0,
        Osc1,
        Osc2,
        NumFmModes
    };
    const std::array<const char*, 6> stringsWaveform = {
        "Sine", "Triangle", "Saw", "Square", "Pulse", "Noise"
    };
    const std::array<const char*, 4> stringsFilterMode = {
        "Off", "TwoPole", "Svf", "FourPole"
    };
    const std::array<const char*, 3> stringsVoiceMode = {
        "Poly", "Mono", "Legato"
    };
    const std::array<const char*, 3> stringsFMMode = {
        "Off", "Osc1", "Osc2"
    };
    static constexpr uint16_t NUM_POLY_VOICES   = 64;
    static constexpr uint16_t NUM_UNISON_VOICES = 16;

    struct HostTempo {
        double barPos;
        double bpm;
        double ppqPos;
    };

    class SmoothSwitch {
        static constexpr double switchFreqHz = 100.0;
        double current                       = 0.0;
        double previous                      = 0.0;
        double mix                           = 1.0;
        bool switching                       = false;

    public:
        void Update(double dt) {
            if (switching) {
                double dMix = fmod(mix, 1.0);
                mix += (1.0 - dMix) * switchFreqHz * dt;
                if (mix >= .99999) {
                    mix       = 1.0;
                    previous = current;
                    switching = false;
                }
            }
        }
        double getPrevious() const {
            return previous;
        }
        double getCurrent() const {
            return current;
        }
        bool isSwitching() const {
            return switching;
        }
        double getMixValue() const {
            return mix;
        }
        void Switch(double value) {
            double dCurrent = getSwitchValue();
            if (dCurrent == value) {
                switching = false;
                previous = current = dCurrent;
            } else {
                previous  = dCurrent;
                current   = value;
                switching = true;
            }
            mix = 0.0;
        }
        double getSwitchValue() const {
            if (switching) {
                return (1.0 - mix) * previous + mix * current;
            }
            return current;
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


    // http://www.kvraudio.com/forum/viewtopic.php?t=375517
    static inline double Blep(double _phase, double _phaseIncrement) {
        if (_phase < _phaseIncrement) {
            _phase /= _phaseIncrement;
            return _phase + _phase - _phase * _phase - 1.0;
        } else if (_phase > 1.0 - _phaseIncrement) {
            _phase = (_phase - 1.0) / _phaseIncrement;
            return _phase * _phase + _phase + _phase + 1.0;
        }
        return 0.0;
    }
    static inline double HalfBlep(double _phase, double _phaseIncrement) {
        if (_phase < _phaseIncrement) {
            _phase /= _phaseIncrement;
            return _phase + _phase - _phase * _phase - 1.0;
        }
        return 0.0;
    }
    class Oscillator {
    public:
        double phase = 0.0;
        double phaseIncrement = 0.0;

    private:
        double triCurrent     = 0.0;
        double triLast        = 0.0;
        double noiseValue     = 19.1919191919191919191919191919191919191919;
        /* waveform generation */

        inline double GeneratePulse(double _phase, double _phaseIncrement, double width) {
            double v = _phase < width ? 1.0 : -1.0;
            v += Blep(_phase, _phaseIncrement);
            v -= Blep(fmod(_phase + (1.0 - width), 1.0), _phaseIncrement);
            return v;
        }
        inline double GenerateHalfBlebPulse(double _phase, double _phaseIncrement, double width) {
            double v = _phase < width ? 1.0 : -1.0;
            v += HalfBlep(_phase, _phaseIncrement);
            if (_phase < 1.0 - _phaseIncrement)
                v -= HalfBlep(fmod(_phase + (1.0 - width), 1.0), _phaseIncrement);

            return v;
        }
        inline double GeneratePulseRaw(double _phase, double width) {
            double v = _phase < width ? 1.0 : -1.0;
            return v;
        }

    public:
        double GetWaveform(Waveforms waveform, bool bleb) {
            dbgassert(!fp_math::isNanOrInfd(phase));
            switch (waveform) {
                case Waveforms::Sine:
                    dbgassert(!fp_math::isNanOrInfd(sin(phase * M_PI * 2.0)));
                    return sin(phase * M_PI * 2.0);
                case Waveforms::Triangle:
                    triLast = triCurrent;
                    if (!bleb) {
                        triCurrent = phaseIncrement * GeneratePulseRaw(phase, .5) + (1.0 - phaseIncrement) * triLast;
                    } else {
                        triCurrent = phaseIncrement * GeneratePulse(phase, phaseIncrement, .5) + (1.0 - phaseIncrement) * triLast;
                    }
                    dbgassert(!fp_math::isNanOrInfd(triCurrent));
                    return triCurrent;
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
                    dbgassert(0);
                    break;
            }
            return 0;
        }

        double GetLfoWaveform(Waveforms waveform, bool halfBleb) {
            dbgassert(!fp_math::isNanOrInfd(phase));
            switch (waveform) {
                case Waveforms::Sine:
                    dbgassert(!fp_math::isNanOrInfd(sin(phase * M_PI * 2.0)));
                    return sin(phase * M_PI * 2.0);
                case Waveforms::Triangle:
                    triLast = triCurrent;
                    if (halfBleb) {
                        triCurrent = phaseIncrement * GenerateHalfBlebPulse(phase, phaseIncrement, .5) + (1.0 - phaseIncrement) * triLast;
                    } else {
                        triCurrent = phaseIncrement * GeneratePulse(phase, phaseIncrement, .5) + (1.0 - phaseIncrement) * triLast;
                    }
                    dbgassert(!fp_math::isNanOrInfd(triCurrent));
                    return triCurrent;
                case Waveforms::Saw:
                    if (halfBleb) {
                        return 1.0 - 2.0 * phase + HalfBlep(phase, phaseIncrement);
                    } else {
                        return 1.0 - 2.0 * phase + Blep(phase, phaseIncrement);
                    }
                case Waveforms::Square:
                    if (halfBleb) {
                        return GenerateHalfBlebPulse(phase, phaseIncrement, .5);
                    }
                    return GeneratePulse(phase, phaseIncrement, .5);
                case Waveforms::Pulse:
                    if (halfBleb) {
                        return GenerateHalfBlebPulse(phase, phaseIncrement, .75);
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
                    dbgassert(0);
                    break;
            }
            return 0;
        }
        double GetWaveform(double dt, double frequency, Waveforms waveform, bool bleb) {
            phaseIncrement = frequency * dt;
            phase          = fp_math::silenceNanInfd(phase + phaseIncrement);
            while (phase > 1.0) phase -= 1.0;
            return GetWaveform(waveform, bleb);
        }
        bool Update(double dt, double frequency) {
            phaseIncrement = frequency * dt;
            phase          = fp_math::silenceNanInfd(phase + phaseIncrement);
            bool b         = phase > 1.0;
            while (phase > 1.0) phase -= 1.0;
            dbgassert(!fp_math::isNanOrInfd(phase));
            return b;
        }
        double Get(double dt, SmoothSwitch& waveform, double frequency, bool bleb) {
            phaseIncrement = frequency * dt;
            phase          = fp_math::silenceNanInfd(phase + phaseIncrement);
            while (phase > 1.0) phase -= 1.0;
            double dSwitchVal = waveform.getSwitchValue();
            dbgassert(!fp_math::isNanOrInfd(dSwitchVal));
            auto roundedVal = math::rounddU32(dSwitchVal);
            dbgassert(roundedVal < static_cast<uint32_t>(Waveforms::NumWaveforms));
            return GetWaveform(static_cast<Waveforms>(roundedVal), bleb);
        }
        double GetLfo(double dt, SmoothSwitch& waveform, double frequency, bool oneShot) {
            phaseIncrement = frequency * dt;
            phase          = fp_math::silenceNanInfd(phase + phaseIncrement);
            if (oneShot && phase > 1.0) {
                phase = 1.0;
            } else {
                while (phase > 1.0) phase -= 1.0;
            }
            double dSwitchVal = waveform.getSwitchValue();
            dbgassert(!fp_math::isNanOrInfd(dSwitchVal));
            auto roundedVal = math::rounddU32(dSwitchVal);
            dbgassert(roundedVal < static_cast<uint32_t>(Waveforms::NumWaveforms));
            return GetLfoWaveform(static_cast<Waveforms>(roundedVal), oneShot);
        }
        void initPhase(double phase) {
            this->phase = phase;
            if (0.0 == phase) {
                triCurrent = triLast = 0.0;
            }
        }
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

        bool IsSilent() const { return std::fabs(b) <= 1E-12; }

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

        bool IsSilent() const { return std::fabs(low) <= 1E-12; }

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

        bool IsSilent() const { return std::fabs(d) <= 1E-12; }

        double Process(double dt, double input, double cutoff, double resonance) {
            // f calculation
            auto f = 2 * sin(M_PI * math::clamp(cutoff * dt, 0.0, 1.0));
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

        bool IsSilentIndividual(FilterModes mode) const {
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

        bool IsSilent(const FilterModes mode) const {
            return IsSilentIndividual(mode);
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

        double Process(double dt, double input, FilterModes filterMode, double cutoff, double resonance) {
            return ProcessIndividual(dt, input, filterMode, cutoff, resonance);
        }
    };

    inline double pitchFactor(double p) { 
        static constexpr auto base = 1.0594630943592953; // = pow(2.0, 1.0 / 12.0);
        static constexpr auto logBase = 0.057762265046662153;//log(base);
        return exp(logBase * p);
        // return pow(base, p); 
    }
    inline double pitchToFrequency(double p) { return 440.0 * pitchFactor(p - 69); }
    struct Voice {
        std::array<double, Parameters::kNumParams> modValues{};
        std::array<float, 8> envelopeValuesCached{};

        Oscillator lfo1;
        Oscillator lfo2;
        double lfoValue        = 0.0;
        double velocity        = 0.0;
        int32_t indexUnison    = 0;
        int note               = 0;
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
        double driftVelocity = 0.0;
        double driftPhase    = 0.0;
        double driftValue      = 0.0;
        double frequency       = 0.0;
        double targetFrequency = 0.0;
        double pitchBend       = 1.0;
        bool bIsActive         = false;

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

        bool bInitial = true;
        void Reset(bool randomPhase) {
            if (randomPhase) {
                oscFm.phase = rand.rng_double();
                osc1a.phase = rand.rng_double();
                osc1b.phase = rand.rng_double();
                osc2a.phase = rand.rng_double();
                osc2b.phase = rand.rng_double();
            } else {
                oscFm.phase = 0.0;
                osc1a.phase = 0.0;
                osc1b.phase = 0.0;
                osc2a.phase = 0.0;
                osc2b.phase = 0.0;
            }
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

        void Start(bool reset) {
            bIsActive = true;
            if (reset) {
                Reset(true);
            }
            volEnv.Start();
            modEnv.Start();
            lfoEnv.Start();
        }

        void UpdateVoiceLfo(double dt, const HostTempo& tempo, const bool retrigLfo1) {
            driftVelocity += getRandom() * 1.0 * dt;
            driftVelocity -= driftVelocity * 2.0 * dt;
            driftPhase += driftVelocity * dt;
            driftValue = .00001 * sin(driftPhase);
            if (lfo2.Update(dt, math::max(tempo.bpm / 4.0, 1.0) / 60.0) && retrigLfo1) {
                lfo1.initPhase(0.0);
            }
        }
    };

    class VoiceUnison {
    public:
        std::array<Voice, NUM_UNISON_VOICES> voices;
        Voice* const first;
        Voice* last;
        int32_t indexPoly = 0;
        int note          = 0;
        int32_t seqNr     = 0;
        seq_rand rand;
        double lfoValue      = 0.0;
        double driftVelocity = 0.0;
        double driftPhase    = 0.0;
        double driftValue    = 0.0;
        int32_t numUnisonActive = 0;

    public:
        VoiceUnison()
            : voices(), first(&voices.front()) {
            last = &voices.back();
        }
        void setUnisonVoiceCount(int32_t unisonVoiceCount) {
            unisonVoiceCount = math::max(1, unisonVoiceCount);
            const auto maxVoices = CtrSize(voices);
            last = &voices[unisonVoiceCount > maxVoices ? maxVoices : unisonVoiceCount];
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
        bool IsReleased() const {
            return std::all_of(first, last, [](auto& voice) { return !voice.bIsActive; });
        };
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
            std::for_each(first, last, [](Voice& voice) {
                voice.Release();
            });
        }

        void SetNote(int n) {
            note = n;
            std::for_each(first, last, [n](Voice& voice) {
                voice.SetNote(n);
            });
        }
        double GetFreqency() const {
            return voices[0].frequency;
        }

        void SetPitchBendFactor(double f) {
            std::for_each(first, last, [f](Voice& voice) {
                voice.SetPitchBendFactor(f);
            });
        }

        void ResetPitch() {
            std::for_each(first, last, [](Voice& voice) {
                voice.ResetPitch();
            });
        }

        void SetVelocity(double v) {
            std::for_each(first, last, [v](Voice& voice) {
                voice.SetVelocity(v);
            });
        }

        void Start(HostTempo& tempo, double lfoPhaseDrift) {
            std::for_each(first, last, [&](Voice& voice) {
                // auto vEnvStagePre = voice.volEnv.stage;
                bool reset = voice.volEnv.stage >= EnvelopeStages::Release;
                voice.Start(reset);
                if (reset) {
                    voice.lfo1.initPhase(fmod(lfoPhaseDrift*this->driftValue * voice.rand.rng_double(), 1.0));
                    voice.lfo2.initPhase(fmod(lfoPhaseDrift*voice.driftValue, 1.0));
                }
                // log_printf("v%d:%d volEnv %d->%d %.4f phases %.4f %.4f %.4f %.4f reset %d\n", 
                //     indexPoly, voice.indexUnison, vEnvStagePre, voice.volEnv.stage, voice.volEnv.value,
                //     voice.osc1a.phase, voice.osc1b.phase, 
                //     voice.osc2a.phase, voice.osc2a.phase, reset);
            });
            seqNr++;
            numUnisonActive = static_cast<int32_t>(last - first);
        }
        void UpdateVoiceLfo(double dt, const HostTempo& tempo) {
            driftVelocity += getRandom() * 1.0 * dt;
            driftVelocity -= driftVelocity * 2.0 * dt;
            driftPhase += driftVelocity * dt;
            driftValue = .0001 * sin(driftPhase);
        }
    };

    struct SynthParam {
        virtual ~SynthParam() = default;
    };

    const Settings settingsOrdered[] = {
        FilterEnabled,
        FilterDriftEnabled,
        TuningDriftEnabled,
        LfoEnabled,
        LfoOneShotEnabled,
        LfoPhaseDriftEnabled,
        LfoShapeType,
        ModulationEnabled,
        ClearModulationEnabled,
        ExprEvaluationEnabled,
        DiagnosticOutputEnabled,
    };
    const std::array<const char*, 11> stringsSettings = {
        "FilterEnabled",
        "ModulationEnabled",
        "LfoEnabled",
        "ClearModulationEnabled",
        "ExprEvaluationEnabled",
        "LfoOneShotEnabled",
        "DiagnosticOutputEnabled",
        "LfoShapeType",
        "TuningDriftEnabled",
        "FilterDriftEnabled",
        "LfoPhaseDriftEnabled",
    };
    
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
        NumModulationSources
    };
    enum ModulationType {
        Function,
        Constant,
        ModulationSource,
        NumModulationTypes
    };
    const std::array<const char*, 20> stringsModSource = {
        "None",
        "Function",
        "Constant",
        "VolEnv",
        "ModEnv",
        "Lfo1",
        "Velocity",
        "VoiceIndex",
        "UnisonVoiceIndex",
        "Pitch",
        "Note",
        "Lfo2",
        "Macro 1",
        "Macro 2",
        "Macro 3",
        "Macro 4",
        "Macro 5",
        "Macro 6",
        "Macro 7",
        "Macro 8"
    };
    static_assert(stringsModSource.size() == 
                static_cast<size_t>(ModulationSourceType::NumModulationSources) +
                static_cast<size_t>(ModulationType::NumModulationTypes) + 1 - 1, "stringsModSource size mismatch");
    static constexpr auto MathExprInputLen = 1 + ModulationSourceType::NumModulationSources;
    const std::array<const char*, MathExprInputLen> stringsShortSrcNames = {
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
        "m8"
    };

    enum ModulationOperator {
        Multiply,
        Add,
        Subtract,
        Divide,
        MultiplyNegative,
        Absolute,
        Clamp,
        Power,
        NumModulationOperators
    };
    enum ModulationRange {
        Unipolar,
        Bipolar,
        Triangle,
        NumModulationRanges
    };
    const std::array<const char*, 8> stringsModOp = {
        "*",
        "+",
        "-",
        "/",
        "*(1-x)",
        "Abs",
        "Clamp",
        "Power"
    };
    struct MathExprParsed {
        std::array<double, MathExprInputLen> inputs{};
        mu::Parser parser;
        int32_t nanInfCounter = 0;
    };
    struct MathExpr {
        String str;
        std::shared_ptr<MathExprParsed> parsedExpr;
        /**
         * @brief parse the expression and store the parsed expression. 
         *        Throws an exception if the expression is invalid.
         * @param String strExpression the expression to parse
         * @return MathExpr parsed expression
         */
        static MathExpr parse(const String& strExpression) {
            MathExpr expr;
            if (strExpression.length()) {
                auto shrdP    = std::make_shared<MathExprParsed>();
                auto& p       = shrdP->parser;
                auto itInputs = shrdP->inputs.data();
                for (auto& name : stringsShortSrcNames) {
                    p.DefineVar(name, itInputs++);
                }
                p.SetExpr(strExpression);
                p.Eval();
                expr.str        = strExpression;
                expr.parsedExpr = std::move(shrdP);
            }
            return expr;
        }
    };
    struct ModulationInput {
        ModulationType type   = ModulationType::ModulationSource;
        ModulationSourceType src  = ModulationSourceType::Lfo1;
        ModulationOperator op = ModulationOperator::Multiply;
        double value          = 1.0;
        MathExpr function;
        ModulationRange range = ModulationRange::Unipolar;
    };
    struct ModulationDestination {
        Parameters parameter = Parameters::FilterCutoff;
        double range         = 1.0;
    };
    struct Modulation {
        std::vector<ModulationInput> inputs;
        std::vector<ModulationDestination> destinations;
    };

    struct SynthParamBase : public SynthParam {
        ParamType type;
        Parameters enumParam;
        String name;
        String shortName;
         // name when used in hierarchical UIs
        String hierarchicalName;
        String format;
        String unit;
        SynthParamBase(ParamType _type, Parameters _enumParam) : type(_type), enumParam(_enumParam) {
        }
        ParamType getType() {
            return this->type;
        }
        ~SynthParamBase() override                                                            = default;
        virtual void set(double f) noexcept                                                   = 0;
        virtual double getAsDouble() const noexcept                                           = 0;
        virtual String getValueDisplay() const                                                = 0;
        virtual param_converted_t convertValueDisplay(const param_unit_t& displayValue) const = 0;
        const String& getName() const {
            return this->name;
        }
        const String& getShortName() const {
            return this->shortName;
        }
        const String& getHierarchicalName() const {
            return this->hierarchicalName;
        }
        const String& getFormat() const {
            return this->format;
        }
        const String& getUnit() const {
            return this->unit;
        }
    };
    struct SynthParam_Float : public SynthParamBase {
        explicit SynthParam_Float(Parameters _enumParam) : SynthParamBase(ParamType::FLOAT, _enumParam) {
        }
        double valDouble = 0.0;
        double fmin      = 0.0;
        double fmax      = 1.0;
        SynthParam_Float* setRange(float _fmin, float _fmax) {
            fmin = _fmin;
            fmax = _fmax;
            return this;
        }
        double Value() const noexcept {
            return math::clamp(valDouble * (fmax - fmin) + fmin, fmin, fmax);
        }
        double ValueModulated(double valModulated) const noexcept {
            return math::clamp((valDouble + valModulated) * (fmax - fmin) + fmin, fmin, fmax);
        }
        void setRangedValue(double f) {
            double fVal = math::max(0.0, math::min(1.0, (f - fmin) / (fmax - fmin)));
            valDouble   = fVal;
        }
        double GetMin() {
            return fmin;
        }
        double GetMax() {
            return fmax;
        }
        void set(double f) noexcept override {
            valDouble = f;
        }
        double getAsDouble() const noexcept override {
            return valDouble;
        }
        String getValueDisplay() const noexcept override {
            return StringFormat(StringAsCStr(format), Value());
        }
        param_converted_t convertValueDisplay(const param_unit_t& displayValue) const override {
            auto val  = atof(StringAsCStr(displayValue.value));
            auto fVal = math::max(0.0, math::min(1.0, (val - fmin) / (fmax - fmin)));
            return { static_cast<float>(fVal), true };
        }
    };
    struct SynthParam_Int : public SynthParamBase {
        explicit SynthParam_Int(Parameters _enumParam) : SynthParamBase(ParamType::INT, _enumParam) {
        }
        SynthParam_Int(ParamType _paramType, Parameters _enumParam) : SynthParamBase(_paramType, _enumParam) {
        }
        double valFloat         = 0.0;
        int32_t iValue          = 0;
        int32_t iValueModulated = 0;
        int32_t iMin            = 0;
        int32_t iMax            = 1;
        SynthParam_Int* setRange(int32_t _iMin, int32_t _iMax) {
            iMin = _iMin;
            iMax = _iMax;
            return this;
        }
        int32_t Value() const noexcept {
            return math::max(iMin, math::min(iMax, this->iValue));
        }
        double ValueModulated(double valModulated) const noexcept {
            const double dMin       = iMin;
            const double dMax       = iMax;
            const double dModulated = (dMax - dMin) * valModulated + double(iValue);
            return math::clamp<double>(dModulated, dMin, dMax);
        }
        int32_t getUnclampped() const noexcept {
            return this->iValue;
        }
        double getAsDouble() const noexcept override {
            return valFloat;
        }
        void set(double f) noexcept override {
            auto iVal = math::rounddS32(f * (iMax - iMin) + iMin);
            iValue    = math::clamp(iVal, iMin, iMax);
            valFloat  = f;
        }
        void setRangedValue(int32_t i) noexcept {
            iValue   = math::clamp(i, iMin, iMax),
            valFloat = math::clamp((iValue - iMin) / static_cast<double>(iMax - iMin), 0.0, 1.0);
        }
        String getValueDisplay() const noexcept override {
            return StringFormat(StringAsCStr(format), Value());
        }
        param_converted_t convertValueDisplay(const param_unit_t& displayValue) const override {
            auto val  = atof(StringAsCStr(displayValue.value));
            auto iVal = math::clamp(math::rounddS32(val), iMin, iMax);
            auto dVal = math::clamp((iVal - iMin) / static_cast<double>(iMax - iMin), 0.0, 1.0);
            return { static_cast<float>(dVal), true };
        }
    };
    struct SynthParam_Enum : public SynthParam_Int {
        explicit SynthParam_Enum(Parameters _enumParam) : SynthParam_Int(ParamType::ENUM, _enumParam) {
        }
        std::vector<String> strings;
        template<typename StrCtrIt>
        SynthParam_Enum* setStrings(const StrCtrIt& begin, const StrCtrIt& end) {
            strings    = std::vector<String>(begin, end);
            this->iMax = CtrSize(strings) - 1;
            return this;
        }
        String getValueDisplay() const noexcept override {
            int val = this->Value();
            if (val >= 0 && val < CtrSize(strings)) {
                return strings[val];
            }
            return StringFormat("%d", val);
        }
        template<typename T>
        T getEnumValue() const noexcept {
            return static_cast<T>(Value());
        }
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
    class SynthImpl : public SynthState {
        friend class PluginVST2_Synth;
        std::vector<SynthParamBase*> vecParams;
        std::vector<Modulation> modulations;
        std::array<VoiceUnison, NUM_POLY_VOICES> voices;
        std::array<bool, Settings::NumSettings> settings{};
        std::vector<std::shared_ptr<PluginViewContainers>> views;
        Oscillator lfo;
        Oscillator lfo2;
        SmoothSwitch osc1Wave;
        SmoothSwitch osc2Wave;
        SmoothSwitch lfoWave;
        // SmoothSwitch filterMode;
        std::vector<int> heldNotes;
        IMidiQueue midiQueue;
        double oneOverSR = 1.0 / 44100.0;
        seq_rand synthRand;
        HostTempo tempo{};
        public:
        int32_t activeVoiceCount = 0;
        int32_t unisonVoiceCount = 0;
        int32_t maxUnisonVoice = 0;
        int32_t polyVoiceCount   = 0;
        private:
        PluginVST2_Synth* const instanceVst2;
        bool bIsInitSamplerate = false;
        String exprError;
        void initImpl() {
            std::memset(settings.data(), 1, sizeof(settings));
            settings[Settings::LfoOneShotEnabled] = true;
            settings[Settings::DiagnosticOutputEnabled] = false;
            settings[Settings::LfoShapeType] = false;
            if (gDebugOverrides != -1) {
                for (int i = 0; i < Settings::NumSettings; i++) {
                    settings[i] = static_cast<bool>((gDebugOverrides>>i)&1);
                }
            }
            
            auto now = static_cast<uint64_t>(getTimeMillis());
            synthRand.rng_seed(now);
            for (size_t i = 0; i < voices.size(); i++) {
                auto& pv = voices[i];
                pv.init(static_cast<int32_t>(i), static_cast<uint64_t>(synthRand.rng_rand()));
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
                    switch(p->type) {
                        case ParamType::FLOAT:
                            p->format = "%.3f";
                            break;
                        case ParamType::INT:
                            p->format = "%d";
                            break;
                        case ParamType::ENUM:
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
            addFloatParam(Parameters::FilterCutoff)->setRange(-22000.0, 22000.0)->setRangedValue(20.0);
            setParamName(getParam(Parameters::FilterCutoff), "Filter Cutoff", "Flt Cut", "Cutoff", "Hz", "%0.2f");
            addFloatParam(Parameters::FilterResonance)->setRange(0.0, 1.0)->setRangedValue(0.0);
            setParamName(getParam(Parameters::FilterResonance), "Filter Resonance", "Flt Res", "Resonance");
            addFloatParam(Parameters::FilterDrive)->setRange(-1.0, 1.0)->setRangedValue(0.0);
            setParamName(getParam(Parameters::FilterDrive), "Filter Drive", "Flt Drv", "Drive", "dB", "%0.2f");
            addFloatParam(Parameters::FilterKeyTracking)->setRange(-24.0, 24.0)->setRangedValue(0.0);
            setParamName(getParam(Parameters::FilterKeyTracking), "Filter Keytracking", "Flt Trk", "Keytrack");
            for (Parameters p = Parameters::Macro01;
                p <= Parameters::Macro08;
                p = static_cast<Parameters>(static_cast<int>(p) + 1))
            {
                addFloatParam(p)->setRange(0.0, 1.0)->setRangedValue(0.0);
                setParamName(getParam(p), "Macro " + std::to_string(static_cast<int>(p) - static_cast<int>(Parameters::Macro01) + 1));
            }

            addFloatParam(Parameters::FmFine)->setRange(-1.0, 1.0)->setRangedValue(0.0);
            setParamName(getParam(Parameters::FmFine), "Fm fine", "Fm fine", "Fine");
            addIntParam(Parameters::FmCoarse)->setRange(0, 48)->setRangedValue(0);
            setParamName(getParam(Parameters::FmCoarse), "Fm Coarse", "Fm Coarse", "Coarse");

            addFloatParam(Parameters::OscMix)->setRange(0.0, 1.0)->setRangedValue(1.0);
            setParamName(getParam(Parameters::OscMix), "Oscillator Mix", "OSC Mix", "Mix");
            addFloatParam(Parameters::Osc1Fine)->setRange(-1.0, 1.0)->setRangedValue(0.0);
            setParamName(getParam(Parameters::Osc1Fine), "Oscillator 1 fine", "OSC1 Fine", "Fine");
            addFloatParam(Parameters::Osc2Fine)->setRange(-1.0, 1.0)->setRangedValue(0.0);
            setParamName(getParam(Parameters::Osc2Fine), "Oscillator 2 fine", "OSC2 Fine", "Fine");
            addFloatParam(Parameters::Osc1Split)->setRange(-1.25, 1.25)->setRangedValue(0.0);
            setParamName(getParam(Parameters::Osc1Split), "Oscillator 1 split", "OSC1 Split", "Split");
            addFloatParam(Parameters::Osc2Split)->setRange(-1.25, 1.25)->setRangedValue(0.0);
            setParamName(getParam(Parameters::Osc2Split), "Oscillator 2 split", "OSC2 Split", "Split");
            addIntParam(Parameters::Osc1Coarse)->setRange(-24, 24)->setRangedValue(0);
            setParamName(getParam(Parameters::Osc1Coarse), "Oscillator 1 coarse", "OSC1 Semi", "Semi");
            addIntParam(Parameters::Osc2Coarse)->setRange(-24, 24)->setRangedValue(0);
            setParamName(getParam(Parameters::Osc2Coarse), "Oscillator 2 coarse", "OSC2 Semi", "Semi");

            addFloatParam(Parameters::VolEnvA)->setRange(0.0, 1.0)->setRangedValue(0.0);
            addFloatParam(Parameters::VolEnvD)->setRange(0.0, 1.0)->setRangedValue(0.5);
            addFloatParam(Parameters::VolEnvS)->setRange(0.0, 1.0)->setRangedValue(1.0);
            addFloatParam(Parameters::VolEnvR)->setRange(0.0, 1.0)->setRangedValue(0.25);
            addFloatParam(Parameters::VolEnvV)->setRange(0.0, 1.0)->setRangedValue(0.0);
            setParamName(getParam(Parameters::VolEnvA), "Volume envelope attack time", "EnvA Att", "Attack");
            setParamName(getParam(Parameters::VolEnvD), "Volume envelope decay time", "EnvA Dec", "Decay");
            setParamName(getParam(Parameters::VolEnvS), "Volume envelope sustain", "EnvA Sus", "Sustain");
            setParamName(getParam(Parameters::VolEnvR), "Volume envelope release time", "EnvA Rel", "Release");
            setParamName(getParam(Parameters::VolEnvV), "Volume envelope velocity sensitivity", "EnvA Vel", "Velocity");

            addFloatParam(Parameters::ModEnvA)->setRange(0.0, 1.0)->setRangedValue(0.0);
            addFloatParam(Parameters::ModEnvD)->setRange(0.0, 1.0)->setRangedValue(0.5);
            addFloatParam(Parameters::ModEnvS)->setRange(0.0, 1.0)->setRangedValue(0.5);
            addFloatParam(Parameters::ModEnvR)->setRange(0.0, 1.0)->setRangedValue(0.5);
            addFloatParam(Parameters::ModEnvV)->setRange(0.0, 1.0)->setRangedValue(0.0);
            setParamName(getParam(Parameters::ModEnvA), "Mod envelope attack time", "EnvM Att", "Attack");
            setParamName(getParam(Parameters::ModEnvD), "Mod envelope decay time", "EnvM Dec", "Decay");
            setParamName(getParam(Parameters::ModEnvS), "Mod envelope sustain", "EnvM Sus", "Sustain");
            setParamName(getParam(Parameters::ModEnvR), "Mod envelope release time", "EnvM Rel", "Release");
            setParamName(getParam(Parameters::ModEnvV), "Mod envelope velocity sensitivity", "EnvM Vel", "Velocity");

            addFloatParam(Parameters::LfoShape)->setRange(-1.0f, 1.0f)->setRangedValue(0.0);
            addFloatParam(Parameters::LfoFrequency)->setRange(1 / 64.0, 16.0)->setRangedValue(4.0);
            addFloatParam(Parameters::LfoDelay)->setRange(0.001f, 1000.0)->setRangedValue(0.1);
            setParamName(getParam(Parameters::LfoShape), "LFO shape", "LFO shape", "Shape");
            setParamName(getParam(Parameters::LfoFrequency), "LFO frequency", "LFO freq", "Frequency");
            setParamName(getParam(Parameters::LfoDelay), "LFO ramp", "LFO ramp", "Ramp");

            addFloatParam(Parameters::VolEnvFm)->setRange(-24.0, 24.0)->setRangedValue(0.0);
            addFloatParam(Parameters::ModEnvFm)->setRange(-24.0, 24.0)->setRangedValue(0.0);
            addFloatParam(Parameters::LfoFm)->setRange(-24.0, 24.0)->setRangedValue(0.0);
            addFloatParam(Parameters::VolEnvCutoff)->setRange(-24000.0, 24000.0)->setRangedValue(0.0);
            addFloatParam(Parameters::ModEnvCutoff)->setRange(-24000.0, 24000.0)->setRangedValue(0.0);
            addFloatParam(Parameters::LfoCutoff)->setRange(-2 * 24000.0, 2 * 24000.0)->setRangedValue(0.0);
            addFloatParam(Parameters::GlideLength)->setRange(0.0, 1.0)->setRangedValue(0.0);
            addFloatParam(Parameters::MasterVolume)->setRange(0.0, 0.5)->setRangedValue(0.25);

            setParamName(getParam(Parameters::VolEnvFm), "Volume envelope to FM amount", "FM Amt EnvA", "Env A Amount");
            setParamName(getParam(Parameters::ModEnvFm), "Modulation envelope to FM amount", "FM Amt EnvM", "Env M Amount");
            setParamName(getParam(Parameters::LfoFm), "LFO to FM amount", "FM Amt LFO", "LFO Amount");
            setParamName(getParam(Parameters::VolEnvCutoff), "Volume envelope to filter cutoff", "Flt EnvA", "Flt EnvA");
            setParamName(getParam(Parameters::ModEnvCutoff), "Modulation envelope to filter cutoff", "Flt EnvM", "Flt EnvM");
            setParamName(getParam(Parameters::LfoCutoff), "Modulation LFO to filter cutoff", "Flt LFO", "Flt LFO");
            setParamName(getParam(Parameters::GlideLength), "Glide length", "Glide");
            setParamName(getParam(Parameters::MasterVolume), "Volume", "Volume", "Volume", "%");


            addEnumParam(Parameters::Osc1Wave)->setStrings(stringsWaveform.begin(), stringsWaveform.end())->setRangedValue(0);
            setParamName(getParam(Parameters::Osc1Wave), "Osc1 Waveform", "Osc1 Waveform", "Waveform");
            addEnumParam(Parameters::Osc2Wave)->setStrings(stringsWaveform.begin(), stringsWaveform.end())->setRangedValue(0);
            setParamName(getParam(Parameters::Osc2Wave), "Osc2 Waveform", "Osc2 Waveform", "Waveform");
            addEnumParam(Parameters::LfoWave)->setStrings(stringsWaveform.begin(), stringsWaveform.end())->setRangedValue(0);
            setParamName(getParam(Parameters::LfoWave), "Lfo Waveform", "Lfo Waveform", "Waveform");
            addEnumParam(Parameters::VoiceMode)->setStrings(stringsVoiceMode.begin(), stringsVoiceMode.end())->setRangedValue(0);
            setParamName(getParam(Parameters::VoiceMode), "Voice Mode");
            addEnumParam(Parameters::FilterMode)->setStrings(stringsFilterMode.begin(), stringsFilterMode.end())->setRangedValue(0);
            setParamName(getParam(Parameters::FilterMode), "Filter Mode", "Flt Mode");
            addEnumParam(Parameters::FmMode)->setStrings(stringsFMMode.begin(), stringsFMMode.end())->setRangedValue(0);
            setParamName(getParam(Parameters::FmMode), "Fm Mode");

            addIntParam(Parameters::Voices)->setRange(1, NUM_POLY_VOICES)->setRangedValue(32);
            addIntParam(Parameters::UnisonVoices)->setRange(1, NUM_UNISON_VOICES)->setRangedValue(3);

            setParamName(getParam(Parameters::Voices), "Polyphonic Voice Maximum", "Voices");
            setParamName(getParam(Parameters::UnisonVoices), "Unison Voices", "Unison");

            addFloatParam(Parameters::Panning)->setRange(-1.0, 1.0)->setRangedValue(0.0);
            setParamName(getParam(Parameters::Panning), "Stereo Panning", "Pan");
            modulations.push_back(Modulation());
        }

    public:
        SynthImpl(PluginVST2_Synth* vst2Plugin)
            : SynthState(),
              instanceVst2(vst2Plugin) {
            (void) instanceVst2;
            initImpl();
        }
        bool getSetting(Settings setting) const {
            return settings[setting];
        }
        void setSetting(Settings setting, bool value) {
            settings[setting] = value;
        }
        void init() {
            for (auto param : this->vecParams) {
                OnParamChange(param->enumParam);
            }
            setModulationType(0, 0, static_cast<int32_t>(ModulationType::ModulationSource)+static_cast<int32_t>(ModulationSourceType::Lfo1));
            setModulationDestination(0, 0, Parameters::FilterCutoff, 0.5);
        }
        void initSampleRate() {
            const auto dt = oneOverSR;
            for (auto& uv : voices) {
                ModulationSourceData data;
                uv.visitVoices([this, dt, &uv, &data](auto& v) {
                    UpdateVoiceModulations(dt, uv, v, data);
                    UpdateVoiceEnvelopeModulations(uv, v);
                    UpdateVoiceEnvelopes(dt, uv, v);
                });
            }
        }
        bool IsBipolarModulation(const Modulation& modulation) const {
            for (auto& source : modulation.inputs) {
                if (source.range == ModulationRange::Bipolar) return true;
            }
            return false;
        }
        double resolveAverageModulation(const Modulation& modulation, Parameters _param) const {
            double modAbsMax = 0.0;
            const SynthParam_Enum* const param = GetParamEnum(Parameters::FilterMode);
            auto filterMode = param->getEnumValue<FilterModes>();
            // FilterModes mode = GetParamEnum(Parameters::FilterMode)->
            // int32_t totalVoiceCount = 0.0;
            for (auto polyvoice : voices) {
                polyvoice.visitVoices([&](const auto& unisonVoice) {
                    if (unisonVoice.isVoiceActive(filterMode)) {
                        modAbsMax = math::absMax(modAbsMax, GetModulatedParamVoiceRaw(unisonVoice, _param));
                        // modAbsMax = math::absMax(modAbsMax, 1.0);
                        // totalVoiceCount++;
                    }
                });
            }
            // return totalVoiceCount == 0 ? 0.0 : (avg / totalVoiceCount);
            return modAbsMax;
        }
        std::optional<std::vector<param_modulation_range_t>> getParamModulationRanges(Parameters _param) {
            std::optional<std::vector<param_modulation_range_t>> result;
            int32_t idxMod = 0;
            for (auto& mod : modulations) {
                bool bIsBipolar = IsBipolarModulation(mod);
                for (auto& modDest : mod.destinations) {
                    if (modDest.parameter == _param) {
                        if (!result) {
                            result = std::vector<param_modulation_range_t>();
                        }
                        auto modIdx = &mod - &modulations.front();
                        result->push_back(
                                param_modulation_range_t{
                                        static_cast<int32_t>(modIdx),
                                        static_cast<int32_t>(modDest.parameter),
                                        static_cast<float>(modDest.range),
                                        bIsBipolar,
                                        resolveAverageModulation(mod, _param) });
                        dbgassert(result->back().sourceId == idxMod);
                    }
                }
                idxMod++;
            }
            return result;
        }
        Modulation* getModulationIfExists(int32_t index) {
            if (index < 0 || index >= CtrSize(modulations)) {
                return nullptr;
            }
            return &modulations[index];
        }
        Modulation& getOrCreateModulation(int32_t index) {
            while (CtrSize(modulations) <= index) {
                modulations.emplace_back();
            }
            return modulations[index];
        }
        int32_t getModulationCount() const {
            return CtrSize(modulations);
        }
        bool setModulationType(int32_t slotIndex, int32_t srcSlotIndex, int32_t typeIdx) {
            auto& modulation = getOrCreateModulation(slotIndex);
            auto numInputs   = CtrSize(modulation.inputs);
            if (typeIdx < 0) {
                if (srcSlotIndex >= 0 && srcSlotIndex < numInputs) {
                    // erase entry
                    modulation.inputs.erase(modulation.inputs.begin() + srcSlotIndex);
                    return true;
                }
                return false;
            }
            const auto modType = typeIdx >= ModulationType::ModulationSource ? ModulationType::ModulationSource : static_cast<ModulationType>(typeIdx);
            const auto modSrcType = modType == ModulationType::ModulationSource ? static_cast<ModulationSourceType>(typeIdx-ModulationType::ModulationSource) : ModulationSourceType::Lfo1;
            if (srcSlotIndex == numInputs) {
                ModulationInput input = {
                    modType,
                    modSrcType,
                    ModulationOperator::Multiply, 
                    0.0, 
                    MathExpr{},
                    ModulationRange::Unipolar,
                };
                modulation.inputs.emplace_back(std::move(input));
                return true;
            } else if (srcSlotIndex < numInputs) {
                auto& mod  = modulation.inputs[srcSlotIndex];
                mod.type = modType;
                mod.src = modSrcType;
                return true;
            }
            return false;
        }
        bool setModulationOperator(int32_t index, int32_t idx, int32_t modOperatorIndex) {
            auto& modulation = getOrCreateModulation(index);
            auto numInputs   = CtrSize(modulation.inputs);
            if (modOperatorIndex >= 0 && modOperatorIndex < ModulationOperator::NumModulationOperators && idx < numInputs) {
                auto& mod = modulation.inputs[idx];
                mod.op    = static_cast<ModulationOperator>(modOperatorIndex);
                return true;
            }
            return false;
        }
        bool setModulationConstant(int32_t index, int32_t idx, double constant) {
            auto& modulation = getOrCreateModulation(index);
            auto numInputs   = CtrSize(modulation.inputs);
            if (idx < numInputs) {
                auto& mod = modulation.inputs[idx];
                mod.value = constant;
                return true;
            }
            return false;
        }
        bool setModulationFunction(int32_t index, int32_t idx, MathExpr&& function) {
            auto& modulation = getOrCreateModulation(index);
            auto numInputs   = CtrSize(modulation.inputs);
            if (idx < numInputs) {
                auto& mod    = modulation.inputs[idx];
                mod.function = std::move(function);
                return true;
            }
            return false;
        }
        bool resetModulationFunction(int32_t index, int32_t idx) {
            auto& modulation = getOrCreateModulation(index);
            auto numInputs   = CtrSize(modulation.inputs);
            if (idx < numInputs) {
                auto& mod = modulation.inputs[idx];
                mod.function.parsedExpr.reset();
                return true;
            }
            return false;
        }
        bool setModulationInputRange(int32_t index, int32_t idx, ModulationRange range) {
            auto& modulation = getOrCreateModulation(index);
            auto numInputs   = CtrSize(modulation.inputs);
            if (idx < numInputs) {
                auto& mod     = modulation.inputs[idx];
                mod.range = range;
                return true;
            }
            return false;
        }

        bool setModulationDestination(int32_t index, int32_t destIdx, int32_t paramIdx, double range) {
            auto& modulation     = getOrCreateModulation(index);
            auto numDestinations = CtrSize(modulation.destinations);
            if (paramIdx < 0 && destIdx < numDestinations) {
                // erase entry
                modulation.destinations.erase(modulation.destinations.begin() + destIdx);
                return true;
            } else if (paramIdx >= 0 && paramIdx < Parameters::kNumParams && destIdx == numDestinations) {
                modulation.destinations.push_back({ static_cast<Parameters>(paramIdx), range });
                return true;
            } else if (paramIdx >= 0 && paramIdx < Parameters::kNumParams && destIdx < numDestinations) {
                modulation.destinations[destIdx] = { static_cast<Parameters>(paramIdx), range };
                return true;
            }
            return false;
        }
        bool setModulationDestRange(int32_t index, int32_t destIdx, double range) {
            auto& modulation     = getOrCreateModulation(index);
            auto numDestinations = CtrSize(modulation.destinations);
            if (destIdx < numDestinations) {
                modulation.destinations[destIdx].range = range;
                return true;
            }
            return false;
        }
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

        void setSamplerate(float sr) {
            if (sr < 1) sr = 1;
            oneOverSR = 1.0 / sr;
            if (!bIsInitSamplerate) {
                bIsInitSamplerate = true;
                initSampleRate();
            }
        }
        double getSamplerate() const {
            return 1.0 / oneOverSR;
        }

        void ProcessMidiMsg(IMidiMsg& msg) {
            midiQueue.Add(msg);
        }
        std::vector<int> getHeldNotes() {
            return heldNotes;
        }
        int32_t getActiveVoiceCount() {
            return activeVoiceCount;
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
        bool getSnapshot(snapshot_t& snapshot) {
            snapshot.version     = 7;
            const auto numParams = CtrSize(vecParams);
            for (int32_t i = 0; i < numParams; ++i) {
                snapshot.params.push_back({ i, vecParams[i]->getAsDouble() });
            }
            const auto numModulations = CtrSize(modulations);
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
                    auto inputType = math::clamp<int32_t>(input.type, 0, ModulationType::NumModulationTypes - 1);
                    auto inputSrcType = math::clamp<int32_t>(input.src, 0, ModulationSourceType::NumModulationSources - 1);
                    auto inputOpType = math::clamp<int32_t>(input.op, 0, ModulationOperator::NumModulationOperators - 1);
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
            return true;
        }
        bool setSnapshot(const snapshot_t& snapshot) {
            if (snapshot.version < 2) {
                dbgassert(0);
                return false;
            }
            const auto numParams = CtrSize(vecParams);
            for (auto& ps : snapshot.params) {
                if (ps.paramIdx >= 0 && ps.paramIdx < numParams) {
                    vecParams[ps.paramIdx]->set(ps.value);
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
                    newModulation.destinations.push_back({ static_cast<Parameters>(dest.paramIdx), dest.range });
                }
                while (CtrSize(modulations) <= msSlotIndex) {
                    modulations.push_back({});
                }
                modulations[msSlotIndex] = std::move(newModulation);
            }
            for (auto& mod : modulations) {
                for (auto& input : mod.inputs) {
                    try {
                        input.function = MathExpr::parse(input.function.str);
                    } catch (mu::Parser::exception_type& e) {
                        input.function = MathExpr{};
                        log_lf(Log::L_ERROR, "Error in expression: %s\n", e.GetMsg().c_str());
                    }
                }
            }
            for (auto& param : vecParams) {
                OnParamChange(param->enumParam);
            }
            return true;
        }

    private:
        float synthRandom() {
            uint32_t rnd32Bits = synthRand.rng_rand();
            return (rnd32Bits & 0xFFFF) / (float) 0xFFFF;
        }
        inline SynthParam_Float* GetParamFloat(Parameters param) noexcept {
            dbgassert(getParam(param) && getParam(param)->type == ParamType::FLOAT);
            return static_cast<SynthParam_Float*>(this->vecParams[param]);
        }
        inline SynthParam_Int* GetParamInt(Parameters param) noexcept {
            dbgassert(getParam(param) && getParam(param)->type == ParamType::INT);
            return static_cast<SynthParam_Int*>(this->vecParams[param]);
        }
        inline SynthParam_Enum* GetParamEnum(Parameters param) noexcept {
            dbgassert(getParam(param) && getParam(param)->type == ParamType::ENUM);
            return static_cast<SynthParam_Enum*>(this->vecParams[param]);
        }
        const SynthParam_Enum* GetParamEnum(Parameters param) const noexcept {
            dbgassert(getParam(param) && getParam(param)->type == ParamType::ENUM);
            return static_cast<SynthParam_Enum*>(this->vecParams[param]);
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
                                    if (voice.note == note) voice.Release();
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
                                auto voiceEnd = polyVoiceCount >= CtrSize(voices) ? std::end(voices) : std::begin(voices) + polyVoiceCount;
                                auto voice    = std::min_element(
                                        std::begin(voices),
                                        voiceEnd,
                                        [](auto& a, auto& b) {
                                            bool aReleased = a.IsReleased();
                                            if (aReleased == b.IsReleased()) {
                                                auto volA = a.GetVolume();
                                                auto volB = b.GetVolume();
                                                if (volA <= 0.0 && volB <= 0.0) {
                                                    return a.seqNr < b.seqNr;
                                                }
                                                return volA < volB;
                                            }
                                            return aReleased;
                                        });
                                dbgassert(voice->getNumUnisonVoices() == unisonVoiceCount);
                                voice->SetNote(note);
                                voice->SetVelocity(velocity);
                                voice->ResetPitch();
                                voice->Start(tempo, lfoPhaseDrift);
                                dbgassert(voice->numUnisonActive == unisonVoiceCount);
                                voice->visitVoices([this, voice](auto& v) {
                                    UpdateVoiceEnvelopeModulations(*voice, v);
                                });
                                maxUnisonVoice = math::max(maxUnisonVoice, unisonVoiceCount);
                                break;
                            }
                            default:
                            case VoiceModes::Mono:
                                voices[0].SetNote(note);
                                voices[0].SetVelocity(velocity);
                                voices[0].Start(tempo, lfoPhaseDrift);
                                maxUnisonVoice = math::max(maxUnisonVoice, unisonVoiceCount);
                                break;
                            case VoiceModes::Legato:
                                voices[0].SetNote(note);
                                if (heldNotes.empty()) {
                                    voices[0].SetVelocity(velocity);
                                    voices[0].ResetPitch();
                                    voices[0].Start(tempo, lfoPhaseDrift);
                                    maxUnisonVoice = math::max(maxUnisonVoice, unisonVoiceCount);
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
                        log_lf(Log::L_WARN, "Unhandled midi msg %d\n", (int32_t) status);
                        break;
                }
                midiQueue.Remove();
            }
        }

        void UpdateParameters(double dt) {
            osc1Wave.Update(dt);
            osc1SplitMix += (targetOsc1SplitMix - osc1SplitMix) * 100.0 * dt;
            osc2Wave.Update(dt);
            osc2SplitMix += (targetOsc2SplitMix - osc2SplitMix) * 100.0 * dt;
            lfoWave.Update(dt);
            oscMix += (targetOscMix - oscMix) * 100.0 * dt;
            // filterMode.Update(dt);
            filterCutoff += (targetFilterCutoff - filterCutoff) * 100.0 * dt;
            filterResonance += (targetFilterResonance - filterResonance) * 100.0 * dt;
            filterKeyTracking += (targetFilterKeyTracking - filterKeyTracking) * 100.0 * dt;
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
                using Vec4D = glm::vec<4, FPType, glm::packed_highp>;
                auto sse8Float = reinterpret_cast<__m256*>(&envParamVals[0]);
                *sse8Float = math::simd::log_v8f(*sse8Float);
                auto pIn = &envParamVals[0];
                FPType* pDataOut = &envParamValsScaled[0];
                for (size_t j = 0; j < LEN_SIMD; j += 4) {
                    Vec4D& valsRef = *reinterpret_cast<Vec4D*>(&pIn[0]);
                    auto vals = valsRef * 0.1f;
                    auto sse4Float = reinterpret_cast<__m128*>(&vals);
                    *sse4Float = math::simd::exp_v4f(*sse4Float);
                    auto floatPtr = reinterpret_cast<float*>(&vals[0]);
                    math::simd::cos_test<float, 4>(floatPtr, pDataOut);
                    for (size_t k = 0; k < 4; k++) {
                        pDataOut[k] = 1000.0f - 999.9f * (.5f - .5f * pDataOut[k]);
                    }
                    pIn += 4;
                    pDataOut += 4;
                }
                for (int i = 0; i < LEN_USED; i++) {
                    *envParamValsPtr[i] = double(envParamValsScaled[i]);
                }
                voice.volEnv.s = GetModulatedParamVoice(voice, Parameters::VolEnvS);
                voice.modEnv.s = GetModulatedParamVoice(voice, Parameters::ModEnvS);
                auto p         = GetParamFloat(Parameters::LfoDelay);
                voice.lfoEnv.a = p->GetMin() + p->GetMax() - (p->ValueModulated(voice.modValues[p->enumParam]));
            }
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
        static inline double noteToLinearScale(double note, double minNote = 69.0) {
            return exp(0.69314718055994530942 * ((note - minNote) / 12.0));
            // return pow(2.0, (note - minNote) / 12.0);
        }
        double EvaluateVoiceModulationMathExpr(VoiceUnison& vu, Voice& voice, const MathExpr& expr, std::array<double, MathExprInputLen>& inputModSources) {
            if (expr.parsedExpr) {
                auto& parsedExpr = *expr.parsedExpr;
                auto& inputs     = parsedExpr.inputs;
                dbgassert(inputs.size() == inputModSources.size());
                memcpy(inputs.data(), inputModSources.data(), sizeof(double) * math::min(inputs.size(), inputModSources.size()));
                double dResult = parsedExpr.parser.Eval();
                if (fp_math::isNanOrInfd(dResult)) {
                    dResult = 0.0;
                    parsedExpr.nanInfCounter++;
                }
                return dResult;
            }
            return 0.0;
        }
        using UnisonVoiceList = std::array<int32_t, NUM_POLY_VOICES*NUM_UNISON_VOICES>;
        using PolyVoiceList = std::array<int32_t, NUM_POLY_VOICES>;

        struct VoiceList {
            UnisonVoiceList unisonVoices;
            PolyVoiceList polyVoices;
            int32_t numUnisonVoices;
            int32_t maxUnisonVoices;
            int32_t numPolyVoices;
        };
        void UpdateAllVoiceStates(double dt, FilterModes filterMode, VoiceList& list) {
            dbgassert((list.numPolyVoices == 0) == (list.numUnisonVoices == 0));
            list.maxUnisonVoices = 0;
            for (int32_t polyIndex = 0; polyIndex < polyVoiceCount; ++polyIndex) {
                auto& uv = voices[polyIndex];
                int32_t numAc=0;
                for (int32_t unisonIndex = 0; unisonIndex < maxUnisonVoice; ++unisonIndex) {
                    auto& v = uv.getVoice(unisonIndex);
                    bool bIsActive = v.isVoiceActive(filterMode);
                    dbgassert(!(!v.bIsActive && bIsActive));
                    v.bIsActive = bIsActive;
                    if (bIsActive) {
                        list.unisonVoices[list.numUnisonVoices++] = polyIndex*NUM_UNISON_VOICES+unisonIndex;
                        list.maxUnisonVoices = math::max(list.maxUnisonVoices, unisonIndex + 1);
                        numAc++;
                    }
                }
                uv.numUnisonActive = numAc;
                if (uv.numUnisonActive) {
                    list.polyVoices[list.numPolyVoices++] = polyIndex;
                }
            }
            maxUnisonVoice = list.maxUnisonVoices;
            dbgassert((list.numPolyVoices > 0) == (list.numUnisonVoices > 0));
        }
        void UpdateAllVoiceLfos(double dt, const VoiceList& list) {
            auto floatParamFreq = static_cast<SynthParam_Float*>(vecParams[Parameters::LfoFrequency]);
            auto floatParamShape = static_cast<SynthParam_Float*>(vecParams[Parameters::LfoShape]);
            const auto lfoOneShot = settings[Settings::LfoOneShotEnabled];
            const auto bpmHz    = math::max(tempo.bpm, 1.0) / 60.0;
            const auto lfoShapeMode = settings[Settings::LfoShapeType];
            for (int32_t p = 0; p < list.numPolyVoices; ++p) {
                auto& uv = voices[list.polyVoices[p]];
                uv.UpdateVoiceLfo(dt, tempo);
                for (int32_t unisonIndex = 0; unisonIndex < list.maxUnisonVoices; ++unisonIndex) {
                    auto& v = uv.getVoice(unisonIndex);
                    v.UpdateVoiceLfo(dt, tempo, !lfoOneShot);
                    
                    if (v.bIsActive) {
                        double lfoFreqHz    = floatParamFreq->ValueModulated(v.modValues[Parameters::LfoFrequency]) * bpmHz;

                        double dVoiceLfoBi  =  v.lfo1.GetLfo(dt, lfoWave, lfoFreqHz, lfoOneShot);
                        double dVoiceLfoUni = 0.5 + 0.5 * dVoiceLfoBi;
                        dbgassert(dVoiceLfoUni >= 0.0 && dVoiceLfoUni <= 1.0);
                        double lfoAmount    = floatParamShape->ValueModulated(v.modValues[Parameters::LfoShape]);
                        double dLfoShapeExp;
                        if (lfoAmount < 0.0) {
                            dLfoShapeExp = 1.0 + dVoiceLfoUni * -lfoAmount * 16.;
                        } else {
                            dLfoShapeExp = 1.0 / (1.0 + dVoiceLfoUni * lfoAmount * 16.);
                        }
                        dbgassert(!fp_math::isNanOrInfd(dVoiceLfoUni));
                        dbgassert(!fp_math::isNanOrInfd(dLfoShapeExp));
                        double dVoiceLfoUniShaped;
                        if (lfoShapeMode) {
                            dVoiceLfoUniShaped = exp(log(dVoiceLfoUni*dVoiceLfoUni) * dLfoShapeExp);
                        } else {
                            dVoiceLfoUniShaped = exp(log(abs(dVoiceLfoUni)) * dLfoShapeExp);
                        }
                        dbgassert(!fp_math::isNanOrInfd(dVoiceLfoUniShaped));
                        v.lfoValue = dVoiceLfoUniShaped;
                    }
                }
            }
        }
        using ModulationSourceData = std::array<double, MathExprInputLen>;

        void UpdateVoiceModulations(double dt, VoiceUnison& vu, Voice& voice, ModulationSourceData& modSrcData) {

            if (!settings[Settings::ModulationEnabled]) {
                return;
            }
            
            auto& voiceModulations = voice.modValues;
            if (settings[Settings::ClearModulationEnabled]) {
                std::memset(voiceModulations.data(), 0, voiceModulations.size() * sizeof(double));
            }
            // std::array<double, MathExprInputLen> sourcesV{};
            modSrcData[1+ModulationSourceType::VolEnv] = voice.volEnv.value;
            modSrcData[1+ModulationSourceType::ModEnv] = voice.modEnv.value;
            modSrcData[1+ModulationSourceType::Lfo1] = voice.lfoValue;
            modSrcData[1+ModulationSourceType::Velocity] = voice.velocity;
            modSrcData[1+ModulationSourceType::VoiceIndex] = this->polyVoiceCount < 2 ? 0.5 : vu.indexPoly / static_cast<double>(this->polyVoiceCount - 1);
            modSrcData[1+ModulationSourceType::UnisonVoiceIndex] = this->unisonVoiceCount < 2 ? 0.5 : voice.indexUnison / static_cast<double>(this->unisonVoiceCount - 1);
            modSrcData[1+ModulationSourceType::Pitch] = noteToLinearScale(voice.note);
            modSrcData[1+ModulationSourceType::Note] = voice.note / 127.0;
            for (auto& modulation : modulations) {
                ModulationSourceData& sources = modSrcData;
                double modVal = 0.0;
                for (size_t j = 0; j < modulation.inputs.size(); j++) {
                    sources.front() = modVal;
                    // if (modulation.destinations.empty())
                    //     continue;
                    auto& input   = modulation.inputs[j];
                    double srcVal = 0.0;
                    switch (input.type) {
                        case ModulationType::ModulationSource:
                            srcVal = sources[input.src + 1];
                            break;
                        case ModulationType::Constant:
                            srcVal = input.value;
                            break;
                        case ModulationType::Function:
                            if (settings[Settings::ExprEvaluationEnabled]) {
                                srcVal = EvaluateVoiceModulationMathExpr(vu, voice, input.function, sources);
                            }
                            break;
                        default:
                            break;
                    }
                    if (input.range == ModulationRange::Bipolar && input.type != ModulationType::Function) {
                        srcVal = (srcVal * 2.0) - 1.0;
                    }
                    if (input.range == ModulationRange::Triangle && input.type != ModulationType::Function) {
                        srcVal = std::fabs((srcVal * 2.0) - 1.0);
                    }
                    if (j > 0 && input.type != ModulationType::Function) {
                        switch (input.op) {
                            case ModulationOperator::Multiply:
                                srcVal = modVal * srcVal;
                                break;
                            case ModulationOperator::Add:
                                srcVal = modVal + srcVal;
                                break;
                            case ModulationOperator::Divide:
                                if (math::abs(srcVal) < 1e-6) {
                                    srcVal = 1e-6;
                                } else {
                                    srcVal = modVal / srcVal;
                                }
                                break;
                            case ModulationOperator::Subtract:
                                srcVal = modVal - srcVal;
                                break;
                            case ModulationOperator::MultiplyNegative:
                                srcVal = modVal * (1-srcVal);
                                break;
                            case ModulationOperator::Absolute:
                                srcVal = abs(modVal * srcVal);
                                break;
                            case ModulationOperator::Power:
                                srcVal = exp(log(srcVal) * modVal);
                                break;
                            case ModulationOperator::Clamp:
                                srcVal = math::clamp(srcVal, double(input.range == ModulationRange::Bipolar) * -1.0, 1.0);
                                break;
                            default:
                                srcVal = modVal;
                                break;
                        }
                    }
                    modVal = srcVal;
                }
                for (auto& dest : modulation.destinations) {
                    size_t destIdx = dest.parameter;
                    voiceModulations[destIdx] += modVal * dest.range;
                }
            }
        }

        void UpdateDrift(double dt) {
            driftVelocity += synthRandom() * 4.0 * dt;
            driftVelocity -= driftVelocity * 2.0 * dt;
            driftPhase += driftVelocity * dt;
            driftValue = .001 * sin(driftPhase * 2.0 * M_PI);
        }
        double GetModulatedParamVoice(Voice& voice, Parameters param) const {
            dbgassert(param < vecParams.size());
            dbgassert(vecParams[param]->type == ParamType::FLOAT);
            return static_cast<SynthParam_Float*>(vecParams[param])->ValueModulated(voice.modValues[param]);
        }
        double GetModulatedIntParamVoice(Voice& voice, Parameters param) const {
            dbgassert(param < vecParams.size());
            dbgassert(vecParams[param]->type == ParamType::INT);
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
            if (settings[Settings::TuningDriftEnabled]) {
                baseFrequency *= (1.0 + (voice.driftValue+driftValue) * tuningDrift);
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

            auto out = 0.0;
            double oscMix = GetModulatedParamVoice(voice, Parameters::OscMix);
            // if (oscMix < .999)
             {
                auto osc1Out = 0.0;
                double osc1SplitFactorA = pitchFactor(GetModulatedParamVoice(voice, Parameters::Osc1Split));
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
                double osc2SplitFactorA = pitchFactor(GetModulatedParamVoice(voice, Parameters::Osc2Split));
                double targetOscSplitMix = osc2SplitFactorA != 0.0 ? 1.0 : 0.0;
                auto osc2Out = 0.0;
                osc2Out += voice.osc2a.Get(dt, osc2Wave, osc2Frequency * osc2SplitFactorA, true);
                dbgassert(!fp_math::isNanOrInfd(osc2Out));
                // if (osc2SplitMix > .001)
                    osc2Out += targetOscSplitMix * voice.osc2b.Get(dt, osc2Wave, osc2Frequency * osc2SplitFactorB, true);
                dbgassert(!fp_math::isNanOrInfd(osc2Out));
                out += osc2Out * sqrt(oscMix);
                dbgassert(!fp_math::isNanOrInfd(out));
            }

            out *= volEnvValue;

            auto filterDrive = GetModulatedParamVoice(voice, Parameters::FilterDrive);
            if (filterDrive < 0.0) {
                out *= 1.0+filterDrive;
            } else {
                filterDrive *= 2.0;
                out *= 1.0+filterDrive;
                if (out > filterDrive)
                    out = filterDrive + (1 - filterDrive) * tanh ((out - filterDrive) / (1 - filterDrive));
                else if (out < -filterDrive)
                    out = -(filterDrive + (1 - filterDrive) * tanh ((-out - filterDrive) / (1 - filterDrive))); 
            }
            if (settings[Settings::FilterEnabled]) {
                // auto cutoff = filterCutoff;
                auto cutoff = GetModulatedParamVoice(voice, Parameters::FilterCutoff);
                cutoff += GetModulatedParamVoice(voice, Parameters::VolEnvCutoff) * volEnvValue;
                cutoff += GetModulatedParamVoice(voice, Parameters::ModEnvCutoff) * modEnvValue;
                cutoff += GetModulatedParamVoice(voice, Parameters::LfoCutoff) * delayedLfoValue;;
                cutoff += pitchFactor(GetModulatedParamVoice(voice, Parameters::FilterKeyTracking)) * osc1Tune * baseFrequency;
                cutoff = math::clamp(cutoff, 20.0, 1.0 / dt * 0.7);
                if (settings[Settings::FilterDriftEnabled]) {
                    cutoff *= 1.0 - filterDrift * (voice.driftValue+driftValue);
                }
                // data = cutoff*dt;
                // auto res = filterResonance;
                auto res = static_cast<SynthParam_Float*>(vecParams[Parameters::FilterResonance])->ValueModulated(voice.modValues[Parameters::FilterResonance]);
                out    = voice.filter.Process(dt, out, filtermode, cutoff, res);
            }
            data = 800.0 * driftValue;
            // out *= volEnvValue;

            return out;
        }

    public:
        void onTransportChanged(bool bIsPlaying) {
            double lfo1Tempo = 1.0;
            double lfo2Tempo = 1.0 / 4.0;
            lfo.initPhase(fmod(tempo.ppqPos * lfo1Tempo, 1.0));
            lfo2.initPhase(fmod(tempo.ppqPos * lfo2Tempo, 1.0));

            // log_lf(Log::L_DEBUG, "Reset LFO phases: at ppq/4 %f\n", fmod(tempo.ppqPos/4.0, 1.0));
            if (settings[Settings::LfoPhaseDriftEnabled]) {
                lfoPhaseDrift = 0.8;
            } else {
                lfoPhaseDrift = 0.0;
            }
            // for (auto& uv : voices) {
            //     uv.visitVoices([&](auto& voice) {
            //         voice.lfo1.initPhase(fmod(tempo.ppqPos * lfo1Tempo + lfoPhaseDrift*driftValue * voice.rand.rng_double(), 1.0));
            //         voice.lfo2.initPhase(fmod(tempo.ppqPos * lfo2Tempo + lfoPhaseDrift*uv.driftValue, 1.0));
            //     });
            // };
        }

        void ProcessSynth(float** inputs, float** outputs, int nFrames) {
            double bpmHz         = math::max(tempo.bpm, 1.0) / 60.0;
            const auto bpmDiv4Hz = math::max(tempo.bpm / 4.0, 1.0) / 60.0;
            const auto mvInv           = sqrt(1.0 / math::max<double>(1.0, this->unisonVoiceCount));
            const auto voiceMode       = GetParamEnum(Parameters::VoiceMode)->getEnumValue<VoiceModes>();
            const FilterModes filterMode = GetParamEnum(Parameters::FilterMode)->getEnumValue<FilterModes>();
            const bool bIsGlideEnabled = voiceMode != VoiceModes::Poly;
            int32_t numActiveVoices    = 0;
            const auto dt = oneOverSR;
            auto tempo = this->tempo;
            ModulationSourceData modSrcData;
            const bool bDiagnostic = settings[Settings::DiagnosticOutputEnabled];
            for (int s = 0; s < nFrames; s++) {
                FlushMidi(s);
                UpdateParameters(dt);
                UpdateDrift(dt);
                if (lfo2.Update(dt, bpmDiv4Hz)) {
                    lfo.initPhase(0.0);
                }
                lfo2Value = 0.5 + 0.5 * lfo2.GetWaveform(Waveforms::Saw, true);
                // calculate lfo freqency in Hz based on tempo
                double lfoFreqHz = GetParamFloat(Parameters::LfoFrequency)->Value() * bpmHz;
                lfoValue         = lfo.Get(dt, lfoWave, lfoFreqHz, false);
                
                VoiceList list{};
                UpdateAllVoiceStates(dt, filterMode, list);
                if (settings[Settings::LfoEnabled]) {
                    UpdateAllVoiceLfos(dt, list);
                }
                modSrcData[1+ModulationSourceType::Lfo2] = lfo2Value;
                modSrcData[1+ModulationSourceType::SrcMacro01] = GetParamFloat(Parameters::Macro01)->Value();
                modSrcData[1+ModulationSourceType::SrcMacro02] = GetParamFloat(Parameters::Macro02)->Value();
                modSrcData[1+ModulationSourceType::SrcMacro03] = GetParamFloat(Parameters::Macro03)->Value();
                modSrcData[1+ModulationSourceType::SrcMacro04] = GetParamFloat(Parameters::Macro04)->Value();
                modSrcData[1+ModulationSourceType::SrcMacro05] = GetParamFloat(Parameters::Macro05)->Value();
                modSrcData[1+ModulationSourceType::SrcMacro06] = GetParamFloat(Parameters::Macro06)->Value();
                modSrcData[1+ModulationSourceType::SrcMacro07] = GetParamFloat(Parameters::Macro07)->Value();
                modSrcData[1+ModulationSourceType::SrcMacro08] = GetParamFloat(Parameters::Macro08)->Value();
                int32_t numActiveVoices = list.numUnisonVoices;
                for (int32_t i = 0; i < numActiveVoices; ++i) {
                    auto& uv = voices[list.unisonVoices[i]/NUM_UNISON_VOICES];
                    auto& v = uv.voices[list.unisonVoices[i]%NUM_UNISON_VOICES];
                    if (bIsGlideEnabled) {
                        v.frequency += (v.targetFrequency - v.frequency) * glideLength * dt;
                    } 
                    UpdateVoiceModulations(dt, uv, v, modSrcData);
                    UpdateVoiceEnvelopeModulations(uv, v);
                    UpdateVoiceEnvelopes(dt, uv, v);
                }
                // for (int32_t polyIndex = 0; polyIndex < polyVoiceCount; ++polyIndex) {
                //     auto& uv = voices[polyIndex];
                //     for (int32_t unisonIndex = 0; unisonIndex < maxUnisonVoice; ++unisonIndex) {
                //         auto& v = uv.getVoice(unisonIndex);
                //         UpdateVoiceEnvelopes(dt, uv, v);
                //     }
                // }
                auto outL        = 0.0;
                if (bDiagnostic) {
                    outL        = -1.0;
                }
                auto outR        = 0.0;

                int32_t numActiveVoicesFrame = 0;
                for (int32_t polyIndex = 0; polyIndex < polyVoiceCount; ++polyIndex) {
                    auto& uv = voices[polyIndex];
                    for (int32_t unisonIndex = 0; unisonIndex < maxUnisonVoice; ++unisonIndex) {
                        auto& v = uv.getVoice(unisonIndex);
                        if (!v.bIsActive) {
                            continue;
                        }
                        auto voiceVolume = GetModulatedParamVoice(v, Parameters::MasterVolume);
                        // auto noise = (synthRand.rng_double()*2-1)*0.002;
                        auto vData = -1.0;
                        double vVal = GetVoiceImpl(dt, uv, v, filterMode, vData);
                        auto voice = vVal * mvInv * voiceVolume;
                        auto panningMinusOneToOne = GetModulatedParamVoice(v, Parameters::Panning);
                        auto panningUnipolar      = panningMinusOneToOne * 0.5 + 0.5;
                        numActiveVoicesFrame++;
                        constexpr bool autopan = false;
                        double pan             = panningUnipolar;
                        if (autopan) {
                            pan += (unisonVoiceCount == 2) ? (unisonIndex & 1) : (unisonIndex / (unisonVoiceCount - 1.0));
                            pan *= 0.5;
                        }
                        outR += voice * sqrt(pan);
                        if (bDiagnostic) {
                            if (unisonIndex == 0) {
                                outL = vData;
                            }
                        } else {
                            outL += voice * sqrt(1.0 - pan);
                        }
                    }
                }
                outputs[0][s]   = fp_math::silenceNanInfd(outL);
                outputs[1][s]   = fp_math::silenceNanInfd(outR);
                
            dbgassert((list.numPolyVoices > 0) == (list.numUnisonVoices > 0));
                numActiveVoices = math::max(math::max(0, list.numUnisonVoices), numActiveVoices);
            }
            this->activeVoiceCount = numActiveVoices;
        }
        double getEnvDuration(double value) const {
            value = math::max(0.0, value);
            return 1000 - 999.9 * (.5 - .5 * cos(pow(value, .1) * M_PI));
            // return 0.1+1000.0*(1.0-pow(value, 0.08+value*-0.12));
        }
        double getEnvDuration2(double value) const {
            value            = math::max(0.0, value);
            double shapedVal = value;
            if (shapedVal > 1.0E-12) {
                auto a = log(shapedVal);
                dbgassert(!fp_math::isNanOrInfd(a));
                auto b = exp(a * 0.1);
                dbgassert(!fp_math::isNanOrInfd(b));
                auto c = cos(b * M_PI);
                dbgassert(!fp_math::isNanOrInfd(c));
                shapedVal = (.5 - .5 * cos(exp(log(shapedVal) * 0.1) * M_PI));
                dbgassert(!fp_math::isNanOrInfd(shapedVal));
            }
            return 1000 - 999.9 * shapedVal;
        }
        void OnParamChange(Parameters parameter) {
            //IMutexLock lock(this);
            //auto value = GetParam(parameter)->Value();
            double value                         = 0.0;
            auto paramInstance                   = getParam(parameter);
            SynthParam_Float* paramFloatOptional = nullptr;
            SynthParam_Int* paramIntOptional     = nullptr;
            SynthParam_Enum* paramEnumOptional   = nullptr;
            switch (paramInstance->getType()) {
                case ParamType::FLOAT:
                    paramFloatOptional = static_cast<SynthParam_Float*>(paramInstance);
                    value              = paramFloatOptional->Value();
                    break;
                case ParamType::INT:
                    paramIntOptional = static_cast<SynthParam_Int*>(paramInstance);
                    value            = paramIntOptional->Value();
                    break;
                case ParamType::ENUM:
                    paramEnumOptional = static_cast<SynthParam_Enum*>(paramInstance);
                    value             = paramEnumOptional->Value();
                    break;
            }

            switch (parameter) {
                case Parameters::UnisonVoices: {
                    auto unisonVoicesCurrent = this->unisonVoiceCount;
                    auto unisonVoicesTarget  = paramIntOptional->Value();
                    if (unisonVoicesCurrent != unisonVoicesTarget) {
                        this->maxUnisonVoice = math::max(maxUnisonVoice, math::max(unisonVoicesTarget, unisonVoicesCurrent));
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
                        this->polyVoiceCount = polyVoicesTarget;
                        for (int i = polyVoicesTarget; i < polyVoicesCurrent; ++i) {
                            voices[i].Release();
                        }
                        // for (int i = polyVoicesCurrent; i < polyVoicesTarget; ++i) {
                        //     voices[i].setUnisonVoiceCount(this->unisonVoiceCount);
                        // }
                    }
                    break;
                }
                case Parameters::Osc1Wave:
                    osc1Wave.Switch(value);
                    break;
                // case Parameters::Osc1Coarse:
                // case Parameters::Osc1Fine: {
                //     auto coarse = GetParamInt(Parameters::Osc1Coarse)->Value();
                //     auto fine   = GetParamFloat(Parameters::Osc1Fine)->Value();
                //     osc1Tune    = pitchFactor(coarse + fine);
                //     break;
                // }
                case Parameters::Osc1Split:
                    targetOsc1SplitMix = value != 0.0 ? 1.0 : 0.0;
                    osc1SplitFactorA   = pitchFactor(value);
                    osc1SplitFactorB   = 1.0;//pitchFactor(value);
                    break;
                case Parameters::Osc2Wave:
                    osc2Wave.Switch(value);
                    break;
                // case Parameters::Osc2Coarse:
                // case Parameters::Osc2Fine: {
                //     auto coarse = GetParamInt(Parameters::Osc2Coarse)->Value();
                //     auto fine   = GetParamFloat(Parameters::Osc2Fine)->Value();
                //     osc2Tune    = pitchFactor(coarse + fine);
                //     break;
                // }
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
                // case Parameters::FilterMode:
                //     filterMode.Switch(value);
                //     break;
                case Parameters::FilterCutoff:
                    targetFilterCutoff = value;
                    break;
                case Parameters::FilterResonance:
                    targetFilterResonance = value;
                    break;
                case Parameters::FilterKeyTracking:
                    targetFilterKeyTracking = value;
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
        }

        void getUiSnapshot(snapshot_t& snapshot);
        void setUiSnapshot(snapshot_t& snapshot);
    };

    void PluginVST2_Synth::initPrograms() {
        for (SynthProgram& program : staticPrograms) {
            program                   = {};
            program.VoiceMode         = 0.000000;
            program.GlideLength       = 0.000000;
            program.FilterMode        = 1.000000;
            program.FilterCutoff      = 0.100000;
            program.FilterResonance   = 0.020000;
            program.FilterKeyTracking = 0.500000;
            program.VolEnvCutoff      = 0.750000;
            program.ModEnvCutoff      = 0.740000;
            program.OscMix            = 0.500000;
            program.Osc1Wave          = 0.400000;
            program.Osc1Coarse        = 0.5;
            program.Osc1Fine          = 0.500000;
            program.Osc1Split         = 0.554000;
            program.Osc2Wave          = 0.400000;
            program.Osc2Coarse        = 0.745;
            program.Osc2Fine          = 0.505000;
            program.Osc2Split         = 0.560000;
            program.LfoAmount         = 0.500000;
            program.LfoFrequency      = 0.5;
            program.LfoDelay          = 0.000000;
            program.LfoCutoff         = 0.500000;
            program.FmMode            = 0.000000;
            program.FmCoarse          = 0.000000;
            program.FmFine            = 0.500000;
            program.VolEnvFm          = 0.500000;
            program.ModEnvFm          = 0.500000;
            program.LfoFm             = 0.500000;
            program.VolEnvA           = 0.075000;
            program.VolEnvD           = 0.500000;
            program.VolEnvS           = 1.000000;
            program.VolEnvR           = 0.650000;
            program.VolEnvV           = 0.800000;
            program.ModEnvA           = 0.220000;
            program.ModEnvD           = 0.500000;
            program.ModEnvS           = 0.500000;
            program.ModEnvR           = 0.700000;
            program.ModEnvV           = 0.600000;
            program.UnisonVoices      = impl->getParam(Parameters::UnisonVoices)->getAsDouble();
            program.PolyVoicesMax     = impl->getParam(Parameters::Voices)->getAsDouble();
        }
        {
            auto& prog        = staticPrograms[0];
            prog.FilterCutoff = 1.0f;
            prog.FilterMode   = 1.0;// TODO: add butto to write out presets in source code format so we can add them here
        }
        {
            auto& prog             = staticPrograms[1];
            prog.VoiceMode         = 0.000000;
            prog.GlideLength       = 0.000000;
            prog.FilterMode        = 0.666667;
            prog.FilterCutoff      = 0.135000;
            prog.FilterResonance   = 0.050000;
            prog.FilterKeyTracking = 0.875000;
            prog.VolEnvCutoff      = 0.590000;
            prog.ModEnvCutoff      = 0.655000;
            prog.OscMix            = 1.000000;
            prog.Osc1Wave          = 0.400000;
            prog.Osc1Coarse        = 0.500000;
            prog.Osc1Fine          = 0.500000;
            prog.Osc1Split         = 0.554000;
            prog.Osc2Wave          = 0.000000;
            prog.Osc2Coarse        = 0.500000;
            prog.Osc2Fine          = 0.500000;
            prog.Osc2Split         = 0.715000;
            prog.LfoAmount         = 0.500000;
            prog.LfoFrequency      = 0.393939;
            prog.LfoDelay          = 0.000000;
            prog.LfoCutoff         = 0.500000;
            prog.FmMode            = 0.500000;
            prog.FmCoarse          = 0.000000;
            prog.FmFine            = 0.500000;
            prog.VolEnvFm          = 0.500000;
            prog.ModEnvFm          = 0.500000;
            prog.LfoFm             = 0.495000;
            prog.VolEnvA           = 0.075000;
            prog.VolEnvD           = 0.400000;
            prog.VolEnvS           = 0.545000;
            prog.VolEnvR           = 0.700000;
            prog.VolEnvV           = 0.770000;
            prog.ModEnvA           = 0.025000;
            prog.ModEnvD           = 0.375000;
            prog.ModEnvS           = 0.165000;
            prog.ModEnvR           = 0.735000;
            prog.ModEnvV           = 0.865000;
        }
    }
    PluginVST2_Synth::PluginVST2_Synth(audioMasterCallback audioMaster)
        : BasePluginVST2(audioMaster, PLUGIN_UID, kNumPrograms, Parameters::kNumParams, kNumInputs, kNumOutputs),
          impl(new SynthImpl(this)),
          vecParams(impl->vecParams) {
        isSynth(true);
        programsAreChunks(true);
        impl->init();
        initPrograms();
    }

    void PluginVST2_Synth::writeCurrentProgram() {
        for (auto param : this->vecParams) {
            switch (param->enumParam) {
                case Parameters::MasterVolume:
                case Parameters::kNumParams:
                    break;
                default:
                    log_printf("%s = %f\n", StringAsCStr(param->shortName), param->getAsDouble());
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
        return impl->getParam(enumParam);
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
            vst_strncpy(label, StringAsCStr(param->unit), PLUGIN_PARAM_STR_MAX_LEN);
        }
    }

    void PluginVST2_Synth::getParameterDisplay(VstInt32 index, char* text) {
        if (text && index >= 0 && index < CtrSize(vecParams)) {
            SynthParamBase* param = vecParams[index];
            String valDisplay     = param->getValueDisplay();
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
            const auto strName    = param->name;
            const auto strDisplay = param->getValueDisplay();
            table.colSizes.resize(2);
            table.colSizes[0] = table.strW->getStringWidth(strName);
            table.colSizes[1] = table.strW->getStringWidth(strDisplay);
            table.colSizes[1] = math::max(table.colSizes[1], table.strW->getStringWidth("12345"));
            table.tableWidth  = table.colSizes[0] + table.colSizes[1];
            table.rows.push_back({ { strName, strDisplay } });
            
            for (auto& mod : impl->modulations) {
                auto modIndex = &mod - &impl->modulations.front();
                for (auto& dest : mod.destinations) {
                    if (dest.parameter == param->enumParam) {
                        for (int j = 0; j < impl->polyVoiceCount; j++) {
                            if (!impl->voices[j].IsReleased()) {
                                for (int i = 0; i < impl->unisonVoiceCount; i++) {
                                    bool bIsBipolar = impl->IsBipolarModulation(mod);
                                    const auto strName = StringFormat("Mod %zd Voice %d, unison %d %s, bipolar: %d", modIndex, j, i, StringAsCStr(param->name), bIsBipolar);
                                    double& ref = impl->voices[j].getVoice(i).modValues[dest.parameter];
                                    table.colSizes[0] = math::max(table.colSizes[0], table.strW->getStringWidth(strName));
                                    table.rows.push_back({ { strName, Table::tbltyperef<double>{ref, "%.2f"} } });
                                }
                                break;
                            }
                        }
                    }
                }
            }
            switch (param->enumParam) {
                case Parameters::VolEnvA:
                case Parameters::VolEnvD:
                case Parameters::VolEnvR:
                case Parameters::ModEnvA:
                case Parameters::ModEnvD:
                case Parameters::ModEnvR:
                    table.rows.push_back({ { Table::tblstr{ "Env 1" }, StringFormat("%f", impl->getEnvDuration(param->getAsDouble())) } });
                    table.rows.push_back({ { Table::tblstr{ "Env 2" }, StringFormat("%f", impl->getEnvDuration2(param->getAsDouble())) } });
                    break;
                default:
                    break;
            }
        }
    }

    float PluginVST2_Synth::getParameter(VstInt32 index) {
        if (index >= 0 && index < CtrSize(vecParams)) {
            SynthParamBase* param = vecParams[index];
            return static_cast<double>(param->getAsDouble());
        }
        return 0.0f;
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
    VstInt32 PluginVST2_Synth::getChunk(void** data, bool isPreset) {
        if (isPreset) {
            *data = nullptr;
            return 0;
        }

        snapshot_t snapshot;
        if (impl->getSnapshot(snapshot)) {
            impl->getUiSnapshot(snapshot);
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
                if (issetprogram && curProgram >= 0 && curProgram < CtrSize(staticPrograms)) {
                    for (auto& param : vecParams) {
                        auto pParamDouble = staticPrograms[curProgram].getProgramParameter(param->enumParam);
                        if (pParamDouble) *pParamDouble = param->getAsDouble();
                    }
                }
                while (snapshotLoaded.uiLayout.size() > views.size()) {
                    log_lf(Log::L_DEBUG, "creating view %d\n", CtrSize(views));
                    createView();
                }
                impl->setUiSnapshot(snapshotLoaded);
                return 1;
            }
            return 0;
        }
        return 0;
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
            if (inputs)
                dsp_util::fillChannels(inputs, this->getAeffect()->numInputs, sampleFrames, 0.0f);
            dsp_util::fillChannels(outputs, this->getAeffect()->numOutputs, sampleFrames, 0.0f);
            this->impl->ProcessSynth(inputs, outputs, sampleFrames);
        }
    }


    SynthProgram::SynthProgram() : SynthProgramParameters() {
        setName("Init");
    }

}// namespace PluginSynth

namespace PluginSynth {
    float getLayoutHeight(guibase* gui) {
        return gui->theme->get(GuiConstant::CONST_ROW_HEIGHT) * 1.33f;
    }
    class guicontainer_modulation_slot_destination : public guictr_base {
        SynthImpl* const synth;
        const int32_t slotIndex;
        const int32_t destSlotIndex;
        guidropdown_generic<String> dropdown;
        guiknob knob;

    public:
        guicontainer_modulation_slot_destination(SynthImpl* _synth, int32_t _slotIndex, int32_t _destSlotIndex)
            : guictr_base(),
              synth(_synth),
              slotIndex(_slotIndex),
              destSlotIndex(_destSlotIndex),
              knob(guiknob::knobtype::KNOB_UNLABELED) {
            padding      = 1;
            sortChildren = true;
            setCanMouseHit(true);
            setLabel(StringFormat("Mod %d Dst %d", _slotIndex, _destSlotIndex));
            knob.setIsBipolar(true);
            auto vecOpts = std::vector<String>();
            vecOpts.emplace_back("None");
            for (auto param : parametersOrdered) {
                auto synthParam = synth->getParam(param);
                if (synthParam && !synthParam->shortName.empty()) {
                    vecOpts.push_back(synthParam->shortName);
                } else {
                    vecOpts.push_back(StringFormat("Param %u", param));
                }
            }
            dropdown.setZOrder(-1);
            dropdown.setOptions(vecOpts);
            dropdown.setLabel(StringFormat("Mod %d Dst %d", _slotIndex, _destSlotIndex));
            dropdown.setCallback([this](int idx, String& value) -> String {
                if (idx >= 0) {
                    {
                        ThreadLock lock = dawCtrl ? dawCtrl->lockPlayThread() : ThreadLock::MakeVoidLock();
                        auto idxOffset  = idx == 0 ? -1 : static_cast<int32_t>(parametersOrdered[idx - 1]);
                        synth->setModulationDestination(slotIndex, destSlotIndex, idxOffset, knob.getValue());
                    }
                    if (parent) {
                        parent->buttonClicked(this);
                    }
                    return value;
                }
                return StringFormat("%d", idx);
            });
            dropdown.setCurrentString("<unused>");
            knob.fnValueEditChanged = [this](float prev, float value) {
                {
                    ThreadLock lock = dawCtrl ? dawCtrl->lockPlayThread() : ThreadLock::MakeVoidLock();
                    synth->setModulationDestRange(slotIndex, destSlotIndex, knob.getValue());
                }
                if (parent) {
                    parent->buttonClicked(this);
                }
            };
            add(&dropdown);
            add(&knob);
        }
        void setFromSynth() {
            auto modulation = synth->getModulationIfExists(slotIndex);
            if (modulation && CtrSize(modulation->destinations) > destSlotIndex) {
                auto& dest = modulation->destinations[destSlotIndex];
                if (dest.parameter < 0) {
                    dropdown.setSelectedIndex(0);
                } else {
                    auto idx = std::find(std::begin(parametersOrdered), std::end(parametersOrdered), dest.parameter);
                    dbgassert(std::end(parametersOrdered) != idx);
                    dropdown.setSelectedIndex(1 + static_cast<int32_t>(idx - std::begin(parametersOrdered)));
                }
                knob.setValueInit(static_cast<float>(dest.range));
            } else {
                dropdown.setSelectedIndex(0);
            }
        }
        ~guicontainer_modulation_slot_destination() override {
            removeGuis();
        }
        void layout() override {
            auto cs       = getSizeContent();
            knob.size     = { cs.y, cs.y };
            knob.pos      = cs - knob.size;
            dropdown.pos  = { 0, 0 };
            dropdown.size = { knob.pos.x - padding, cs.y };
            for (guibase* gui : guis) {
                gui->layout();
            }
        }
        void determineSize(ivec2& prefSize) override {
            prefSize.y = math::roundfS32(getLayoutHeight(this));
        }
    };
    class gui_notify_error : public guictxtmenu_base {
    protected:
        bool hadMouseFocus = false;
        guibutton btnHide;
        int64_t tmDelay  = 0L;
        int64_t tmCreate = 0L;
        String strErrSrc;
        String strErrMsg;

    public:
        gui_notify_error(String errSource, String errMessage) : guictxtmenu_base() {
            setCanMouseHit(true);
            setBackgroundRendered(true);
            setBackgroundRenderedInset(false);
            add(&btnHide);
            padding           = 6;
            margin            = 0;
            canTakeInputFocus = true;
            strErrSrc         = std::move(errSource);
            strErrMsg         = std::move(errMessage);
            btnHide.setText("Hide");
        }
        ~gui_notify_error() override {
            removeGuis();
        }
        void setDelay(int64_t _tmDelay) {
            this->tmDelay  = _tmDelay;
            this->tmCreate = getTimeMillis();
        }
        void onTick(AppCtrl* appctrl) override {
            auto tmLeft = math::max<int64_t>(0, this->tmDelay - (getTimeMillis() - this->tmCreate));
            if (tmLeft <= 0) {
                closeContextMenu();
            }
        }
        void buttonClicked(guibase* button) override {
            closeContextMenu();
        }
        void determineSize(ivec2& prefSize) override {
        }
        bool isTransient() const override {
            return true;
        }
        void layout() override {
            auto cs      = getSizeContent();
            btnHide.size = ivec2(cs.x / 7, cs.y - padding * 2) - ivec2(padding);
            btnHide.pos  = ivec2(cs.x - btnHide.size.x - padding, cs.y - btnHide.size.y);
            btnHide.pos.y /= 2;
            for (auto* g : guis) {
                g->layout();
            }
            this->fontSize = cs.y * 0.45f;
        }
        void render(NVGcontext* vg) override {
            nvgSave(vg);
            guictxtmenu_base::render(vg);
            nvgRestore(vg);
            if (strErrSrc.length() > 0) {
                auto cs = getSizeContent();
                nvgSave(vg);
                setScissorTransform(vg);
                ivec2 renderSize(btnHide.pos.x - padding, cs.y);
                ivec2 renderPos(0);
                int fontScale = math::roundfS32((this->fontSize > 0 ? this->fontSize : math::min(renderSize.y, renderSize.x)));
                renderCenteredMultilineText(vg, theme, strErrSrc + "\n" + strErrMsg, fontScale, getLabelColor(), renderPos, renderSize);
                nvgRestore(vg);
            }
        }
        GuiColor::constant_t getLabelColor() const override {
            return GuiColor::COL_INVALID_INPUT;
        }
    };
    class guicontainer_modulation_slot_source : public guictr_base {
        SynthImpl* const synth;
        const int32_t slotIndex;
        const int32_t srcSlotIndex;
        double constant = 0.0;
        guidropdown_generic<String> dropdownOperator;
        guidropdown_generic<String> dropdownSource;
        gui_numberinput_double inputConstant;
        gui_textfield textfieldFunction;
        guibutton buttonInputRange;
        std::function<bool(String)> fnValidateFunction;
    public:
        guicontainer_modulation_slot_source(SynthImpl* _synth, int32_t _slotIndex, int32_t _srcSlotIndex)
            : guictr_base(),
              synth(_synth),
              slotIndex(_slotIndex),
              srcSlotIndex(_srcSlotIndex),
              inputConstant(&constant) {
            padding      = 1;
            sortChildren = true;
            setCanMouseHit(true);
            setLabel(StringFormat("Mod %d Input %d", _slotIndex, _srcSlotIndex));
            {
                auto vecOpts = std::vector<String>();
                for (size_t i = 0; i < ModulationOperator::NumModulationOperators; ++i) {
                    vecOpts.emplace_back(stringsModOp[i]);
                }
                // place it right after the source dropdown
                dropdownOperator.setOptions(vecOpts);
                // dropdownOperator.setLabel(StringFormat("Mod %d Op %d", slotIndex, srcSlotIndex));
                dropdownOperator.setCallback([this](int idx, String& value) -> String {
                    {
                        ThreadLock lock = dawCtrl ? dawCtrl->lockPlayThread() : ThreadLock::MakeVoidLock();
                        synth->setModulationOperator(slotIndex, srcSlotIndex, idx);
                    }
                    if (parent) {
                        parent->buttonClicked(this);
                    }
                    return dropdownOperator.optionToString(value);
                });
            }
            {
                auto vecOpts = std::vector<String>();
                for (const auto& opt : stringsModSource) {
                    vecOpts.emplace_back(opt);
                }
                dropdownSource.setZOrder(1);
                dropdownSource.setOptions(vecOpts);
                dropdownSource.setLabel(StringFormat("Mod %d Src %d", slotIndex, srcSlotIndex));
                dropdownSource.setCallback([this](int idx, String& value) -> String {
                    if (idx >= 0) {
                        {
                            ThreadLock lock = dawCtrl ? dawCtrl->lockPlayThread() : ThreadLock::MakeVoidLock();
                            synth->setModulationType(slotIndex, srcSlotIndex, idx - 1);
                        }
                        if (parent) {
                            parent->buttonClicked(this);
                        }
                        return dropdownSource.optionToString(value);
                    }
                    return "";
                });
            }
            {
                inputConstant.setLabel(StringFormat("Mod %d Constant %d", slotIndex, srcSlotIndex));
                inputConstant.fnValueEditChanged = [this](gui_numberinput_field_base*, double value) {
                    {
                        ThreadLock lock = dawCtrl ? dawCtrl->lockPlayThread() : ThreadLock::MakeVoidLock();
                        synth->setModulationConstant(slotIndex, srcSlotIndex, value);
                    }
                    if (parent) {
                        parent->buttonClicked(this);
                    }
                };
                inputConstant.fnClamp = [](double value) -> double {
                    return math::clamp(value, -10.0, 10.0);
                };
            }
            {
                buttonInputRange.setLabel("Bipolar");
            }
            {
                textfieldFunction.setLabel(StringFormat("Mod %d Function %d", slotIndex, srcSlotIndex));
                // textfieldFunction.setTextfieldColor(GuiColor::COL_TEXTBOX_TEXT);
                textfieldFunction.setInputActivates(true);
                textfieldFunction.setReturnCommits(true);
                fnValidateFunction = ([this](const String& value) {
                    {
                        try {
                            MathExpr expr = MathExpr::parse(value);
                            {
                                ThreadLock lock = dawCtrl ? dawCtrl->lockPlayThread() : ThreadLock::MakeVoidLock();
                                synth->setModulationFunction(slotIndex, srcSlotIndex, std::move(expr));
                            }
                            textfieldFunction.setLabel(StringFormat("Mod %d Function %d", slotIndex, srcSlotIndex));
                            textfieldFunction.setTextfieldColor(GuiColor::COL_TEXTBOX_TEXT);
                        } catch (mu::Parser::exception_type& e) {
                            log_lf(Log::L_ERROR, "Error in expression: %s\n", e.GetMsg().c_str());
                            textfieldFunction.setLabel(StringFormat("Error in expression: %s", e.GetMsg().c_str()));
                            textfieldFunction.setTextfieldColor(GuiColor::COL_INVALID_INPUT);
                            // auto tooltip       = new gui_notify_error("Failed parsing expression", e.GetMsg());
                            // auto ctrlSize      = dawCtrl->m_size;
                            // tooltip->size      = ivec2(620, 80);
                            // tooltip->maxHeight = tooltip->size.y;
                            // tooltip->layout();
                            // tooltip->setDelay(10000);
                            // dawCtrl->openOverlayGui(
                            //     tooltip,
                            //     ivec2(ctrlSize.x / 2, ctrlSize.y - 100) - tooltip->size / 2,
                            //     BASECTRL_WND_POS_RELATIVE | BASECTRL_WND_IS_TOOLTIP);
                            {
                                ThreadLock lock = dawCtrl ? dawCtrl->lockPlayThread() : ThreadLock::MakeVoidLock();
                                MathExpr expr;
                                expr.str        = value;
                                expr.parsedExpr = nullptr;
                                {
                                    ThreadLock lock = dawCtrl ? dawCtrl->lockPlayThread() : ThreadLock::MakeVoidLock();
                                    synth->setModulationFunction(slotIndex, srcSlotIndex, std::move(expr));
                                }
                            }
                        }
                    }
                    if (parent) {
                        parent->buttonClicked(this);
                    }
                    return true;
                });
                textfieldFunction.setChangeCallback(fnValidateFunction);
                textfieldFunction.setEndEditCallback(fnValidateFunction);
            }
            add(&dropdownOperator);
            add(&dropdownSource);
            add(&inputConstant);
            add(&textfieldFunction);
            add(&buttonInputRange);
        }
        ~guicontainer_modulation_slot_source() override {
            removeGuis();
        }
        void setFromSynth() {
            auto modulation = synth->getModulationIfExists(slotIndex);
            dropdownOperator.setVisible(srcSlotIndex > 0 && (modulation && CtrSize(modulation->inputs) > srcSlotIndex));
            if (modulation && CtrSize(modulation->inputs) > srcSlotIndex) {
                auto& src = modulation->inputs[srcSlotIndex];
                switch (src.type) {
                    case ModulationType::Function:
                    case ModulationType::Constant:
                        dropdownSource.setSelectedIndex(static_cast<int32_t>(src.type)+1);
                        break;
                    case ModulationType::ModulationSource:
                        dropdownSource.setSelectedIndex(static_cast<int32_t>(src.src)+3);
                        break;
                    default:
                        dropdownSource.setSelectedIndex(0);
                        break;
                }
                dropdownOperator.setSelectedIndex(static_cast<int32_t>(src.op));
                textfieldFunction.setVisible(textfieldFunction.isEditing() || src.type == ModulationType::Function);
                if (!textfieldFunction.isEditing()) {
                    textfieldFunction.setValue(src.function.str);
                }
                if (!src.function.str.empty() && !src.function.parsedExpr) {
                    textfieldFunction.setLabel("Error in expression");
                    textfieldFunction.setTextfieldColor(GuiColor::COL_INVALID_INPUT);
                } else if (src.function.parsedExpr && src.function.parsedExpr->nanInfCounter) {
                    textfieldFunction.setLabel(StringFormat("%d NaN/Inf detected", src.function.parsedExpr->nanInfCounter));
                    textfieldFunction.setTextfieldColor(GuiColor::COL_INVALID_INPUT);
                }
                dropdownOperator.setVisible(dropdownOperator.isVisible() && (src.type != ModulationType::Function));
                inputConstant.setVisible(src.type == ModulationType::Constant);
                buttonInputRange.setVisible(src.type != ModulationType::Constant && src.type != ModulationType::Function);
                switch (src.range) {
                    case ModulationRange::Bipolar:
                        buttonInputRange.setText("+/-");
                        buttonInputRange.setLabel("Bipolar [-1.0 - 1.0]");
                        break;
                    case ModulationRange::Unipolar:
                        buttonInputRange.setText("+");
                        buttonInputRange.setLabel("Unipolar [0.0 - 1.0]");
                        break;
                    case ModulationRange::Triangle:
                        buttonInputRange.setText("\\/");
                        buttonInputRange.setLabel("Triangle [0.0 - 1.0]");
                        break;
                    default:
                        break;
                }
                constant = src.value;
            } else {
                dropdownSource.setSelectedIndex(0);
                dropdownOperator.setSelectedIndex(0);
                textfieldFunction.setVisible(false);
                inputConstant.setVisible(false);
                buttonInputRange.setVisible(false);
                buttonInputRange.setText("+");
                buttonInputRange.setLabel("Unipolar");
                constant = 1.0;
            }
        }
        void layout() override {
            auto cs = getSizeContent();
            // dbgassert(cs.x > 0);
            auto sizeRightOperator = cs.x;
            dropdownSource.pos     = { 0, 0 };
            if (dropdownOperator.isVisible()) {
                auto partialSize      = cs.x * 1 / 4;
                dropdownOperator.pos  = {};
                dropdownOperator.size = { partialSize - padding, cs.y };
                sizeRightOperator     = cs.x - partialSize;
                dropdownSource.pos.x  = dropdownOperator.right() + padding;
            }
            auto widthButtonBipolar = math::roundfS32(size.y);
            if (buttonInputRange.isVisible()) {
                sizeRightOperator -= widthButtonBipolar;
                buttonInputRange.size = { widthButtonBipolar, cs.y };
                buttonInputRange.pos  = { cs.x - widthButtonBipolar, 0 };
            }
            dropdownSource.size = { sizeRightOperator - padding, cs.y };
            if (inputConstant.isVisible()) {
                auto partialSize2     = (sizeRightOperator) *3 / 10;
                dropdownSource.size.x = dropdownSource.size.x - partialSize2 - padding;
                inputConstant.size    = { partialSize2, cs.y };
                inputConstant.pos     = { dropdownSource.right() + padding, 0 };
            }
            if (textfieldFunction.isVisible()) {
                auto partialSize2      = (sizeRightOperator) *7 / 10;
                dropdownSource.size.x  = dropdownSource.size.x - partialSize2 - padding;
                textfieldFunction.size = { partialSize2, cs.y };
                textfieldFunction.pos  = { dropdownSource.right() + padding, 0 };
                // textfieldFunction.setFontSize(textfieldFunction.getSize.y);
            }
            for (guibase* gui : guis) {
                gui->layout();
                // dbgassert(!gui->isVisible() || (gui->size.x > 0 && gui->size.y > 0));
            }
        }
        void buttonClicked(guibase* button) override {
            if (button == &buttonInputRange) {
                {

                    ThreadLock lock = dawCtrl ? dawCtrl->lockPlayThread() : ThreadLock::MakeVoidLock();
                    auto modulation = synth->getModulationIfExists(slotIndex);
                    if (modulation && CtrSize(modulation->inputs) > srcSlotIndex) {
                        auto& input = modulation->inputs[srcSlotIndex];
                        synth->setModulationInputRange(slotIndex, srcSlotIndex, static_cast<ModulationRange>((static_cast<int32_t>(input.range) + 1) % ModulationRange::NumModulationRanges));
                    }
                }
                if (parent) {
                    parent->buttonClicked(this);
                }
            }
            parent->buttonClicked(button);
            guictr_base::buttonClicked(button);
        }
        void determineSize(ivec2& prefSize) override {
            prefSize.y = math::roundfS32(getLayoutHeight(this));
        }
    };
    class guicontainer_modulation_slot : public guictr_base {
        SynthImpl* const synth;
        const int32_t slotIndex;
        std::vector<guicontainer_modulation_slot_source*> sources;
        std::vector<guicontainer_modulation_slot_destination*> destinations;

    public:
        explicit guicontainer_modulation_slot(SynthImpl* synth, int32_t slotIndex)
            : guictr_base(),
              synth(synth),
              slotIndex(slotIndex) {
            padding      = 2;
            sortChildren = true;
            setCanMouseHit(true);
            setLabel(StringFormat("Modulation %d", slotIndex + 1));
        }
        ~guicontainer_modulation_slot() override {
            removeGuis();
            for (auto& d : sources) {
                delete d;
            }
            for (auto& d : destinations) {
                delete d;
            }
        }
        void setFromSynth() {
            auto modulation     = synth->getModulationIfExists(slotIndex);
            auto modSourceCount = modulation ? modulation->inputs.size() : 0;
            auto modDestCount   = modulation ? modulation->destinations.size() : 0;
            while (sources.size() > modSourceCount + 1) {
                remove(sources.back());
                delete sources.back();
                sources.pop_back();
            }
            while (sources.size() < modSourceCount + 1) {
                const auto srcIdx = CtrSize(sources);
                auto dropdown     = new guicontainer_modulation_slot_source(synth, slotIndex, srcIdx);
                dropdown->setZOrder(-srcIdx * 10);
                sources.push_back(dropdown);
                add(sources.back());
            }
            while (destinations.size() > 1 && destinations.size() > modDestCount + 1) {
                remove(destinations.back());
                delete destinations.back();
                destinations.pop_back();
            }
            while (destinations.size() < modDestCount + 1) {
                const auto dstIdx = CtrSize(destinations);
                auto dstSlot      = new guicontainer_modulation_slot_destination(synth, slotIndex, dstIdx);
                // place it after source and operator dropdowns
                dstSlot->setZOrder(-(1000 + dstIdx * 10 + 2));
                destinations.push_back(dstSlot);
                add(destinations.back());
            }
            for (auto& src : sources) {
                src->setFromSynth();
            }
            for (auto& dst : destinations) {
                dst->setFromSynth();
            }
        }
        void render(NVGcontext* vg) override {
            this->renderDebug(vg, dbgcolorsArray[1 + (slotIndex % (dbgcolorsArraySize - 1))]);
            guictr_base::render(vg);
        }
        void buttonClicked(guibase* button) override {
            parent->buttonClicked(button);
            guictr_base::buttonClicked(button);
        }
        void determineSize(ivec2& prefSize) override {
            ivec2 sizeTotal = {};
            for (auto& src : guis) {
                src->size = prefSize;
                src->determineSize(src->size);
                sizeTotal.y += src->size.y;
            }
            prefSize.y = sizeTotal.y + padding * 2;
        }
        void layout() override {
            auto cs   = getSizeContent();
            ivec2 pos = {};
            auto rowHeight = cs.y / CtrSize(guis);
            for (auto& slot : guis) {
                slot->pos    = pos;
                slot->size.x = cs.x;
                slot->size.y = rowHeight;
                slot->layout();
                pos.y = slot->bottom();
            }
        }
    };
    class guicontainer_modulation : public guictr_base {
        SynthImpl* const synth;
        std::vector<guicontainer_modulation_slot*> slots;
        bool bGuiNeedsRefresh = true;
    public:
        explicit guicontainer_modulation(SynthImpl* synth)
            : guictr_base(),
              synth(synth) {
            padding = 2;
            setLabel("Modulation");
            setCanMouseHit(true);
        }
        ~guicontainer_modulation() override {
            removeGuis();
            for (auto& slot : slots) {
                delete slot;
            }
        }
        void layout() override {
            auto cs   = getSizeContent();
            ivec2 pos = {};
            for (auto& slot : slots) {
                slot->size = cs;
                slot->determineSize(slot->size);
                slot->pos = pos;
                slot->layout();
                pos.y = slot->bottom();
            }
            for (guibase* gui : guis) {
                if (!stl_contains(slots, gui)) {
                    gui->layout();
                }
            }
        }
        void setFromSynth() {
            auto modulations = synth->getModulationCount();
            while (CtrSize(slots) <= modulations) {
                slots.push_back(new guicontainer_modulation_slot(synth, CtrSize(slots)));
                add(slots.back());
            }
            for (auto& slot : slots) {
                slot->setFromSynth();
            }
            if (parent && size.x > 0 && size.y > 0) {
                layout();
            }
        }
        void buttonClicked(guibase* button) override {
            onChildLayoutChanged(this);
            bGuiNeedsRefresh = true;
            guictr_base::buttonClicked(button);
        }
        void onTick(AppCtrl* ctrl) override {
            if (bGuiNeedsRefresh) {
                bGuiNeedsRefresh = false;
                setFromSynth();
            }
            guictr_base::onTick(ctrl);
        }
        void determineSize(ivec2& prefSize) override {
            ivec2 sizeTotal = {};
            for (auto& src : guis) {
                src->size = prefSize;
                src->determineSize(src->size);
                sizeTotal.y += src->size.y;
            }
            prefSize.y = sizeTotal.y + padding * 2;
        }
    };

    class guiknob_synthparam : public guiknob_pluginparam {
        SynthImpl* const synth;
        const Parameters param;

    public:
        explicit guiknob_synthparam(SynthImpl* _impl, Parameters _param, guiknob::knobtype _knobtype = guiknob::knobtype::KNOB_LABELED)
            : guiknob_pluginparam(PARAM_OFFSET_EXTERNAL + static_cast<int32_t>(_param), static_cast<int32_t>(_param), _knobtype),
              synth(_impl),
              param(_param) {
        }
        std::optional<std::vector<param_modulation_range_t>> getKnobModulationRanges() override {
            auto synthParam = synth->getParam(param);
            if (synthParam) {
                return synth->getParamModulationRanges(param);
            }
            return std::nullopt;
        }
    };
    class gui_listsynthsettings : public gui_list_entry {
        SynthImpl* const synth;
        const Settings setting;
        const String name;

    public:
        explicit gui_listsynthsettings(SynthImpl* _synth, Settings _setting, String _settingName)
            : gui_list_entry(), synth(_synth), setting(_setting), name(_settingName) {
            icon = -1;
        }

        String getText() override { return name; }
        void dragMoveOn(guibase* target, ivec2 mousepos) override {}
        void dragReleaseOn(guibase* target, ivec2 mousepos) override {}
        void handleDraggedBegin(MouseEvent& evt) override { toggle(); }
        bool enabled() {
            return synth->getSetting(setting);
        }
        bool toggle() {
            bool bEnbl = enabled();
            bEnbl = !bEnbl;
            synth->setSetting(setting, bEnbl);
            if (parent && parent->parent) {
                parent->parent->buttonClicked(this);
            }
            return false;
        }

        void render(NVGcontext* vg) override {

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
            ivec2 posIcon  = {inner.x - (int)spacing - sizeIcon.y, (inner.y - sizeIcon.y) / 2};
            bool enbl = enabled();

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
    class guicontainer_plugin_synth : public guictr_base, public splitter_cb {
        struct _synth_gui_param_knob {
            guiknob_pluginparam* knob;
            Parameters param;
        };
        PluginVST2_Synth* const plugin;
        effectbase* const module;
        gui_textfield editfield;
        std::vector<_synth_gui_param_knob> knobs;
        guictr_scrollbar scrollContainerModulation;
        guicontainer_modulation modulation;
        gui_list list;
        gui_list list2;
        Splitter splitter;
        bool bGuiNeedsRefresh = true;

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
    public:
        explicit guicontainer_plugin_synth(PluginVST2_Synth* plugin)
            : guictr_base(),
              plugin(plugin),
              module(plugin->getHostSideHandle()),
              scrollContainerModulation(),
              modulation(plugin->getSynth()),
              splitter(1, 0.8f)
        {
            list.padding = 2;
            list2.padding = 2;
            splitter.setMinMax(0.3f, 0.85f);
            splitter.setCallback(this);
            setBackgroundRendered(true);
            editfield.setFlag(FLG_NO_LAYOUT, true);
            editfield.setVisible(false);
            editfield.setAlignment(gui_textfield::Alignment::Center);
            editfield.setReturnCommits(true);
            padding = 4;
            // margin  = 4;
            knobs.reserve(Parameters::kNumParams);
            for (auto param : parametersOrdered) {
                auto knob = new guiknob_synthparam(plugin->getSynth(),
                                                   param,
                                                   guiknob::knobtype::SLIDER_LABELED);
                knobs.push_back(_synth_gui_param_knob{ knob, param });
                add(knobs.back().knob);
            }
            add(&list);
            add(&list2);
            add(&scrollContainerModulation);
            scrollContainerModulation.add(&modulation);
            scrollContainerModulation.maxHeight = -1;
            // add(&modulation);
            add(&editfield);
            add(&splitter);
            // initialize settings list
            {
                std::vector<gui_list_entry*> _newListIn;
                for (auto setting : settingsOrdered) {
                    _newListIn.push_back(new gui_listsynthsettings{plugin->getSynth(), setting, stringsSettings[setting]});
                }
                int idx = 0;
                for (auto* p : _newListIn) {
                    p->id = 0x1f | (idx++ << 8);
                }
                list2.setList(_newListIn);
                // list2.layout();
            }
        }
        ~guicontainer_plugin_synth() override {
            removeGuis();
            for (auto& synthKnob : knobs) {
                delete synthKnob.knob;
            }
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
                    if (!gui->isVisible())
                        continue;
                    if (gui->mouseHitTest(localMouse, evt)) {
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
                auto* param = plugin->getSynth()->getParam(synthKnob.param);
                if (param) {
                    synthKnob.knob->setLabel(param->getHierarchicalName());
                }
#endif
#if BUILD_EXTERNAL_PLUGIN
                synthKnob.knob->setAudioEffect(plugin);
#endif
            }
            bGuiNeedsRefresh = true;
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
            if (bGuiNeedsRefresh) {
                modulation.setFromSynth();
                layout();
                bGuiNeedsRefresh = false;
            }
            PluginVST2_Synth* thisImpl = this->plugin;
            auto synthImpl             = plugin->getSynth();
            std::vector<int> heldNotes = synthImpl->getHeldNotes();//TODO: not threadsafe
            std::vector<String> strings;
            strings.reserve(8);
            strings.push_back(StringFormat("Drift Val %f", synthImpl->driftValue));
            strings.push_back(StringFormat("Drift Freq %f", (synthImpl->driftVelocity*synthImpl->getSamplerate())));
            strings.push_back(StringFormat("Voices %d", synthImpl->getActiveVoiceCount()));
            strings.push_back(StringFormat("maxUnisonVoice %d", synthImpl->maxUnisonVoice));
            strings.push_back(StringFormat("unisonVoiceCount %d", synthImpl->unisonVoiceCount));
            strings.push_back(StringFormat("polyVoiceCount %d", synthImpl->polyVoiceCount));
            String s = "Held notes: ";
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
                if (gui->isVisible()) {
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
        void layout() override {
            auto cs                = getSizeContent();
            const auto controlsWidth = splitter.leftOrTop(cs.x);
            const auto modulationWidth = splitter.rightOrBottom(cs.x);
            splitter.pos  = ivec2(controlsWidth - Splitter::SPLITTER_LAYOUT_THICKNESS/2, 0);
            splitter.size = ivec2(Splitter::SPLITTER_LAYOUT_THICKNESS, cs.y);
            scrollContainerModulation.size        = ivec2(modulationWidth, cs.y) - ivec2(INSET_CTR_SPACING * 2);
            scrollContainerModulation.pos         = ivec2(cs.x - modulationWidth, 0) + ivec2(INSET_CTR_SPACING);
            scrollContainerModulation.maxHeight = cs.y;
            scrollContainerModulation.determineSize(scrollContainerModulation.size);
            cs                     = ivec2(controlsWidth, cs.y);
            const auto numRows = 3;
            const auto numKnobs = CtrSize(knobs);
            const auto innerSize = vec2(cs.x, cs.y*3/4) - vec2(padding*2);
            const auto sizeListY = cs.y * 1 / 4;
            const auto numKnobsPerRow = math::ceilfS32(numKnobs / float(numRows));
            auto innerRowHeight = float(innerSize.y - (numRows - 1) * (INSET_CTR_SPACING)) / numRows;
            auto innerColWidth = float(innerSize.x - (numKnobsPerRow - 1) * (INSET_CTR_SPACING)) / numKnobsPerRow;
            auto knobSizeF = vec2(innerColWidth, innerRowHeight);
            auto knobPos   = ivec2(padding);
            auto knobSize =  ivec2(math::roundfS32(knobSizeF.x), math::roundfS32(knobSizeF.y));
            int32_t knobIdx = 0;
            for (auto& synthKnob : knobs) {
                synthKnob.knob->pos  = knobPos;
                synthKnob.knob->size = knobSize;
                synthKnob.knob->setLabelsFontScale(0.7f, 0.8f);
                knobPos.x += knobSize.x + INSET_CTR_SPACING;
                if (++knobIdx >= numKnobsPerRow) {
                    knobIdx = 0;
                    knobPos.x = padding;
                    knobPos.y += knobSize.y + INSET_CTR_SPACING;
                }
            }
            if (knobPos.x + knobSize.x > cs.x - padding) {
                knobPos.x = padding;
                knobPos.y += knobSize.y + INSET_CTR_SPACING;
            }
            list.pos  = vec2(padding, cs.y-sizeListY + padding);
            list.size = cs - list.pos - ivec2(padding*3, padding);
            list.size.x /= 2;
            list2.pos  = vec2(list.right() + padding*2, list.pos.y);
            list2.size = list.size;
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
                auto paramIdx          = param->getParamIdx();
                auto paramValue        = module->getParamValueDisplay(paramIdx);
                editfield.mCallbackEnd = [this, param, paramValue, paramIdx](const std::string& str) {
                    auto paramConverted = module->convertParamValueDisplay(param->getParamIdx(), param_unit_t{ str, paramValue.unit });
                    if (paramConverted.success) {
                        module->setParamValue(paramIdx, paramConverted.floatVal, FLG_PAR_UPDATE_USER);
                        if (param->fnValueEditChanged)
                            param->fnValueEditChanged(param->getValue(), paramConverted.floatVal);
                    }
                    editfield.setVisible(false);
                    return true;
                };
                auto layout    = param->getLayout();
                editfield.pos  = layout.pValue;
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

        void setUiLayout(const ui_layout_t& layout) {
            splitter.setScale(layout.splitPos);
        }

        bool getUiLayout(ui_layout_t& layout) const {
            layout = ui_layout_t{splitter.getScale()};
            return true;
        }
        void onChildLayoutChanged(guibase* g) override {
            bGuiNeedsRefresh = true;
            if (this->parent) {
                this->parent->onChildLayoutChanged(this);
            }
        }
    };

    const char* getName() {
        return PLUGIN_EFFECT_NAME;
    }
    AudioEffectX* createPlugin(audioMasterCallback audioMaster) {
        return new PluginVST2_Synth(audioMaster);
    }
    using ViewCtrType = SinglePluginViewContainers<guicontainer_plugin_synth, PluginVST2_Synth>;
    std::shared_ptr<PluginViewContainers> PluginVST2_Synth::createView() {
        auto view = std::make_shared<ViewCtrType>(this, 1280, 720);
        this->views.push_back(view);
        this->impl->views.push_back(view);
        return view;
    }

    void SynthImpl::getUiSnapshot(snapshot_t& snapshot) {
        auto numViews = CtrSize(views);
        snapshot.uiLayout.resize(numViews);
        for (int32_t i = 0; i < numViews; ++i) {
            auto implCtrType = dynamic_cast<ViewCtrType*>(this->views[i].get());
            if (!implCtrType || !implCtrType->getPluginUI().getUiLayout(snapshot.uiLayout[i])) {
                snapshot.uiLayout[i] = {};
            }
        }
    }
    void SynthImpl::setUiSnapshot(snapshot_t& snapshot) {
        auto numViews = CtrSize(snapshot.uiLayout);
        for (int32_t i = 0; i < numViews; ++i) {
            if (i >= CtrSize(views)) {
                continue;
            }
            auto implCtrType = dynamic_cast<ViewCtrType*>(this->views[i].get());
            if (!implCtrType) {
                continue;
            }
            implCtrType->getPluginUI().setUiLayout(snapshot.uiLayout[i]);
        }
    }
}// namespace PluginSynth

#include <cmath>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <limits>
#include <muParser.h>
#include <nanovg.h>
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
#include "fileio.h"
#include "gui/contextmenu/contextmenu_base.h"
#include "gui/controls/list.h"
#include "gui/controls/textfield.h"
#include "gui/dropdown/dropdown_generic.h"
#include "guicolors.h"
#include "guiconstant.h"
#include "guiglobals.h"
#include "logging.h"
#include "math/seq_math.h"
#include "math/simd_math.h"
#include "plugins/synth/synth-plugin.h"
#include "projectfile-snapshot.h"
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
    int32_t gDebugOverrides               = -1;
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
                    previous  = current;
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
        double phase          = 0.0;
        double phaseIncrement = 0.0;
        double phaseFade      = 0.0;
    private:
        double triCurrent = 0.0;
        double triLast    = 0.0;
        double noiseValue = 19.1919191919191919191919191919191919191919;
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

        double GetLfoWaveform(Waveforms waveform, bool halfBleb, double phase) {
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
            dbgassert(phase >= -1.0);
            if (oneShot && phase > 1.0) {
                phase = 1.0;
            } else {
                while (phase > 1.0)
                    phase -= 1.0;
            }
            double p = phase;
            while (p < 0.0) p += 1.0;
            dbgassert(p >= 0.0 && p <= 1.0);
            double dSwitchVal = waveform.getSwitchValue();
            dbgassert(!fp_math::isNanOrInfd(dSwitchVal));
            auto roundedVal = math::rounddU32(dSwitchVal);
            dbgassert(roundedVal < static_cast<uint32_t>(Waveforms::NumWaveforms));
            double v = GetLfoWaveform(static_cast<Waveforms>(roundedVal), phase > 0.5 && oneShot, p);
            dbgassert(v >= -1.0 && v <= 1.0);
            return v;
        }
        void initPhase(double _phase, double _phaseFade) {
            while (_phase < -1.0) _phase += 1.0;
            phase = _phase;
            dbgassert(phase >= -1.0 && phase <= 1.0);
            if (0.0 == _phase) {
                triCurrent = triLast = 0.0;
            }
            dbgassert(phase >= -1.0 && phase <= 1.0);
            phaseFade = _phaseFade;
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
        // static constexpr auto base    = 1.0594630943592953;  // = pow(2.0, 1.0 / 12.0);
        // return pow(base, p);
        static constexpr auto logBase = 0.057762265046662153;//log(base);
        return exp(logBase * p);
    }
    inline double pitchToFrequency(double p) { return 440.0 * pitchFactor(p - 69); }
    struct Voice {
        std::array<double, Parameters::kNumParams> modValues{};
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
            if (bRandomPhase) {
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

        void Start(bool resetOscs, bool resetEnvelopes) {
            bIsActive = true;
            if (resetOscs) {
                ResetPhases(true);
            }
            if (resetEnvelopes) {
                ResetEnvelopes();
            }
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
        int note          = 0;
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
            last = &voices.back();
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
            // seqNr++;
            numUnisonActive = static_cast<int32_t>(last - first);
        }
        void UpdateVoiceDrift(double dt, const HostTempo& tempo) {
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
    const std::array<const char*, 13> stringsSettings = {
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
                                  static_cast<size_t>(ModulationType::NumModulationTypes) + 1 - 1,
                  "stringsModSource size mismatch");
    static constexpr auto MathExprInputLen                               = 1 + ModulationSourceType::NumModulationSources;
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
        ModulationType type      = ModulationType::ModulationSource;
        ModulationSourceType src = ModulationSourceType::Lfo1;
        ModulationOperator op    = ModulationOperator::Multiply;
        double value             = 1.0;
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

    class PresetManager {
    public:
        struct Preset {
            String name;
            String path;
            bool isFavorite = false;
        };

    private:
        String presetPath;
        std::vector<Preset> presets;
        std::vector<Preset> favorites;

    public:
        const String& getPresetPath() const {
            return presetPath;
        }
        void load(const String& path) {
            presetPath = path;
            presets.clear();
            favorites.clear();

            std::vector<FileFound> files;
            findFilesWithExt(path, "preset", true, files);
            for (const auto& file : files) {
                Preset preset;
                preset.name = file.name;
                preset.path = file.path;
                presets.push_back(preset);
            }
        }
        void reload() {
            load(presetPath);
        }
        const std::vector<Preset>& getPresets() const {
            return presets;
        }
        const std::vector<Preset>& getFavorites() const {
            return favorites;
        }
    };
    using ModulationSourceData = std::array<double, MathExprInputLen>;

    class SynthImpl : public SynthState {
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
        std::vector<SynthParamBase*> vecParams;
        std::vector<Modulation> modulations;
        std::array<double, Parameters::kNumParams> modulationValuesMin;
        std::array<double, Parameters::kNumParams> modulationValuesMax;
        std::array<VoiceUnison, NUM_POLY_VOICES> voices;
        std::array<float, Settings::NumSettings> settings{};
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
        int32_t seq = 0;
        HostTempo tempo{};
        ModulationSourceData modSrcData{};
        VoiceList prevVoiceList{};

    public:
        int32_t activeVoiceCount  = 0;
        int32_t unisonVoiceCount  = 0;
        int32_t maxUnisonVoice    = 0;
        int32_t polyVoiceCount    = 0;
        int32_t minPolyVoiceIndex = 0;
        int32_t maxPolyVoiceIndex = 0;

    private:
        PluginVST2_Synth* const instanceVst2;
        bool bIsInitSamplerate = false;
        PresetManager::Preset currentPreset;
        String exprError;
        PresetManager presetManager;
        void initImpl() {
            for (auto& setting : settings) {
                setting = 1.0;
            }
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
                    switch (p->type) {
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
            setParamName(getParam(Parameters::FilterCutoff), "Filter Cutoff", "Flt Cut", "Cutoff", "Hz", "%.0f");
            addFloatParam(Parameters::FilterResonance)->setRange(0.0, 1.0)->setRangedValue(0.0);
            setParamName(getParam(Parameters::FilterResonance), "Filter Resonance", "Flt Res", "Resonance");
            addFloatParam(Parameters::FilterDrive)->setRange(-1.0, 1.0)->setRangedValue(0.0);
            setParamName(getParam(Parameters::FilterDrive), "Filter Drive", "Flt Drv", "Drive", "dB", "%0.2f");
            addFloatParam(Parameters::FilterKeyTracking)->setRange(-24.0, 24.0)->setRangedValue(0.0);
            setParamName(getParam(Parameters::FilterKeyTracking), "Filter Keytracking", "Flt Trk", "Keytrack");
            for (Parameters p = Parameters::Macro01;
                 p <= Parameters::Macro08;
                 p = static_cast<Parameters>(static_cast<int>(p) + 1)) {
                addFloatParam(p)->setRange(0.0, 1.0)->setRangedValue(0.0);
                setParamName(getParam(p), "Macro " + std::to_string(static_cast<int>(p) - static_cast<int>(Parameters::Macro01) + 1));
            }

            addFloatParam(Parameters::FmFine)->setRange(-1.0, 1.0)->setRangedValue(0.0);
            setParamName(getParam(Parameters::FmFine), "FM fine", "FM fine", "Fine");
            addIntParam(Parameters::FmCoarse)->setRange(0, 48)->setRangedValue(0);
            setParamName(getParam(Parameters::FmCoarse), "FM Coarse", "FM Coarse", "Coarse");

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
            addFloatParam(Parameters::LfoPhase)->setRange(-1.0, 1.0)->setRangedValue(0.0);
            setParamName(getParam(Parameters::LfoShape), "LFO shape", "LFO shape", "Shape");
            setParamName(getParam(Parameters::LfoFrequency), "LFO frequency", "LFO freq", "Frequency");
            setParamName(getParam(Parameters::LfoDelay), "LFO ramp", "LFO ramp", "Ramp");
            setParamName(getParam(Parameters::LfoPhase), "LFO phase", "LFO phase", "Phase");

            addFloatParam(Parameters::VolEnvFm)->setRange(-24.0, 24.0)->setRangedValue(0.0);
            addFloatParam(Parameters::ModEnvFm)->setRange(-24.0, 24.0)->setRangedValue(0.0);
            addFloatParam(Parameters::LfoFm)->setRange(-24.0, 24.0)->setRangedValue(0.0);
            addFloatParam(Parameters::VolEnvCutoff)->setRange(-24000.0, 24000.0)->setRangedValue(0.0);
            addFloatParam(Parameters::ModEnvCutoff)->setRange(-24000.0, 24000.0)->setRangedValue(0.0);
            addFloatParam(Parameters::LfoCutoff)->setRange(-2 * 24000.0, 2 * 24000.0)->setRangedValue(0.0);
            addFloatParam(Parameters::GlideLength)->setRange(0.0, 1.0)->setRangedValue(0.0);
            addFloatParam(Parameters::MasterVolume)->setRange(0.0, 0.5)->setRangedValue(0.25);

            setParamName(getParam(Parameters::VolEnvFm), "Volume envelope to FM amount", "FM Amt EnvA", "Env Vol");
            setParamName(getParam(Parameters::ModEnvFm), "Modulation envelope to FM amount", "FM Amt EnvM", "Env Mod");
            setParamName(getParam(Parameters::LfoFm), "LFO to FM amount", "FM Amt LFO", "LFO1");
            setParamName(getParam(Parameters::VolEnvCutoff), "Volume envelope to filter cutoff", "Flt EnvA", "Env Vol", "Hz", "%.0f");
            setParamName(getParam(Parameters::ModEnvCutoff), "Modulation envelope to filter cutoff", "Flt EnvM", "Env Mod", "Hz", "%.0f");
            setParamName(getParam(Parameters::LfoCutoff), "LFO to Filter Cutoff", "LFO1 Amount", "LFO1", "Hz", "%.0f");
            setParamName(getParam(Parameters::GlideLength), "Glide length", "Glide");
            setParamName(getParam(Parameters::MasterVolume), "Volume", "Volume", "Volume", "%");


            addEnumParam(Parameters::Osc1Wave)->setStrings(stringsWaveform.begin(), stringsWaveform.end())->setRangedValue(0);
            setParamName(getParam(Parameters::Osc1Wave), "Osc1 Waveform", "Osc1 Waveform", "Waveform");
            addEnumParam(Parameters::Osc2Wave)->setStrings(stringsWaveform.begin(), stringsWaveform.end())->setRangedValue(0);
            setParamName(getParam(Parameters::Osc2Wave), "Osc2 Waveform", "Osc2 Waveform", "Waveform");
            addEnumParam(Parameters::LfoWave)->setStrings(stringsWaveform.begin(), stringsWaveform.end())->setRangedValue(0);
            setParamName(getParam(Parameters::LfoWave), "LFO1 Waveform", "LFO1 Waveform", "Waveform");
            addEnumParam(Parameters::VoiceMode)->setStrings(stringsVoiceMode.begin(), stringsVoiceMode.end())->setRangedValue(0);
            setParamName(getParam(Parameters::VoiceMode), "Voice Mode");
            addEnumParam(Parameters::FilterMode)->setStrings(stringsFilterMode.begin(), stringsFilterMode.end())->setRangedValue(0);
            setParamName(getParam(Parameters::FilterMode), "Filter Mode", "Flt Mode");
            addEnumParam(Parameters::FmMode)->setStrings(stringsFMMode.begin(), stringsFMMode.end())->setRangedValue(0);
            setParamName(getParam(Parameters::FmMode), "FM Mode");

            addIntParam(Parameters::Voices)->setRange(1, NUM_POLY_VOICES)->setRangedValue(32);
            addIntParam(Parameters::UnisonVoices)->setRange(1, NUM_UNISON_VOICES)->setRangedValue(3);

            setParamName(getParam(Parameters::Voices), "Polyphonic Voice Maximum", "Voices");
            setParamName(getParam(Parameters::UnisonVoices), "Unison Voices", "Unison");

            addFloatParam(Parameters::Panning)->setRange(-1.0, 1.0)->setRangedValue(0.0);
            setParamName(getParam(Parameters::Panning), "Stereo Panning", "Pan");
            modulations.emplace_back();
            String defaultPresetPath = App::Platform::toUserdataPath(String("presets/") + PLUGIN_EFFECT_NAME);
            CreateDirectoryIfNotExists(defaultPresetPath);
            presetManager.load(defaultPresetPath);
            setPreset(defaultPresetPath, "Untitled");
        }

    public:
        explicit SynthImpl(PluginVST2_Synth* vst2Plugin)
            : SynthState(),
              instanceVst2(vst2Plugin) {
            (void) instanceVst2;
            initImpl();
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
        void setSetting(Settings setting, bool value) {
            settings[setting] = static_cast<float>(value);
        }
        void init() {
            for (auto param : this->vecParams) {
                OnParamChange(param->enumParam);
            }
            setModulationType(0, 0, static_cast<int32_t>(ModulationType::ModulationSource) + static_cast<int32_t>(ModulationSourceType::Lfo1));
            setModulationDestination(0, 0, Parameters::FilterCutoff, 0.5);
        }
        void initSampleRate() {
            const auto dt = oneOverSR;
            for (auto& uv : voices) {
                uv.visitVoices([this, dt, &uv](auto& v) {
                    UpdateVoiceModulations(uv, v, modSrcData);
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
        double getModulationAmountMin(Parameters param) const {
            return modulationValuesMin[param];
        }
        double getModulationAmountMax(Parameters param) const {
            return modulationValuesMax[param];
        }
        std::optional<std::vector<param_modulation_range_t>> getParamModulationRanges(Parameters _param) {
            //TODO: result can be cached
            std::optional<std::vector<param_modulation_range_t>> result;
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
                                        bIsBipolar });
                    }
                }
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
            const auto modType    = typeIdx >= ModulationType::ModulationSource ? ModulationType::ModulationSource : static_cast<ModulationType>(typeIdx);
            const auto modSrcType = modType == ModulationType::ModulationSource ? static_cast<ModulationSourceType>(typeIdx - ModulationType::ModulationSource) : ModulationSourceType::Lfo1;
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
                auto& mod = modulation.inputs[srcSlotIndex];
                mod.type  = modType;
                mod.src   = modSrcType;
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
                auto& mod = modulation.inputs[idx];
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
            snapshot.version     = 8;
            const auto numParams = CtrSize(vecParams);
            snapshot.params.reserve(numParams);
            for (int32_t i = 0; i < numParams; ++i) {
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

            for (auto& setting : snapshot.settings) {
                if (setting.paramIdx >= 0 && setting.paramIdx < CtrSize(settings)) {
                    settings[setting.paramIdx] = setting.range;
                } else {
                    dbgassert(0);
                }
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
        void StartVoice(VoiceUnison& voice, bool bTriggerMono) {
            voice.Start(tempo, lfoPhaseDrift);
            dbgassert(voice.numUnisonActive == unisonVoiceCount);
            voice.visitVoices([this, &voice, bTriggerMono](Voice& v) {
                UpdateVoiceEnvelopeModulations(voice, v);
                UpdateVoiceModulations(voice, v, modSrcData);
                bool resetOscs      = v.volEnv.stage >= EnvelopeStages::Idle;
                bool resetEnvelopes = v.volEnv.stage >= EnvelopeStages::Idle;
                bool resetLfos      = v.volEnv.stage >= EnvelopeStages::Release;
                
                v.Start(resetOscs, resetEnvelopes);
                if (resetLfos) {
                    bool bFadeLfo = v.volEnv.stage < EnvelopeStages::Idle;
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
        void FlushMidi(int sample) {
            while (!midiQueue.Empty()) {
                auto message = midiQueue.Peek();
                if (message.mOffset > sample) break;

                auto voiceMode      = GetParamEnum(Parameters::VoiceMode)->getEnumValue<VoiceModes>();
                auto status         = message.StatusMsg();
                auto ctrl           = message.ControlChangeIdx();
                auto note           = message.NoteNumber();
                auto velocity       = pow(message.Velocity() * .0078125, 1.25);

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
                                            bool aReleased = a.IsInactive();
                                            if (aReleased == b.IsInactive()) {
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
                                StartVoice(*voice, false);
                                voice->seqNr = seq++;
                                break;
                            }
                            default:
                            case VoiceModes::Mono:
                                voices[0].SetNote(note);
                                voices[0].SetVelocity(velocity);
                                StartVoice(voices[0], true);
                                voices[0].seqNr = 1;
                                break;
                            case VoiceModes::Legato:
                                voices[0].SetNote(note);
                                if (heldNotes.empty()) {
                                    voices[0].SetVelocity(velocity);
                                    voices[0].ResetPitch();
                                    StartVoice(voices[0], true);
                                    voices[0].seqNr = 1;
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
                using Vec4D      = glm::vec<4, FPType, glm::packed_highp>;
                auto sse8Float   = reinterpret_cast<__m256*>(&envParamVals[0]);
                *sse8Float       = math::simd::log_v8f(*sse8Float);
                auto pIn         = &envParamVals[0];
                FPType* pDataOut = &envParamValsScaled[0];
                for (size_t j = 0; j < LEN_SIMD; j += 4) {
                    Vec4D& valsRef = *reinterpret_cast<Vec4D*>(&pIn[0]);
                    auto vals      = valsRef * 0.1f;
                    auto sse4Float = reinterpret_cast<__m128*>(&vals);
                    *sse4Float     = math::simd::exp_v4f(*sse4Float);
                    auto floatPtr  = reinterpret_cast<float*>(&vals[0]);
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
                    bool bIsActive = v.isVoiceActive(filterMode);
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
                        dbgassert(v.lfo1.phase >= -1.0 && v.lfo1.phase <= 1.0);
                        double lfoFreqHz    = floatParamFreq->ValueModulated(v.modValues[Parameters::LfoFrequency]) * bpmHz;
                        double dVoiceLfoBi  = v.lfo1.GetLfo(dt, lfoWave, lfoFreqHz, lfo1OneShot);
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

        void UpdateVoiceModulations(VoiceUnison& vu, Voice& voice, ModulationSourceData& modSrcData) {

            if (!getSetting(Settings::ModulationEnabled)) {
                return;
            }

            auto& voiceModulations = voice.modValues;
            if (getSetting(Settings::ClearModulationEnabled)) {
                std::memset(voiceModulations.data(), 0, voiceModulations.size() * sizeof(double));
            }
            // std::array<double, MathExprInputLen> sourcesV{};
            modSrcData[1 + ModulationSourceType::VolEnv]           = voice.volEnv.value;
            modSrcData[1 + ModulationSourceType::ModEnv]           = voice.modEnv.value;
            modSrcData[1 + ModulationSourceType::Lfo1]             = voice.lfoValue;
            modSrcData[1 + ModulationSourceType::Velocity]         = voice.velocity;
            modSrcData[1 + ModulationSourceType::VoiceIndex]       = this->polyVoiceCount < 2 ? 0.5 : vu.indexPoly / static_cast<double>(this->polyVoiceCount - 1);
            modSrcData[1 + ModulationSourceType::UnisonVoiceIndex] = this->unisonVoiceCount < 2 ? 0.5 : voice.indexUnison / static_cast<double>(this->unisonVoiceCount - 1);
            modSrcData[1 + ModulationSourceType::Pitch]            = noteToLinearScale(voice.note);
            modSrcData[1 + ModulationSourceType::Note]             = voice.note / 127.0;
            for (auto& modulation : modulations) {
                ModulationSourceData& sources = modSrcData;
                double modVal                 = 0.0;
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
                            if (getSetting(Settings::ExprEvaluationEnabled)) {
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
                                srcVal = modVal * (1 - srcVal);
                                break;
                            case ModulationOperator::Absolute:
                                srcVal = abs(modVal * srcVal);
                                break;
                            case ModulationOperator::Power:
                                srcVal = exp(log(srcVal) * modVal);
                                break;
                            case ModulationOperator::Clamp:
                                srcVal = math::clamp(modVal, double(input.range == ModulationRange::Bipolar) * -1.0 * srcVal, srcVal);
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

            out *= volEnvValue;

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
                // data = cutoff*dt;
                // auto res = filterResonance;
                auto res = static_cast<SynthParam_Float*>(vecParams[Parameters::FilterResonance])->ValueModulated(voice.modValues[Parameters::FilterResonance]);
                out      = voice.filter.Process(dt, out, filtermode, cutoff, res);
            }
            data = voice.lfoValue;
            // out *= volEnvValue;

            return out;
        }

    public:
        void onTransportChanged(bool bIsPlaying) {
            seq              = 1;
            double lfo1Tempo = 1.0;
            double lfo2Tempo = 1.0 / 4.0;
            lfo.initPhase(fmod(tempo.ppqPos * lfo1Tempo, 1.0), true);
            lfo2.initPhase(fmod(tempo.ppqPos * lfo2Tempo, 1.0), true);

            // log_lf(Log::L_DEBUG, "Reset LFO phases: at ppq/4 %f\n", fmod(tempo.ppqPos/4.0, 1.0));
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

        void ProcessSynth(float** inputs, float** outputs, int nFrames) {
            double bpmHz                                     = math::max(tempo.bpm, 1.0) / 60.0;
            const auto bpmDiv4Hz                             = math::max(tempo.bpm / 4.0, 1.0) / 60.0;
            const auto mvInv                                 = sqrt(1.0 / math::max<double>(1.0, this->unisonVoiceCount));
            const auto voiceMode                             = GetParamEnum(Parameters::VoiceMode)->getEnumValue<VoiceModes>();
            const FilterModes filterMode                     = GetParamEnum(Parameters::FilterMode)->getEnumValue<FilterModes>();
            const bool bIsGlideEnabled                       = voiceMode != VoiceModes::Poly;
            int32_t numActiveVoices                          = 0;
            const auto dt                                    = oneOverSR;
            modSrcData[1 + ModulationSourceType::SrcMacro01] = GetParamFloat(Parameters::Macro01)->Value();
            modSrcData[1 + ModulationSourceType::SrcMacro02] = GetParamFloat(Parameters::Macro02)->Value();
            modSrcData[1 + ModulationSourceType::SrcMacro03] = GetParamFloat(Parameters::Macro03)->Value();
            modSrcData[1 + ModulationSourceType::SrcMacro04] = GetParamFloat(Parameters::Macro04)->Value();
            modSrcData[1 + ModulationSourceType::SrcMacro05] = GetParamFloat(Parameters::Macro05)->Value();
            modSrcData[1 + ModulationSourceType::SrcMacro06] = GetParamFloat(Parameters::Macro06)->Value();
            modSrcData[1 + ModulationSourceType::SrcMacro07] = GetParamFloat(Parameters::Macro07)->Value();
            modSrcData[1 + ModulationSourceType::SrcMacro08] = GetParamFloat(Parameters::Macro08)->Value();
            const bool bDiagnostic                           = getSetting(Settings::DiagnosticOutputEnabled);
            if (getSetting(Settings::ShowModulationRanges)) {
                std::memset(modulationValuesMax.data(), 0, modulationValuesMax.size()*sizeof(double));
                std::memset(modulationValuesMin.data(), 0, modulationValuesMin.size()*sizeof(double));
            }
            for (int s = 0; s < nFrames; s++) {
                FlushMidi(s);
                UpdateParameters(dt);
                UpdateDrift(dt);
                if (lfo2.Update(dt, bpmDiv4Hz)) {
                    lfo.initPhase(0.0, false);
                }
                lfo2Value = 0.5 + 0.5 * lfo2.GetWaveform(Waveforms::Saw, true);
                // calculate lfo freqency in Hz based on tempo
                double lfoFreqHz = GetParamFloat(Parameters::LfoFrequency)->Value() * bpmHz;
                lfoValue         = lfo.Get(dt, lfoWave, lfoFreqHz, false);

                VoiceList list{};
                UpdateAllVoiceStates(dt, filterMode, list);
                if (getSetting(Settings::LfoEnabled)) {
                    UpdateAllVoiceLfos(dt, list);
                }
                modSrcData[1 + ModulationSourceType::Lfo2] = lfo2Value;
                int32_t numUnisonVoices = list.numUnisonVoices;
                for (int32_t i = 0; i < numUnisonVoices; ++i) {
                    auto& uv = voices[list.unisonVoices[i] / NUM_UNISON_VOICES];
                    auto& v  = uv.voices[list.unisonVoices[i] % NUM_UNISON_VOICES];
                    if (bIsGlideEnabled) {
                        v.frequency += (v.targetFrequency - v.frequency) * glideLength * dt;
                    }
                    UpdateVoiceModulations(uv, v, modSrcData);
                    UpdateVoiceEnvelopeModulations(uv, v);
                    UpdateVoiceEnvelopes(dt, uv, v);
                    if (getSetting(Settings::ShowModulationRanges)) {
                        for (int j = 0; j < Parameters::kNumParams; ++j) {
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

                for (int32_t polyIndex = list.polyVoiceIndexFirst; polyIndex < list.polyVoiceIndexLast; ++polyIndex) {
                    auto& uv = voices[polyIndex];
                    for (int32_t unisonIndex = 0; unisonIndex < maxUnisonVoice; ++unisonIndex) {
                        auto& v = uv.getVoice(unisonIndex);
                        if (!v.bIsActive) {
                            continue;
                        }
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
                            if (unisonIndex == 0 && uv.seqNr == 1) {
                                outL = vData;
                            }
                        } else {
                            outL += voice * sqrt(1.0 - pan);
                        }
                    }
                }
                outputs[0][s] = fp_math::silenceNanInff(static_cast<float>(outL));
                outputs[1][s] = fp_math::silenceNanInff(static_cast<float>(outR));

                dbgassert((list.numPolyVoices > 0) == (list.numUnisonVoices > 0));
                numActiveVoices = math::max(math::max(0, list.numPolyVoices), numActiveVoices);
                if (s == nFrames - 1) {
                    prevVoiceList = list;
                }
            }
            this->activeVoiceCount = numActiveVoices;
        }

        void OnParamChange(Parameters parameter) {
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
        void setPreset(const String& path, const String& name) {
            currentPreset.path = path;
            currentPreset.name = name;
        }

        const PresetManager::Preset& getPreset() const {
            return currentPreset;
        }
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
    int32_t PluginVST2_Synth::loadPreset(const String& presetPath) {
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
                    impl->setSnapshot(snapshotLoaded);
                    if (curProgram >= 0 && curProgram < CtrSize(staticPrograms)) {
                        for (auto& param : vecParams) {
                            auto pParamDouble = staticPrograms[curProgram].getProgramParameter(param->enumParam);
                            if (pParamDouble) *pParamDouble = param->getAsDouble();
                        }
                    }
                    while (snapshotLoaded.uiLayout.size() > views.size()) {
                        createView();
                    }
                    impl->setUiSnapshot(snapshotLoaded);
                    String path;
                    String name;
                    String ext;
                    SplitPath(presetPath, &path, &name, &ext);
                    impl->setPreset(presetPath, name);

                    for (auto& pviewctr : this->views) {
                        if (pviewctr->isInUse()) {
                            pviewctr->onSetParameter(-1, 0.0f);
                        }
                    }
                    updateDisplay();
                    return 0;
                }
                return -3;
            }
            return -2;
        }
        return -1;
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
        float rowHeight;

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
            for (auto param : parametersModulate) {
                auto synthParam = synth->getParam(param);
                if (synthParam && !synthParam->name.empty()) {
                    vecOpts.push_back(synthParam->name);
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
                        if (idx > 0 && idx-1 < sizeof(parametersModulate) / sizeof(parametersModulate[0])) {
                            synth->setModulationDestination(slotIndex, destSlotIndex, parametersModulate[idx-1], knob.getValue());
                        } else {
                            synth->setModulationDestination(slotIndex, destSlotIndex, -1, knob.getValue());
                        }
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
                    auto idx = std::find(std::begin(parametersModulate), std::end(parametersModulate), dest.parameter);
                    if (std::end(parametersModulate) != idx) {
                        dropdown.setSelectedIndex(1 + static_cast<int32_t>(idx - std::begin(parametersModulate)));
                        auto* param = synth->getParam(dest.parameter);
                        if (param) {
                            dropdown.setCurrentString(param->getShortName());
                        }
                    } else {
                        dropdown.setSelectedIndex(-1);
                    }
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
            prefSize.y = math::roundfS32(rowHeight);
        }
        void setRowHeight(float rowHeight) {
            this->rowHeight = rowHeight;
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
        float rowHeight;

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
                        dropdownSource.setSelectedIndex(static_cast<int32_t>(src.type) + 1);
                        break;
                    case ModulationType::ModulationSource:
                        dropdownSource.setSelectedIndex(static_cast<int32_t>(src.src) + 3);
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
            prefSize.y = math::roundfS32(rowHeight);
        }
        void setRowHeight(float rowHeight) {
            this->rowHeight = rowHeight;
        }
    };
    class guictr_synth_title : public guictr_base {
        float titleHeight = 10.0f;
    public:
        guictr_synth_title() = default;
        void renderContainerLabel(NVGcontext* vg) override {
            if (isFlag(FLG_RENDER_LABEL) && label.length()) {
                auto cs                = getSizeContent();
                const auto bgColor     = getInnerBackgroundColorFromState(getStateFlags());
                renderTextLabel(vg,
                                vec2(getPosContent()) + vec2(padding, titleHeight / 2.0),
                                vec2(getSizeContent()) - vec2(INSET_TITLE + 2, 0),
                                label,
                                theme,
                                titleHeight,
                                theme->getContrastColor(bgColor),
                                NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
            }
        }
        void setTitleHeight(float height) {
            titleHeight = height;
        }
        float getTitleHeight() const {
            return titleHeight;
        }
    };
    class guicontainer_modulation_slot : public guictr_synth_title {
        SynthImpl* const synth;
        const int32_t slotIndex;
        std::vector<guicontainer_modulation_slot_source*> sources;
        std::vector<guicontainer_modulation_slot_destination*> destinations;

    public:
        explicit guicontainer_modulation_slot(SynthImpl* synth, int32_t slotIndex)
            : synth(synth),
              slotIndex(slotIndex) {
            padding      = 1;
            margin       = 0;
            sortChildren = true;
            setLabel(StringFormat("Modulation %d", slotIndex + 1));
            setBackgroundRendered(true);
            setBackgroundRenderedInset(true);
            setFlag(FLG_RENDER_LABEL, true);
            setCanMouseHit(true);
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
            // auto halfSize = vec2(size) * 0.5f;
            // vec2 posScrolled = vec2(pos) + halfSize;
            // nvgTransformByState(vg, 2, &posScrolled.x, &posScrolled.y);
            // if (posScrolled.y < -halfSize.y) {
            //     log_printf("Skip: right, bottom %d %d, in parent space: %f %f\n", right(), bottom(), posScrolled.x, posScrolled.y);
            //     return;
            // }
            guictr_base::render(vg);
        }
        void drawBackground(NVGcontext* vg, const guitheme_t* theme, ivec2 posInset, ivec2 sizeInset, int margin, bool drawInset) {
            if (sizeInset.y > 0 && sizeInset.x > 0) {
                nvgTranslateZ(vg, -2.0f);
                // nvgShapeAntiAlias(vg, 0);
                nvgBeginPath(vg);
                nvgRect(vg, pos.x, posInset.y, size.x, getTitleHeight());
                auto color = dbgcolorsArray[1 + (slotIndex % (dbgcolorsArraySize - 1))];
                color.a = 0.5f;
                nvgFillColor(vg, color);
                nvgFill(vg);
                nvgBeginPath(vg);
                nvgRect(vg, pos.x, posInset.y, size.x, sizeInset.y);
                nvgStrokeWidth(vg, theme->get(GuiConstant::CONST_GUI_INSET_WIDGET_BG));
                nvgStrokeColor(vg, color);
                nvgStroke(vg);
                // nvgShapeAntiAlias(vg, USE_NANOVG_AA);
                nvgTranslateZ(vg, 1.0f);
            }
        }
        void buttonClicked(guibase* button) override {
            parent->buttonClicked(button);
            guictr_base::buttonClicked(button);
        }
        void determineSize(ivec2& prefSize) override {
            vec2 sizeTotal = {0, 0};
            for (auto& src : guis) {
                src->size = prefSize;
                src->determineSize(src->size);
                sizeTotal.y += src->size.y;
            }
            prefSize.y = math::ceilfS32(getTitleHeight() + sizeTotal.y + padding * 2);
        }
        void setRowHeight(float height) {
            setTitleHeight(height);
            for (auto& src : sources) {
                src->setRowHeight(height*1.5f);
            }
            for (auto& dst : destinations) {
                dst->setRowHeight(height*1.5f);
            }
        }
        void layout() override {
            auto cs        = getSizeContent();
            vec2 pos      = {0, getTitleHeight()};
            auto rowHeight = (cs.y-pos.y) / CtrSize(guis);
            for (auto& slot : guis) {
                slot->pos    = pos;
                slot->size.x = cs.x;
                // slot->size.y = math::roundfS32(rowHeight);
                slot->layout();
                pos.y = slot->bottom();
            }
        }
    };
    class guicontainer_modulation : public guictr_synth_title {
        guictr_scrollbar scrollContainerModulation;
        SynthImpl* const synth;
        std::vector<guicontainer_modulation_slot*> slots;
        bool bGuiNeedsRefresh = true;

    public:
        explicit guicontainer_modulation(SynthImpl* synth)
            : synth(synth) {
            margin  = 0;
            padding = 0;
            setLabel("Modulation");
            setBackgroundRendered(false);
            setBackgroundRenderedInset(false);
            setFlag(FLG_RENDER_LABEL, false);
            setCanMouseHit(true);
            scrollContainerModulation.maxHeight = -1;
            add(&scrollContainerModulation);
            scrollContainerModulation.setLayoutMode(autolayout_mode::LAYOUT_VERTICAL);
        }
        ~guicontainer_modulation() override {
            scrollContainerModulation.removeGuis();
            removeGuis();
            for (auto& slot : slots) {
                delete slot;
            }
        }
        void layout() override {
            scrollContainerModulation.pos = {0, getTitleHeight()};
            scrollContainerModulation.size = size;
            auto cs   = getSizeContent();
            scrollContainerModulation.maxHeight = size.y;
            scrollContainerModulation.determineSize(scrollContainerModulation.size);
            // ivec2 pos = {0, 0};
            // for (auto& slot : slots) {
            //     slot->size = cs;
            //     slot->determineSize(slot->size);
            //     slot->pos = pos;
            //     slot->layout();
            //     pos.y = slot->bottom();
            // }
            for (guibase* gui : guis) {
                // if (!stl_contains(slots, gui)) {
                    gui->layout();
                // }
            }
        }
        void setTitleHeight(float height) {
            guictr_synth_title::setTitleHeight(isFlag(FLG_RENDER_LABEL) ? height*1.5f : 0);
            for (auto& slot : slots) {
                slot->setRowHeight(height);
            }
        }
        void setFromSynth() {
            auto modulations = synth->getModulationCount();
            while (CtrSize(slots) <= modulations) {
                slots.push_back(new guicontainer_modulation_slot(synth, CtrSize(slots)));
                scrollContainerModulation.add(slots.back());
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
            prefSize.y = math::ceilfS32(getTitleHeight() + sizeTotal.y + padding * 2);
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
            m_layout.inset = 2;
        }
        std::optional<std::vector<param_modulation_range_t>> getKnobModulationRanges() override {
            if (!synth->getSetting(Settings::ShowModulationRanges)) {
                return std::nullopt;
            }
            auto synthParam = synth->getParam(param);
            if (synthParam) {
                return synth->getParamModulationRanges(param);
            }
            return std::nullopt;
        }
        Parameters getParam() const {
            return param;
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
    class guictr_synth_param_container : public guictr_synth_title {
        SynthImpl* const synth;
        vec2 sliderSize;
        std::vector<guiknob_synthparam*> knobs;
    public:
        explicit guictr_synth_param_container(SynthImpl* synth)
            : synth(synth), sliderSize(0.0f) {
            margin  = 4;
            padding = 4;
            setBackgroundRendered(true);
            setBackgroundRenderedInset(true);
            setFlag(FLG_RENDER_LABEL, true);
            setCanMouseHit(true);
        }
        void addParamKnob(guiknob_synthparam* knob) {
            knobs.push_back(knob);
            add(knob);
        }

        // GuiColor::constant_t getOuterBackgroundColorFromState(int32_t stateflags) const override {
        //     if (focused()) {
        //         return GuiColor::COL_BG_DRKER;
        //     }
        //     return GuiColor::COL_BG_DRKER2;
        // }


        void render(NVGcontext* vg) override {
            if (!isVisible()) {
                log_printf("warning, skip rendering container with state !isVisible()\n");
                return;
            }
            if (isBackgroundRendered()) {
                renderBackground(vg);
            }
            if (!setScissorTransform(vg)) {
                return;
            }
            for (auto c : guis) {
                if (!c->isVisible()) {
                    //log_printf("warning, skip rendering child container with state !isVisible()\n");
                    continue;
                }
                if (c->size.x <= 0 || c->size.y <= 0) {
                    log_printf("warning, skip rendering child container %s with size <= 0 0\n", StringAsCStr(c->getClassName()));
                    continue;
                }
                {
                    nvgSave(vg);
                    c->render(vg);
                    nvgRestore(vg);
                }
            }
            if (synth->getSetting(Settings::ShowModulationRanges)) {
                for (auto k : knobs) {
                    auto valueMin = static_cast<float>(this->synth->getModulationAmountMin(k->getParam()));
                    auto valueMax = static_cast<float>(this->synth->getModulationAmountMax(k->getParam()));
                    auto layout = k->getLayout();
                    auto col = dbgcolorsArray[0];
                    col.a = 0.5;
                    k->renderRangeIndicator(vg, layout.pKnob, layout.sKnob, valueMin, valueMax, col, 9, 10);
                }
            }
        }
        void layoutParameterGroup(ivec2& prefSize, vec2 knobSize, float titleHeight) {
            this->setTitleHeight(titleHeight);
            auto cs                = getSizeContent();
            const auto knobsPerCol = 3;
            const auto innerSize   = vec2(cs.x, cs.y - titleHeight);
            auto knobPos           = ivec2(0, titleHeight);
            ;
            auto sliderSize      = vec2(knobSize.x, innerSize.y);
            this->sliderSize     = sliderSize;
            knobSize.y           = (innerSize.y - padding * (knobsPerCol - 1)) / float(knobsPerCol);
            auto knobSizeRounded = ivec2(math::roundfS32(this->sliderSize.x), math::roundfS32(this->sliderSize.y));
            int32_t knobIdx      = 0;
            for (auto knob : guis) {
                if (knob->id == 1) {
                    auto offset = knobIdx % knobsPerCol;
                    knob->pos   = knobPos + ivec2(0, offset * (knobSize.y + padding));
                    knob->size  = knobSize;
                    knobIdx++;
                    if (knobIdx % knobsPerCol == 0) {
                        knobPos.x += knobSizeRounded.x + padding;
                    }
                }
            }
            if (knobIdx % knobsPerCol != 0) {
                knobPos.x += knobSizeRounded.x + padding;
            }
            for (auto knob : guis) {
                if (knob->id == 0) {
                    knob->pos  = knobPos;
                    knob->size = knobSizeRounded;
                    knobPos.x += knobSizeRounded.x + padding;
                }
            }
            prefSize.x = math::roundfS32(knobPos.x - padding) + padding * 2;
        }
        void layout() override {
            for (guibase* gui : guis) {
                gui->layout();
            }
        }
        void buttonClicked(guibase* button) override {
            parent->buttonClicked(button);
        }
    };
    class guicontainer_plugin_synth_editor : public guictr_base, public splitter_cb {
        struct _synth_gui_param_knob {
            Parameters param;
            guiknob_pluginparam* knob;
            guiknob::knobtype type;
            guictr_base* parentContainer;
            ivec2 pos;
            ivec2 size;
        };
        PluginVST2_Synth* const plugin;
        effectbase* const module;
        gui_textfield editfield;
        std::vector<_synth_gui_param_knob> vecParamUI;
        std::vector<guictr_synth_param_container*> containers;
        std::vector<_synth_gui_param_knob> vecListParam;
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
        explicit guicontainer_plugin_synth_editor(PluginVST2_Synth* plugin)
            : guictr_base(),
              plugin(plugin),
              module(plugin->getHostSideHandle()),
              modulation(plugin->getSynth()),
              ctrOsc1(plugin->getSynth()),
              ctrOsc2(plugin->getSynth()),
              ctrFm(plugin->getSynth()),
              ctrFilter(plugin->getSynth()),
              ctrAmp(plugin->getSynth()),
              ctrEnvV(plugin->getSynth()),
              ctrEnvM(plugin->getSynth()),
              ctrLfo(plugin->getSynth()),
              ctrMacro(plugin->getSynth()),
              splitter(1, 0.8f) {
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
            auto ctrs      = { &ctrAmp, &ctrFilter, &ctrLfo, &ctrOsc1, &ctrOsc2, &ctrFm, &ctrEnvV, &ctrEnvM, &ctrMacro };
            int32_t ctrIdx = 1;
            for (auto* ctr : ctrs) {
                ctr->id = ctrIdx;
                add(ctr);
                containers.push_back(ctr);
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

            vecParamUI.reserve(Parameters::kNumParams);
            for (auto param : parametersOrdered) {
                auto type = guiknob::knobtype::SLIDER_LABELED;
                switch (param) {
                    // case Parameters::MasterVolume:
                    case Parameters::Voices:
                    case Parameters::UnisonVoices:
                    case Parameters::Osc1Wave:
                    case Parameters::Osc2Wave:
                    case Parameters::FilterMode:
                    case Parameters::LfoWave:
                    case Parameters::VoiceMode:
                    case Parameters::FmMode:
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
                        ctr = &ctrOsc1;
                        break;
                    case Parameters::Osc2Wave:
                    case Parameters::Osc2Coarse:
                    case Parameters::Osc2Fine:
                    case Parameters::Osc2Split:
                        ctr = &ctrOsc2;
                        break;
                    case Parameters::LfoDelay:
                    case Parameters::LfoFrequency:
                    case Parameters::LfoPhase:
                    case Parameters::LfoShape:
                    case Parameters::LfoWave:
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
                        ctr = &ctrEnvM;
                        break;
                    case Parameters::VolEnvA:
                    case Parameters::VolEnvD:
                    case Parameters::VolEnvS:
                    case Parameters::VolEnvR:
                    case Parameters::VolEnvV:
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
                        unreachable();
                        break;
                }
                auto knob = new guiknob_synthparam(plugin->getSynth(),
                                                   param,
                                                   type);
                if (ctr) {
                    ctr->addParamKnob(knob);
                    knob->id = type == guiknob::knobtype::KNOB_LABELED ? 1 : 0;
                }
                vecParamUI.push_back({ param, knob, type, ctr, ivec2(0), ivec2(32, 32) });
                if (!vecParamUI.back().parentContainer) {
                    vecListParam.push_back(vecParamUI.back());
                }
            }
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
                    _newListIn.push_back(new gui_listsynthsettings{ plugin->getSynth(), setting, stringsSettings[setting] });
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
            for (auto& synthKnob : vecParamUI) {
                delete synthKnob.knob;
            }
            for (auto& ctr : containers) {
                delete ctr;
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
            if (index >= 0 && index < (int32_t) vecParamUI.size()) {
                return vecParamUI[index].knob;
            }
            return nullptr;
        }

        void onSetParameter(int32_t index, float value) {
            if (index == -1) {
                bGuiNeedsRefresh = true;
                return;
            }
#if BUILD_EXTERNAL_PLUGIN
            guiknob_pluginparam* knob = getKnobFromParameter(index);
            if (knob) {
                knob->setValueInit(value);
            }
#endif
        }
        void onGuiOpen() {
            for (auto& synthKnob : vecParamUI) {
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
            for (auto& synthKnob : vecParamUI) {
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
            strings.push_back(StringFormat("Drift Freq %f", (synthImpl->driftVelocity * synthImpl->getSamplerate())));
            strings.push_back(StringFormat("Voices %d", synthImpl->getActiveVoiceCount()));
            strings.push_back(StringFormat("minPolyVoiceIndex %d", synthImpl->minPolyVoiceIndex));
            strings.push_back(StringFormat("maxPolyVoiceIndex %d", synthImpl->maxPolyVoiceIndex));
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
            for (auto& knob : vecParamUI) {
                if (knob.type == guiknob::knobtype::KNOB_LABELED) {
                    knob.knob->setLabelsFontScale(0.9f, 0.9f);
                    knob.knob->setLabelsScale(0.2f, 0.2f);
                }
                if (knob.type == guiknob::knobtype::SLIDER_LABELED) {
                    knob.knob->setLabelsFontScale(0.7f, 0.8f);
                    knob.knob->setLabelsScale(0.1f, 0.1f);
                }
            }
            auto cs                             = getSizeContent();
            const auto titleHeight = math::roundfS32(cs.y * 0.1f * 0.27f);
            modulation.setTitleHeight(titleHeight);
            const auto controlsWidth            = splitter.leftOrTop(cs.x) - padding / 2;
            const auto modulationWidth          = splitter.rightOrBottom(cs.x) - padding / 2;
            splitter.pos                        = ivec2(controlsWidth - Splitter::SPLITTER_LAYOUT_THICKNESS / 2, 0);
            splitter.size                       = ivec2(Splitter::SPLITTER_LAYOUT_THICKNESS, cs.y);
            modulation.size      = ivec2(modulationWidth, cs.y);
            modulation.pos       = ivec2(cs.x - modulationWidth, 0);
            int scale            = 4;//!list.isVisible() && !list2.isVisible()?4:3;
            cs                   = ivec2(controlsWidth, cs.y);
            const auto numRows   = 3;
            const auto numKnobs  = CtrSize(containers);
            const auto innerSize = vec2(cs.x, cs.y * scale / 4) - vec2(padding * 2);
            const auto numCols   = math::ceilfS32(numKnobs / float(numRows));
            auto innerRowHeight  = float(innerSize.y - (numRows - 1) * (padding)) / numRows;
            auto innerColWidth   = float(innerSize.x - (numCols - 1) * (padding)) / numCols;
            auto knobSizeF       = vec2(innerColWidth, innerRowHeight);
            auto modulePos       = ivec2(0);
            auto moduleSize      = ivec2(math::roundfS32(knobSizeF.x), math::roundfS32(knobSizeF.y));
            int32_t colIdx       = 0;

            auto knobSize = vec2((innerSize.x - 27 * padding) / 19, 0);

            for (auto& ctr : containers) {
                ctr->pos  = modulePos + ivec2(0, 0);
                ctr->size = moduleSize - ivec2(0, 0);
                ctr->layoutParameterGroup(ctr->size, knobSize, titleHeight);

                modulePos.x = ctr->right() + padding;
                if (++colIdx >= numCols) {
                    colIdx      = 0;
                    modulePos.x = 0;
                    modulePos.y += moduleSize.y + padding;
                }
            }

            list.pos   = ctrFm.getRightTop() + ivec2(padding, 0);
            list.size  = ivec2(controlsWidth - list.pos.x - padding, (ctrFm.size.y - padding) / 2);
            list2.pos  = vec2(list.left(), list.bottom() + padding);
            list2.size = ivec2(controlsWidth - list2.pos.x - padding, (ctrFm.size.y - padding) / 2);
            // auto listHeight = math::roundfS32(math::clamp<float>(cs.y*0.1f*0.33f, getLayoutHeight(this) / 2, getLayoutHeight(this)));
            list.setRowHeight(titleHeight);
            list2.setRowHeight(titleHeight);

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

        void setUiLayout(const ui_layout_t& layout) {
            splitter.setScale(layout.splitPos);
        }

        bool getUiLayout(ui_layout_t& layout) const {
            layout = ui_layout_t{ splitter.getScale() };
            return true;
        }
        void onChildLayoutChanged(guibase* g) override {
            bGuiNeedsRefresh = true;
            if (this->parent) {
                this->parent->onChildLayoutChanged(this);
            }
        }
    };
    class guicontainer_plugin_synth_preset_browser : public guictr_base, public splitter_cb {

        PluginVST2_Synth* const plugin;

    public:
        explicit guicontainer_plugin_synth_preset_browser(PluginVST2_Synth* plugin)
            : guictr_base(),
              plugin(plugin) {
        }

        void handleSplitterChanged(Splitter& splitter, float scale, int clampedAt) override {
            onChildLayoutChanged(this);
        }

        ivec2 getContainerSize() override {
            return size;
        }

        void onChildLayoutChanged(guibase* g) override {
            // bGuiNeedsRefresh = true;
            if (this->parent) {
                this->parent->onChildLayoutChanged(this);
            }
        }
    };

    /* top select menu */
    class guidropdown_select_preset_ctxt : public guictxtmenu {
        PluginVST2_Synth* plugin;
        PresetManager presetManager;
        int lvl = 0;

        class ctxtmenu_entry_folder : public ctxtmenu_entry {
            String path;
            bool bIsMenuOpen = false;

        public:
            bool isFolder() const { return true; }
            void setIsMenuOpen(bool isMenuOpen) { this->bIsMenuOpen = isMenuOpen; }
            bool isMenuOpen() const { return bIsMenuOpen; }
            String getPath() const { return path; }
            ctxtmenu_entry_folder(const String& _title, const String& _path, int id)
                : ctxtmenu_entry(_title, id), path(_path) {
            }
            void render(ivec2 ctxtSize, NVGcontext* vg, int idx, ivec2 mouse) override {
                if (contains(ctxtSize, mouse)) {
                    nvgBeginPath(vg);
                    nvgRect(vg, 0, y, ctxtSize.x, height);
                    nvgFillColor(vg, theme->getColor(GuiColor::COL_CTXTMNU_HILIGHT));
                    nvgFill(vg);
                }

                renderTextLabel(vg,
                                vec2(leftOffset(), y + height * 0.5f),
                                vec2(width - leftOffset(), height),
                                title,
                                theme,
                                fontSize,
                                THEMECOL_TEXT,
                                NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
                String rightSide = ">";
                if (rightSide.length()) {
                    auto defoffset = this->fontSize / 2.4f;
                    renderTextLabel(vg,
                                    vec2(width - defoffset, y + height * 0.5f),
                                    vec2(width, height),
                                    rightSide,
                                    theme,
                                    fontSize,
                                    THEMECOL_TEXT,
                                    NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
                }
            }
        };
        class ctxtmenu_entry_preset : public ctxtmenu_entry {
            const PresetManager::Preset& preset;
            bool bIsMenuOpen = false;

        public:
            bool isFolder() const { return false; }
            String getPath() const { return preset.path; }
            String getName() const { return preset.name; }
            bool isMenuOpen() const { return bIsMenuOpen; }
            void setIsMenuOpen(bool isMenuOpen) { this->bIsMenuOpen = isMenuOpen; }
            ctxtmenu_entry_preset(const PresetManager::Preset& _preset, int id)
                : ctxtmenu_entry(_preset.name, id),
                  preset(_preset) {
            }
            void render(ivec2 ctxtSize, NVGcontext* vg, int idx, ivec2 mouse) override {
                if (contains(ctxtSize, mouse)) {
                    nvgBeginPath(vg);
                    nvgRect(vg, 0, y, ctxtSize.x, height);
                    nvgFillColor(vg, theme->getColor(GuiColor::COL_CTXTMNU_HILIGHT));
                    nvgFill(vg);
                }

                renderTextLabel(vg,
                                vec2(leftOffset(), y + height * 0.5f),
                                vec2(width - leftOffset(), height),
                                title,
                                theme,
                                fontSize,
                                THEMECOL_TEXT,
                                NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
            }
        };

    public:
        explicit guidropdown_select_preset_ctxt(PluginVST2_Synth* _plugin, PresetManager _presetManager, const String& presetPath, int lvl = 0)
            : plugin(_plugin), presetManager(std::move(_presetManager)), lvl(lvl) {
            int32_t idx = 0;
            std::vector<String> paths;
            std::vector<ctxtmenu_entry_preset*> presetsCurrent;
            for (auto& preset : presetManager.getPresets()) {
                if (StrStartsWith(preset.path, presetPath)) {
                    String partPath = presetPath.length() + 1 < preset.path.length() ? preset.path.substr(presetPath.length() + 1) : preset.path;
                    String presetSubPath;
                    SplitPath(partPath, &presetSubPath, nullptr, nullptr);
                    String folderName;
                    SplitPath(presetSubPath, nullptr, &folderName, nullptr);

                    if (folderName.length() && folderName == presetSubPath && !stl_contains(paths, presetSubPath)) {
                        paths.push_back(presetSubPath);
                        addEntry(new ctxtmenu_entry_folder(folderName, presetPath + FILE_PATHSEP_STR + presetSubPath, (idx++) << 1 | 1));
                    }
                    if (presetSubPath.empty())
                        presetsCurrent.push_back(new ctxtmenu_entry_preset(preset, (idx++) << 1));
                }
            }
            for (auto preset : presetsCurrent) {
                addEntry(preset);
            }
        }

        void clickedElement(ctxtmenu_entry* e, int _id) override {
            auto appCtrlParent = parentCtrl->getParentCtrl();
            if (appCtrlParent) appCtrlParent->closeAllContextMenus();
            if ((_id & 1) == 0) {
                auto const ctxtEndpointEntry = static_cast<ctxtmenu_entry_preset*>(e);
                if (!ctxtEndpointEntry->isFolder()) {
                    ::ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
                    plugin->loadPreset(ctxtEndpointEntry->getPath());
                }
            }
        }

        void closeAllSubmenus() {
            auto appCtrlParent = parentCtrl->getParentCtrl();
            bool anyOpen       = false;
            for (ctxtmenu_entry* ctxtEntry : entries) {
                if (ctxtEntry && (ctxtEntry->id & 1)) {
                    auto entry = dynamic_cast<ctxtmenu_entry_folder*>(ctxtEntry);
                    anyOpen |= entry->isMenuOpen();
                    entry->setIsMenuOpen(false);
                }
            }
            if (anyOpen) {
                //close all menus deeper than this menu
                appCtrlParent->closeAppMenusAtLvl(lvl + 1);
            }
        }

        bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
            if (this->contains(mpos)) {
                ivec2 localMouse         = this->toContainerSpace(mpos);
                ctxtmenu_entry* entryHit = nullptr;
                for (ctxtmenu_entry* e : entries) {
                    int n = e->getClicked(size, localMouse);
                    if (n >= 0) {
                        entryHit = e;
                        break;
                    }
                }
                if (!entryHit) {
                    //TODO: maybe defer closing for usability
                    closeAllSubmenus();
                }

                auto ctxtEntry = dynamic_cast<ctxtmenu_entry*>(entryHit);
                if (ctxtEntry && (ctxtEntry->id & 1)) {
                    auto entry = dynamic_cast<ctxtmenu_entry_folder*>(entryHit);
                    if (!entry->isMenuOpen()) {
                        //close other submenu at same level
                        closeAllSubmenus();

                        //and open new one
                        guictxtmenu_base* popup = nullptr;
                        popup                   = new guidropdown_select_preset_ctxt(plugin, presetManager, entry->getPath(), lvl + 1);
                        dbgassert(popup);
                        if (popup) {
                            entry->setIsMenuOpen(true);
                            popup->size = size;
                            popup->setFontSize(entry->fontSize);
                            popup->size.x               = math::max(CONTEXT_MENU_MIN_WIDTH, popup->size.x);
                            auto appCtrlParent          = parentCtrl->getParentCtrl();
                            ivec2 screenPosParentParent = appCtrlParent->toScreenSpace(ivec2(0, 0));
                            ivec2 screenPosParent       = parentCtrl->toScreenSpace(toScreenSpace(ivec2(right() + 2, top() + entryHit->y)));
                            appCtrlParent->openAppMenu(lvl + 1, popup, screenPosParent - screenPosParentParent - popup->pos + ivec2(1));
                        }
                    }
                }
                for (guibase* gui : guis) {
                    if (!gui->isVisible())
                        continue;
                    if (gui->mouseHitTest(localMouse, evt)) {
                        return true;
                    }
                }
                if (canMouseHit()) {
                    evt.requestFocus(this);
                    return true;
                }
            }
            return false;
        }
    };

    class guidropdown_select_preset : public guidropdownbase {
        PluginVST2_Synth* const plugin;
        PresetManager presetManager;

    public:
        explicit guidropdown_select_preset(PluginVST2_Synth* plugin)
            : guidropdownbase(),
              plugin(plugin),
              presetManager(plugin->getSynth()->getPresetManager()) {
        }
        String getString() override {
            return plugin->getSynth()->getPreset().name;
        }
        void handleDraggedRelease(MouseEvent& evt) override {
            presetManager.reload();
            auto* popup         = new guidropdown_select_preset_ctxt(plugin, presetManager, presetManager.getPresetPath());
            popup->size         = size;
            auto fontSizeScaled = math::clamp(size.y, 4, 48) * FONT_AUTOSCALE;
            popup->setFontSize(fontSizeScaled);
            popup->size.x      = math::max(CONTEXT_MENU_MIN_WIDTH, popup->size.x);
            auto appCtrlParent = parentCtrl;
            appCtrlParent->openAppMenu(0, popup, toScreenSpace(ivec2(0, size.y)) - popup->pos + ivec2(1));
        }
    };
    class guicontainer_plugin_synth_voicestates : public guictr_base {
        SynthImpl* const synth;
        SynthImpl::VoiceList list{};
    public:
        explicit guicontainer_plugin_synth_voicestates(SynthImpl* synth)
            : guictr_base(),
              synth(synth)
        {
            setBackgroundRendered(true);
            padding = 2;
            margin  = 0;
        }

        void onTick(AppCtrl* ctrl) override {
            if (dawCtrl) {
                auto lock = dawCtrl->getDaw()->getPlayThread()->tryLockThread();
                if (lock.isLocked()) {
                    list = synth->getVoiceListPrev();
                }
            }
        }
        void render(NVGcontext* vg) override {
            guictr_base::render(vg);
            auto cs = getSizeContent();
            auto voiceWidth = math::max(2.5f, float(cs.x) / NUM_POLY_VOICES);
            int inset = padding;
            auto voicePos = vec2(inset);
            auto voiceSize = vec2(voiceWidth, cs.y - inset * 2);
            NVGpaint paint{};
            paint.image      = -1;
            paint.customPar  = 1;
            paint.outerColor = paint.innerColor = theme->getColor(GuiColor::COL_NOTE_MUTE);
            paint.outerColor.r = 0.12f;
            paint.innerColor.a = 0.12f;
            int numBatched = NUM_POLY_VOICES;
            for (int polyIndex = 0; polyIndex < NUM_POLY_VOICES; ++polyIndex) {
                nvgBatchedRect(vg, voicePos.x, voicePos.y, voiceSize.x-2, voiceSize.y);
                voicePos.x += voiceSize.x;    
            }
            if (numBatched > 0) {
                nvgFillPaint(vg, paint);
                nvgBatchedRender(vg);
            }
            numBatched = list.numPolyVoices;
            for (int i = 0; i < list.numPolyVoices; ++i) {
                int polyIndex = list.polyVoices[i];
                voicePos.x = inset + polyIndex * voiceSize.x;
                nvgBatchedRect(vg, voicePos.x, voicePos.y, voiceSize.x-2, voiceSize.y);
            }
            paint.outerColor = paint.innerColor = theme->getColor(GuiColor::COL_NOTE_PLAYING);
            if (numBatched > 0) {
                nvgFillPaint(vg, paint);
                nvgBatchedRender(vg);
            }
        }
    };
    class guicontainer_plugin_synth_header : public guictr_base {
        PluginVST2_Synth* const plugin;
        guicontainer_plugin_synth_voicestates voiceStates;
        guidropdown_select_preset selectPreset;
        guibutton prev;
        guibutton next;

    public:
        explicit guicontainer_plugin_synth_header(PluginVST2_Synth* plugin)
            : guictr_base(),
              plugin(plugin),
              voiceStates(plugin->getSynth()),
              selectPreset(plugin)
        {
            padding = 0;
            margin  = 0;
            prev.setText("<");
            next.setText(">");
            add(&voiceStates);
            add(&selectPreset);
            add(&prev);
            add(&next);
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
                auto& prMgr     = plugin->getSynth()->getPresetManager();
                auto& presets   = prMgr.getPresets();
                auto& curPreset = plugin->getSynth()->getPreset();
                size_t i        = 0;
                for (; i < presets.size(); ++i) {
                    if (presets[i].path == curPreset.path) {
                        break;
                    }
                }
                size_t nextIdx = (i + dir + presets.size()) % presets.size();
                if (nextIdx < presets.size()) {
                    ::ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
                    plugin->loadPreset(presets[nextIdx].path);
                }
            }
        }
        ~guicontainer_plugin_synth_header() override {
            removeGuis();
        }
        void layout() override {
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
    class guicontainer_plugin_synth : public guictr_base {
        guicontainer_plugin_synth_editor editor;
        guicontainer_plugin_synth_preset_browser browser;
        guicontainer_plugin_synth_header header;

    public:
        explicit guicontainer_plugin_synth(PluginVST2_Synth* plugin)
            : editor(plugin), browser(plugin), header(plugin) {
            padding = 0;
            margin  = 0;
            setBackgroundRendered(false);
            add(&header);
            add(&editor);
            add(&browser);
            browser.setVisible(false);
        }
        ~guicontainer_plugin_synth() override {
            remove(&browser);
            remove(&editor);
            add(&header);
        }
        void layout() override {
            header.size.y = math::roundfS32(getLayoutHeight(this));
            editor.pos.y    = header.size.y;
            browser.pos.y   = header.size.y;
            auto cs         = size;
            header.size.x = cs.x;
            cs.y -= header.size.y;
            editor.size  = cs;
            browser.size = cs;
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
            w = 1280;
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

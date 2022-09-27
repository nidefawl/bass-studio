#include <cmath>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <iterator>
#include <limits>
#include <muParser.h>
#include <nanovg.h>
#include <optional>
#include <utility>
#include <vector>
#include <map>
#include <deque>
#include <memory>
#include <vstsdk-host-2.4/aeffectx.h>
#include "assert_dbg.h"
#include "automation.h"
#include "compiler.h"
#include "config.h"
#include "host/plugin/internal_plugin.h"
#include "host/projectcontroller.h"
#include "seq_time.h"
#include "shape.h"
#include "fileio.h"
#include "gui/contextmenu/contextmenu.h"
#include "gui/contextmenu/contextmenu_base.h"
#include "gui/controls/list.h"
#include "gui/controls/textfield.h"
#include "gui/dropdown/dropdown.h"
#include "gui/dropdown/dropdown_generic.h"
#include "gui/dropdown/dropdown_preset_tree.h"
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
#include "shape.h"
#include "gui/shape/shapeeditor.h"
#include "str_util.h"
#include "dsp_util.h"
#include "color_util.h"
#include "threads/playbackthread.h"
#include "threads/threadlock.h"
#include "tls.h"
#include "util/presetmanager.h"
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
#include "gui/views/notify.h"

#include "basectrl.h"

#include "platform.h"

#include "../plugin.h"
#include "synth-plugin.h"
#include "synth-snapshot.h"
#include "plugins/plugin.h"
#include "plugins/plugin-base.h"
#include "plugins/plugin-window.h"
#include "audioblock.h"
#include "midi-defs.h"
#include "IPlugMidi.h"
#include <glm/gtx/fast_exponential.hpp>
#include <vstsdk-plugin-2.4/audioeffectx.h>

#if BUILD_EXTERNAL_PLUGIN
    return PluginSynth::c// TODO: only lock VST2 versions (using same synth-plugin.o)reatePlugin(audioMaster);
AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {
}
#endif


namespace PluginSynth {

    int32_t gDebugOverrides               = -1;
    const char* const PLUGIN_EFFECT_NAME  = "Synth";
    const uint32_t PLUGIN_UID = 1314080845; //"SYNT";
    const char* const PLUGIN_PRODUCT_NAME = "Synth";

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
        Shaper,
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

    const std::array<const char*, 7> stringsWaveform = {
        "Sine", "Triangle", "Saw", "Square", "Pulse", "Shaper", "Noise"
    };
    const std::array<const char*, 2> stringsReset = {
        "Always", "Hold"
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
        DAW::Shape::shape_t* shape{};

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

        void setShape(DAW::Shape::shape_t* _shape) {
            shape = _shape;
        }

        void initPhase(double _phase, double _phaseFade) {
            while (_phase < -1.0) _phase += 1.0;
            phase = _phase;
            dbgassert(phase >= -1.0 && phase <= 3.0);
            if (0.0 == _phase) {
                triCurrent = triLast = 0.0;
            }
            phaseFade = _phaseFade;
        }

        bool Update(double dt, double frequency) {
            phaseIncrement = frequency * dt;
            phase          = fp_math::silenceNanInfd(phase + phaseIncrement);
            bool b         = phase > 1.0;
            while (phase > 1.0) phase -= 1.0;
            dbgassert(!fp_math::isNanOrInfd(phase));
            return b;
        }

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
                case Waveforms::Shaper:
                    dbgassert(shape);
                    return -1.0 + 2.0 *this->shape->sampleCurve(static_cast<float>(phase), false);
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
                case Waveforms::Shaper:
                    dbgassert(shape);
                    return -1.0 + 2.0 *this->shape->sampleCurve(static_cast<float>(phase), false);
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

        bool IsSilent() const { return std::fabs(b) < 1E-12; }

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

        bool IsSilent() const { return std::fabs(low) < 1E-12; }

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

        bool IsSilent() const { return std::fabs(d) < 1E-12; }

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
        bool bTriggerSmoothing = false;
        double prevVolEnv = 0.0;
        double prevCutoff = 0.0;

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
    enum ModulationType {
        Function,
        Constant,
        ModulationSource,
        NumModulationTypes,
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
        "m8",
        "r1"
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

        ~SynthParamBase() override                  = default;
        virtual void set(double f) noexcept         = 0;
        virtual double getAsDouble() const noexcept = 0;
        virtual String getValueDisplay() const      = 0;
        virtual void resetToInitial() noexcept      = 0;
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
        double valInitial = 0.0;
        double fmin      = 0.0;
        double fmax      = 1.0;
        SynthParam_Float* setRange(double _fmin, double _fmax) {
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
        void setInitialValue(double f) {
            double fVal = math::max(0.0, math::min(1.0, (f - fmin) / (fmax - fmin)));
            valDouble = valInitial = fVal;
        }
        void resetToInitial() noexcept override {
            valDouble = valInitial;
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
        double valInitial       = 0.0;
        int32_t iValue          = 0;
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
        void setInitialValue(int32_t i) noexcept {
            iValue   = math::clamp(i, iMin, iMax),
            valFloat = valInitial = math::clamp((iValue - iMin) / static_cast<double>(iMax - iMin), 0.0, 1.0);
        }
        void resetToInitial() noexcept override {
            iValue   = math::clamp(static_cast<int32_t>(valInitial * (iMax - iMin) + iMin), iMin, iMax);
            valFloat = valInitial;
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

    using ModulationSourceData = std::array<double, MathExprInputLen>;
    class PluginLockable {
        DawInstance* const daw;
        std::recursive_mutex m_mutex;
        std::atomic<int32_t> m_lockCount{ 0 };
    public:
        explicit PluginLockable(DawInstance* daw) 
            : daw(daw) {
        }
        ThreadLock lock() {
            if (daw)
                return daw->lockPlayThread();
            return ThreadLock::MakeThreadLock(m_mutex, this->m_lockCount, false);
        }
        ThreadLock lockProcessing() {
            if (daw)
                return ThreadLock::MakeVoidLock();
            return ThreadLock::MakeThreadLock(m_mutex, this->m_lockCount, false);
        }
        ThreadLock tryLock() {
            if (daw)
                return daw->getPlayThread()->tryLockThread();
            return ThreadLock::MakeThreadLock(m_mutex, this->m_lockCount, true);
        }
        virtual ~PluginLockable() = default;
    };
    class module_synth;
    class SynthImpl : public PluginLockable, public SynthState {
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
        friend class module_synth;
        std::vector<SynthParamBase*> vecParams;
        std::vector<Modulation> modulations;
        std::array<double, Parameters::kNumParams> modulationValuesMin{};
        std::array<double, Parameters::kNumParams> modulationValuesMax{};
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
        DAW::Shape::shape_t lfoShape;
    public:
        int32_t activeVoiceCount  = 0;
        int32_t unisonVoiceCount  = 0;
        int32_t maxUnisonVoice    = 0;
        int32_t polyVoiceCount    = 0;
        int32_t minPolyVoiceIndex = 0;
        int32_t maxPolyVoiceIndex = 0;

    private:
        module_synth* const moduleInstance;
        PluginVST2_Synth* const instanceVst2;
        bool bIsInitSamplerate = false;
        PresetManager::Preset currentPreset;
        PresetManager presetManager;
        void resetLfoShape() {
            lfoShape.pts.clear();
            lfoShape.pts.push_back({{ 0.5, 0.5 }, 0.5});
        }
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
            resetLfoShape();
            auto pShape = &lfoShape;
            lfo.setShape(pShape);
            lfo2.setShape(pShape);
            for (size_t i = 0; i < voices.size(); i++) {
                auto& pv = voices[i];
                pv.init(static_cast<int32_t>(i), static_cast<uint64_t>(synthRand.rng_rand()));
                pv.visitVoices([&](Voice& v) {
                    v.osc1a.setShape(pShape);
                    v.osc1b.setShape(pShape);
                    v.osc2a.setShape(pShape);
                    v.osc2b.setShape(pShape);
                    v.oscFm.setShape(pShape);
                    v.lfo1.setShape(pShape);
                    v.lfo2.setShape(pShape);
                });
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

            addFloatParam(Parameters::OscMix)->setRange(0.0, 1.0)->setInitialValue(1.0);
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
            addEnumParam(Parameters::LfoWave)->setStrings(stringsWaveform.begin(), stringsWaveform.end())->setInitialValue(static_cast<int32_t>(Waveforms::Shaper));
            setParamName(getParam(Parameters::LfoWave), "LFO1 Waveform", "LFO1 Waveform", "Waveform");
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
            modulations.emplace_back();
            String defaultPresetPath = App::Platform::toUserdataPath(String("presets/") + PLUGIN_EFFECT_NAME);
            CreateDirectoryIfNotExists(defaultPresetPath);
            presetManager.load(defaultPresetPath);
            setPreset(defaultPresetPath, "Untitled");
        }

    public:
        explicit SynthImpl(PluginVST2_Synth* vst2Plugin)
            : 
            PluginLockable(daw_tls::getTls().dawInstance),
            SynthState(),
            moduleInstance(nullptr),
            instanceVst2(vst2Plugin)
        {
            initImpl();
        }
        explicit SynthImpl(module_synth* module)
            : 
            PluginLockable(daw_tls::getTls().dawInstance),
            SynthState(),
            moduleInstance(module),
            instanceVst2(nullptr)
        {
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
        void setLfoShape(const DAW::Shape::shape_base_t& shape) {
            lfoShape.pts = shape.pts;
            notifyUiChanges();
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

        void processMidiMessages(const std::vector<IMidiMsg>& midiEvents) {
            midiQueue.AddAll(midiEvents);
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
            snapshot.version     = SYNTH_SNAPSHOT_VERSION;
            const auto numParams = CtrSize(vecParams);
            snapshot.params.reserve(numParams);
            for (int32_t i = 0; i < numParams; ++i) {
                dbgassert(vecParams[i]->getAsDouble() >= 0.0 && vecParams[i]->getAsDouble() <= 1.0);
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
            snapshot.shapes.push_back(shape_snapshot_t{ 0, DAW::Shape::shape_preset_t{1, "LFO", lfoShape} });
            return true;
        }

        bool setSnapshot(const snapshot_t& snapshot) {
            if (snapshot.version < 2) {
                dbgassert(0);
                return false;
            }
            const auto numParams = CtrSize(vecParams);
            
            for (auto& param : vecParams) {
                param->resetToInitial();
                dbgassert(param->getAsDouble() >= 0.0 && param->getAsDouble() <= 1.0);
            }
            for (auto& ps : snapshot.params) {
                if (ps.paramIdx >= 0 && ps.paramIdx < numParams) {
                    vecParams[ps.paramIdx]->set(math::clamp(ps.value, 0.0, 1.0));
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
            resetLfoShape();
            for (auto& shape : snapshot.shapes) {
                if (shape.type == 0) {
                    lfoShape.pts = shape.shape.curve.pts;
                } else {
                    dbgassert(0);
                }
            }
            if (lfoShape.pts.size() < 2) {
                lfoShape.pts.push_back({{ 0.5, 0.5 }, 0.5});
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

        int32_t loadPreset(const String& presetPath);
        std::shared_ptr<PluginViewContainers> createViewCtrImpl();
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
            auto holdVolEnv = GetParamEnum(Parameters::VolEnvTriggerMode)->Value() == 1;
            auto holdModEnv = GetParamEnum(Parameters::ModEnvTriggerMode)->Value() == 1;
            auto holdLfo1 = GetParamEnum(Parameters::Lfo1TriggerMode)->Value() == 1;
            auto holdLfo1Ramp = GetParamEnum(Parameters::Lfo1RampTriggerMode)->Value() == 1;
            auto holdOsc1Phase = GetParamEnum(Parameters::Osc1PhaseResetMode)->Value() == 1;
            auto holdOsc2Phase = GetParamEnum(Parameters::Osc2PhaseResetMode)->Value() == 1;
            
            voice.Start(tempo, lfoPhaseDrift);
            dbgassert(voice.numUnisonActive == unisonVoiceCount);
            voice.visitVoices([&](Voice& v) {
                bool isSilent = v.volEnv.stage >= EnvelopeStages::Idle || !v.bIsActive;
                UpdateVoiceEnvelopeModulations(voice, v);
                UpdateVoiceModulations(voice, v, modSrcData);
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
            modSrcData[1 + ModulationSourceType::Lfo1Ramp]         = voice.lfoEnv.value;
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
            // lockProcessing only locks VST2 versions of the plugin
            auto lock = this->lockProcessing();

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
                            if (unisonIndex == 0/*  && uv.seqNr == 1 */) {
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

    class module_synth : public internalplugin {
        SynthImpl* const impl;
        std::vector<SynthParamBase*>& vecParams;
    public:
        using ThreadLock = std::lock_guard<std::recursive_mutex>;
        explicit module_synth(int32_t _projectGlobalId, IHostCallback* _hostCallback)
            : internalplugin("Synth", getModuleType(), _projectGlobalId, _hostCallback),
          impl(new SynthImpl(this)),
          vecParams(impl->vecParams)
        {
            bCanReceiveMidi = true;
            isSynth = true;
            for (const auto& paramEntry : vecParams) {
                int idx = PARAM_ENABLE + 1 + (&paramEntry - &vecParams.front());
                automatable_param_t* regparam = registerParam(idx);
                dbgassert(regparam && regparam->idx > 0);
                regparam->defaultValue = paramEntry->getAsDouble();
                regparam->value = paramEntry->getAsDouble();
                regparam->name  = paramEntry->shortName;
                regparam->unit  = paramEntry->unit;
                switch (paramEntry->type) {
                    case ParamType::FLOAT:
                        break;
                    case ParamType::INT:
                    case ParamType::ENUM:
                        auto paramInt = dynamic_cast<SynthParam_Int*>(paramEntry);
                        dbgassert(paramInt);
                        auto params = paramInt->iMax - paramInt->iMin;
                        regparam->quantizationSteps = params;
                        break;
                }
            }
            impl->init();
        }

        int getModuleType() override { return PLUGIN_TYPE_SYNTH; };
        void getUiSnapshot(snapshot_t& snapshot);
        void setUiSnapshot(snapshot_t& snapshot);

        std::shared_ptr<PluginViewContainers> createViewCtrInternal() override {
            return this->impl->createViewCtrImpl();
        }

        void setSampleFormat(sampleformat_t sampleFormat) override {
            internalplugin::setSampleFormat(sampleFormat);
            this->impl->setSamplerate(sampleFormat.sampleRate);
        }

        param_converted_t convertParamValueDisplay(int32_t idx, const param_unit_t& displayValue) override {
            if (idx > 0 && idx - 1 < CtrSize(vecParams)) {
                SynthParamBase* param = vecParams[idx-1];
                return param->convertValueDisplay(displayValue);
            }
            return internalplugin::convertParamValueDisplay(idx, displayValue);
        }

        param_unit_t convertParamValueToDisplay(int32_t idx, float value) override {
            if (idx > 0 && idx - 1 < CtrSize(vecParams)) {
                SynthParamBase* param = vecParams[idx-1];
                String valDisplay     = param->getValueDisplay();
                return {valDisplay, param->unit};
            }
            return internalplugin::convertParamValueToDisplay(idx, value);
        }

        void postSetParameter(int32_t idx, float preVal, float val, int flags) override {
            if (idx > 0 && idx - 1 < CtrSize(vecParams)) {
                SynthParamBase* param = vecParams[idx-1];
                param->set(getParamValue(idx));
                this->impl->OnParamChange(param->enumParam);
            }
            internalplugin::postSetParameter(idx, preVal, val, flags);
        }

        void processMidiMessages(std::vector<IMidiMsg>& midiEvents) override { 
            this->impl->processMidiMessages(midiEvents);
        }

        void process(AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) override {
            dbgassert(format.sampleRate > 0 && this->impl->oneOverSR == 1.0/format.sampleRate);
            dbgassert(out->channels >= 2);
            dbgassert(out->samples >= numSamples);
            auto ppqPos = tick / double(TICKS_QUARTER);
            auto barStartPos = math::floord(tick / double(TICKS_BAR)) * 4;
            this->impl->setPPQPos(ppqPos);
            this->impl->setBarPos(barStartPos);
            this->impl->setTempo(project_controller_t::get()->getCurrentTempoBPM()); //TODO: use hostCallback or provide time info struct in process() parameter list
            // TODO: transport changes
            // if (timeinfo && timeinfo->flags & kVstTransportChanged) {
            //     this->impl->onTransportChanged(timeinfo->flags & kVstTransportPlaying);
            // }
            out->clear();
            this->impl->ProcessSynth(in->buf, out->buf, numSamples);
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
                getParam(idx + 1)->value = vecParams[idx]->getAsDouble();
            }
        }

        void addPropertiesParameterTooltip(Table::tbl& table, int idx) override {
            if (idx > 0 && idx - 1 < CtrSize(vecParams)) {
                SynthParamBase* param = vecParams[idx-1];
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

        void notifyUiChanges() {
            for (auto& pviewctr : this->views) {
                if (pviewctr->isInUse()) {
                    pviewctr->onSetParameter(-1, 0.0f);
                }
            }
            if (hostCallback) {
                hostCallback->onUiChanged(this);
            }
        }
        SynthParamBase* getSynthParam(Parameters enumParam) {
            return impl->getParam(enumParam);
        }
        SynthImpl* getSynth() {
            return impl;
        }
    };

    PluginVST2_Synth::PluginVST2_Synth(audioMasterCallback audioMaster)
        : BasePluginVST2(audioMaster, PLUGIN_UID, 0, Parameters::kNumParams, kNumInputs, kNumOutputs),
          impl(new SynthImpl(this)),
          vecParams(impl->vecParams) {
        isSynth(true);
        programsAreChunks(true);
        impl->init();
    }

    void PluginVST2_Synth::onPresetLoaded() {
        updateDisplay();
    }

    SynthImpl* PluginVST2_Synth::getSynth() {
        return this->impl;
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
            param->set(value);
            this->impl->OnParamChange(param->enumParam);
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
}// namespace PluginSynth

template<>
effectbase* makeInstance<PluginSynth::module_synth>(int32_t _projectGlobalId, IHostCallback* _hostCallback) {
    return new PluginSynth::module_synth(_projectGlobalId, _hostCallback);
}


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
        float rowHeight = HEIGHT_DEFAULT_INPUT;

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
                        ThreadLock lock = synth->lock();
                        if (idx > 0 && idx-1 < static_cast<int32_t>(sizeof(parametersModulate) / sizeof(parametersModulate[0]))) {
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
                    ThreadLock lock = synth->lock();
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
        float rowHeight = HEIGHT_DEFAULT_INPUT;

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
                        ThreadLock lock = synth->lock();
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
                for (auto& str : stringsModSource) {
                    vecOpts.emplace_back(str);
                }
                dropdownSource.setZOrder(1);
                dropdownSource.setOptions(vecOpts);
                dropdownSource.setLabel(StringFormat("Mod %d Src %d", slotIndex, srcSlotIndex));
                dropdownSource.setCallback([this](int idx, String& value) -> String {
                    if (idx >= 0) {
                        {
                            ThreadLock lock = synth->lock();
                            if (idx < CtrSize(modSrcTypesOrdered)) {
                                synth->setModulationType(slotIndex, srcSlotIndex, modSrcTypesOrdered[idx]);
                            } else {
                                synth->setModulationType(slotIndex, srcSlotIndex, -1);
                            }
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
                        ThreadLock lock = synth->lock();
                        synth->setModulationConstant(slotIndex, srcSlotIndex, value);
                    }
                    if (parent) {
                        parent->buttonClicked(this);
                    }
                };
                inputConstant.fnClamp = [](double value) -> double {
                    return value;
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
                                ThreadLock lock = synth->lock();
                                synth->setModulationFunction(slotIndex, srcSlotIndex, std::move(expr));
                            }
                            textfieldFunction.setLabel(StringFormat("Mod %d Function %d", slotIndex, srcSlotIndex));
                            textfieldFunction.setTextfieldColor(GuiColor::COL_TEXTBOX_TEXT);
                        } catch (mu::Parser::exception_type& e) {
                            log_lf(Log::L_ERROR, "Error in expression: %s\n", e.GetMsg().c_str());
                            textfieldFunction.setLabel(StringFormat("Error in expression: %s", e.GetMsg().c_str()));
                            textfieldFunction.setTextfieldColor(GuiColor::COL_INVALID_INPUT);
                            // auto tooltip       = new gui_notify();
                            // tooltip->setMessage("Failed parsing expression", e.GetMsg());
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
                                ThreadLock lock = synth->lock();
                                MathExpr expr;
                                expr.str        = value;
                                expr.parsedExpr = nullptr;
                                {
                                    ThreadLock lock = synth->lock();
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
                    case ModulationType::ModulationSource: {
                        auto idx = std::find(std::begin(modSrcTypesOrdered), std::end(modSrcTypesOrdered), 2+static_cast<int32_t>(src.src));
                        if (std::end(modSrcTypesOrdered) != idx) {
                            dropdownSource.setSelectedIndex(static_cast<int32_t>(idx - std::begin(modSrcTypesOrdered)));
                        } else {
                            dropdownSource.setSelectedIndex(0);
                        }
                        break;
                    }
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

                    ThreadLock lock = synth->lock();
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
                const auto bgColor = getInnerBackgroundColorFromState(getStateFlags());
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

        virtual void layoutParameterGroup(ivec2& prefSize, vec2 knobSize, float titleHeight) {
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

        void drawBackground(NVGcontext* vg, const guitheme_t* theme, ivec2 posInset, ivec2 sizeInset, int margin, bool drawInset) override {
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
            scrollContainerModulation.maxHeight = size.y;
            scrollContainerModulation.determineSize(scrollContainerModulation.size);
            for (auto* gui : guis) {
                gui->layout();
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
        explicit guiknob_synthparam(int32_t idx, int32_t idxExternal, SynthImpl* _impl, Parameters _param, guiknob::knobtype _knobtype = guiknob::knobtype::KNOB_LABELED)
            : guiknob_pluginparam(idxExternal, idx, _knobtype),
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

    class guictr_synth_param_container : public guictr_synth_title {
        SynthImpl* const synth;
        std::vector<guiknob_synthparam*> knobs;
        vec2 sliderSize{ 0.0f, 0.0f };
    public:
        explicit guictr_synth_param_container(SynthImpl* synth)
            : synth(synth) {
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
                if (c->size.x <= 5 || c->size.y <= 5) {
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

        void layout() override {
            for (guibase* gui : guis) {
                gui->layout();
            }
        }

        void buttonClicked(guibase* button) override {
            parent->buttonClicked(button);
        }

        void layoutParameterGroup(ivec2& prefSize, vec2 knobSize, float titleHeight) override {
            int newPadding = 0;
            while (newPadding < 4 && newPadding * 48 < prefSize.y) {
                newPadding++;
            }
            padding = newPadding;
            this->setTitleHeight(titleHeight);
            auto cs                = getSizeContent();
            const auto knobsPerCol = 3;
            const auto innerSize   = vec2(cs.x, cs.y - titleHeight);
            auto knobPos           = ivec2(0, titleHeight);
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
            vec2 sizeFull = innerSize;
            for (auto knob : guis) {
                if (knob->id == 2) {
                    knob->pos  = vec2(knobPos.x, titleHeight);
                    knob->size = sizeFull;
                    knobPos.x += math::floorfS32(sizeFull.x + padding);
                }
            }
            prefSize.x = math::roundfS32(knobPos.x - padding) + padding * 2;
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
        PluginVST2_Synth* const vst2Instance;
        SynthImpl* const synth;
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
        explicit guicontainer_plugin_synth_editor(effectbase* module, SynthImpl* synth, PluginVST2_Synth* plugin)
            : guictr_base(),
              vst2Instance(plugin),
              synth(synth),
              moduleInstance(module),
              shapeEditor(makeShapeEditor()),
              modulation(synth),
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
            for (auto& row :moduleLayout) {
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

            vecParamUI.resize(Parameters::kNumParams);
            for (auto param : parametersOrdered) {
                auto type = guiknob::knobtype::SLIDER_LABELED;
                if (!stl_contains(parametersModulate, param)) {
                    type = guiknob::knobtype::KNOB_LABELED;
                }
                switch (synth->getParam(param)->getType()) {
                    case ParamType::ENUM:
                    case ParamType::INT:
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
                    case Parameters::LfoWave:
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
                if (moduleInstance) idx++;
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
            shapeEditor->setShapeEditorCallback([synth=this->synth](const DAW::Shape::shape_base_t& shape) -> void {
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
            for (auto& synthKnob : vecParamUI) {
                delete synthKnob.knob;
            }
            for (auto& ctr : containers) {
                delete ctr;
            }
            if (shapeEditor)
                delete shapeEditor->getGuiContainer();
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
                for (auto& synthKnob : vecParamUI) {
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
            auto synthImpl             = this->synth;
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
            dbgassert(moduleInstance);
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

    class guicontainer_plugin_synth_voicestates : public guictr_base {
        SynthImpl* const synth;
        SynthImpl::VoiceList list{};
    public:
        explicit guicontainer_plugin_synth_voicestates(SynthImpl* synth)
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
            guictr_base::render(vg);
            auto cs = getSizeContent();
            auto voiceWidth = math::max(2.5f, float(cs.x) / NUM_POLY_VOICES);
            int inset = padding;
            auto voicePos = vec2(inset);
            auto voiceSize = vec2(voiceWidth, cs.y - inset * 2);
            {
                auto colBg = theme->getColor(GuiColor::COL_NOTE_MUTE);
                colBg.a = 0.12f;
                for (int polyIndex = 0; polyIndex < NUM_POLY_VOICES; ++polyIndex) {
                    nvgBatchedRect(vg, voicePos.x, voicePos.y, voiceSize.x-2, voiceSize.y);
                    voicePos.x += voiceSize.x;    
                }
                NVGpaint paint{};
                paint.image      = -1;
                paint.innerColor = theme->getColor(GuiColor::COL_NOTE_MUTE);
                paint.customPar  = 1;
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
                paint.customPar  = 1;
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

    class guicontainer_plugin_synth_header : public guictr_base {
        SynthImpl* const synth;
        guicontainer_plugin_synth_voicestates voiceStates;
        guidropdown_select_preset selectPreset;
        guibutton prev;
        guibutton next;

    public:
        explicit guicontainer_plugin_synth_header(SynthImpl* _synth)
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
            selectPreset.setCallback([this](const String& path) {
                ThreadLock lock = synth->lock();
                this->synth->loadPreset(path);
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

    class guicontainer_plugin_synth : public guictr_base {
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
        explicit guicontainer_plugin_synth(module_synth* module)
            : editor(module, module->getSynth(), nullptr), header(module->getSynth()) {
            padding = 0;
            margin  = 0;
            setBackgroundRendered(false);
            add(&header);
            add(&editor);
        }

        ~guicontainer_plugin_synth() override {
            remove(&editor);
            add(&header);
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

    std::shared_ptr<PluginViewContainers> PluginVST2_Synth::createViewCtrVst2() {
        return this->impl->createViewCtrImpl();
    }

    class SynthPluginViewCtr : public PluginViewContainers {
    public:
        guicontainer_plugin_synth ctr_main;
        explicit SynthPluginViewCtr(module_synth* eff)
            : ctr_main(eff) {
        }
        explicit SynthPluginViewCtr(PluginVST2_Synth* eff)
            : ctr_main(eff) {
        }
        ~SynthPluginViewCtr() override = default;
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

    void module_synth::getUiSnapshot(snapshot_t& snapshot) {
        for (auto& view : views) {
            auto implCtrType = dynamic_cast<SynthPluginViewCtr*>(view.get());
            ui_layout_t layout{};
            if (implCtrType && implCtrType->getPluginUI().getUiLayout(layout)) {
                layout.uiId = view->getUiId();
                snapshot.uiLayout.push_back(layout);
            }
        }
    }

    void module_synth::setUiSnapshot(snapshot_t& snapshot) {
        for (auto& uis : snapshot.uiLayout) {
            auto view = getViewCtr(uis.uiId);
            if (!view)
                continue;
            auto implCtrType = dynamic_cast<SynthPluginViewCtr*>(view.get());
            if (!implCtrType)
                continue;
            implCtrType->getPluginUI().setUiLayout(uis);
        }
    }

    void PluginVST2_Synth::getUiSnapshot(snapshot_t& snapshot) {
        for (auto& view : views) {
            auto implCtrType = dynamic_cast<SynthPluginViewCtr*>(view.get());
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
            auto implCtrType = dynamic_cast<SynthPluginViewCtr*>(view.get());
            if (!implCtrType)
                continue;
            implCtrType->getPluginUI().setUiLayout(uis);
        }
    }

    int32_t SynthImpl::loadPreset(const String& presetPath) {
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
                    if (instanceVst2) {
                        instanceVst2->setUiSnapshot(snapshotLoaded);
                    }
                    if (moduleInstance) {
                        moduleInstance->setUiSnapshot(snapshotLoaded);
                        moduleInstance->onPresetLoaded();
                    }
                    notifyUiChanges();
                    if (instanceVst2) {
                        instanceVst2->onPresetLoaded();
                    }
                    return 0;
                }
                return -3;
            }
            return -2;
        }
        return -1;
    }

    std::shared_ptr<PluginViewContainers> SynthImpl::createViewCtrImpl() {
        if (this->moduleInstance) {
            this->views.push_back(std::make_shared<SynthPluginViewCtr>(this->moduleInstance));
            return this->views.back();
        }
        if (this->instanceVst2) {
            this->views.push_back(std::make_shared<SynthPluginViewCtr>(this->instanceVst2));
            return this->views.back();
        }
        return nullptr;
    }

}// namespace PluginSynth

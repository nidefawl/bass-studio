#pragma once
#include "host/shape/shape.h"
#include "types.h"
#include <array>

namespace PluginSynth {
    
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
enum class VoiceModesMono : int32_t {
    Mono = 0,
    Legato,
    NumVoiceModesMono
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
                if (a == 0.0 && value == 0.0) {
                    stage = EnvelopeStages::Decay;
                    break;
                }
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

//  http://www.kvraudio.com/forum/viewtopic.php?t=375517
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
            // case Waveforms::Shaper:
            //     dbgassert(shape);
            //     return -1.0 + 2.0 *this->shape->sampleCurve(static_cast<float>(phase), false);
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
            // case Waveforms::Shaper:
            //     dbgassert(shape);
            //     return -1.0 + 2.0 *this->shape->sampleCurve(static_cast<float>(phase), false);
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
    double GetWaveformShaper(double dt, double frequency, bool bleb) {
        phaseIncrement = frequency * dt;
        phase          = fp_math::silenceNanInfd(phase + phaseIncrement);
        while (phase > 1.0) phase -= 1.0;
        return -1.0 + 2.0 *this->shape->sampleCurve(static_cast<float>(phase), false);
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

    double GetLfo(double dt, double frequency, bool oneShot) {
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
        // double dSwitchVal = waveform.getSwitchValue();
        // dbgassert(!fp_math::isNanOrInfd(dSwitchVal));
        // auto roundedVal = math::rounddU32(dSwitchVal);
        // dbgassert(roundedVal < static_cast<uint32_t>(Waveforms::NumWaveforms));
        // double v = GetLfoWaveform(static_cast<Waveforms>(roundedVal), phase > 0.5 && oneShot, p);
        // dbgassert(v >= -1.0 && v <= 1.0);
        // return v;
        return -1.0 + 2.0 *this->shape->sampleCurve(static_cast<float>(phase), false);
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

} // namespace PluginSynth
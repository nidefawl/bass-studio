#pragma once
#include "host/shape/shape.h"
#include "math/seq_math.h"
#include "math/simd_math.h"
#include "plugins/lfo/lfo-types.hpp"
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
    Triggered = 0,
    Attack,
    Hold,
    Decay,
    Sustain,
    Release,
    Idle,
};
enum class EnvelopeShaping : int32_t {
    Linear = 0,
    Pow,
    Exp,
};

struct Envelope {
    static constexpr double OFF_STATE = 0.02;
    static constexpr double MIN_SECONDS = 0.0001;
    static constexpr double MAX_SECONDS = 1.0;
    static constexpr double GetTimeBaseFromParam(double p, double dmin = Envelope::MIN_SECONDS, double dmax = Envelope::MAX_SECONDS) {
        if (p <= OFF_STATE)
            return 0.0;
        p = (p - OFF_STATE) / (1.0 - OFF_STATE);
        return 1.0 / (p * (dmax - dmin) + dmin);
    }
    static constexpr double GetSecondsFromParam(double p, double dmin = Envelope::MIN_SECONDS, double dmax = Envelope::MAX_SECONDS) {
        if (p <= OFF_STATE)
            return 0.0;
        p = (p - OFF_STATE) / (1.0 - OFF_STATE);
        return p * (dmax - dmin) + dmin;
    }
    static constexpr double GetParamFromTimeMillis(double tMillis, double dmin = Envelope::MIN_SECONDS, double dmax = Envelope::MAX_SECONDS) {
        if (tMillis <= 0.0)
            return 0.0;
        double t = tMillis / 1000.0;
        return math::clamp((t - dmin) / (dmax - dmin), 0.0, 1.0) * (1.0 - OFF_STATE) + OFF_STATE;
    }

    double phase    = 0.0;
    double value    = 0.0;
    double relValue = 0.0;
    double a = 0.0;
    double h = 0.0;
    double d = 0.5;
    double s = 1.0;
    double r = 0.5;
    std::array<double, 3> shapes = {0.75, 0.35, 0.25};
    EnvelopeStages stage = EnvelopeStages::Idle;
    EnvelopeShaping shaping = EnvelopeShaping::Pow;

    bool IsReleased() const { return stage == EnvelopeStages::Release || stage == EnvelopeStages::Idle; }

    void Reset() { value = 0.0; }
    void Start() { stage = EnvelopeStages::Triggered; }
    void Release() { 
        if (stage >= EnvelopeStages::Release) return;
        stage = EnvelopeStages::Release;
        relValue = value;
        phase = 0.0; 
    }

    bool IsSustain() const {
        return stage == EnvelopeStages::Sustain;
    }
    bool IsIdle() const { return stage == EnvelopeStages::Idle; }

    double shapeSegmentExp(double x, double shape) {
        double shapeBi = 1.0 - shape * 2.0;
        return exp((1.0 - x) * shapeBi) * x;
    }
    // does not sound clean enough (noticable in short attack phase)
    double shapeSegmentPow(double x, double shape) {
        double shapeBi  = 1.0 - shape * 2.0;
        double shapeBiAbs = fabs(shapeBi);
        if (shapeBiAbs != 0.0) {
            double shapeExp = 0.0;
            double scale2   = 0.2 + x * 0.8;
            if (shapeBi < 0.0) {
                shapeExp = 1.0 + scale2 * shapeBiAbs * 16.0;
            } else {
                shapeExp = 1.0 / (1.0 + scale2 * shapeBiAbs * 16.0);
            }
            return pow(x, shapeExp);
        }
        return x;
    }
    double shapeSegment(double x, double shape) {
        switch (shaping) {
            case EnvelopeShaping::Exp:
                return shapeSegmentExp(x, shape);
            case EnvelopeShaping::Pow:
                return shapeSegmentPow(x, shape);
            case EnvelopeShaping::Linear:
            default:
                return x;
        }
    }
    double clampDuration(double f) {
        return math::clamp(f, 1.0 / 100.0, 1.0 / MIN_SECONDS);
    }
    float getEnvDurationSeconds(EnvelopeStages stage) {
        switch (stage) {
            case EnvelopeStages::Attack:
            case EnvelopeStages::Triggered:
                return float(1.0 / clampDuration(a));
            case EnvelopeStages::Hold:
                return float(1.0 / clampDuration(h));
            case EnvelopeStages::Decay:
                return float(1.0 / clampDuration(d));
            case EnvelopeStages::Sustain:
                return 0.0;
            case EnvelopeStages::Release:
                return float(1.0 / clampDuration(r));
            default:
                break;
        }
        return 0.0;
    }
    void Update(double dt) {
        switch (stage) {
            case EnvelopeStages::Triggered:
                stage = EnvelopeStages::Attack;
                phase = 0.0;
                /* fallthrough */
            case EnvelopeStages::Attack:
                // minimum att time: 1 sample
                if (a == 0.0 || phase >= 1.0) {
                    value = 1.0;
                    phase = 0.0;
                    stage = EnvelopeStages::Hold;
                } else {
                    value = shapeSegment(phase, shapes[0]);
                    phase += clampDuration(a) * dt;
                }
                break;
            case EnvelopeStages::Hold:
                // minimum hold time: 1 sample
                if (h == 0.0 || phase >= 1.0) {
                    // value = s;
                    phase = 0.0;
                    stage = EnvelopeStages::Decay;
                } else {
                    // value = 1.0;
                    phase += clampDuration(h) * dt;
                }
                break;
            case EnvelopeStages::Decay:
                // minimum dec time: 1 sample
                if (d == 0.0 || phase >= 1.0) {
                    value = s;
                    phase = 0.0;
                    stage = EnvelopeStages::Sustain;
                } else {
                    value = 1.0 - shapeSegment(phase, shapes[1]) * (1.0 - s);
                    phase += clampDuration(d) * dt;
                }
                break;
            case EnvelopeStages::Sustain:
                value = value + (s - value) * dt * (1.0 / 0.01);
                break;
            case EnvelopeStages::Release:
                // minimum rel time: 1 sample
                if (r == 0.0 || phase >= 1.0) {
                    value = 0.0;
                    phase = 0.0;
                    stage = EnvelopeStages::Idle;
                } else {
                    value = shapeSegment(1.0 - phase, shapes[2]) * relValue;
                    phase += clampDuration(r) * dt;
                }
                break;
            default:
                break;
        }
    }
};

struct EnvelopeAD {
    double attack = 0.0;
    double decay = 0.5;

    EnvelopeStages stage = EnvelopeStages::Idle;
    double value         = 0.0;

    bool IsReleased() const { return stage == EnvelopeStages::Release || stage == EnvelopeStages::Idle; }

    void Reset() { value = 0.0; }
    void Start() { stage = EnvelopeStages::Triggered; }
    void Release() { stage = EnvelopeStages::Release; }

    bool IsSustain() const {
        return stage == EnvelopeStages::Decay;
    }
    bool IsIdle() const { return stage == EnvelopeStages::Idle; }

    void Update(double dt) {
        switch (stage) {
            case EnvelopeStages::Triggered:
                stage = EnvelopeStages::Attack;
                break;
            case EnvelopeStages::Attack:
                // if (a == 0.0 && value == 0.0) {
                //     stage = EnvelopeStages::Decay;
                //     break;
                // }
                value += (1.1 - value) * math::max(attack, 0.01/1000.0) * dt;
                if (value >= 1.0) {
                    value = 1.0;
                    stage = EnvelopeStages::Hold;
                }
                break;
            case EnvelopeStages::Hold:
                // TODO: implement hold
                stage = EnvelopeStages::Decay;
                break;
            case EnvelopeStages::Decay:
                break;
            case EnvelopeStages::Release:
                value += (-.1 - value) * math::max(decay, 0.01/1000.0) * dt;
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

inline double noteToLinearScale(double note, double minNote = 69.0) {
    return exp(0.69314718055994530942 * ((note - minNote) / 12.0));
    // return pow(2.0, (note - minNote) / 12.0);
}

template<typename FPType, size_t LEN_SIMD = 8>
inline void ShapeLogLikeSIMD(const FPType valsIn[LEN_SIMD], FPType valsOut[LEN_SIMD], FPType exponent = 0.75f) {
    using Vec4D      = glm::vec<4, FPType, glm::aligned_highp>;
    auto sse8Float   = reinterpret_cast<const __m256*>(&valsIn[0]);
   __m256 sse8Float2 = math::simd::log_v8f(*sse8Float);
    auto pIn         = reinterpret_cast<FPType*>(&sse8Float2);
    FPType* pDataOut = &valsOut[0];
    for (size_t j = 0; j < LEN_SIMD; j += 4) {
        Vec4D& valsRef = *reinterpret_cast<Vec4D*>(&pIn[0]);
        auto vals      = valsRef * exponent;
        auto sse4Float = reinterpret_cast<__m128*>(&vals);
        *sse4Float     = math::simd::exp_v4f(*sse4Float);
        auto floatPtr  = reinterpret_cast<FPType*>(&vals[0]);
        math::simd::cos_test<FPType, 4>(floatPtr, pDataOut);
        for (size_t k = 0; k < 4; k++) {
            pDataOut[k] = (.5f - .5f * pDataOut[k]);
        }
        pIn += 4;
        pDataOut += 4;
    }
}

} // namespace PluginSynth

namespace DAW::LFO {

struct LFOParameters  : public ::DAW::LFO::LFOSyncParameters {
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
            auto index = math::clamp<int32_t>(math::floordS32(d * CtrSize(syncRatios)), 0, CtrSize(syncRatios) - 1);
            return double(syncRatios[index].denominator * bpm) / double(syncRatios[index].numerator * 60.0 * 4.0);
        }
    }
};
class LFO final : public DAW::LFO::LFORateMinMaxAutomation {
    double phase          = 0.0;
    DAW::LFO::lfo_automation_src_synced_t srcSync;
    std::shared_ptr<DAW::LFO::lfo_automation_src_random_t> srcRand;
    LFOParameters* params{};
    PluginSynth::Envelope envRamp;
    uint64_t customSeed = 0;
public:
    LFO() {
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
    const DAW::LFO::lfo_automation_src_random_t* getSourceRand() const {
        return srcRand.get();
    }
    const DAW::LFO::lfo_automation_src_synced_t* getSourceSync() const {
        return &srcSync;
    }
    void setRandomMode(int32_t mode) {
        if (mode != -1 && srcRand && srcRand->getModeId() == mode) {
            return;
        }
        using namespace DAW::LFO;
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
        envRamp.a = PluginSynth::Envelope::GetTimeBaseFromParam(params->rampDuration);
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
            double _unused = 0.0;
            p = std::modf(p, &_unused); 
        }
        return params->shape.sampleCurve(float(p), false);
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
    float getScaledRate(DAW::LFO::LFOSyncParameters* sync, float rate) override { 
        return float(1.0 / params->paramToFreqHz(rate));
    };
};
}
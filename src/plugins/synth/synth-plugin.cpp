#include <math.h>
#include <algorithm>
#include <stdio.h>
#include <vector>
#include <deque>
#include <memory>
#include "config.h"
#include "math/seq_math.h"
#include "str_util.h"
#include "dsp_util.h"
#include "color_util.h"

#include "gui/gui.h"
#include "gui/guicontainer.h"
#include "gui/pluginviewcontainers.h"
#include "gui/button.h"
#include "gui/knob.h"
#include "gui/guiinputfield.h"
#include "gui/knobpluginparam.h"
#include "gui/guicontainer.h"
#include "gui/guicontextmenu_daw.h"

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
//#include "midi-msg.h"
#include "midi-defs.h"
#include "IPlugMidi.h"

#define PLUGIN_EFFECT_NAME "Synth"
#define PLUGIN_UID "SYNT"
#define PLUGIN_PRODUCT_NAME "Synth VST2.4"

#if BUILD_EXTERNAL_PLUGIN
#define MAX_PARAM_STR_LEN kVstMaxParamStrLen
AudioEffect* createEffectInstance (audioMasterCallback audioMaster)
{
	return PluginSynth::createPlugin (audioMaster);
}
#else

#endif

namespace PluginSynth {

enum class Waveforms : int32_t
{
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

struct SmoothSwitch
{
	double current = -1.0;
	double previous = -1.0;
	double mix = 1.0;
	bool switching = false;

	void Update(double dt)
	{
		if (switching) {
			mix += (1.0 - mix) * 100.0 * dt;
			if (mix > .999) {
				mix = 1.0;
				switching = false;
			}
		}
	}

	void Switch(double value)
	{
		if (current == value) return;
		previous = current;
		current = value;
		mix = 0.0;
		switching = true;
	}
};
enum class EnvelopeStages : int32_t
{
	Attack = 0,
	Decay,
	Release,
	Idle,
};

struct Envelope
{
	// these defaults are used for the lfo delay envelope
	double a = 0.0;
	double d = 0.5;
	double s = 1.0;
	double r = 0.5;

	EnvelopeStages stage = EnvelopeStages::Idle;
	double value = 0.0;

	bool IsReleased() { return stage == EnvelopeStages::Release || stage == EnvelopeStages::Idle; }

	void Reset() { value = 0.0; }
	void Start() { stage = EnvelopeStages::Attack; }
	void Release() { stage = EnvelopeStages::Release; }

	void Update(double dt)
	{
		switch (stage)
		{
		case EnvelopeStages::Attack:
			value += (1.1 - value) * a * dt;
			if (value >= 1.0)
			{
				value = 1.0;
				stage = EnvelopeStages::Decay;
			}
			break;
		case EnvelopeStages::Decay:
			value += (s - value) * d * dt;
			break;
		case EnvelopeStages::Release:
			value += (-.1 - value) * r * dt;
			if (value <= 0.0)
			{
				value = 0.0;
				stage = EnvelopeStages::Idle;
			}
			break;
		default:
			break;
		}
	}
};
struct Oscillator
{
	double phase = 0.0;
	double phaseIncrement = 0.0;
	double triCurrent = 0.0;
	double triLast = 0.0;
	double noiseValue = 19.1919191919191919191919191919191919191919;

	// waveform generation //

	// http://www.kvraudio.com/forum/viewtopic.php?t=375517
	inline double Blep(double phase, double phaseIncrement)
	{
		if (phase < phaseIncrement)
		{
			phase /= phaseIncrement;
			return phase + phase - phase * phase - 1.0;
		}
		else if (phase > 1.0 - phaseIncrement)
		{
			phase = (phase - 1.0) / phaseIncrement;
			return phase * phase + phase + phase + 1.0;
		}
		return 0.0;
	}

	inline double GeneratePulse(double phase, double phaseIncrement, double width)
	{
		double v = phase < width ? 1.0 : -1.0;
		v += Blep(phase, phaseIncrement);
		v -= Blep(fmod(phase + (1.0 - width), 1.0), phaseIncrement);
		return v;
	}
	double GetWaveform(Waveforms waveform)
	{
		switch (waveform)
		{
		case Waveforms::Sine:
			return sin(phase * M_PI*2.0);
		case Waveforms::Triangle:
			triLast = triCurrent;
			triCurrent = phaseIncrement * GeneratePulse(phase, phaseIncrement, .5) + (1.0 - phaseIncrement) * triLast;
			return triCurrent * 5.0;
		case Waveforms::Saw:
			return 1.0 - 2.0 * phase + Blep(phase, phaseIncrement);
			break;
		case Waveforms::Square:
			return GeneratePulse(phase, phaseIncrement, .5);
		case Waveforms::Pulse:
			return GeneratePulse(phase, phaseIncrement, .75);
		case Waveforms::Noise:
			// Ove Karlsen's noise algorithm
			// http://musicdsp.org/showArchiveComment.php?ArchiveID=217
			noiseValue += 19.0;
			noiseValue *= noiseValue;
			noiseValue -= (int)noiseValue;
			return noiseValue - .5;
		default:
			break;
		}
		return 0;
	}

	double Get(double dt, double frequency)
	{
		phaseIncrement = frequency * dt;
		phase += phaseIncrement;
		while (phase > 1.0) phase -= 1.0;
		return GetWaveform(Waveforms::Sine);
	}

	double Get(double dt, SmoothSwitch &waveform, double frequency)
	{
		phaseIncrement = frequency * dt;
		phase += phaseIncrement;
		while (phase > 1.0) phase -= 1.0;

		if (waveform.switching)
		{
			auto out = 0.0;
			out += (1.0 - waveform.mix) * GetWaveform((Waveforms)(int)waveform.previous);
			out += waveform.mix * GetWaveform((Waveforms)(int)waveform.current);
			return out;
		}
		return GetWaveform((Waveforms)(int)waveform.current);
	}
};

enum class FilterModes : int32_t
{
	Off = 0,
	TwoPole,
	Svf,
	FourPole,
	NumFilterModes
};
std::vector<String> stringsFilterMode = {
	"Off", "TwoPole", "Svf", "FourPole"
};

// fast trig //
inline double fastAtan(double x) { return x / (1.0 + .28 * (x * x)); }
struct TwoPoleFilter
{
	double a = 0.0;
	double b = 0.0;

	void Reset()
	{
		a = 0.0;
		b = 0.0;
	}

	bool IsSilent() { return b == 0.0; }

	double Process(double dt, double input, double cutoff, double resonance)
	{
		// f calculation
		auto f = 2 * sin(M_PI * cutoff * dt);
		f = f > .99 ? .99 : f < .01 ? .01 : f;

		// feedback calculation
		auto feedback = resonance + resonance / (1.0 - f);
		feedback = fastAtan(feedback * .1) * 10.0;

		// main processing
		a += f * (input - a + feedback * (a - b));
		a = fastAtan(a * .1) * 10.0;
		b += f * (a - b);
		b = fastAtan(b * .1) * 10.0;

		return b;
	}
};

struct StateVariableFilter
{
	double band = 0.0;
	double low = 0.0;

	void Reset()
	{
		band = 0.0;
		low = 0.0;
	}

	bool IsSilent() { return low == 0.0; }

	double Process(double dt, double input, double cutoff, double resonance)
	{
		// f calculation
		auto f = 2 * sin(M_PI * cutoff * dt);
		f = f > 1.0 ? 1.0 : f < .01 ? .01 : f;

		// resonance rolloff
		auto maxResonance = 1.0 - f * f * f * f * f;
		resonance = resonance > maxResonance ? maxResonance : resonance;

		// main processing
		auto high = input - (low + band * (1.0 - resonance));
		band += f * high;
		low += f * band;
		low = fastAtan(low * .1) * 10.0;

		return low;
	}
};

struct FourPoleFilter
{
	double a = 0.0;
	double b = 0.0;
	double c = 0.0;
	double d = 0.0;

	void Reset()
	{
		a = 0.0;
		b = 0.0;
		c = 0.0;
		d = 0.0;
	}

	bool IsSilent() { return d == 0.0; }

	double Process(double dt, double input, double cutoff, double resonance)
	{
		// f calculation
		auto f = 2 * sin(M_PI * cutoff * dt);
		f = f > .99 ? .99 : f < .01 ? .01 : f;

		// feedback calculation
		auto feedback = resonance + resonance / (1.0 - f);
		feedback = fastAtan(feedback * .1) * 10.0;

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

struct Filter
{
	TwoPoleFilter twoPoleFilter;
	StateVariableFilter stateVariableFilter;
	FourPoleFilter fourPoleFilter;

	void Reset()
	{
		twoPoleFilter.Reset();
		stateVariableFilter.Reset();
		fourPoleFilter.Reset();
	}

	bool IsSilentIndividual(FilterModes mode)
	{
		switch (mode)
		{
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

	bool IsSilent(SmoothSwitch mode)
	{
		if (mode.switching)
		{
			return IsSilentIndividual((FilterModes)(int)mode.previous) && IsSilentIndividual((FilterModes)(int)mode.current);
		}
		return IsSilentIndividual((FilterModes)(int)mode.current);
	}

	double ProcessIndividual(double dt, double input, FilterModes mode, double cutoff, double resonance)
	{
		switch (mode)
		{
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

	double Process(double dt, double input, SmoothSwitch mode, double cutoff, double resonance)
	{
		if (mode.switching)
		{
			auto out = 0.0;
			out += (1.0 - mode.mix) * ProcessIndividual(dt, input, (FilterModes) (int) mode.previous, cutoff, resonance);
			out += mode.mix * ProcessIndividual(dt, input, (FilterModes) (int) mode.current, cutoff, resonance);
			return out;
		}
		return ProcessIndividual(dt, input, (FilterModes)(int)mode.current, cutoff, resonance);
	}
};
enum class VoiceModes : int32_t
{
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
struct Voice
{
	Envelope volEnv;
	Envelope modEnv;
	Envelope lfoEnv;
	int note = 0;
	double targetFrequency = 0.0;
	double frequency = 0.0;
	double velocity = 0.0;
	double pitchBend = 1.0;
	Oscillator oscFm;
	Oscillator osc1a;
	Oscillator osc1b;
	Oscillator osc2a;
	Oscillator osc2b;
	Filter filter;

	bool IsReleased() { return volEnv.IsReleased(); }
	double GetVolume() { return volEnv.value; }

	void Reset(bool osc1OutOfPhase, bool osc2OutOfPhase)
	{
		oscFm.phase = 0.0;
		osc1a.phase = 0.0;
		osc1b.phase = osc1OutOfPhase ? .33 : 0.0;
		osc2a.phase = 0.0;
		osc2b.phase = osc2OutOfPhase ? .33 : 0.0;
		volEnv.Reset();
		modEnv.Reset();
		lfoEnv.Reset();
		filter.Reset();
	}

	void Release()
	{
		volEnv.Release();
		modEnv.Release();
		lfoEnv.Release();
	}

	void SetNote(int n)
	{
		note = n;
		targetFrequency = pitchToFrequency(note);
	}

	void SetPitchBendFactor(double f) { pitchBend = f; }

	void ResetPitch() { frequency = targetFrequency; }

	void SetVelocity(double v) { velocity = v; }

	void Start(bool osc1OutOfPhase, bool osc2OutOfPhase)
	{
		if (volEnv.stage == EnvelopeStages::Idle)
			Reset(osc1OutOfPhase, osc2OutOfPhase);
		volEnv.Start();
		modEnv.Start();
		lfoEnv.Start();
	}
};

enum class FmModes : int32_t
{
	Off = 0,
	Osc1,
	Osc2,
	NumFmModes
};
std::vector<String> stringsFMMode {
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
	SynthParamBase(ParamType _type, Parameters _enumParam) :
			type(_type), enumParam(_enumParam) {

	}
	ParamType getType() {
		return this->type;
	}
	virtual ~SynthParamBase() = default;
	virtual void set(float f) {
	}
	virtual float getAsFloat() {
		return 0.0f;
	}
	virtual void getValueDisplay(char* _out) {
		vst_strncpy(_out, "", MAX_PARAM_STR_LEN);
	}
	virtual void getLabel(char* _out) {
		vst_strncpy(_out, "", MAX_PARAM_STR_LEN);
	}
};
struct SynthParam_Float: public SynthParamBase {
	SynthParam_Float(Parameters _enumParam) :
			SynthParamBase(ParamType::FLOAT, _enumParam) {

	}
	double valDouble = 0.0;
	double fmin = 0.0;
	double fmax = 1.0;
	SynthParam_Float* setRange(float _fmin, float _fmax) {
		fmin = _fmin;
		fmax = _fmax;
		return this;
	}
	double Value() {
		return math::max(fmin, math::min(fmax, (valDouble) * (fmax-fmin) + fmin));
	}
	void setRangedValue(double f) {
		double fVal = math::max(0.0, math::min(1.0, (f-fmin)/(fmax-fmin)));
		valDouble = fVal;
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
	void getValueDisplay(char* _out) override {
		snprintf(_out, MAX_PARAM_STR_LEN, StringAsCStr(format), Value());
	}
};
struct SynthParam_Int : public SynthParamBase {
	SynthParam_Int(Parameters _enumParam) :
			SynthParamBase(ParamType::INT, _enumParam) {

	}
	SynthParam_Int(ParamType _paramType, Parameters _enumParam) :
			SynthParamBase(_paramType, _enumParam) {

	}
	float valFloat = 0.0f;
	int32_t iValue = 0;
	int32_t iMin = 0;
	int32_t iMax = 1;
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
		iValue = math::max(iMin, math::min(iMax, (int32_t) math::round(f*(iMax-iMin)+iMin)));
		setRangedValue(iValue);
	}
	void setRangedValue(int32_t i) {
		iValue = math::max(iMin, math::min(iMax, i));
		this->valFloat = math::max(0.0, math::min(1.0, (iValue-iMin)/(double) (iMax-iMin)));
	}
	void getValueDisplay(char* _out) override {

		snprintf(_out, MAX_PARAM_STR_LEN, StringAsCStr(format), Value());
	}
};
struct SynthParam_Enum : public SynthParam_Int {
	SynthParam_Enum(Parameters _enumParam) :
		SynthParam_Int(ParamType::ENUM, _enumParam) {
	}
	std::vector<String> strings;
	SynthParam_Enum* setStrings(std::vector<String> strings) {
		this->strings = strings;
		this->iMax = strings.size() - 1;
		return this;
	}
	void getValueDisplay(char* _out) override {
		String s;
		int val = this->Value();
		if (val >= 0 && val < strings.size()) {
			s = strings[val];
		}
		snprintf(_out, MAX_PARAM_STR_LEN, "%s", StringAsCStr(s));
	}
	template<typename T>
	T getEnumValue() {
		return (T) Value();
	}
};
void setParamName(SynthParamBase* p, String name, String shortName, String format) {
	p->name = name;
	p->shortName = shortName;
	p->format = format;
}
const int numVoices = 8;
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
	Oscillator lfo;
	SmoothSwitch osc1Wave;
	SmoothSwitch osc2Wave;
	SmoothSwitch filterMode;
	std::array<Voice, numVoices> voices;
	std::vector<int> heldNotes;
	IMidiQueue midiQueue;
	double dt = 1.0 / 44100.0;
	seq_rand synthRand;
	PluginVST2_Synth* instanceVst2 = nullptr;
	double lastCutoffModulated = 0.0;
public:
	SynthImpl() : SynthState() {
		auto now = getTimeMillis();
		synthRand.rng_seed(now);
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
private:
	float synthRandom() {
		uint32_t rnd32Bits = synthRand.rng_rand();
		return (rnd32Bits&0xFFFF) / (float)0xFFFF;
	}
	SynthParamBase* GetParam(Parameters param) {
		assert(instanceVst2);
		return instanceVst2->getParam(param);
	}
	SynthParam_Float* GetParamFloat(Parameters param) {
		assert(instanceVst2);
		return dynamic_cast<SynthParam_Float*>(instanceVst2->getParam(param));
	}
	SynthParam_Int* GetParamInt(Parameters param) {
		assert(instanceVst2);
		return dynamic_cast<SynthParam_Int*>(instanceVst2->getParam(param));
	}
	SynthParam_Enum* GetParamEnum(Parameters param) {
		assert(instanceVst2);
		return dynamic_cast<SynthParam_Enum*>(instanceVst2->getParam(param));
	}
	void FlushMidi(int sample)
	{
		while (!midiQueue.Empty())
		{
			auto message = midiQueue.Peek();
			if (message.mOffset > sample) break;

			auto voiceMode = GetParamEnum(Parameters::VoiceMode)->getEnumValue<VoiceModes>();
			auto status = message.StatusMsg();
			auto ctrl = message.ControlChangeIdx();
			auto note = message.NoteNumber();
			auto velocity = pow(message.Velocity() * .0078125, 1.25);
			auto osc1OutOfPhase = osc1SplitFactorA > 1.0;
			auto osc2OutOfPhase = osc2SplitFactorA > 1.0;

			if (status == IMidiMsg::kNoteOn && velocity == 0) status = IMidiMsg::kNoteOff;

			switch (status)
			{
			case IMidiMsg::kNoteOff:
				heldNotes.erase(
					std::remove(
						std::begin(heldNotes),
						std::end(heldNotes),
						note
					),
					std::end(heldNotes)
				);

				switch (voiceMode)
				{
				case VoiceModes::Poly:
					for (auto &voice : voices)
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
				switch (voiceMode)
				{
				case VoiceModes::Poly:
				{
					// get the quietest voice, prioritizing voices that are released
					auto voice = std::min_element(
						std::begin(voices),
						std::end(voices),
						[](Voice a, Voice b)
					{
						return a.IsReleased() == b.IsReleased() ? a.GetVolume() < b.GetVolume() : a.IsReleased();
					}
					);
					voice->SetNote(note);
					voice->SetVelocity(velocity);
					voice->ResetPitch();
					voice->Start(osc1OutOfPhase, osc2OutOfPhase);
					break;
				}
				default:
				case VoiceModes::Mono:
					voices[0].SetNote(note);
					voices[0].SetVelocity(velocity);
					voices[0].Start(osc1OutOfPhase, osc2OutOfPhase);
					break;
				case VoiceModes::Legato:
					voices[0].SetNote(note);
					if (heldNotes.empty())
					{
						voices[0].SetVelocity(velocity);
						voices[0].ResetPitch();
						voices[0].Start(osc1OutOfPhase, osc2OutOfPhase);
					}
					break;
				}

				heldNotes.push_back(note);
				break;
			case IMidiMsg::kPitchWheel:
			{
				auto pitchBendFactor = pitchFactor(message.PitchWheel() * 2.0);
				for (auto &voice : voices) voice.SetPitchBendFactor(pitchBendFactor);
				break;
			}
			case IMidiMsg::kControlChange:
			{
				switch (ctrl) {
				case IMidiMsg::kAllNotesOff:
					for (auto &voice : voices) {
						voice.Release();
					}
					heldNotes.clear();
					break;
				default:
					break;
				}
			}
				break;
			default:
				log_printf("Unhandled midi msg %d\n", (int32_t) status);
				break;
			}
			midiQueue.Remove();
		}
	}

	void UpdateParameters()
	{
		osc1Wave.Update(dt);
		osc1SplitMix += (targetOsc1SplitMix - osc1SplitMix) * 100.0 * dt;
		osc2Wave.Update(dt);
		osc2SplitMix += (targetOsc2SplitMix - osc2SplitMix) * 100.0 * dt;
		oscMix += (targetOscMix - oscMix) * 100.0 * dt;
		filterMode.Update(dt);
		filterCutoff += (targetFilterCutoff - filterCutoff) * 100.0 * dt;
		filterResonance += (targetFilterResonance - filterResonance) * 100.0 * dt;
		filterKeyTracking += (targetFilterKeyTracking - filterKeyTracking) * 100.0 * dt;
		masterVolume += (targetMasterVolume - masterVolume) * 100.0 * dt;
	}

	void UpdateDrift()
	{
		driftVelocity += synthRandom() * 10000.0 * dt;
		driftVelocity -= driftVelocity * 2.0 * dt;
		driftPhase += driftVelocity * dt;
		driftValue = .001 * sin(driftPhase);
	}

	double GetVoice(Voice &voice)
	{
		voice.volEnv.Update(dt);
		if (voice.volEnv.stage == EnvelopeStages::Idle && voice.filter.IsSilent(filterMode)) return 0.0;
		voice.modEnv.Update(dt);
		voice.lfoEnv.Update(dt);
		auto volEnvV = GetParamFloat(Parameters::VolEnvV)->Value();
		auto volEnvValue = (1.0 - volEnvV) * voice.volEnv.value + volEnvV * voice.volEnv.value * voice.velocity;
		auto modEnvV = GetParamFloat(Parameters::ModEnvV)->Value();
		auto modEnvValue = (1.0 - modEnvV) * voice.modEnv.value + modEnvV * voice.modEnv.value * voice.velocity;
		auto delayedLfoValue = lfoValue * voice.lfoEnv.value;

		voice.frequency += (voice.targetFrequency - voice.frequency) * glideLength * dt;

		auto baseFrequency = voice.frequency * voice.pitchBend * (1.0 + driftValue);
		auto osc1Frequency = osc1Tune * baseFrequency;
		auto osc2Frequency = osc2Tune * baseFrequency;

		auto fmMode = GetParamEnum(Parameters::FmMode)->getEnumValue<FmModes>();
		switch (fmMode)
		{
		case FmModes::Osc1:
		case FmModes::Osc2:
		{
			auto fmAmount = baseFmAmount;
			fmAmount += GetParamFloat(Parameters::VolEnvFm)->Value() * volEnvValue;
			fmAmount += GetParamFloat(Parameters::ModEnvFm)->Value() * modEnvValue;
			fmAmount += GetParamFloat(Parameters::LfoFm)->Value() * delayedLfoValue;

			auto fmMultiplier = pitchFactor(voice.oscFm.Get(dt, osc1Frequency) * fmAmount);
			switch (fmMode)
			{
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
		if (oscMix < .999)
		{
			auto osc1Out = 0.0;
			osc1Out += voice.osc1a.Get(dt, osc1Wave, osc1Frequency * osc1SplitFactorA);
			if (osc1SplitMix > .001)
				osc1Out += osc1SplitMix * voice.osc1b.Get(dt, osc1Wave, osc1Frequency * osc1SplitFactorB);
			out += osc1Out * sqrt(1.0 - oscMix);
		}
		if (oscMix > .001)
		{
			auto osc2Out = 0.0;
			osc2Out += voice.osc2a.Get(dt, osc2Wave, osc2Frequency * osc2SplitFactorA);
			if (osc2SplitMix > .001)
				osc2Out += osc2SplitMix * voice.osc2b.Get(dt, osc2Wave, osc2Frequency * osc2SplitFactorB);
			out += osc2Out * sqrt(oscMix);
		}

		out *= volEnvValue;

		auto cutoff = filterCutoff;
		cutoff += GetParamFloat(Parameters::VolEnvCutoff)->Value() * volEnvValue;
		cutoff += GetParamFloat(Parameters::ModEnvCutoff)->Value() * modEnvValue;
		cutoff += GetParamFloat(Parameters::LfoCutoff)->Value() * delayedLfoValue;
		cutoff += GetParamFloat(Parameters::FilterKeyTracking)->Value() * baseFrequency;
		cutoff *= 1.0 - driftValue;
		lastCutoffModulated = cutoff;
		out = voice.filter.Process(dt, out, filterMode, cutoff, filterResonance);

		return out;
	}
public:
	void ProcessReplacing(float** inputs, float** outputs, int nFrames)
	{
		for (int s = 0; s < nFrames; s++)
		{
			FlushMidi(s);
			UpdateParameters();
			UpdateDrift();
			lfoValue = lfo.Get(dt, GetParamFloat(Parameters::LfoFrequency)->Value());
			auto out = 0.0;
			for (auto &v : voices) out += GetVoice(v);
			out *= masterVolume;
			outputs[0][s] = out;
			outputs[1][s] = out;
		}
	}

	void Reset()
	{
//		IMutexLock lock(this);
//		dt = 1.0 / GetSampleRate();
	}

	void GrayOutControls()
	{
//		auto osc1Enabled = GetParam(Parameters::OscMix)->Value() > 0.0;
//		auto osc2Enabled = GetParam(Parameters::OscMix)->Value() < 1.0;
//		auto osc1Noise = (Waveforms)(int)GetParam(Parameters::Osc1Wave)->Value() == Waveforms::Noise;
//		auto osc2Noise = (Waveforms)(int)GetParam(Parameters::Osc2Wave)->Value() == Waveforms::Noise;
//		auto fmEnabled = (GetParam(Parameters::FmMode)->Value() == 1 && osc1Enabled && !osc1Noise) ||
//			(GetParam(Parameters::FmMode)->Value() == 2 && osc2Enabled && !osc2Noise);
//		auto filterEnabled = GetParam(Parameters::FilterMode)->Value();
//		auto modEnvEnabled = GetParam(Parameters::ModEnvFm)->Value() != 0.0 || GetParam(Parameters::ModEnvCutoff)->Value() != 0.0;
//		auto vibratoEnabled = GetParam(Parameters::LfoFm)->Value() != 0.0 || GetParam(Parameters::LfoCutoff)->Value() != 0.0 ||
//			GetParam(Parameters::LfoAmount)->Value() < 0.0 || (GetParam(Parameters::LfoAmount)->Value() > 0.0 && osc2Enabled);
//
//		// oscillator 1
//		pGraphics->GetControl(1)->GrayOut(!osc1Enabled);
//		pGraphics->GetControl(2)->GrayOut(!((osc1Enabled && !osc1Noise) || fmEnabled));
//		pGraphics->GetControl(3)->GrayOut(!((osc1Enabled && !osc1Noise) || fmEnabled));
//		pGraphics->GetControl(4)->GrayOut(!(osc1Enabled && !osc1Noise));
//
//		// oscillator 2
//		pGraphics->GetControl(5)->GrayOut(!osc2Enabled);
//		for (int i = 6; i < 9; i++) pGraphics->GetControl(i)->GrayOut(!(osc2Enabled && !osc2Noise));
//
//		// fm
//		for (int i = 12; i < 14; i++) pGraphics->GetControl(i)->GrayOut(!fmEnabled);
//		for (int i = 41; i < 44; i++) pGraphics->GetControl(i)->GrayOut(!fmEnabled);
//
//		// filter
//		for (int i = 15; i < 18; i++) pGraphics->GetControl(i)->GrayOut(!filterEnabled);
//		for (int i = 44; i < 47; i++) pGraphics->GetControl(i)->GrayOut(!filterEnabled);
//
//		// mod sources
//		for (int i = 28; i < 38; i++) pGraphics->GetControl(i)->GrayOut(!modEnvEnabled);
//		for (int i = 39; i < 41; i++) pGraphics->GetControl(i)->GrayOut(!vibratoEnabled);
//
//		// glide
//		pGraphics->GetControl(48)->GrayOut(!GetParam(Parameters::VoiceMode)->Value());
	}

	void OnParamChange(Parameters parameter)
	{
		//		IMutexLock lock(this);
//		auto value = GetParam(parameter)->Value();
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
		switch (parameter)
		{
		case Parameters::Osc1Wave:
			osc1Wave.Switch(value);
			break;
		case Parameters::Osc1Coarse:
		case Parameters::Osc1Fine:
		{
			auto coarse = GetParamInt(Parameters::Osc1Coarse)->Value();
			auto fine = GetParamFloat(Parameters::Osc1Fine)->Value();
			osc1Tune = pitchFactor(coarse + fine);
			break;
		}
		case Parameters::Osc1Split:
			targetOsc1SplitMix = value != 0.0 ? 1.0 : 0.0;
			osc1SplitFactorA = pitchFactor(-value);
			osc1SplitFactorB = pitchFactor(value);
			break;
		case Parameters::Osc2Wave:
			osc2Wave.Switch(value);
			break;
		case Parameters::Osc2Coarse:
		case Parameters::Osc2Fine:
		{
			auto coarse = GetParamInt(Parameters::Osc2Coarse)->Value();
			auto fine = GetParamFloat(Parameters::Osc2Fine)->Value();
			osc2Tune = pitchFactor(coarse + fine);
			break;
		}
		case Parameters::Osc2Split:
			targetOsc2SplitMix = value != 0.0 ? 1.0 : 0.0;
			osc2SplitFactorA = pitchFactor(-value);
			osc2SplitFactorB = pitchFactor(value);
			break;
		case Parameters::OscMix:
			targetOscMix = 1.0 - value;
			break;
		case Parameters::FmCoarse:
		case Parameters::FmFine:
		{
			auto fmCoarse = GetParamInt(Parameters::FmCoarse)->Value();
			auto fmFine = GetParamFloat(Parameters::FmFine)->Value();
			baseFmAmount = fmCoarse + fmFine;
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
		case Parameters::VolEnvA:
		{
			auto volEnvA = 1000 - 999.9 * (.5 - .5 * cos(pow(value, .1) * M_PI));
			for (auto &v : voices) v.volEnv.a = volEnvA;
			break;
		}
		case Parameters::VolEnvD:
		{
			auto volEnvD = 1000 - 999.9 * (.5 - .5 * cos(pow(value, .1) * M_PI));
			for (auto &v : voices) v.volEnv.d = volEnvD;
			break;
		}
		case Parameters::VolEnvS:
			for (auto &v : voices) v.volEnv.s = value;
			break;
		case Parameters::VolEnvR:
		{
			auto volEnvR = 1000 - 999.9 * (.5 - .5 * cos(pow(value, .1) * M_PI));
			for (auto &v : voices) v.volEnv.r = volEnvR;
			break;
		}
		case Parameters::ModEnvA:
		{
			auto modEnvA = 1000 - 999.9 * (.5 - .5 * cos(pow(value, .1) * M_PI));
			for (auto &v : voices) v.modEnv.a = modEnvA;
			break;
		}
		case Parameters::ModEnvD:
		{
			auto modEnvD = 1000 - 999.9 * (.5 - .5 * cos(pow(value, .1) * M_PI));
			for (auto &v : voices) v.modEnv.d = modEnvD;
			break;
		}
		case Parameters::ModEnvS:
			for (auto &v : voices) v.modEnv.s = value;
			break;
		case Parameters::ModEnvR:
		{
			auto modEnvR = 1000 - 999.9 * (.5 - .5 * cos(pow(value, .1) * M_PI));
			for (auto &v : voices) v.modEnv.r = modEnvR;
			break;
		}
		case Parameters::LfoDelay:
		{
			auto p = static_cast<SynthParam_Float*>(GetParam(parameter));
			auto lfoDelay = p->GetMin() + p->GetMax() - p->Value();
			for (auto &v : voices) v.lfoEnv.a = lfoDelay;
			break;
		}
		case Parameters::LfoCutoff:
			lfoToCutoff = copysign((value * .000125) * (value * .000125) * 8000.0, value);
			break;
		case Parameters::VoiceMode:
			switch (GetParamEnum(parameter)->getEnumValue<VoiceModes>())
			{
			case VoiceModes::Mono:
			case VoiceModes::Legato:
				for (int i = 1; i < numVoices; i++) {
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
		default:
			break;
		}

		GrayOutControls();
	}
};
PluginVST2_Synth::PluginVST2_Synth (audioMasterCallback audioMaster)
	: BasePluginVST2(audioMaster, PLUGIN_UID, kNumPrograms, Parameters::kNumParams, kNumInputs, kNumOutputs)
{
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
	addFloatParam(Parameters::FilterCutoff)->setRange(20.0, 22000.0)->setRangedValue(20.0);
	setParamName(getParam(Parameters::FilterCutoff), "Filter Cutoff", "Flt Cut", "%f");
	addFloatParam(Parameters::FilterResonance)->setRange(0.0, 1.0)->setRangedValue(0.0);
	setParamName(getParam(Parameters::FilterResonance), "Filter Resonance", "Flt Res", "%f");
	addFloatParam(Parameters::FilterKeyTracking)->setRange(-1.0, 1.0)->setRangedValue(0.0);
	setParamName(getParam(Parameters::FilterKeyTracking), "Filter Keytrack8ing", "Flt Trk", "%f");

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
	addFloatParam(Parameters::Osc1Split)->setRange(-1.0, 1.0)->setRangedValue(0.0);
	setParamName(getParam(Parameters::Osc1Split), "Oscillator 1 split", "OSC1 Split", "%f");
	addFloatParam(Parameters::Osc2Split)->setRange(-1.0, 1.0)->setRangedValue(0.0);
	setParamName(getParam(Parameters::Osc2Split), "Oscillator 2 split", "OSC2 Split", "%f");
	addIntParam(Parameters::Osc1Coarse)->setRange(-24, 24)->setRangedValue(-5);
	setParamName(getParam(Parameters::Osc1Coarse), "Oscillator 1 coarse", "OSC1 Semi", "%d");
	addIntParam(Parameters::Osc2Coarse)->setRange(-24, 24)->setRangedValue(-5);
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

	addFloatParam(Parameters::LfoAmount)->setRange(-0.1, 0.1)->setRangedValue(0.0);
	addFloatParam(Parameters::LfoFrequency)->setRange(0.1, 10.0)->setRangedValue(4.0);
	addFloatParam(Parameters::LfoDelay)->setRange(0.1, 1000.0)->setRangedValue(0.1);
	setParamName(getParam(Parameters::LfoAmount), "LFO amount", "LFO amt", "%f");
	setParamName(getParam(Parameters::LfoFrequency), "LFO frequency", "LFO freq", "%f");
	setParamName(getParam(Parameters::LfoDelay), "LFO ramp", "LFO ramp", "%f");

	addFloatParam(Parameters::VolEnvFm)->setRange(-24.0, 24.0)->setRangedValue(0.0);
	addFloatParam(Parameters::ModEnvFm)->setRange(-24.0, 24.0)->setRangedValue(0.0);
	addFloatParam(Parameters::LfoFm)->setRange(-24.0, 24.0)->setRangedValue(0.0);
	addFloatParam(Parameters::VolEnvCutoff)->setRange(-8000.0, 8000.0)->setRangedValue(0.0);
	addFloatParam(Parameters::ModEnvCutoff)->setRange(-8000.0, 8000.0)->setRangedValue(0.0);
	addFloatParam(Parameters::LfoCutoff)->setRange(-8000.0, 8000.0)->setRangedValue(0.0);
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
	addEnumParam(Parameters::VoiceMode)->setStrings(stringsVoiceMode)->setRangedValue(0);
	setParamName(getParam(Parameters::VoiceMode), "Voice Mode", "Voice Mode", "%d");
	addEnumParam(Parameters::FilterMode)->setStrings(stringsFilterMode)->setRangedValue(0);
	setParamName(getParam(Parameters::FilterMode), "Filter Mode", "Flt Mode", "%d");
	addEnumParam(Parameters::FmMode)->setStrings(stringsFMMode)->setRangedValue(0);
	setParamName(getParam(Parameters::FmMode), "Fm Mode", "Fm Mode", "%d");
	createEditorWindow(createView());
	for (auto param : this->vecParams) {
		this->impl->OnParamChange(param->enumParam);
	}
}

PluginVST2_Synth::~PluginVST2_Synth ()
{
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

void PluginVST2_Synth::setProgram (VstInt32 program)
{
	if (program < 0 || program >= kNumPrograms)
		return;
	curProgram = program;
}

void PluginVST2_Synth::setProgramName (char* name)
{
}

void PluginVST2_Synth::getProgramName (char* name)
{
	if (name)
		name[0] = 0;
//	if (name != NULL && curProgram >= 0)
//		vst_strncpy(name, programs[curProgram].name, kVstMaxProgNameLen);
}

void PluginVST2_Synth::getParameterLabel (VstInt32 index, char* label)
{
	if (index >= 0 && index < vecParams.size()) {
		SynthParamBase* param = vecParams[index];
		param->getLabel(label);
	}
}

void PluginVST2_Synth::getParameterDisplay (VstInt32 index, char* text)
{
	text[0] = 0;
	if (index >= 0 && index < vecParams.size()) {
		SynthParamBase* param = vecParams[index];
		param->getValueDisplay(text);
	}
}

void PluginVST2_Synth::getParameterName (VstInt32 index, char* label)
{
	if (index >= 0 && index < vecParams.size()) {
		SynthParamBase* param = vecParams[index];
		vst_strncpy(label, StringAsCStr(param->shortName), MAX_PARAM_STR_LEN);
	}
}

void PluginVST2_Synth::setParameter (VstInt32 index, float value)
{
	if (index >= 0 && index < vecParams.size()) {
		SynthParamBase* param = vecParams[index];
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

float PluginVST2_Synth::getParameter (VstInt32 index)
{
	float value = 0;
	if (index >= 0 && index < vecParams.size()) {
		SynthParamBase* param = vecParams[index];
		value = param->getAsFloat();
	}
	return value;
}

bool PluginVST2_Synth::getProgramNameIndexed (VstInt32 category, VstInt32 index, char* text)
{
	if (index >= 0 && index < kNumPrograms)
	{
		vst_strncpy (text, "Default", kVstMaxProgNameLen);
		return true;
	}
	return false;
}

bool PluginVST2_Synth::getEffectName (char* name)
{
	vst_strncpy(name, PLUGIN_EFFECT_NAME, kVstMaxEffectNameLen);
	return true;
}

bool PluginVST2_Synth::getProductString (char* text)
{
	vst_strncpy(text, PLUGIN_PRODUCT_NAME, kVstMaxProductStrLen);
	return true;
}

VstInt32 PluginVST2_Synth::getVendorVersion ()
{
	return 1;
}

VstInt32 PluginVST2_Synth::canDo (char* text)
{
//	if (!strcmp(text, PlugCanDos::canDoReceiveVstEvents))
//		return 1;
	if (!strcmp(text, PlugCanDos::canDoReceiveVstMidiEvent))
		return 1;
	if (!strcmp(text, PlugCanDos::canDoReceiveVstTimeInfo))
		return 1;
	if (!strcmp(text, PlugCanDos::canDoReceiveVstEvents))
		return 1;
	return -1;	// explicitly can't do; 0 => don't know
}

void PluginVST2_Synth::setSampleRate(float sampleRate)  {
	AudioEffectX::setSampleRate(sampleRate);
	this->impl->setSamplerate(sampleRate);
}
void PluginVST2_Synth::setBlockSize(VstInt32 blockSize) {
	AudioEffectX::setBlockSize(blockSize);
}
VstInt32 PluginVST2_Synth::processEvents (VstEvents* events) {
	assert(events);
	if (events) {
		ThreadLock lock(this->getMutex());
		int32_t len = events->numEvents;
//		if (events->numEvents)
//		log_printf("events->numEvents %d\n", events->numEvents);
		for (int i = 0; i < len; i++) {
			auto pEvent = events->events[i];
			if (pEvent->type == VstEventTypes::kVstMidiType) {
			    VstMidiEvent* pME = (VstMidiEvent*) pEvent;
			    IMidiMsg msg(pME->deltaFrames, pME->midiData[0], pME->midiData[1], pME->midiData[2]);
	            impl->ProcessMidiMsg(msg);
//				log_printf("event[%d].type %d\n", i, pME->type);
//				log_printf("event[%d].byteSize %d\n", i, pME->byteSize);
//				log_printf("event[%d].deltaFrames %d\n", i, pME->deltaFrames);
//				log_printf("event[%d].flags %d\n", i, pME->flags);
//				log_printf("event[%d].noteLength %d\n", i, pME->noteLength);
//				log_printf("event[%d].noteOffset %d\n", i, pME->noteOffset);
//				log_printf("event[%d].midiData %02X%02X%02X%02X\n", i,
//						(unsigned)pME->midiData[0], (unsigned)pME->midiData[1], (unsigned)pME->midiData[2], (unsigned)pME->midiData[3]);
//				log_printf("event[%d].detune %d\n", i, (unsigned)pME->detune);
//				log_printf("event[%d].noteOffVelocity %d\n", i, (unsigned)pME->noteOffVelocity);
//				log_printf("event[%d].reserved1 %d\n", i, (unsigned)pME->reserved1);
//				log_printf("event[%d].reserved2 %d\n", i, (unsigned)pME->reserved2);
			}
		}
	}
	return 1;
}
void PluginVST2_Synth::processReplacing(float** inputs, float** outputs, VstInt32 sampleFrames)
{
	if (issetprogram)
		return;

	if (sampleFrames != blockSize) {
		return;
	}
	ThreadLock lock(this->getMutex());
	if (this->getAeffect()->numOutputs == 1) {
		if (inputs)
			memset(inputs[0], 0, sizeof(float)*sampleFrames);
		memset(outputs[0], 0, sizeof(float)*sampleFrames);
	} else if (this->getAeffect()->numOutputs == 2) {
		if (inputs)
			dsp_util::fillChannels(inputs, this->getAeffect()->numInputs, sampleFrames, 0.0f);
		dsp_util::fillChannels(outputs, this->getAeffect()->numOutputs, sampleFrames, 0.0f);
		this->impl->ProcessReplacing(inputs, outputs, sampleFrames);
	}
}


Program::Program()
{
	vst_strncpy(name, "Init", kVstMaxProgNameLen);
}

}

namespace PluginSynth {


class guicontainer_plugin_synth : public guictr_base {
	vstplugin* vstHostSide = nullptr;
	PluginVST2_Synth* curEffect = nullptr;
	struct _synth_gui_param_knob {
		guiknob* knob;
		Parameters param;
	};
	std::vector<_synth_gui_param_knob> knobs;
	guiknob_pluginparam knobParam0;
public:
	guicontainer_plugin_synth()
	: guictr_base(),
		 knobParam0(PARAM_OFFSET_EXTERNAL+(int)Parameters::FilterCutoff, (int)Parameters::FilterCutoff) {
		setBackgroundRendered(true);
		padding = 4;
		margin = 4;
		add(&knobParam0);
	}
	~guicontainer_plugin_synth() {
		remove(&knobParam0);
	}
	virtual bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
		if (this->contains(mpos)) {
			ivec2 localMouse = this->toContainerSpace(mpos);
			for (guibase* gui : guis) {
				if (gui->mouseHitTest(localMouse, evt)) {
					return true;
				}
			}
			if (evt.type == MouseHitType::MOUSE_LEFT) {
				evt.requestFocus(this);
				return true;
			}
		}
		return false;
	}

	guiknob_pluginparam* getKnobFromParameter(int32_t index) {
		switch (index) {
			case Parameters::FilterCutoff:
				return &knobParam0;
		}
		return nullptr;
	}
	void onSetParameter(int32_t index, float value) {
		guiknob_pluginparam* knob = getKnobFromParameter(index);
		if (knob && curEffect) {
			knob->setValueInit(value);
			knob->setDisplayValueFromEffect();
		}
	}
	void onGuiOpen(AudioEffect* eff) {
		this->curEffect = dynamic_cast<PluginVST2_Synth*>(eff);
		assert(this->curEffect);
		knobParam0.setAudioEffect(eff);
	}
	void onGuiClose(AudioEffect* eff) {
		this->curEffect = nullptr;
	}
	void setVSTPlugin(vstplugin* vstHostSide)  {
		this->vstHostSide = vstHostSide;
	#if BUILD_VSTHOST
		knobParam0.setEffectInstance(vstHostSide);
	#endif
	}
	void onTick(AppCtrl* ctrl) {
		for (guibase* gui : guis) {
			gui->onTick(ctrl);
		}
	}
	void prerender(NVGcontext* vg) {
		for (guibase* gui : guis) {
			gui->prerender(vg);
		}
	}

	void render(NVGcontext* vg) {
		if (isBackgroundRendered()) {
			renderBackground(vg);
		}
		if (!setScissorTransform(vg)) {
			return;
		}

		for (guibase* gui : guis) {
			nvgSave(vg);
			gui->render(vg);
			nvgRestore(vg);
		}
		PluginVST2_Synth* thisImpl = this->curEffect;
		PluginVST2_Synth::ThreadLock lock(thisImpl->getMutex());
		std::vector<String> strings;
//		this->curEffect->
		String str;
		str = StringFormat("Blocksize %d", this->curEffect->getBlockSize());
		strings.push_back(str);
		str = StringFormat("Samplerate %f", this->curEffect->getSampleRate());
		strings.push_back(str);
		int flags = 0;
		for (int i = 8; i < 16; i++) {
			flags |= (1<<i);
		}
		VstTimeInfo* timeinfo = thisImpl->getTimeInfo(flags);
		assert(timeinfo);
		strings.push_back(StringFormat("samplePos %f", timeinfo->samplePos));
		strings.push_back(StringFormat("sampleRate %f", timeinfo->sampleRate));
		strings.push_back(StringFormat("nanoSeconds %f", timeinfo->nanoSeconds));
		strings.push_back(StringFormat("ppqPos %f", timeinfo->ppqPos));
		strings.push_back(StringFormat("tempo %f", timeinfo->tempo));
		strings.push_back(StringFormat("barStartPos %f", timeinfo->barStartPos));
		strings.push_back(StringFormat("cycleStartPos %f", timeinfo->cycleStartPos));
		strings.push_back(StringFormat("cycleEndPos %f", timeinfo->cycleEndPos));
		strings.push_back(StringFormat("timeSigNumerator %d", timeinfo->timeSigNumerator));
		strings.push_back(StringFormat("timeSigDenominator %d", timeinfo->timeSigDenominator));
		strings.push_back(StringFormat("smpteOffset %d", timeinfo->smpteOffset));
		strings.push_back(StringFormat("smpteFrameRate %d", timeinfo->smpteFrameRate));
		strings.push_back(StringFormat("samplesToNextClock %d", timeinfo->samplesToNextClock));
		strings.push_back(String("flags ")+FormatBinaryString<int16_t>(timeinfo->flags&0xFFFF));
//		this->
		setFont(vg, 16, G_WHITE, NVG_ALIGN_TOP | NVG_ALIGN_LEFT);
		float lineh;
		nvgTextMetrics(vg, NULL, NULL, &lineh);
		int y = INSET_CTR_SPACING;
		int x = this->knobParam0.right()+INSET_CTR_SPACING;
//		int x = INSET_CTR_SPACING;
		for (String& s : strings) {
			nvgText(vg, x, y, StringAsCStr(s), NULL);
			y += lineh;
		}
		std::vector<int> heldNotes = thisImpl->getSynth()->getHeldNotes(); //TODO: not threadsafe
		String s = "Held notes: ";
		for (int i : heldNotes) {
			s += String(noteName(i))+",";
			if (s.length() > 32) {
				nvgText(vg, x, y, StringAsCStr(s), NULL);
				s = "";
				y += lineh;
			}
		}
		if (heldNotes.empty())
			s += "<empty>";
		if (s.length() > 0) {
			nvgText(vg, x, y, StringAsCStr(s), NULL);
			s = "";
			y += lineh;
		}
//		//stress test thread safety
//		for (int i = 0; i < 10000; i++) {
//			std::vector<int> heldNotes = thisImpl->getSynth()->getHeldNotes(); //TODO: not threadsafe
//		}

	}
	void layout() {
		ivec2 cs = getSizeContent();
		const int inset = 4;
		const int knobSize = math::max(32, (cs.x-inset*3)/2);
		knobParam0.size = ivec2(64, 90);
		knobParam0.pos = ivec2(inset);
		for (guibase* gui : guis) {
			gui->layout();
		}
	}
	bool handleKeyInput(KeyEvent& event) override {
		if (event.type != KeyEventType::K_RELEASE) {

		}
		return false;
	}
	void buttonClicked(guibase* button) override {
	}
};



class ViewContainers_Plugin_Synth : public PluginViewContainersImpl {
public:
	guicontainer_plugin_synth ctr_main;
	ViewContainers_Plugin_Synth() : PluginViewContainersImpl(280, 360)
	{
	}
	virtual ~ViewContainers_Plugin_Synth() {
	}
	void layout(int32_t winW, int32_t winH) override {
		ctr_main.pos = {0, 0};
		ctr_main.size = {winW, winH};
	}
	void addTo(std::vector<guictr_base*>& v) override {
		 v.push_back(&ctr_main);
	}
	void onGuiOpen(AudioEffect* eff) override {
		ctr_main.onGuiOpen(eff);
	}
	void onGuiClose(AudioEffect* eff) override {
		ctr_main.onGuiClose(eff);
	}
	void onSetParameter(int32_t index, float value) override {
		ctr_main.onSetParameter(index, value);
	}
	void getFixedSize(int32_t* w, int32_t* h) override {
		*w = this->width;
		*h = this->height;
	}
	void setVSTPlugin(vstplugin* hostsideplugin)  {
		ctr_main.setVSTPlugin(hostsideplugin);
	}
};


	const char* getName() {
		return PLUGIN_EFFECT_NAME;
	}
	AudioEffectX* createPlugin (audioMasterCallback audioMaster)
	{
		return new PluginVST2_Synth (audioMaster);
	}
	std::shared_ptr<PluginViewContainers> PluginVST2_Synth::createView() {
		std::shared_ptr<PluginViewContainers> view = std::make_shared<ViewContainers_Plugin_Synth>();
		this->views.push_back(view);
		return view;
	}
}

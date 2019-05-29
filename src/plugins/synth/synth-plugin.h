#pragma once
#include <vector>
#include <cmath>
#include "../plugin-base.h"
#include "../plugin-base.h"
#include "vstsdk-plugin-2.4/audioeffectx.h"


namespace PluginSynth {

class PluginVST2_Synth;

enum
{
	// Global
	kNumPrograms = 0, // wonder if that works
	kNumOutputs = 2,
	kNumInputs = 2,
};
enum Parameters
{
	MasterVolume = 0,
	VoiceMode,
	GlideLength,
	FilterMode,
	FilterCutoff,
	FilterResonance,
	FilterKeyTracking,
	VolEnvCutoff,
	ModEnvCutoff,
	OscMix,
	Osc1Wave,
	Osc1Coarse,
	Osc1Fine,
	Osc1Split,
	Osc2Wave,
	Osc2Coarse,
	Osc2Fine,
	Osc2Split,
	LfoAmount,
	LfoFrequency,
	LfoDelay,
	LfoCutoff,
	FmMode,
	FmCoarse,
	FmFine,
	VolEnvFm,
	ModEnvFm,
	LfoFm,
	VolEnvA,
	VolEnvD,
	VolEnvS,
	VolEnvR,
	VolEnvV,
	ModEnvA,
	ModEnvD,
	ModEnvS,
	ModEnvR,
	ModEnvV,
	kNumParams
};

class SynthState
{
public:
	int osc1Wave = 0;
	int osc2Wave = 0;
	int voiceMode = 0;
	int filterMode = 0;
	int fmMode = 0;

	double lfoValue = 0.0;
	double driftVelocity = 0.0;
	double driftPhase = 0.0;
	double driftValue = 0.0;

	double osc1Tune = 1.0;
	double targetOsc1SplitMix = 0.0;
	double osc1SplitMix = 0.0;
	double osc1SplitFactorA = 1.0;
	double osc1SplitFactorB = 1.0;
	double osc2Tune = 1.0;
	double targetOsc2SplitMix = 0.0;
	double osc2SplitMix = 0.0;
	double osc2SplitFactorA = 1.0;
	double osc2SplitFactorB = 1.0;
	double targetOscMix = 0.0;
	double oscMix = 0.0;
	double baseFmAmount = 0.0;
	double targetFilterCutoff = 0.0;
	double filterCutoff = 0.0;
	double targetFilterResonance = 0.0;
	double filterResonance = 0.0;
	double targetFilterKeyTracking = 0.0;
	double filterKeyTracking = 0.0;
	double lfoToCutoff = 0.0;
	double glideLength = 0.0;
	double targetMasterVolume = 0.0;
	double masterVolume = 0.0;
};
class ProgramParameters {
public:
	~ProgramParameters() = default;
protected:
	double Osc1Wave = 0.0;
	double Osc1Coarse = 0.0;
	double Osc1Fine = 0.0;
	double Osc1Split = 0.0;
	double Osc2Wave = 0.0;
	double Osc2Coarse = 0.0;
	double Osc2Fine = 0.0;
	double Osc2Split = 0.0;
	double OscMix = 0.0;
	double FmMode = 0.0;
	double FmCoarse = 0.0;
	double FmFine = 0.0;
	double FilterMode = 0.0;
	double FilterCutoff = 0.0;
	double FilterResonance = 0.0;
	double FilterKeyTracking = 0.0;
	double VolEnvA = 0.0;
	double VolEnvD = 0.0;
	double VolEnvS = 0.0;
	double VolEnvR = 0.0;
	double VolEnvV = 0.0;
	double ModEnvA = 0.0;
	double ModEnvD = 0.0;
	double ModEnvS = 0.0;
	double ModEnvR = 0.0;
	double ModEnvV = 0.0;
	double LfoAmount = 0.0;
	double LfoFrequency = 0.0;
	double LfoDelay = 0.0;
	double VolEnvFm = 0.0;
	double VolEnvCutoff = 0.0;
	double ModEnvFm = 0.0;
	double ModEnvCutoff = 0.0;
	double LfoFm = 0.0;
	double LfoCutoff = 0.0;
	double VoiceMode = 0.0;
	double GlideLength = 0.0;
	double MasterVolume = 0.0;
};
class Program : public ProgramParameters
{
	friend class PluginVST2_Synth;
public:
	Program();
	~Program() {}

private:
	char name[kVstMaxProgNameLen+1];
};

struct SynthParamBase;
class SynthImpl;
class PluginVST2_Synth : public BasePluginVST2 {

public:
	PluginVST2_Synth (audioMasterCallback audioMaster);
	~PluginVST2_Synth ();

	virtual void setSampleRate (float sampleRate);
	virtual void setBlockSize (VstInt32 blockSize);
	VstInt32 processEvents (VstEvents* events) override;	///< Called when new MIDI events come in
	void processReplacing (float** inputs, float** outputs, VstInt32 sampleFrames) override;
	PluginViewContainers* createView() override;

	virtual void setProgram(VstInt32 program);
	virtual void setProgramName(char* name);
	virtual void getProgramName(char* name);
	virtual bool beginSetProgram() { this->issetprogram = true; return false; }	///< Called before a program is loaded
	virtual bool endSetProgram() { this->issetprogram = false; return false; }		///< Called after a program was loaded
	virtual bool getProgramNameIndexed (VstInt32 category, VstInt32 index, char* text);

	virtual void setParameter (VstInt32 index, float value) override;
	virtual float getParameter (VstInt32 index) override;
	virtual void getParameterLabel (VstInt32 index, char* label) override;
	virtual void getParameterDisplay (VstInt32 index, char* text) override;
	virtual void getParameterName (VstInt32 index, char* text) override;

	virtual bool getEffectName (char* name);
	virtual bool getProductString (char* text);
	virtual VstPlugCategory getPlugCategory ()
	{
		return kPlugCategEffect;
	}
	virtual VstInt32 getVendorVersion ();
	virtual VstInt32 canDo (char* text);

	SynthParamBase* getParam(Parameters enumParam);

#ifdef DISPATCHER_DEBUG_TRACE
	VstIntPtr dispatcher(VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt);
#endif // DEBUG

private:
	std::vector<SynthParamBase*> vecParams;
	SynthImpl* impl;
//	Program singleProgram;
};
AudioEffectX* createPlugin (audioMasterCallback audioMaster);
const char* getName();
}


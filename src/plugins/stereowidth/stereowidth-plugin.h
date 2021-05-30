#pragma once
#include <vector>
#include <cmath>
#include "../plugin-base.h"
#include "vstsdk-plugin-2.4/audioeffectx.h"


namespace PluginStereoWidth {

class PluginVST2_StereoWidth;

enum
{
	// Global
	kNumPrograms = 0, // wonder if that works
	kNumOutputs = 2,
	kNumInputs = 2,
};

enum
{
	kStereoWidth = 0,
	kGain = 1,
	kNumParams
};



//------------------------------------------------------------------------------------------
// ProgramParameters
//------------------------------------------------------------------------------------------


class ProgramParameters
{
public:
	float width;
	float gain;
};



//------------------------------------------------------------------------------------------
// FSM_VST_Program
//------------------------------------------------------------------------------------------


class BaseVST2_ProgramStereoWidth : public ProgramParameters
{
	friend class PluginVST2_StereoWidth;
public:
	BaseVST2_ProgramStereoWidth ();
	~BaseVST2_ProgramStereoWidth() {}

private:
	char name[kVstMaxProgNameLen+1];
};


//------------------------------------------------------------------------------------------
// FSM_VST_Plugin
//------------------------------------------------------------------------------------------

class PluginVST2_StereoWidth : public BasePluginVST2 {

public:
	PluginVST2_StereoWidth (audioMasterCallback audioMaster);
	~PluginVST2_StereoWidth ();

	void processReplacing (float** inputs, float** outputs, VstInt32 sampleFrames) override;
	std::shared_ptr<PluginViewContainers> createView() override;

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

	BaseVST2_ProgramStereoWidth* current() {
		return &singleProgram;
	}

#ifdef DISPATCHER_DEBUG_TRACE
	VstIntPtr dispatcher(VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt);
#endif // DEBUG

private:
	BaseVST2_ProgramStereoWidth singleProgram;
//	BaseVST2_Program programs[kNumPrograms];
};

AudioEffectX* createPlugin (audioMasterCallback audioMaster);
const char* getName();
}


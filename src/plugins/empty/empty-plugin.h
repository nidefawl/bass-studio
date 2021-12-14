#pragma once

#include <vector>
#include <cmath>
#include "../plugin-base.h"
#include "vstsdk-plugin-2.4/audioeffectx.h"

namespace PluginEmptyVST2 {

class EmptyPluginVST2;

enum
{
	// Global
	kNumPrograms = 1,
	kNumOutputs = 2,
	kNumInputs = 2,
};

enum
{
	kNumParams = 0
};



//------------------------------------------------------------------------------------------
// ProgramParameters
//------------------------------------------------------------------------------------------


class ProgramParameters
{
};



//------------------------------------------------------------------------------------------
// FSM_VST_Program
//------------------------------------------------------------------------------------------


class BaseVST2_Program : public ProgramParameters
{
	friend class EmptyPluginVST2;
public:
	BaseVST2_Program ();
	~BaseVST2_Program() {}

private:
	char name[kVstMaxProgNameLen+1];
};


//------------------------------------------------------------------------------------------
// FSM_VST_Plugin
//------------------------------------------------------------------------------------------
class guictr_emptyvst;
class EmptyPluginVST2 : public BasePluginVST2 {
	friend class guictr_emptyvst;
protected:
	int numCalls = 0;
	int numCalls2 = 0;
public:
	EmptyPluginVST2 (audioMasterCallback audioMaster);
	virtual ~EmptyPluginVST2 ();

	void processReplacing (float** inputs, float** outputs, VstInt32 sampleFrames) override;
	std::shared_ptr<PluginViewContainers> createView() override;

	virtual void setProgram(VstInt32 program);
	virtual void setProgramName(char* name);
	virtual void getProgramName(char* name);
	virtual bool beginSetProgram() { this->issetprogram = true; return false; }	///< Called before a program is loaded
	virtual bool endSetProgram() { this->issetprogram = false; return false; }		///< Called after a program was loaded
	virtual bool getProgramNameIndexed (VstInt32 category, VstInt32 index, char* text);

	virtual void setParameter (VstInt32 index, float value);
	virtual float getParameter (VstInt32 index);
	virtual void getParameterLabel (VstInt32 index, char* label);
	virtual void getParameterDisplay (VstInt32 index, char* text);
	virtual void getParameterName (VstInt32 index, char* text);

	virtual bool getInputProperties (VstInt32 index, VstPinProperties* properties);
	virtual bool getOutputProperties (VstInt32 index, VstPinProperties* properties);

	bool getEffectName (char* name) override;
	bool getVendorString (char* text) override;
	bool getProductString (char* text) override;
	virtual VstPlugCategory getPlugCategory ()
	{
		return kPlugCategEffect;
	}
	virtual VstInt32 getVendorVersion ();
	virtual VstInt32 canDo (char* text);

	BaseVST2_Program* current() {
		return &(curProgram >= 0 && curProgram < kNumPrograms ? programs[curProgram] : programs[0]);
	}

#ifdef DISPATCHER_DEBUG_TRACE
	VstIntPtr dispatcher(VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt);
#endif // DEBUG

private:
	BaseVST2_Program programs[kNumPrograms];
};
AudioEffectX* createPlugin (audioMasterCallback audioMaster);
const char* getName();
}


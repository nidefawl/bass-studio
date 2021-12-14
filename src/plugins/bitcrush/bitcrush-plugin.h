#pragma once
#include <vector>
#include <cmath>
#include "../plugin-base.h"
#include "vstsdk-plugin-2.4/audioeffectx.h"

#define BITCRUSH_BITS_MIN 0
#define BITCRUSH_BITS_MAX 4

struct DelayLine;
namespace PluginBitcrush {

class PluginVST2_Bitcrush;

enum
{
	// Global
	kNumPrograms = 0, // wonder if that works
	kNumOutputs = 2,
	kNumInputs = 2,
};

enum
{
	kBitcrush = 0,
	kNumParams
};


class ProgramParameters
{
public:
	int32_t bitcrush = 0;
};

class Program : public ProgramParameters
{
	friend class PluginVST2_Bitcrush;
public:
	Program();
	~Program() {}

private:
	char name[kVstMaxProgNameLen+1];
};


class PluginVST2_Bitcrush : public BasePluginVST2 {

public:
	PluginVST2_Bitcrush (audioMasterCallback audioMaster);
	~PluginVST2_Bitcrush ();

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

	bool getEffectName (char* name) override;
	bool getVendorString (char* text) override;
	bool getProductString (char* text) override;
	virtual VstPlugCategory getPlugCategory ()
	{
		return kPlugCategEffect;
	}
	virtual VstInt32 getVendorVersion ();
	virtual VstInt32 canDo (char* text);

	Program* current() {
		return &singleProgram;
	}

#ifdef DISPATCHER_DEBUG_TRACE
	VstIntPtr dispatcher(VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt);
#endif // DEBUG

private:
	void setNewBitcrushLvl(int32_t nbitcrushLvlh);
	Program singleProgram;
	int32_t curBitcrush = 0;
	int32_t newBitcrush = 0;
	std::atomic<bool> bitcrushLvlChanged{false};
//	BaseVST2_Program programs[kNumPrograms];
};
AudioEffectX* createPlugin (audioMasterCallback audioMaster);
const char* getName();
}


#pragma once
#include <vector>
#include <cmath>
#include "../plugin-base.h"
#include "vstsdk-plugin-2.4/audioeffectx.h"


namespace PluginHostInfo {

class PluginVST2_HostInfo;

enum
{
	// Global
	kNumPrograms = 0, // wonder if that works
	kNumOutputs = 2,
	kNumInputs = 2,
};

enum
{
	kTestParam = 0,
	kNumParams
};


class ProgramParameters
{
public:
	float testValue = 0.0f;
};

class Program : public ProgramParameters
{
	friend class PluginVST2_HostInfo;
public:
	Program();
	~Program() {}

private:
	char name[kVstMaxProgNameLen+1];
};


struct PluginVST2_HostInfo_impl_t {

	std::vector<uint8_t> dataPlugin;

	std::vector<uint8_t> dataPreset;
	PluginVST2_HostInfo_impl_t() {

	}
};
class PluginVST2_HostInfo : public BasePluginVST2 {
	PluginVST2_HostInfo_impl_t* const impl;
public:
	PluginVST2_HostInfo (audioMasterCallback audioMaster);
	~PluginVST2_HostInfo ();

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
	virtual VstInt32 getChunk (void** data, bool isPreset = false) override;
	virtual VstInt32 setChunk (void* data, VstInt32 byteSize, bool isPreset = false) override;


	virtual bool getEffectName (char* name);
	virtual bool getProductString (char* text);
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
	Program singleProgram;
};
AudioEffectX* createPlugin (audioMasterCallback audioMaster);
const char* getName();
}


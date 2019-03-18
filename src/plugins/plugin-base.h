#pragma once

#include <vector>
#include <cmath>
#include "plugin.h"
#include "../vstsdk-plugin-2.4/audioeffect.h"
#include "../vstsdk-plugin-2.4/audioeffectx.h"

#define PLUGIN_VENDOR_NAME "MichaelH"

class PluginViewContainers;
class BasePluginVST2 : public AudioEffectX {
protected:
	std::vector<PluginViewContainers*> views;

public:
	BasePluginVST2 (audioMasterCallback audioMaster,
			const char* pluginUIDStr,
			VstInt32 numPrograms,
			VstInt32 numParams,
			VstInt32 numInputs,
			VstInt32 numOutputs);
	virtual ~BasePluginVST2 ();

	void createEditorWindow(PluginViewContainersImpl* view);

	virtual void open ();		///< Called when plug-in is initialized
	virtual void close () {}	///< Called when plug-in will be released
	virtual void suspend () {}	///< Called when plug-in is switched to off
	virtual void resume () {}	///< Called when plug-in is switched to on

	virtual void processReplacing (float** inputs, float** outputs, VstInt32 sampleFrames) = 0;
	virtual PluginViewContainers* createView() = 0;


	virtual bool getInputProperties (VstInt32 index, VstPinProperties* properties);
	virtual bool getOutputProperties (VstInt32 index, VstPinProperties* properties);
	virtual bool getVendorString (char* text);

//	virtual void setProgram(VstInt32 program);
//	virtual void setProgramName(char* name);
//	virtual void getProgramName(char* name);
//	virtual bool beginSetProgram() { this->issetprogram = true; return false; }	///< Called before a program is loaded
//	virtual bool endSetProgram() { this->issetprogram = false; return false; }		///< Called after a program was loaded
//	virtual bool getProgramNameIndexed (VstInt32 category, VstInt32 index, char* text);
//
//	virtual void setParameter (VstInt32 index, float value);
//	virtual float getParameter (VstInt32 index);
//	virtual void getParameterLabel (VstInt32 index, char* label);
//	virtual void getParameterDisplay (VstInt32 index, char* text);
//	virtual void getParameterName (VstInt32 index, char* text);
//
//	virtual void setSampleRate (float sampleRate);
//	virtual void setBlockSize (VstInt32 blockSize);
//	virtual bool getInputProperties (VstInt32 index, VstPinProperties* properties);
//	virtual bool getOutputProperties (VstInt32 index, VstPinProperties* properties);
//
//	virtual bool getEffectName (char* name);
//	virtual bool getVendorString (char* text);
//	virtual bool getProductString (char* text);
//	virtual VstPlugCategory getPlugCategory ()
//	{
//		return kPlugCategEffect;
//	}
//	virtual VstInt32 getVendorVersion ();
//	virtual VstInt32 canDo (char* text);
//
protected:
	bool issetprogram = false;
};

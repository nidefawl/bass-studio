#include <math.h>
#include <algorithm>
#include <stdio.h>
#include <memory>
#include "config.h"
#include "str_util.h"
#include "dsp_util.h"

#include "platform.h"

#include "../plugin.h"
#include "empty-plugin.h"
#include "../../gui/pluginviewcontainers.h"
#include "vstsdk-plugin-2.4/audioeffect.h"
#include "vstsdk-plugin-2.4/audioeffectx.h"
#include "logging.h"
#ifdef _MSC_VER
#include <Windows.h>
#endif

#if defined(PLUGIN_BUILD_CRASHVERSION) || defined(BUILD_VSTHOST)
#define PLUGIN_EFFECT_NAME "CrashVST2x"
#else
#define PLUGIN_EFFECT_NAME "Empty"
#endif

#define PLUGIN_VENDOR_NAME "MichaelH"
#define PLUGIN_UID "EMPT" //advanced gui test plugin
#define PLUGIN_PRODUCT_NAME "empty test plugin VST2.x "
//

#if BUILD_EXTERNAL_PLUGIN
AudioEffect* createEffectInstance (audioMasterCallback audioMaster)
{
	return PluginEmptyVST2::createPlugin(audioMaster);
}
#endif

namespace PluginEmptyVST2 {

EmptyPluginVST2::EmptyPluginVST2 (audioMasterCallback audioMaster)
	: BasePluginVST2(audioMaster, PLUGIN_UID, kNumPrograms, kNumParams, kNumInputs, kNumOutputs)
{
	createEditorWindow(static_cast<PluginViewContainersImpl*>(createView()));

	curProgram = 0;
}


EmptyPluginVST2::~EmptyPluginVST2 ()
{
}

void EmptyPluginVST2::setProgram (VstInt32 program)
{
	if (program < 0 || program >= kNumPrograms)
		return;
	curProgram = program;
}

void EmptyPluginVST2::setProgramName (char* name)
{
}

void EmptyPluginVST2::getProgramName (char* name)
{
	if (name != NULL && curProgram >= 0)
		vst_strncpy(name, programs[curProgram].name, kVstMaxProgNameLen);
}

void EmptyPluginVST2::getParameterLabel (VstInt32 index, char* label)
{
	vst_strncpy(label, "", kVstMaxParamStrLen);
}

void EmptyPluginVST2::getParameterDisplay (VstInt32 index, char* text)
{
	text[0] = 0;
}

void EmptyPluginVST2::getParameterName (VstInt32 index, char* label)
{
}

void EmptyPluginVST2::setParameter (VstInt32 index, float value)
{

}

float EmptyPluginVST2::getParameter (VstInt32 index)
{
	return 0;
}


bool EmptyPluginVST2::getInputProperties (VstInt32 index, VstPinProperties* properties)
{
	if (index == 0 || index == 1)
	{
		properties->flags = kVstPinIsActive | kVstPinIsStereo;
	}
	if (index == 0)
	{
		strcpy(properties->label,	   "Left input");
		strcpy(properties->shortLabel, "L in");
		return true;
	}
	else if (index == 1)
	{
		strcpy(properties->label,	   "Right input");
		strcpy(properties->shortLabel, "R in");
		return true;
	}
	return false;
}
bool EmptyPluginVST2::getOutputProperties (VstInt32 index, VstPinProperties* properties)
{
	if (index == 0 || index == 1)
	{
		properties->flags = kVstPinIsActive | kVstPinIsStereo;
	}
	if (index == 0)
	{
		strcpy(properties->label,	   "Left output");
		strcpy(properties->shortLabel, "L out");
		return true;
	}
	else if (index == 1)
	{
		strcpy(properties->label,	   "Right output");
		strcpy(properties->shortLabel, "R out");
		return true;
	}
	return false;
}

bool EmptyPluginVST2::getProgramNameIndexed (VstInt32 category, VstInt32 index, char* text)
{
	if (index >= 0 && index < kNumPrograms)
	{
		vst_strncpy (text, programs[index].name, kVstMaxProgNameLen);
		return true;
	}
	return false;
}

bool EmptyPluginVST2::getEffectName (char* name)
{
	vst_strncpy(name, "DrumSynth", kVstMaxEffectNameLen);
	return true;
}

bool EmptyPluginVST2::getVendorString (char* text)
{
	vst_strncpy(text, PLUGIN_VENDOR_NAME, kVstMaxVendorStrLen);
	return true;
}

bool EmptyPluginVST2::getProductString (char* text)
{
	vst_strncpy(text, PLUGIN_PRODUCT_NAME, kVstMaxProductStrLen);
	return true;
}

VstInt32 EmptyPluginVST2::getVendorVersion ()
{
	return 2;
}

VstInt32 EmptyPluginVST2::canDo (char* text)
{
	if (!strcmp (text, "receiveVstEvents"))
		return 1;
	if (!strcmp(text, "receiveVstTimeInfo"))
		return 1;
	return -1;	// explicitly can't do; 0 => don't know
}

void EmptyPluginVST2::processReplacing(float** inputs, float** outputs, VstInt32 sampleFrames)
{
	numCalls++;
	if (issetprogram)
		return;

	if (sampleFrames != blockSize) {
		return;
	}
	if (this->getAeffect()->numOutputs == 1) {
		if (inputs)
			memset(inputs[0], 0, sizeof(float)*sampleFrames);
		memset(outputs[0], 0, sizeof(float)*sampleFrames);
	} else if (this->getAeffect()->numOutputs == 2) {
		if (inputs)
			dsp_util::fillSilence(inputs, sampleFrames);
		dsp_util::fillSilence(outputs, sampleFrames);
#if defined(PLUGIN_BUILD_CRASHVERSION) || defined(BUILD_VSTHOST)
//		my_printf("producing segfault\n", 0);
		int64_t* ptr = nullptr;
		ptr = static_cast<int64_t*>((void*)0xBAADF00D);
		int64_t val = *ptr;
		my_printf("val = %lld WTF\n", val);
#endif
	}
	numCalls2++;
}


BaseVST2_Program::BaseVST2_Program()
{
	vst_strncpy(name, "Init", kVstMaxProgNameLen);
}


const char* getName() {
	return PLUGIN_EFFECT_NAME;
}
}

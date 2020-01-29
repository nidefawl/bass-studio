#include <math.h>
#include <algorithm>
#include <stdio.h>
#include <vector>
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
#include "info-plugin.h"
#include "plugins/plugin.h"
#include "plugins/plugin-base.h"
#include "plugins/plugin-window.h"
#include "vstsdk-plugin-2.4/audioeffect.h"
#include "vstsdk-plugin-2.4/audioeffectx.h"
#include "audioblock.h"

#define PLUGIN_EFFECT_NAME "HostInfo"
#define PLUGIN_UID "INFO"
#define PLUGIN_PRODUCT_NAME "HostInfo VST2.4"

#if BUILD_EXTERNAL_PLUGIN
AudioEffect* createEffectInstance (audioMasterCallback audioMaster)
{
	return PluginHostInfo::createPlugin (audioMaster);
}
#endif


namespace PluginHostInfo {

PluginVST2_HostInfo::PluginVST2_HostInfo (audioMasterCallback audioMaster)
	: BasePluginVST2(audioMaster, PLUGIN_UID, kNumPrograms, kNumParams, kNumInputs, kNumOutputs), impl(new PluginVST2_HostInfo_impl_t())
{
	programsAreChunks(true);
	createEditorWindow(static_cast<PluginViewContainersImpl*>(createView()));

}


PluginVST2_HostInfo::~PluginVST2_HostInfo ()
{
	delete impl;
}

void PluginVST2_HostInfo::setProgram (VstInt32 program)
{
	if (program < 0 || program >= kNumPrograms)
		return;
	curProgram = program;
}

void PluginVST2_HostInfo::setProgramName (char* name)
{
}

void PluginVST2_HostInfo::getProgramName (char* name)
{
	if (name)
		name[0] = 0;
//	if (name != NULL && curProgram >= 0)
//		vst_strncpy(name, programs[curProgram].name, kVstMaxProgNameLen);
}

void PluginVST2_HostInfo::getParameterLabel (VstInt32 index, char* label)
{
	switch (index) {
		case kTestParam:
			vst_strncpy(label, "", kVstMaxParamStrLen);
			return;
		default:
			vst_strncpy(label, "", kVstMaxParamStrLen);
	}
}

void PluginVST2_HostInfo::getParameterDisplay (VstInt32 index, char* text)
{
	text[0] = 0;
	switch (index)
	{
		case kTestParam: {
			snprintf(text, kVstMaxParamStrLen, "%f", current()->testValue);
			break;
		}
	}
}

void PluginVST2_HostInfo::getParameterName (VstInt32 index, char* label)
{
	switch (index)
	{
	case kTestParam:		vst_strncpy(label, "Test", kVstMaxParamStrLen);	return;
	}
}

void PluginVST2_HostInfo::setParameter (VstInt32 index, float value)
{
	Program *ap = current();
	switch (index) {
	case kTestParam:
		ap->testValue = value;
		break;
	}
#if BUILD_VSTHOST
	for (PluginViewContainers* pviewctr : this->views) {
		pviewctr->onSetParameter(index, value);
	}
#else
	if (this->editor) {
		static_cast<pluginwindow*>(this->editor)->onSetParameter(index, value);
	}
#endif
}

float PluginVST2_HostInfo::getParameter (VstInt32 index)
{
	Program *ap = current();
	float value = 0;
	switch (index) {
	case kTestParam:
		value = ap->testValue;
		break;
	}
	return value;
}

bool PluginVST2_HostInfo::getProgramNameIndexed (VstInt32 category, VstInt32 index, char* text)
{
	if (index >= 0 && index < kNumPrograms)
	{
		vst_strncpy (text, "Default", kVstMaxProgNameLen);
		return true;
	}
	return false;
}

bool PluginVST2_HostInfo::getEffectName (char* name)
{
	vst_strncpy(name, PLUGIN_EFFECT_NAME, kVstMaxEffectNameLen);
	return true;
}

bool PluginVST2_HostInfo::getProductString (char* text)
{
	vst_strncpy(text, PLUGIN_PRODUCT_NAME, kVstMaxProductStrLen);
	return true;
}

VstInt32 PluginVST2_HostInfo::getVendorVersion ()
{
	return 1;
}

VstInt32 PluginVST2_HostInfo::canDo (char* text)
{
	if (!strcmp(text, "receiveVstTimeInfo"))
		return 1;
	return -1;	// explicitly can't do; 0 => don't know
}
///< Host stores plug-in state. Returns the size in bytes of the chunk (plug-in allocates the data array)
VstInt32 PluginVST2_HostInfo::getChunk (void** data, bool isPreset) {
	log_printf("getChunk isPreset = %d: PTR %08X\n", isPreset, (uint64_t)(data));
	if (isPreset) {
		impl->dataPreset.resize(1000);
		std::fill(impl->dataPreset.begin(), impl->dataPreset.end(), 0xAA);
		*data = impl->dataPreset.data();
		return impl->dataPreset.size();
	} else {
		impl->dataPlugin.resize(2000);
		std::fill(impl->dataPlugin.begin(), impl->dataPlugin.end(), 0x11);
		*data = impl->dataPlugin.data();
		return impl->dataPlugin.size();
	}

	return 0;
}
///< Host restores plug-in state
VstInt32 PluginVST2_HostInfo::setChunk (void* data, VstInt32 byteSize, bool isPreset) {
	log_printf("setChunk size %d, isPreset = %d: PTR %08X\n", byteSize, isPreset, (uint64_t)(data));
	if (isPreset && byteSize == 1000) {
		impl->dataPreset.resize(1000);
		memcpy(impl->dataPreset.data(), data, byteSize);
		return byteSize;
	} else if (!isPreset && byteSize == 2000) {
		impl->dataPreset.resize(1000);
		memcpy(impl->dataPreset.data(), data, byteSize);
		return byteSize;
	} else {
		log_printf("mismatch :( \n", 0);
	}

	return 0;
}

void PluginVST2_HostInfo::processReplacing(float** inputs, float** outputs, VstInt32 sampleFrames)
{
//	numCalls++;
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
			dsp_util::fillChannels(inputs, this->getAeffect()->numInputs, sampleFrames, 0.0f);
		dsp_util::fillChannels(outputs, this->getAeffect()->numOutputs, sampleFrames, 0.0f);
	}
//	numCalls2++;
}


Program::Program()
{
	vst_strncpy(name, "Init", kVstMaxProgNameLen);
}

}

namespace PluginHostInfo {


class guicontainer_plugin_latency : public guictr_base {
	vstplugin* vstHostSide = nullptr;
	AudioEffect* curEffect = nullptr;
	guiknob_pluginparam knobParam0;

public:
	guicontainer_plugin_latency()
	: guictr_base(), knobParam0(PARAM_OFFSET_EXTERNAL+kTestParam, kTestParam) {
		setBackgroundRendered(true);
		padding = 4;
		margin = 4;
		add(&knobParam0);
	}
	~guicontainer_plugin_latency() {
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
			case kTestParam:
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
		this->curEffect = eff;
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
		std::vector<String> strings;
//		this->curEffect->
		String str;
		str = StringFormat("Blocksize %d", this->curEffect->getBlockSize());
		strings.push_back(str);
		str = StringFormat("Samplerate %.0f", this->curEffect->getSampleRate());
		strings.push_back(str);
		AudioEffectX* effx = dynamic_cast<AudioEffectX*>(this->curEffect);
		int flags = 0;
		for (int i = 8; i < 16; i++) {
			flags |= (1<<i);
		}
		VstTimeInfo* timeinfo = effx->getTimeInfo(flags);
		assert(timeinfo);
		strings.push_back(StringFormat("samplePos %.0f", timeinfo->samplePos));
		strings.push_back(StringFormat("sampleRate %.0f", timeinfo->sampleRate));
		strings.push_back(StringFormat("nanoSeconds %.0f", timeinfo->nanoSeconds));
		strings.push_back(StringFormat("ppqPos %.0f", timeinfo->ppqPos));
		strings.push_back(StringFormat("tempo %.0f", timeinfo->tempo));
		strings.push_back(StringFormat("barStartPos %.0f", timeinfo->barStartPos));
		strings.push_back(StringFormat("cycleStartPos %.0f", timeinfo->cycleStartPos));
		strings.push_back(StringFormat("cycleEndPos %.0f", timeinfo->cycleEndPos));
		strings.push_back(StringFormat("timeSigNumerator %d", timeinfo->timeSigNumerator));
		strings.push_back(StringFormat("timeSigDenominator %d", timeinfo->timeSigDenominator));
		strings.push_back(StringFormat("smpteOffset %d", timeinfo->smpteOffset));
		strings.push_back(StringFormat("smpteFrameRate %d", timeinfo->smpteFrameRate));
		strings.push_back(StringFormat("samplesToNextClock %d", timeinfo->samplesToNextClock));
		strings.push_back(StringFormat("flags %d", timeinfo->flags));
		setFont(vg, 16, G_WHITE, NVG_ALIGN_TOP | NVG_ALIGN_LEFT);
		float lineh;
		nvgTextMetrics(vg, NULL, NULL, &lineh);
		int y = INSET_CTR_SPACING;
		int x = this->knobParam0.right()+INSET_CTR_SPACING;
		for (String& s : strings) {
			nvgText(vg, x, y, StringAsCStr(s), NULL);
			y += lineh;
		}

	}
	void layout() {
		ivec2 cs = getSizeContent();
		const int inset = 4;
//		const int knobSize = math::max(32, (cs.x-inset*3)/2);
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



class ViewContainers_Plugin_Latency : public PluginViewContainersImpl {
public:
	guicontainer_plugin_latency ctr_main;
	ViewContainers_Plugin_Latency() : PluginViewContainersImpl(280, 360)
	{
	}
	virtual ~ViewContainers_Plugin_Latency() {
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
		return new PluginVST2_HostInfo (audioMaster);
	}
	PluginViewContainers* PluginVST2_HostInfo::createView() {
		auto* v = new ViewContainers_Plugin_Latency();
		this->views.push_back(v);
		return v;
	}
}

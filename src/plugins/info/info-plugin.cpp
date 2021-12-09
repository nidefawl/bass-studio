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
#include "midi-defs.h"
#include "../synth/IPlugMidi.h"

#define PLUGIN_EFFECT_NAME "HostInfo"
#define PLUGIN_UID "INFO"
#define PLUGIN_PRODUCT_NAME "HostInfo VST2.4"

#if BUILD_EXTERNAL_PLUGIN
AudioEffect* createEffectInstance (audioMasterCallback audioMaster)
{
	return PluginHostInfo::createPlugin (audioMaster);
}
#endif


using StdThreadLock = std::lock_guard<std::recursive_mutex>;

namespace PluginHostInfo {


struct PluginVST2_HostInfo_impl_t {

	std::vector<uint8_t> dataPlugin;
	std::vector<uint8_t> dataPreset;
	std::recursive_mutex mutex;
	IMidiQueue midiQueue;
	std::vector<int> heldNotes;
	std::recursive_mutex& getMutex() {
		return mutex;
	}
	PluginVST2_HostInfo_impl_t() {

	}

	void processMidiBlockEnd(int sampleFrames)
	{
		midiQueue.Flush(sampleFrames);
		if (!midiQueue.Empty()) {
			dbgassert(0);
		}
	}
	void processMidiSamplePos(int sample)
	{
		while (!midiQueue.Empty())
		{
			auto message = midiQueue.Peek();
			if (message.mOffset > sample)
				break;

			auto status = message.StatusMsg();
			auto ctrl = message.ControlChangeIdx();
			auto note = message.NoteNumber();
			auto velocity = pow(message.Velocity() * .0078125, 1.25);

			if (status == IMidiMsg::kNoteOn && velocity == 0)
				status = IMidiMsg::kNoteOff;

			switch (status) {
				case IMidiMsg::kNoteOff:
					dbgassert(stl_contains(heldNotes, note));
					removeEntry(heldNotes, note);
//					heldNotes.erase(
//							std::remove(std::begin(heldNotes),
//									std::end(heldNotes), note),
//									std::end(heldNotes));
					break;
				case IMidiMsg::kNoteOn:
					heldNotes.push_back(note);
					break;
				case IMidiMsg::kPitchWheel: {
					break;
				}
				case IMidiMsg::kControlChange: {
					switch (ctrl) {
						case IMidiMsg::kAllNotesOff:
							//heldNotes.clear();
							break;
						default:
							break;
					}
					break;
				}
			default:
				log_printf("Unhandled midi msg %d\n", (int32_t) status);
				break;
			}
			midiQueue.Remove();
		}
	}
	void ProcessMidiMsg(IMidiMsg& msg) {
		midiQueue.Add(msg);
	}
};

PluginVST2_HostInfo::PluginVST2_HostInfo (audioMasterCallback audioMaster)
	: BasePluginVST2(audioMaster, PLUGIN_UID, kNumPrograms, kNumParams, kNumInputs, kNumOutputs), impl(new PluginVST2_HostInfo_impl_t())
{
	programsAreChunks(true);
	isSynth(true);
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

/* Return parameter properties */
bool PluginVST2_HostInfo::getParameterProperties (VstInt32 index, VstParameterProperties* p) {

	if (index == 0 && p) {
		memset(p, 0, sizeof(VstParameterProperties));
		vst_strncpy(p->label, "Dummy Parameter", kVstMaxLabelLen);
		vst_strncpy(p->shortLabel, "dummy", kVstMaxShortLabelLen);
		return true;
	}
	return false;
}
VstInt32 PluginVST2_HostInfo::processEvents (VstEvents* events) {
	assert(events);
	if (events) {
		StdThreadLock lock(impl->getMutex());
		int32_t len = events->numEvents;
		if (events->numEvents)
		log_printf("events->numEvents %d\n", events->numEvents);
		for (int i = 0; i < len; i++) {
			auto pEvent = events->events[i];
			if (pEvent->type == VstEventTypes::kVstMidiType) {
			    VstMidiEvent* pME = (VstMidiEvent*) pEvent;
			    IMidiMsg msg(pME->deltaFrames, pME->midiData[0], pME->midiData[1], pME->midiData[2]);
	            impl->ProcessMidiMsg(msg);
				log_printf("event[%d].type %d\n", i, pME->type);
				log_printf("event[%d].byteSize %d\n", i, pME->byteSize);
				log_printf("event[%d].deltaFrames %d\n", i, pME->deltaFrames);
				log_printf("event[%d].flags %d\n", i, pME->flags);
				log_printf("event[%d].noteLength %d\n", i, pME->noteLength);
				log_printf("event[%d].noteOffset %d\n", i, pME->noteOffset);
				log_printf("event[%d].midiData %02X%02X%02X%02X\n", i,
						(unsigned)pME->midiData[0], (unsigned)pME->midiData[1], (unsigned)pME->midiData[2], (unsigned)pME->midiData[3]);
				log_printf("event[%d].detune %d\n", i, (unsigned)pME->detune);
				log_printf("event[%d].noteOffVelocity %d\n", i, (unsigned)pME->noteOffVelocity);
				log_printf("event[%d].reserved1 %d\n", i, (unsigned)pME->reserved1);
				log_printf("event[%d].reserved2 %d\n", i, (unsigned)pME->reserved2);
			}
		}
	}
	return 1;
}
VstInt32 PluginVST2_HostInfo::canDo (char* text)
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
		impl->dataPreset.resize(byteSize);
		memcpy(impl->dataPreset.data(), data, byteSize);
		return byteSize;
	} else if (!isPreset && byteSize == 2000) {
		impl->dataPreset.resize(byteSize);
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
	dbgassert(sampleFrames <= blockSize);
	if (!issetprogram && sampleFrames <= blockSize) {

		for (int s = 0; s < sampleFrames; s++)
		{
			impl->processMidiSamplePos(s);
//			UpdateParameters();
//			UpdateDrift();
//			lfoValue = lfo.Get(dt, GetParamFloat(Parameters::LfoFrequency)->Value());
//			auto out = 0.0;
//			for (auto &v : voices) out += GetVoice(v);
//			out *= masterVolume;
//			outputs[0][s] = out;
//			outputs[1][s] = out;
		}
		if (this->getAeffect()->numOutputs == 1) {
			memcpy(outputs[0], inputs[0], sizeof(float)*sampleFrames);
		} else if (this->getAeffect()->numOutputs == 2) {
			memcpy(outputs[0], inputs[0], sizeof(float)*sampleFrames);
			memcpy(outputs[1], inputs[1], sizeof(float)*sampleFrames);
		}
		impl->processMidiBlockEnd(sampleFrames);
	}

//	numCalls2++;
}
PluginVST2_HostInfo_impl_t* getImpl(PluginVST2_HostInfo* plugin) {
	return plugin->impl;
}


Program::Program()
{
	vst_strncpy(name, "Init", kVstMaxProgNameLen);
}

}

namespace PluginHostInfo {


class guicontainer_plugin_HostInfo : public guictr_base {
	vstplugin* vstHostSide = nullptr;
	PluginVST2_HostInfo* curEffect = nullptr;
	guiknob_pluginparam knobParam0;

public:
	guicontainer_plugin_HostInfo()
	: guictr_base(), knobParam0(PARAM_OFFSET_EXTERNAL+kTestParam, kTestParam) {
		setBackgroundRendered(true);
		padding = 4;
		margin = 4;
		add(&knobParam0);
	}
	~guicontainer_plugin_HostInfo() {
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
		this->curEffect = dynamic_cast<PluginVST2_HostInfo*>(eff);
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
		PluginVST2_HostInfo_impl_t* curEffectImpl = getImpl(curEffect);
		if (!curEffectImpl) {
			dbgassert(0);
			return;
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
		strings.push_back(StringFormat("samplePos %.4f", timeinfo->samplePos));
		strings.push_back(StringFormat("sampleRate %.3f", timeinfo->sampleRate));
		strings.push_back(StringFormat("nanoSeconds %.2f", timeinfo->nanoSeconds));
		strings.push_back(StringFormat("ppqPos %.5f", timeinfo->ppqPos));
		strings.push_back(StringFormat("tempo %.4f", timeinfo->tempo));
		strings.push_back(StringFormat("barStartPos %.4f", timeinfo->barStartPos));
		strings.push_back(StringFormat("cycleStartPos %.4f", timeinfo->cycleStartPos));
		strings.push_back(StringFormat("cycleEndPos %.4f", timeinfo->cycleEndPos));
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
		{
			StdThreadLock lock(curEffectImpl->getMutex());
			std::vector<int> heldNotes = curEffectImpl->heldNotes; //TODO: not threadsafe
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
		}


		for (String& s : strings) {
			nvgText(vg, x, y, StringAsCStr(s), NULL);
			y += lineh;
		}




		for (guibase* gui : guis) {
			nvgSave(vg);
			gui->render(vg);
			nvgRestore(vg);
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



class ViewContainers_Plugin_HostInfo : public PluginViewContainersImpl {
public:
	guicontainer_plugin_HostInfo ctr_main;
	ViewContainers_Plugin_HostInfo() : PluginViewContainersImpl(280, 360)
	{
	}
	virtual ~ViewContainers_Plugin_HostInfo() {
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
	std::shared_ptr<PluginViewContainers> PluginVST2_HostInfo::createView() {
		std::shared_ptr<PluginViewContainers> view = std::make_shared<ViewContainers_Plugin_HostInfo>();
		this->views.push_back(view);
		return view;
	}
}

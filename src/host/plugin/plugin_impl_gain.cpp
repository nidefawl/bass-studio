#include <stdint.h>
#include <stdbool.h>
#include <vector>

#include "plugin_impl_gain.h"
#include "event.h"
#include "str_util.h"
#include "color_util.h"
#include "guicolors.h"
#include "../../gui/gui.h"
#include "../../gui/guicontainer.h"

#include "modules.h"
#include "base_plugin.h"
#include "internal_plugin.h"

#include "../host/mainctrl.h"
#include "../host/vst_host.h"
#include "../host/plugindatabase.h"
#include "../../gui/pluginctr.h"
#include "../threads/playbackthread.h"
#include "gui/pluginviewcontainers.h"

#include "track.h"
#include "track_impl.h"
#include "../../gui/guiplugin.h"

#include "audioblock.h"
#include "meter.h"
#include "track.h"
#include "track_impl.h"
#include "snapshot.h"
#include "file/memoryarchive.h"
#include "host/effect_graph.h"

class guiknob_pluginparameter : public guiknob_labeled_base {
	effectbase* hostSidePlugin = nullptr;
public:
	guiknob_pluginparameter(int _paramIdx) : guiknob_labeled_base(false) {
		paramIdx = _paramIdx;
		fnValueEditChanged = [this](float preVal, float val) {
			if (hostSidePlugin) {
				hostSidePlugin->setParamValue(paramIdx, val, FLG_PAR_UPDATE_USER);
				setDisplayValueFromEffect();
			}
		};
		setAutomationHandlers();
	}
	virtual ~guiknob_pluginparameter() {
	}
	void setEffectInstance(effectbase* eff) {
		this->hostSidePlugin = eff;
		paramAutomatable = eff;
		if (eff) {
			setValueInit(hostSidePlugin->getParamValue(paramIdx));
			setLabel(hostSidePlugin->getParamName(paramIdx));
		}
		setDisplayValueFromEffect();
	}
	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
		if (this->contains(mpos)) {
			if (evt.type != MouseHitType::MOUSE_RIGHT)
			{
				if (guiknob::mouseHitTest(mpos, evt)) {
					return true;
				}
			}
			evt.requestFocus(this);
			return true;
		}
		return false;
	}
//#endif
	void setDisplayValueFromEffect() {
		if (this->hostSidePlugin) {
			String display = hostSidePlugin->formatDisplayValue(paramIdx);
//			String displayUnit = curEffect->getParamName(internalEffectIdx);
			this->valueDisplay = display;
		} else {

			this->valueDisplay = "???";
		}
	}
	virtual void setDisplayValue(float f) override {
	}
};
//base class for plugin UIs of internal effects
class guieffect_internal : public guictr_base {
protected:
	internalplugin* const module;
	std::vector<guiknob_pluginparameter*> knobs;
	void addKnob(guiknob_pluginparameter* knob) {
		knobs.push_back(knob);
		add(knob);
	}
public:
	guieffect_internal(module_gain* _eff);
	~guieffect_internal() {
		removeGuis();
	}

};

class guimodule_gain : public guieffect_internal {
	guiknob_pluginparameter knobgain;
public:
	guimodule_gain(module_gain* _eff);
	~guimodule_gain() {
		removeGuis();
	}
	void determineSize(ivec2& prefSize) override {
	}
	void layout() override {
		ivec2 layoutpos = ivec2(0);
		ivec2 cs = getSizeContent();

		const int numElements = 1;
		const int inset = 4;
		const int knobSize = math::max(32, (cs.x-inset*(numElements+1))/numElements);
		knobgain.size = ivec2(knobSize, cs.y-inset*2);
		knobgain.pos = layoutpos + ivec2(inset);
		for (guibase* gui : guis) {
			gui->layout();
		}
	}
	void render(NVGcontext* vg) {
	//	nvgBeginPath(vg);
	//	nvgRect(vg, pos.x, pos.y, size.x, size.y);
	//	nvgFillColor(vg, GUI_COLORRGB(50, 50, 150, 180));
	//	nvgFill(vg);
		if (isBackgroundRendered()) {
			renderBackground(vg);
		}
		if (!setScissorTransform(vg)) {
			return;
		}
	//	ivec2 cs = getSizeContent();
	//	nvgBeginPath(vg);
	//	nvgRect(vg, 0, 0, cs.x, cs.y);
	//	nvgFillColor(vg, GUI_COLORRGB(50, 150, 150, 180));
	//	nvgFill(vg);
	//	nvgBeginPath(vg);
	//	const int INS = 2;
	//	nvgRect(vg, INS, INS, cs.x-INS*2, cs.y-INS*2);
	//	nvgFillColor(vg, GUI_COLORRGB(150, 150, 50, 180));
	//	nvgFill(vg);

		for (guibase* gui : guis) {
			nvgSave(vg);
			gui->render(vg);
			nvgRestore(vg);
		}

	}

};
guieffect_internal::guieffect_internal(module_gain* _eff)
	: guictr_base(),
	  module(_eff)
{
}
guimodule_gain::guimodule_gain(module_gain* _eff)
: guieffect_internal(_eff), knobgain(PARAM_GAIN)
{
    setBackgroundRendered(true);
    padding = 4;
    margin = 4;
	addKnob(&knobgain);
//	isHorizontalTitle = false;
	knobgain.setEffectInstance(module);
}



struct module_gain::internal_handles_t {
//	std::unique_ptr<guimodule_gain> gui;
};

module_gain::module_gain(int32_t _projectGlobalId)
: internalplugin("Gain", PLUGIN_TYPE_GAIN, _projectGlobalId), handle(new module_gain::internal_handles_t{})
{
//#define PARAM_GROUPPLUGIN_INPUT_GAIN PARAM_OFFSET_IMPL
	struct effectgroup_param_entry_t {
		int32_t id;
		String name;
		float val;
	};
	const std::array<effectgroup_param_entry_t, 2> parameterTypes { {
		{PARAM_GAIN, "Gain", 1.0f},
		{PARAM_PAN, "Pan", 0.5f},
//		{PARAM_GROUPPLUGIN_INPUT_GAIN, "Input Gain", 1.0f},
	} };
	for (const effectgroup_param_entry_t& paramEntry : parameterTypes) {
		automatable_param_t* regparam = registerParam(paramEntry.id);
		regparam->value = paramEntry.val;
		regparam->label = paramEntry.name;
		regparam->shortLabel = paramEntry.name;
	}
}
module_gain::~module_gain()
{
	delete handle;
	if (blockInputs)
		delete blockInputs;
	if (blockOutputs)
		delete blockOutputs;
}



float module_gain::dispatchGetParameter(int32_t idx) {
	return 0;
}
void module_gain::dispatchSetParameter(int32_t idx, float val) {

}
int32_t module_gain::getDelay() {
	return 0;
}
void module_gain::resume() {
}
void module_gain::sleep() {
}
void module_gain::unload(vsthost* host) {
	dbgassert(vstHost == host);
	effectbase::unload(host);
	onPreUnload();
}
void module_gain::onPreUnload() {
}
void module_gain::load(vsthost* host) {
	effectbase::load(host);
	this->blockInputs = new AudioBlock(2, host->sampleFormat.blockSize);
	this->blockOutputs = new AudioBlock(2, host->sampleFormat.blockSize);
	bIsEnabled = this->getParamValue(PARAM_ENABLE) > 0.5;
	if (bIsEnabled) {
		this->resume();
	} else {
		this->sleep();
	}
}

void module_gain::breakTrackLink() {
	bIsSetup = false;
	internalplugin::breakTrackLink();
}
void module_gain::setTrackLink(audio_stage_t* trImpl) {
	bIsSetup = true;
	internalplugin::setTrackLink(trImpl);
}

String module_gain::getInfo(std::vector<String>& list) {
	return "";
}

void module_gain::onTick(double since) {
	meter.onTick(since);
	meterIn.onTick(since);
}

void module_gain::process(AudioBlock* in, AudioBlock* out, int32_t samplePos, int32_t numSamples, playback_state state) {
	dbgassert(getTrackLink()->sampleFormat == this->format && in->samples == format.blockSize
			&& out->samples == format.blockSize && format.blockSize > 0 && format.sampleRate > 0);

    out->clear();
	/* Calculate group gain level */
	float fGain = 1.0f;
    if (dsp_util::getGainLvl(getParamValue(PARAM_GAIN), fGain)) {
        out->addFromOp(in, AudioBlock::mix_op::ADD, math::clamp(fGain, 0.0f, 2.0f));
	}
}
void module_gain::postProcess(AudioBlock* out, int32_t samples, bool hasProcessed) {
	meterIn.update(this->blockInputs, 1.0f);
	meter.update(out, 1.0f);
}

void module_gain::loadSnapshot(const plugin_snapshot_t& pluginSnapshot)  {
	internalplugin::loadSnapshot(pluginSnapshot);
}
void module_gain::makeSnapshot(plugin_snapshot_t& snapshot, bool storePluginChunks) {
	internalplugin::makeSnapshot(snapshot, storePluginChunks);
}
String module_gain::formatDisplayValue(int32_t idx) {
	float fGain = 1.0f;
    if (dsp_util::getGainLvl(getParamValue(PARAM_GAIN), fGain)) {
    	return StringFormat("%.3f dB", dsp_util::dBFS(fGain));
	}
    return "-INF";
}


class ViewContainersModuleGain : public PluginViewContainers {
protected:
	uint32_t width;
	uint32_t height;
public:
	guimodule_gain ctr_main;
	ViewContainersModuleGain(module_gain* moduleGain, uint32_t _width, uint32_t _height) : width(_width), height(_height), ctr_main(moduleGain)
	{
	}
	virtual ~ViewContainersModuleGain() {
	}
	void layout(int32_t winW, int32_t winH) override {
		ctr_main.pos = {0, 0};
		ctr_main.size = {winW, winH};
	}
	void addTo(std::vector<guictr_base*>& v) override {
		 v.push_back(&ctr_main);
	}
	void onGuiOpen(AudioEffect* eff) override {
//		ctr_main.onGuiOpen(eff);
	}
	void onGuiClose(AudioEffect* eff) override {
//		ctr_main.onGuiClose(eff);
	}
	void onSetParameter(int32_t index, float value) override {
//		ctr_main.onSetParameter(index, value);
	}
	void getFixedSize(int32_t* w, int32_t* h) override {
		*w = this->width;
		*h = this->height;
	}
	void setVSTPlugin(vstplugin* hostsideplugin)  {
//		ctr_main.setVSTPlugin(hostsideplugin);
	}
};
//namespace PluginStereoWidth {
//	AudioEffectX* createPlugin (audioMasterCallback audioMaster)
//	{
//		return new PluginVST2_StereoWidth (audioMaster);
//	}
//}
std::shared_ptr<PluginViewContainers> module_gain::createInternalView() {
	if (views.size()) {
		for (auto& existingView : views) {
			if (!existingView->isInUse()) {
				existingView->setUsed();
				return existingView;
			}
		}
	}
	auto v = std::make_shared<ViewContainersModuleGain>(this, 320, 320);
	this->views.push_back(v);
	return v;
}
template<>
effectbase* makeInstance<module_gain>(int32_t _projectGlobalId) {
	return new module_gain(_projectGlobalId);
}


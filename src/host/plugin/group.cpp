#include <stdint.h>
#include <stdbool.h>
#include <vector>
#include <fstream>
#include <sstream>
#include <streambuf>
#include <algorithm>
#include <memory>
#include <cereal/cereal.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/polymorphic.hpp>
#include <cereal/cereal_optional_nvp.hpp>

#include "group.h"
#include "math/seq_math.h"
#include "event.h"
#include "str_util.h"
#include "color_util.h"
#include "guicolors.h"
#include "renderresources.h"
#include "../../gui/list.h"
#include "../../gui/guimeter.h"
#include "../../gui/knob.h"
#include "../../gui/button.h"
#include "../../gui/gui.h"
#include "../../gui/guicontainer.h"
#include "../../gui/guiplugin.h"
#include "../../gui/pluginctr.h"
#include "../../gui/pluginlist.h"

#include "base_plugin.h"
#include "internal_plugin.h"

#include "basectrl.h"
#include "../host/mainctrl.h"
#include "../host/vst_host.h"
#include "../host/plugindatabase.h"
#include "../threads/playbackthread.h"

#include "track.h"
#include "track_impl.h"
#include "snapshot.h"
#include "../../file/memoryarchive.h"

class guimodule_group : public guiplugin {
public:
	module_group* const module;
	guictr_plugins ctr;
	guimodule_group(module_group* _vst);
	~guimodule_group() {
		remove(&ctr);
	}
	void render(NVGcontext* vg) override;
	void buttonClicked(guibase* _button) override;
	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
	void onChildLayoutChanged(guibase* g) override;
	void determineSize(glm::ivec2& prefSize) override {
		assert(module->getAudioStage());

		const int32_t hpt = theme->get(GuiConstant::CONST_PLUGIN_TITLE_HEIGHT);
		int32_t meterW = math::max(16, (int32_t)(theme->get(GuiConstant::CONST_METER_WIDTH)*hpt/32.0));
		ctr.pos = ivec2(hpt, 0);
//		ctr.size = ivec2(size.y, size.y);
//		ctr.layout();
//		my_printf("determineSize ctr.children.size() %d\n", ctr.guis.size());
//		int len = (int)ctr.guis.size();
//		for (int i = 0; i < len; i++) {
//			guibase* g = ctr.guis[i];
//			my_printf("%s guis[%d] %d %d %d %d\n", StringAsCStr(ctr.getClassName()), i, g->pos.x, g->pos.y, g->size.x, g->size.y);
//		}
		glm::ivec2 prefSizeGrpContent = {prefSize.y, prefSize.y};
		ctr.size = prefSizeGrpContent;
//		ctr.determineSize(prefSizeGrpContent);
		ctr.layout();
		prefSize.x = hpt+ctr.size.x+meterW;
	}
	void layoutModule(ivec2 pos, ivec2 contentS, int32_t inset1) override {
	}
	void removeGuis() override {
		removeUNCHECKED(&ctr);
		for (guibase* g : guis) {
			g->onRemove();
			g->setParent(nullptr);
		}
		guis.clear();
		addUNCHECKED(&ctr);
	}
	void onAdded() {
		ctr.showTrack(module->getAudioStage());
	}
};

guimodule_group::guimodule_group(module_group* _vst)
: guiplugin(_vst),
  module(_vst) {
	isHorizontalTitle = false;
	ctr.isDefaultPluginCtr = false;
	ctr.margin = ctr.padding = 0;
	add(&ctr);
}
void guimodule_group::onChildLayoutChanged(guibase* g) {
//	ctr.showTrack(module->getAudioStage());
	layout();
	if (this->parent != NULL) {
		this->parent->onChildLayoutChanged(this);
	}
}

void guimodule_group::render(NVGcontext* vg) {
	assert(ctr.parent == this);
	dragdrop_target_indicator& target = MainCtrl::get()->getDragDropTarget();
	bool extend = target.ptr == &this->ctr;
	int extX = 8;
	if (extend) {
		size.x += extX;
	}
	renderBase(vg);
	nvgSave(vg);
	ctr.render(vg);
	nvgRestore(vg);
	buttonBypass.render(vg);
	if (extend) {
		nvgTranslate(vg, extX, 0);
	}
	buttonDelete.render(vg);

	meter.render(vg);
	if (extend) {
		size.x -= extX;
	}
}
bool guimodule_group::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
	if (contains(mpos)) {
		if (evt.getDraggedThing() == this)
			return false;
		ivec2 localMouse = this->toContainerSpace(mpos);
		if ( evt.type == MouseHitType::MOUSE_DRAGDROP_OBJECT) {
			if (ctr.mouseHitTest(localMouse, evt)) {
				return true;
			}
			const int32_t hpt = theme->get(GuiConstant::CONST_PLUGIN_TITLE_HEIGHT);
			if (localMouse.x <= hpt-10 || localMouse.x > size.x-hpt+10)
				return false;
			evt.requestFocus(&this->ctr);
			return true;
		}
		if (buttonBypass.mouseHitTest(localMouse, evt)) {
			return true;
		}
		if (buttonDelete.mouseHitTest(localMouse, evt)) {
			return true;
		}
		if (ctr.mouseHitTest(localMouse, evt)) {
			return true;
		}
		if (isShift(evt.kbmods)) {
			if (MainCtrl::get()->getPluginSel().pluginCtr != this->parent) {
				return true;
			}
		}
		evt.requestFocus(this);
		return true;
	}
	return false;
}
void guimodule_group::buttonClicked(guibase* _button) {
	if (_button == &buttonBypass) {
    	ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
    	float f = effect->getParamValue(PARAM_ENABLE);
    	float f2 = f > 0.5 ? 0 : 1;
    	effect->setParamValue(PARAM_ENABLE, f2, 2);
    	effect->postSetParameter(PARAM_ENABLE, f, f2, 2);

	}
	if (_button == &buttonDelete) {
    	removePlugin(module);
	}
}


struct module_group::internal_handles_t {
	std::unique_ptr<guimodule_group> gui;
//	guimodule_group * gui;
};
struct module_group_preset {
	std::vector<int32_t> plugins;
};
module_group::module_group(int32_t _projectGlobalId)
: internalplugin("Group", PLUGIN_TYPE_GROUP, _projectGlobalId), handle(new module_group::internal_handles_t{0}), audio(nullptr)
{
}
module_group::~module_group()
{
	delete handle;
	if (blockInputs)
		delete blockInputs;
	if (blockOutputs)
		delete blockOutputs;
}



float module_group::dispatchGetParameter(int32_t idx) {
	return 0;
}
void module_group::dispatchSetParameter(int32_t idx, float val) {

}
guiplugin* module_group::makeGui() {
	if (!handle->gui) {
		assert(this->audio);
		handle->gui = std::make_unique<guimodule_group>(this);
		handle->gui->setTitle(StringFormat("%s", StringAsCStr(this->sName)));
		this->audio->pluginCtr = &this->handle->gui->ctr;
		handle->gui->ctr.stage = this->audio;
		handle->gui->ctr.track = this->audio->getTrack();
	}
	return handle->gui.get();
//	return handle->gui;
}
guiplugin* module_group::getGui() {
	return handle->gui.get();
//	return handle->gui;
}
int32_t module_group::getDelay() {
	return audio->getLatency();
}
void module_group::resume() {
}
void module_group::sleep() {
}
void module_group::unload(vsthost* host) {
	effectbase::unload(host);
	onPreUnload();
	host->releaseAudioStage(audio);
	this->audio = nullptr;
}
void module_group::onPreUnload() {
	vsthost* host = vsthost::getInstance();
	std::vector<effectbase*> effects = this->audio->effects; // make a copy before unloading plugins
	for (effectbase* effect : effects) {
		host->unloadPlugin(effect);
	}
}
void module_group::load(vsthost* host) {
	effectbase::load(host);
	this->audio = host->createAudioStage();
	this->blockInputs = new AudioBlock(2, host->lBlockSize);
	this->blockOutputs = new AudioBlock(2, host->lBlockSize);
	bIsEnabled = this->getParamValue(PARAM_ENABLE) > 0.5;
	if (bIsEnabled) {
		this->resume();
	} else {
		this->sleep();
	}
}

void module_group::breakTrackLink() {
	assert(this->audio);
	assert(this->audio->parent);
	this->audio->parent->removeAudioStage(this->audio);
	assert(this->audio->parent == nullptr);
	this->audio->owner = nullptr;
	bIsSetup = false;
	internalplugin::breakTrackLink();
}
void module_group::setTrackLink(audio_stage_t* trImpl) {
	assert(this->audio);
	assert(trImpl != this->audio);
	trImpl->addAudioStage(this->audio);
	assert(this->audio->parent == trImpl);
	this->audio->owner = this;
	bIsSetup = true;
	internalplugin::setTrackLink(trImpl);
}
void module_group::process(AudioBlock* in, AudioBlock* out, int32_t samples) {
	audio->input.copyFrom(in);
	vsthost::getInstance()->processAudio(audio, &audio->input, &audio->output, samples);
	out->copyFrom(&audio->output);
}
String module_group::getInfo(std::vector<String>& list) {
	return "";
}

void module_group::onTick(double since) {
	meter.onTick(since);
	audio->onTick(since);
}
void module_group::getChildAudioStages(std::vector<audio_stage_t*>& targets) {
	targets.push_back(this->audio);
}
void module_group::postProcess(AudioBlock* out, int32_t samples, bool hasProcessed) {
	meter.update(out);
	if (!hasProcessed) {
		for (effectbase* effect : audio->effects) {
			effect->postProcess(out, samples, hasProcessed);
		}
	}
}
using namespace cereal;

template<class Archive>
void serialize(Archive & archive, module_group_preset & m)
{
	archive(cereal::make_nvp("plugins", m.plugins));
};

void module_group::loadSnapshot(const plugin_snapshot_t& pluginSnapshot)  {
	assert(audio);
	this->audio->loadPlugins(pluginSnapshot.pluginSnapshots);
}
void module_group::makeSnapshot(plugin_snapshot_t& snapshot, bool storePluginChunks) {
	assert(audio);
	internalplugin::makeSnapshot(snapshot, storePluginChunks);
	std::vector<effectbase*> effects = audio->effects;
	snapshot.pluginSnapshots.reserve(effects.size());
	for (effectbase* effect : effects) {
		plugin_snapshot_t ps;
		effect->makeSnapshot(ps, storePluginChunks);
		snapshot.pluginSnapshots.push_back(std::move(ps));
	}
}


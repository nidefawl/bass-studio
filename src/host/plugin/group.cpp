#include <stdint.h>
#include <stdbool.h>
#include <glm/glm.hpp>
#include <glm/vec2.hpp>
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
#include "../../file/memoryarchive.h"

#include "group.h"
#include "event.h"
#include "str_util.h"
#include "color_util.h"
#include "guicolors.h"
#include "renderresources.h"
#include "../../gui/list.h"
#include "../../gui/guimeter.h"
#include "../../gui/knob.h"
#include "../../gui/gui.h"
#include "../../gui/guicontainer.h"
#include "../../gui/button.h"
#include "../../gui/plugin.h"
#include "../../gui/pluginctr.h"
#include "../../gui/pluginlist.h"

#include "base_plugin.h"
#include "internal_plugin.h"

#include "../host/mainctrl.h"
#include "../host/vst_host.h"
#include "../host/plugindatabase.h"
#include "../threads/playbackthread.h"

#include "track.h"
#include "track_impl.h"
#include "leak_detect.h"
#include "snapshot.h"

using glm::vec2;
using glm::ivec2;


constexpr int32_t meterW = HEIGHT_PLUGIN_TITLE;
class guimodule_group : public guiplugin {
public:
	module_group* const module;
	guictr_plugins ctr;
	guimodule_group(module_group* _vst);
	~guimodule_group() {
		remove(&ctr);
		my_printf("DSTR!\n",0);
	}
	void render(NVGcontext* vg) override;
	void renderBase(NVGcontext* vg) override;
	void buttonClicked(guibase* _button) override;
	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
	void onChildLayoutChanged(guibase* g) override;
	void determineSize() override {
		assert(module->getAudioStage());

		ctr.size = ivec2(size.y, size.y);
		ctr.layout();
		my_printf("determineSize ctr.children.size() %d\n", ctr.guis.size());
		int len = (int)ctr.guis.size();
		for (int i = 0; i < len; i++) {
			guibase* g = ctr.guis[i];
			my_printf("%s guis[%d] %d %d %d %d\n", StringAsCStr(ctr.getClassName()), i, g->pos.x, g->pos.y, g->size.x, g->size.y);
		}
		ctr.determineSize();
		size.x = HEIGHT_PLUGIN_TITLE+meterW;
		size.x += ctr.size.x;
	}
	void layout() override {
		int32_t inset1 = (HEIGHT_PLUGIN_TITLE - buttonBypass.size.y) / 2;
		ivec2 contentS(size.x - meterW-HEIGHT_PLUGIN_TITLE, size.y);
		ivec2 contentP(HEIGHT_PLUGIN_TITLE, 0);
		buttonBypass.pos.y = inset1;
		buttonBypass.pos.x = inset1;
		buttonDelete.pos.y = inset1;
		buttonDelete.pos.x = size.x - buttonDelete.size.x - inset1;
		titlePosX = buttonBypass.right();
		layoutModule(contentP, contentS, inset1);
		meter.pos = ivec2(size.x - meterW, HEIGHT_PLUGIN_TITLE);
		meter.size = ivec2(meterW, contentS.y);
		meter.layout();
	}
	void layoutModule(ivec2 pos, ivec2 contentS, int32_t inset1) override {
		ctr.pos = pos;
//		ctr.size = contentS;
//		assert(ctr.parent == this);
//		ctr.placeholder.size.x = std::max(100, size.y*3/5);
		titlePosX = 0;
	}
	void removeGuis() override {
		removeUNCHECKED(&ctr);
		for (guibase* g : guis) {
			g->onRemove();
			g->parent = NULL;
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

void guimodule_group::renderBase(NVGcontext* vg) {
	if (!setScissorTransform(vg)) {
		return;
	}
	nvgBeginPath(vg);
	nvgRoundedRect(vg, 0, 0, size.x, size.y, G_RND);
	NVGcolor c;
	if (MainCtrl::get()->isCtrOrChildFocused(this)) {
		c = g_guiColors[COL_BG_DRK_FOCUSED];
	} else {
		c = g_guiColors[COL_BG_BRT];
	}
	nvgFillColor(vg, GUI_COLOR(G_S2));
	nvgFill(vg);
	nvgBeginPath(vg);
	nvgRoundedRectVarying(vg, 0, 0, HEIGHT_PLUGIN_TITLE, size.y, G_RND, G_RND, 0, 0);
	nvgFillColor(vg, c);
	nvgFill(vg);
	if (this->text[0]) {
		setFont(vg, (int)(HEIGHT_PLUGIN_TITLE*0.8), G_WHITE, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
		nvgSave(vg);
		nvgTranslate(vg, titlePosX+HEIGHT_PLUGIN_TITLE/2, size.y);
		nvgRotate(vg, -M_PI/2.0);
//		nvgTranslate(vg, -HEIGHT_PLUGIN_TITLE, 0);
//		nvgText(vg, 0, 0, StringAsCStr(this->text), NULL);
		nvgText(vg, INSET_TITLE*2, 0, StringAsCStr(this->text), NULL);
		nvgRestore(vg);
	}
	nvgBeginPath(vg);
	nvgRoundedRect(vg, 0, 0, size.x, size.y, G_RND);
	nvgStrokeColor(vg, GUI_COLOR(G_S1));
	nvgStrokeWidth(vg, G_STROKE);
	nvgStroke(vg);
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
			if (localMouse.x <= HEIGHT_PLUGIN_TITLE-10 || localMouse.x > size.x-HEIGHT_PLUGIN_TITLE+10)
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


struct internal_handles_t {
	std::unique_ptr<guimodule_group> gui;
//	guimodule_group * gui;
};
struct module_group_preset {
	std::vector<int32_t> plugins;
};
module_group::module_group(int32_t _projectGlobalId)
: internalplugin(PLUGIN_TYPE_GROUP, _projectGlobalId), handle(new internal_handles_t{0}), audio(nullptr)
{
	this->sName = "Group";
#ifndef NDEBUG
		this->szName = this->sName.c_str();
#endif
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
void module_group::unload() {
	onPreUnload();
	delete this->audio;
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
	bIsSetup = false;
	internalplugin::breakTrackLink();
}
void module_group::setTrackLink(audio_stage_t* trImpl) {
	assert(this->audio);
	assert(trImpl != this->audio);
	trImpl->addAudioStage(this->audio);
	this->audio->parent = trImpl;
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


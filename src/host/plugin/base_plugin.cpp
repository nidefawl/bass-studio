#include <vector>
#include "base_plugin.h"
#include "track.h"
#include "track_impl.h"
#include "str_util.h"
#include "logging.h"
#include "snapshot.h"
#include "saferef.h"

#include "gui/gui.h"
#include "meter.h"
#include "gui/guicontainer.h"
#include "gui/guiplugin.h"
#include "gui/button.h"
#include "gui/pluginctr.h"
#include "host/mainctrl.h"
#include "host/vst_host.h"

//static std::vector<plugin_snapshot_t> vec;

track_t* effectbase::getTrack() {
	audio_stage_t* stage = getTrackLink();
	if (!stage)
		return nullptr;
	return stage->getTrack();
}

SafeRef<effectbase> effectbase::makeSafeRef() {
	if (!safeRef.handler) {
		safeRef.handler = vsthost::getInstance()->getSafeRefStore();
		safeRef.refId = safeRef.handler->safeRefCreate(this);
	}
	return safeRef;
}
effectbase::~effectbase() {
	if (safeRef.handler) {
		safeRef.handler->safeRefDestroy(safeRef.refId);
	}
}
effectbase::effectbase(String _sName, int32_t _pluginType, int32_t _projectGlobalId)
: pluginType(_pluginType), projectGlobalId(_projectGlobalId), sName(_sName) {
#define PARAM_PLUGIN_DUMMY 1
	struct effectbase_param_entry_t {
		int32_t id;
		String name;
		float val;
	};
	const std::array<effectbase_param_entry_t, 2> parameterTypes { {
		{PARAM_ENABLE, "Enabled", 1.0f},
		{PARAM_PLUGIN_DUMMY, "Dummy", 1.0f},
	} };
	for (const effectbase_param_entry_t& paramEntry : parameterTypes) {
		automatable_param_t* regparam = registerParam(paramEntry.id);
		regparam->value = paramEntry.val;
		regparam->label = paramEntry.name;
		regparam->shortLabel = paramEntry.name;
	}
	getAutomation(PARAM_ENABLE)->quantizationSteps = 1;
}
effectbase::effectbase() : pluginType(0), projectGlobalId(0), sName("") {

}
void effectbase::onTick(double since) {
	meter.onTick(since);
}

void effectbase::postProcess(AudioBlock* out, int32_t samples, bool hasProcessed) {
	meter.update(out);
}

void effectbase::breakTrackLink() {
	audio_stage_t* audioStage = this->trackImpl;
	trackImpl = nullptr;
	while (audioStage != nullptr) {
		guictr_plugins* pluginCtr = audioStage->pluginCtr;
		if (pluginCtr) {
			assert(MainCtrl::get());
			plugin_selection& sel = MainCtrl::get()->getPluginSel();
			if (sel.pluginCtr == pluginCtr) {
				sel.clear();
			}
			my_printf("Update audiostage of %s which is %s\n", StringAsCStr(pluginCtr->getClassName()),
				pluginCtr->isDefaultPluginCtr ? "default" : "group");
			pluginCtr->showTrack(audioStage);
		}
		audioStage = audioStage->parent;
	}
}
void effectbase::setTrackLink(audio_stage_t* audioStage) {
	trackImpl = audioStage;
	while (audioStage != nullptr) {
		guictr_plugins* plugins = audioStage->pluginCtr;
		if (plugins) {
			my_printf("Update audiostage of %s which is %s\n", StringAsCStr(plugins->getClassName()),
					plugins->isDefaultPluginCtr ? "default" : "group");
			plugins->showTrack(audioStage);
		}
		audioStage = audioStage->parent;
	}
}

class guideferred : public guiplugin {
	effect_deferred* const module;
	guibutton btnLoad;
public:
	guideferred(effect_deferred* _eff) : guiplugin(_eff), module(_eff) {
		isHorizontalTitle = false;
		buttonBypass.icon = -1;
		buttonBypass.colorActive = GuiColor::COL_BTN_BG_DEFAULT_ACTIVE;
		buttonBypass.getState = nullptr;
		meter.setVisible(false);
		add(&btnLoad);
	}
	~guideferred() {
		remove(&btnLoad);
	}
	void render(NVGcontext* vg) override;
	void buttonClicked(guibase* _button) override;
	void layoutModule(ivec2 pos, ivec2 contentS, int32_t inset1) override {
		btnLoad.pos = pos;
		btnLoad.size = contentS;
		btnLoad.layout();
		btnLoad.setFontSize(math::max(12, size.y/8));
		btnLoad.setText(String("Load\n")+module->getName());
	}
//	void determineSize(ivec2& prefSize) override {
////		assert(module->getAudioStage());
////
//		const int32_t hpt = theme->get(GuiConstant::CONST_PLUGIN_TITLE_HEIGHT);
////		int32_t meterW = math::max(16, (int32_t)(theme->get(GuiConstant::CONST_METER_WIDTH)*hpt/32.0));
////		prefSize.x = hpt+ctr.size.x+meterW;
//		prefSize.x = hpt;
//	}
};
struct effect_deferred_impl {
	plugin_snapshot_t snapshot;
	std::unique_ptr<guideferred> gui = nullptr;
	int moduleType = 0;
};
effect_deferred::~effect_deferred() {
	delete mImpl;
}
String effect_deferred::getDfrdPluginName() {
	return mImpl->snapshot.name;
}
void effect_deferred::onPreUnload() {
	my_printf("onPreUnload effect_deferred %08X %s\n", (int64_t)mImpl, StringAsCStr(mImpl->snapshot.name));
}
plugin_snapshot_t effect_deferred::getSnapshot() const {
	return mImpl->snapshot;
}
//toDeferred()
//static std::shared_ptr<effect_deferred> fromEffect();
/*static*/ std::shared_ptr<effect_deferred> effect_deferred::fromEffect(effectbase* eff) {
	auto def = eff->toDeferred();
	return std::shared_ptr<effect_deferred>(def);
}
effect_deferred* effectbase::toDeferred() {
	effect_deferred* eff = new effect_deferred();
	eff->mImpl = new effect_deferred_impl();
	return eff;
}

//std::shared_ptr<effect_deferred> loadPluginDeferred(const plugin_snapshot_t& snapshot) {
//	auto def = std::make_shared<effect_deferred>();
//	def->mImpl = new effect_deferred_impl();
//	def->mImpl->snapshot = snapshot;
//	def->mImpl->moduleType = snapshot.pluginType;
//	return def;
//}

effect_deferred* loadPluginDeferred(const plugin_snapshot_t& snapshot) {
	auto def = new effect_deferred();
	def->mImpl = new effect_deferred_impl();
	def->sName = snapshot.name;
	def->projectGlobalId = snapshot.projectGlobalId;
	def->bIsEnabled = snapshot.enabled;
//	def->uId = snapshot.uId;
	def->mImpl->snapshot = snapshot;
	def->mImpl->moduleType = snapshot.pluginType;
	return def;
}


void effect_deferred::loadSnapshot(const plugin_snapshot_t& snapshot) {
	this->mImpl->snapshot = snapshot;
	this->mImpl->moduleType = snapshot.pluginType;
}
int32_t effect_deferred::getDelay() {
	return 0;
}
String effect_deferred::getInfo(std::vector<String>& list) {
	return "";
}
int effect_deferred::getModuleType() {
	return PLUGIN_TYPE_DEFERRED;
}
void effect_deferred::makeSnapshot(plugin_snapshot_t& ps, bool storePluginChunks) {
	ps = this->mImpl->snapshot;
}
void effect_deferred::process(AudioBlock* in, AudioBlock* out, int32_t samples) {

}
bool effect_deferred::show() {
	return false;
}
bool effect_deferred::close() {
	return false;
}
void effect_deferred::resume() {

}
void effect_deferred::sleep() {

}
String effect_deferred::getAutomatableName() {
	return "plugin";
}
float effect_deferred::getParamValue(int32_t idx) {
	return 0;
}
void effect_deferred::setParamValue(int32_t idx, float val, int flags) {

}
automationlane_snapshot_t effect_deferred::toRef() {
	automationlane_snapshot_t ref;
	ref.type = AUTOMATABLE_EFFECT;
	ref.refId = this->projectGlobalId;
	return ref;
}
void guideferred::render(NVGcontext* vg) {
	btnLoad.setVisible(layoutMode==0);
	guiplugin::render(vg);
}



void guideferred::buttonClicked(guibase* _button) {
	guiplugin::buttonClicked(_button);
	if (_button == &btnLoad) {
		ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
		vsthost* host = vsthost::getInstance();
		host->activateDeferred(module);
	}
}
guiplugin* effect_deferred::makeGui() {
	assert(this->mImpl);
	if (!this->mImpl->gui) {
		this->mImpl->gui = std::make_unique<guideferred>(this);
		this->mImpl->gui->setTitle(StringFormat("%s", StringAsCStr(this->sName)));
	}
	return this->mImpl->gui.get();
}
guiplugin* effect_deferred::getGui()
{
	assert(this->mImpl);
	assert(this->mImpl->gui.get());
	return this->mImpl->gui.get();
}

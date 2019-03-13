#include "base_plugin.h"
#include "track.h"
#include "track_impl.h"
#include "str_util.h"
#include "logging.h"

#include "../../gui/guiplugin.h"
#include "../../host/mainctrl.h"
#include "../../gui/pluginctr.h"



track_t* effectbase::getTrack() {
	audio_stage_t* stage = getTrackLink();
	if (!stage)
		return nullptr;
	return stage->getTrack();
}
effectbase::effectbase(int32_t _pluginType, int32_t _projectGlobalId) : pluginType(_pluginType), projectGlobalId(_projectGlobalId) {
	struct effectbase_param_entry_t {
		String name;
		float val;
	};
	const std::array<effectbase_param_entry_t, 1> parameterTypes { {
		{"Enabled", 1.0f},
	} };
	params.reserve(parameterTypes.size());
	int32_t idx = 0;
	for (const auto& paramEntry : parameterTypes) {
		automatable_param_t automatable = {};
		automatable.idx = idx;
		automatable.internalIdx = -1;
		automatable.category = 0;
		automatable.value = paramEntry.val;
		automatable.label = paramEntry.name;
		automatable.shortLabel = paramEntry.name;
		params.push_back(automatable);
		mixerParams.push_back(automatable);
		idx++;
	}
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
		plugin_selection& sel = MainCtrl::get()->getPluginSel();
		if (sel.pluginCtr == audioStage->pluginCtr) {
			sel.clear();
		}
		guictr_plugins* plugins = audioStage->pluginCtr;
		if (plugins) {
			my_printf("Update audiostage of %s which is %s\n", StringAsCStr(plugins->getClassName()),
					plugins->isDefaultPluginCtr ? "default" : "group");
			plugins->showTrack(audioStage);
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

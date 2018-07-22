#include "base_plugin.h"
#include "track.h"
#include "track_impl.h"
#include "str_util.h"
#include "logging.h"
#include "../../gui/plugin.h"
#include "../../gui/pluginctr.h"



track_t* effectbase::getTrack() {
	audio_stage_t* stage = getTrackLink();
	if (!stage)
		return nullptr;
	return stage->getTrack();
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

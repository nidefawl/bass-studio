#include "base_plugin.h"
#include "track.h"
#include "track_impl.h"



track_t* effectbase::getTrack() {
	audio_stage_t* stage = getTrackLink();
	if (!stage)
		return nullptr;
	return stage->getTrack();
}

void effectbase::onTick(double since) {
	meter.onTick(since);
}

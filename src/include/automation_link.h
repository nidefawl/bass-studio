#pragma once
#include "track.h"
#include "automation.h"

#include "../host/vst_plugin.h"
#include "../host/vst_plugin_handles.h"

struct plugin_track_link_t : public plugin_reference_t {
	track_t* const track;
	vstplugin* const plugin;
	int32_t const paramIdx;
	plugin_track_link_t(track_t* _track, vstplugin* _plugin, int32_t _paramIdx)
	: plugin_reference_t(),
	track(_track), plugin(_plugin), paramIdx(_paramIdx)
	{

	}
	void onSrcDelete() {
		plugin->unregisterAutomationSrc(paramIdx);
	}
	void onDstDelete() {
		track->getAutomation().setTarget(NULL, -1);
	}
	void setDst(vstplugin* plugin, int32_t paramIdx) {
		track->getAutomation().setTarget(plugin, paramIdx);
	}
	plugin_param_autiomation_src_t serialize() {
		return {plugin->handle->slot, track->idx, paramIdx};
	}
};

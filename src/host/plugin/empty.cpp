#include <stdint.h>
#include <stdbool.h>
#include "empty.h"
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
#include "../../gui/guiplugin.h"


class guimodule_empty : public guiplugin {
public:
	module_empty* const module;
	guimodule_empty(module_empty* _vst);
	~guimodule_empty() {
		my_printf("DSTR!\n",0);
	}
	void layoutModule(ivec2 pos, ivec2 contentS, int32_t inset1) override {
	}
};

guimodule_empty::guimodule_empty(module_empty* _vst)
: guiplugin(_vst),
  module(_vst) {
}


struct module_empty::internal_handles_t {
	std::unique_ptr<guimodule_empty> gui;
//	guimodule_empty * gui;
};
module_empty::module_empty(int32_t _projectGlobalId)
: internalplugin("Empty", PLUGIN_TYPE_EMPTY, _projectGlobalId), handle(new module_empty::internal_handles_t{0})
{
}
module_empty::~module_empty()
{
	delete handle;
}



float module_empty::dispatchGetParameter(int32_t idx) {
	return 0;
}
void module_empty::dispatchSetParameter(int32_t idx, float val) {

}
guiplugin* module_empty::makeGui() {
	if (!handle->gui) {
		handle->gui = std::make_unique<guimodule_empty>(this);
		handle->gui->setTitle(StringFormat("%s", StringAsCStr(this->sName)));
//		handle->gui = new guimodule_empty(this);
	}
	return handle->gui.get();
//	return handle->gui;
}
guiplugin* module_empty::getGui() {
	return handle->gui.get();
//	return handle->gui;
}
int32_t module_empty::getDelay() {
	return 0;
}
void module_empty::resume() {
}
void module_empty::sleep() {
}
void module_empty::unload(vsthost* host) {
	effectbase::unload(host);
}
void module_empty::load(vsthost* host) {
	effectbase::load(host);
	this->blockInputs = new AudioBlock(math::max(2, 2), format.blockSize);
	this->blockOutputs = new AudioBlock(math::max(2, 2), format.blockSize);
	bIsEnabled = this->getParamValue(PARAM_ENABLE) > 0.5;
	if (bIsEnabled) {
		this->resume();
	} else {
		this->sleep();
	}
}
void module_empty::process(AudioBlock* in, AudioBlock* out, int32_t samplePos, int32_t numSamples, playback_state state) {
	dbgassert(getTrackLink()->sampleFormat == this->format && in->samples == format.blockSize && out->samples == format.blockSize && format.blockSize > 0 && format.sampleRate > 0);
	out->copyFrom(in);
}
String module_empty::getInfo(std::vector<String>& list) {
	return "";
}
template<>
effectbase* makeInstance<module_empty>(int32_t _projectGlobalId) {
	return new module_empty(_projectGlobalId);
}


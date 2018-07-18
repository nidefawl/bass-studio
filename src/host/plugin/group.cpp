#include <stdint.h>
#include <stdbool.h>
#include <glm/glm.hpp>
#include <glm/vec2.hpp>
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

using glm::vec2;
using glm::ivec2;


class guimodule_group : public guiplugin {
public:
	module_group* const module;
	guimodule_group(module_group* _vst);
	~guimodule_group() {
		my_printf("DSTR!\n",0);
	}
	void render(NVGcontext* vg) override;
	void buttonClicked(guibase* _button) override;
	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
	void layoutModule(int32_t inset1, ivec2 contentS) override {
	}
};

guimodule_group::guimodule_group(module_group* _vst)
: guiplugin(_vst),
  module(_vst) {
}

void guimodule_group::render(NVGcontext* vg) {
	renderBase(vg);
	buttonBypass.render(vg);
	buttonDelete.render(vg);

	meter.render(vg);
}
bool guimodule_group::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
	if (contains(mpos)) {
		ivec2 mouseLocal = mpos - pos;
		if (buttonBypass.mouseHitTest(mouseLocal, evt)) {
			return true;
		}
		if (buttonDelete.mouseHitTest(mouseLocal, evt)) {
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
		if (module->bIsEnabled) {
//			module->sleep();
		} else {
//			module->resume();
		}
		if (module->isSynth) {
//			vsthost::getInstance()->sendNotesOff(module);
		}

	}
	if (_button == &buttonDelete) {
    	ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
    	vsthost::getInstance()->unloadPlugin(module);
	}
}


struct internal_handles_t {
	std::unique_ptr<guimodule_group> gui;
//	guimodule_group * gui;
};
module_group::module_group(int32_t _projectGlobalId)
: internalplugin(_projectGlobalId), handle(new internal_handles_t{0})
{
	this->sName = "Group";
#ifndef NDEBUG
		this->szName = this->sName.c_str();
#endif
}
module_group::~module_group()
{
	delete handle;
}



float module_group::dispatchGetParameter(int32_t idx) {
	return 0;
}
void module_group::dispatchSetParameter(int32_t idx, float val) {

}
guibase* module_group::makeGui() {
	if (!handle->gui) {
		handle->gui = std::make_unique<guimodule_group>(this);
		handle->gui->setTitle(StringFormat("%s", StringAsCStr(this->sName)));
//		handle->gui = new guimodule_group(this);
	}
	return handle->gui.get();
//	return handle->gui;
}
guibase* module_group::getGui() {
	return handle->gui.get();
//	return handle->gui;
}
int32_t module_group::getDelay() {
	return 0;
}
void module_group::process(AudioBlock* in, AudioBlock* out, int32_t samples) {
	out->copyFrom(in);
}
String module_group::getInfo(std::vector<String>& list) {
	return "";
}


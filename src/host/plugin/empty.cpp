#include <stdint.h>
#include <stdbool.h>
#include <glm/glm.hpp>
#include <glm/vec2.hpp>
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
#include "leak_detect.h"
#include "../../gui/guiplugin.h"

using glm::vec2;
using glm::ivec2;


class guimodule_empty : public guiplugin {
public:
	module_empty* const module;
	guimodule_empty(module_empty* _vst);
	~guimodule_empty() {
		my_printf("DSTR!\n",0);
	}
	void render(NVGcontext* vg) override;
	void buttonClicked(guibase* _button) override;
	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
	void layoutModule(ivec2 pos, ivec2 contentS, int32_t inset1) override {
		layoutButtons();
	}
};

guimodule_empty::guimodule_empty(module_empty* _vst)
: guiplugin(_vst),
  module(_vst) {
}

void guimodule_empty::render(NVGcontext* vg) {
	renderBase(vg);
	buttonBypass.render(vg);
	buttonDelete.render(vg);

	meter.render(vg);
}
bool guimodule_empty::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
	if (contains(mpos)) {
		if (evt.type == MouseHitType::MOUSE_DRAGDROP_OBJECT) {
			return false;
		}
		ivec2 localMouse = this->toContainerSpace(mpos);
		if (buttonBypass.mouseHitTest(localMouse, evt)) {
			return true;
		}
		if (buttonDelete.mouseHitTest(localMouse, evt)) {
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

void guimodule_empty::buttonClicked(guibase* _button) {
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


struct module_empty::internal_handles_t {
	std::unique_ptr<guimodule_empty> gui;
//	guimodule_empty * gui;
};
module_empty::module_empty(int32_t _projectGlobalId)
: internalplugin(PLUGIN_TYPE_EMPTY, _projectGlobalId), handle(new module_empty::internal_handles_t{0})
{
	this->sName = "Empty";
#ifndef NDEBUG
		this->szName = this->sName.c_str();
#endif
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
void module_empty::unload() { }
void module_empty::load(vsthost* host) {
	bIsEnabled = this->getParamValue(PARAM_ENABLE) > 0.5;
	if (bIsEnabled) {
		this->resume();
	} else {
		this->sleep();
	}
}
void module_empty::process(AudioBlock* in, AudioBlock* out, int32_t samples) {
	out->copyFrom(in);
}
String module_empty::getInfo(std::vector<String>& list) {
	return "";
}


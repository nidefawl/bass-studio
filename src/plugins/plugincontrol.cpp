#include "plugincontrol.h"

#include "glheaders.h"
#include <nanovg.h>
#include <time.h>
#include <algorithm>
#include <functional>
#include <vector>
#include <memory>
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstdarg>
#include <glm/glm.hpp>


#include "window.h"
#include "platform.h"

#include "keyboard.h"
#include "commands.h"

#include "basectrl.h"
#include "exceptions.h"
#include "color_util.h"
#include "str_util.h"
#include "settings.h"
#include "logging.h"
#include "menu.h"
#include "msgbox.h"

#include "../gui/gui.h"
#include "../gui/guicontainer.h"
#include "../gui/button.h"
#include "../gui/guicontextmenu_base.h"
#include "../gui/scrollbar.h"
#include "../gui/statusbar.h"
#include "../gui/guimenu.h"
#include "leak_detect.h"
#include "plugin.h"

using glm::vec2;
using glm::ivec2;
using glm::vec4;
using glm::ivec4;
using std::min;
using std::max;


void PluginControl::destroy()
{
	if (!isOK) {
		return;
	}
	isOK = false;
	delete view;
}

void PluginControl::menuCommand(int cmd) {
	switch (cmd) {
	case CMD_EXIT:
		mainWindow->requestClose();
		break;

	}
}
void PrintHelp()
{
    printf("-r,--host <addr>:       Connect to remote host\n"
           "-h,--help:              Show this help\n");
    exit(1);
}

void PluginControl::initApp(int argc, char* argv[]) {
}
PluginControl::PluginControl(PluginViewContainersImpl* _view) : AppCtrl(), view(_view) {
}
PluginControl::~PluginControl() {
	delete view;
}
void PluginControl::postInit() {
}

bool PluginControl::init(window_main* window, NVGcontext* nanovg)
{
	this->mainWindow = window;
	this->window = window;
	this->vg = nanovg;

	if (firstInit) {
		firstInit = false;
		view->addTo(this->containers);
		for (guictr_base *ctr : containers) {
			ctr->setControl(this);
		}
	}

	isOK = true;
	return isOK;
}

void PluginControl::mouseMoved(ivec2 mousePos, ivec2 deltaPos) {
	BaseCtrl::mouseMoved(mousePos, deltaPos);
}

void PluginControl::relayout(int32_t w, int32_t h) {
	closeAllAppMenus();
	closeContextMenu();
	m_size = ivec2(w, h);
	view->layout(w, h);

	for (guictr_base *ctr : containers) {
		ctr->layout();
	}
}

bool PluginControl::processGlobalKeyevent(KeyEvent& event) {
	if (event.type != KeyEventType::K_RELEASE) {
		if (isKC(KC_UNDO, event)) {
			menuCommand(CMD_UNDO);
			return true;
		}
		if (isKC(KC_REDO, event)) {
			menuCommand(CMD_REDO);
			return true;
		}
		if (isKC(KC_NEW, event)) {
			menuCommand(CMD_FILE_NEW);
			return true;
		}
		if (isKC(KC_OPEN, event)) {
			menuCommand(CMD_FILE_OPEN);
			return true;
		}
		if (isKC(KC_SAVE, event)) {
			menuCommand(CMD_FILE_SAVE);
			return true;
		}
		if (isKC(KC_SAVEAS, event)) {
			menuCommand(CMD_FILE_SAVEAS);
			return true;
		}
	}
	return false;
}

bool PluginControl::mouseDownPre() {
	closeAllContextMenus();
	return true;
}

void PluginControl::onTick()
{
	for (guictr_base *ctr : containers) {
		ctr->onTick(this);
	}
	mainWindow->requestRedraw();
}
void PluginControl::render(int32_t x, int32_t y, int32_t w, int32_t h, float ratio) {
	BaseCtrl::render(x, y, w, h, ratio);
}

class AudioEffect;
void PluginControl::onGuiOpen(AudioEffect* eff) {
	this->view->onGuiOpen(eff);
}
void PluginControl::onGuiClose(AudioEffect* eff) {
	this->view->onGuiClose(eff);
}
void PluginControl::onSetParameter(int32_t index, float value) {
	this->view->onSetParameter(index, value);
}

#ifndef BUILD_BUILTIN_EFFECT
int initDebugWindow() {
	return 0;
}
#endif

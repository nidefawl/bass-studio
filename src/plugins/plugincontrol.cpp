#include "plugincontrol.h"

#include "glheaders.h"
#include <nanovg.h>
#include <GLFW/glfw3.h>
#include <ctime>
#include <algorithm>
#include <functional>
#include <vector>
#include <memory>
#include <cstdio>
#include <cstdarg>


#include "window.h"
#include "platform.h"

#include "keyboard.h"
#include "commands.h"

#include "basectrl.h"
#include "exceptions.h"
#include "color_util.h"
#include "str_util.h"
#include "logging.h"
#include "menu.h"
#include "msgbox.h"

#include "../gui/gui.h"
#include "gui/container/container.h"
#include "gui/controls/button.h"
#include "gui/contextmenu/contextmenu_base.h"
#include "gui/controls/scrollbar.h"
#include "gui/controls/statusbar.h"
#include "gui/menu/menu.h"
#include "plugin.h"


void PluginControl::destroy() {
    if (!isOK) {
        return;
    }
    isOK = false;
    if (view) {
        //delete view;
        view->setFree();
        view = nullptr;
    }
}

void PluginControl::menuCommand(menucmd_t command) {
    switch (command.command) {
        case CMD_EXIT:
            mainWindow->requestClose();
            break;
    }
}

void PluginControl::initApp(const std::vector<String>& args) {
}

PluginControl::PluginControl(std::shared_ptr<PluginViewContainers> _view) : AppCtrl(), view(std::move(_view)) {
}

PluginControl::~PluginControl() {
    if (view) {
        view->setFree();
        //delete view;
        view = nullptr;
    }
}
void PluginControl::startApp() {
}

bool PluginControl::initAppWindow(window_main* window, NVGcontext* nanovg) {
    this->mainWindow = window;
    this->window     = window;
    this->vg         = nanovg;

    if (firstInit) {
        firstInit = false;
        view->addTo(this->containers);
        for (guictr_base* ctr : containers) {
            ctr->setControl(this);
        }
    }

    isOK = true;
    return isOK;
}
void PluginControl::render(NVGcontext* nanovgCtxt, int32_t x, int32_t y, int32_t w, int32_t h, float ratio) {
    AppCtrl::render(nanovgCtxt, x, y, w, h, ratio);
}

void PluginControl::mouseMoved(ivec2 mousePos, ivec2 deltaPos) {
    BaseCtrl::mouseMoved(mousePos, deltaPos);
}

void PluginControl::relayout(int32_t w, int32_t h) {
    closeAllAppMenus();
    closeContextMenu();
    view->layout(w, h);

    for (guictr_base* ctr : containers) {
        ctr->layout();
    }
}

bool PluginControl::processGlobalKeyevent(KeyEvent& event) {
    if (event.type != KeyEventType::K_RELEASE) {
        if (isKC(KC_UNDO, event)) {
            menuCommand(CMD_NOARG(CMD_UNDO));
            return true;
        }
        if (isKC(KC_REDO, event)) {
            menuCommand(CMD_NOARG(CMD_REDO));
            return true;
        }
        if (isKC(KC_NEW, event)) {
            menuCommand(CMD_NOARG(CMD_FILE_NEW));
            return true;
        }
        if (isKC(KC_OPEN, event)) {
            menuCommand(CMD_NOARG(CMD_FILE_OPEN));
            return true;
        }
        if (isKC(KC_SAVE, event)) {
            menuCommand(CMD_NOARG(CMD_FILE_SAVE));
            return true;
        }
        if (isKC(KC_SAVEAS, event)) {
            menuCommand(CMD_NOARG(CMD_FILE_SAVEAS));
            return true;
        }
    }
    return false;
}

bool PluginControl::mouseDownPre() {
    closeAllContextMenus();
    return true;
}

void PluginControl::onTick() {
    for (guictr_base* ctr : containers) {
        ctr->onTick(this);
    }
    mainWindow->requestRedraw();
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

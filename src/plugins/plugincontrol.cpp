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


#include "tls.h"
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
#include "host/daw/mainctrl.h"


void PluginControl::destroy() {
    if (!isOK) {
        return;
    }
    log_lf(Log::L_ERROR, "Destroy PluginControl %s\n", StringAsCStr(windowName));
    isOK = false;
    if (view) {
        for (guictr_base* ctr : containers) {
            ctr->setControl(nullptr);
        }
        view->setFree();
        containers.clear();
        view = nullptr;
    }
}

bool PluginControl::menuCommand(const menucmd_t& command) {
    switch (command.command) {
        case CMD_EXIT:
            mainWindow->requestClose();
            return true;
    }
    return false;
}

void PluginControl::initApp(const std::vector<String>& args) {
}

PluginControl::PluginControl(AppCtrl* parent, std::shared_ptr<PluginViewContainer> _view)
: AppCtrl(parent), view(std::move(_view))
{
    if (!this->commands) {
        this->commands = &commandMgr;
    }
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
    AppCtrl* parentCtrl = getDawCtrl();
    if(parentCtrl) {
        m_scale     = parentCtrl->m_scale;
        *getTheme() = *parentCtrl->getTheme();
    }


    isOK = true;
    return isOK;
}
void PluginControl::render(NVGcontext* nanovgCtxt, int32_t x, int32_t y, int32_t w, int32_t h, float ratio) {
    AppCtrl::render(nanovgCtxt, x, y, w, h, ratio);
}

void PluginControl::mouseMoved(ivec2 mousePos, ivec2 deltaPos, KeyboardMods kbmods) {
    BaseCtrl::mouseMoved(mousePos, deltaPos, kbmods);
}

void PluginControl::relayout(int32_t w, int32_t h) {
    closeAllAppMenus();
    closeContextMenu();
    view->layout(w, h);

    for (guictr_base* ctr : containers) {
        if (ctr->isVisible()) {
            ctr->layout();
        }
    }
}

bool PluginControl::mouseDownPre() {
    closeAllContextMenus();
    return true;
}

void PluginControl::onTick() {
    for (guictr_base* ctr : containers) {
        if (ctr->isVisible())
            ctr->onTick(this);
    }
    for (guictr_base* ctr : containers) {
        if (ctr->isVisible())
            ctr->onIdle();
    }
}

void PluginControl::onGuiOpen() {
    this->view->onGuiOpen();
}

void PluginControl::onGuiClose() {
    this->view->onGuiClose();
}

void PluginControl::onSetParameter(int32_t index, float value) {
    this->view->onSetParameter(index, value);
}

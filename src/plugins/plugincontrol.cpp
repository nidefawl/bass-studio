#include "plugincontrol.hpp"

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


#include "tls.hpp"
#include "window.hpp"
#include "platform.hpp"

#include "keyboard.hpp"
#include "commands.hpp"

#include "basectrl.hpp"
#include "exceptions.hpp"
#include "color_util.hpp"
#include "str_util.hpp"
#include "logging.hpp"
#include "menu.hpp"
#include "msgbox.hpp"

#include "../gui/gui.hpp"
#include "gui/container/container.hpp"
#include "gui/controls/button.hpp"
#include "gui/contextmenu/contextmenu_base.hpp"
#include "gui/controls/scrollbar.hpp"
#include "gui/controls/statusbar.hpp"
#include "gui/menu/menu.hpp"
#include "plugin.hpp"
#include "host/daw/mainctrl.hpp"


void PluginControl::destroy() {
    if (!isOK) {
        return;
    }
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

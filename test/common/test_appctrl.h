#pragma once
#include <list>
#include <vector>
#include <set>
#include <stdint.h>
#include <memory>

#include "math/vec.h"
#include "config.h"
#include "str_util.h"
#include "basectrl.h"
#include "window.h"
#include "menu.h"
#include "mouse.h"
#include "keyboard.h"
#include "event.h"
#include "logging.h"
#include "hires_timer.h"
#include "rand.h"

#ifndef FIXED_APP_TYPE
#define FIXED_APP_TYPE -1
#endif

struct NVGcontext;
class guibase;
class guictr_base;
class gui_statusbar;
class guictxtmenu_base;
class appwindow_main;

struct TestMenus {
    ngui::Menu file;
};

namespace TestApp {
    class ViewContainers;
}
class guictr_debuginfo;
class TestAppCtrl : public AppCtrl {
    TestApp::ViewContainers* view = nullptr;
    TestMenus menus;
    hires_timer_t timer;
    seq_rand rand;
    int32_t preselectedApp = FIXED_APP_TYPE;
    bool bDrawBackbuffer = false;
public:
    static TestAppCtrl* get();

    TestAppCtrl() : AppCtrl(nullptr) {
    }
    ~TestAppCtrl() override = default;

    void initApp(const std::vector<String>& args) override;
    bool initAppWindow(window_main* window, NVGcontext* nanovg) override;
    void startApp() override;
    void mouseMoved(ivec2 mousePos, ivec2 deltaPos, KeyboardMods kbmods) override;
    bool menuCommand(const menucmd_t& command) override;
    void onTick() override;
    void destroy() override;
    void relayout(int32_t w, int32_t h) override;
    void relayout() override { BaseCtrl::relayout(); };
    bool mouseDownPre() override;
    void render(NVGcontext* nanovgCtxt, int32_t x, int32_t y, int32_t w, int32_t h, float ratio) override;
    void prerender(NVGcontext* nanovgCtxt, int32_t x, int32_t y, int32_t w, int32_t h, float pixelRatio) override;

    void focusReceived() override {
    }
    void focusLost() override {
    }


    void dragContainerRelayout(drag_ctr_event evt) override {
        if (evt.evtType == BaseCtrl::drag_ctr_event_type::DRAG_END) {
            BaseCtrl::relayout();
        }
    }
};

#pragma once
#include <list>
#include <vector>
#include <set>
#include <stdint.h>
#include <memory>

#include "math/vec.hpp"
#include "config.hpp"
#include "str_util.hpp"
#include "basectrl.hpp"
#include "window.hpp"
#include "menu.hpp"
#include "mouse.hpp"
#include "keyboard.hpp"
#include "event.hpp"
#include "logging.hpp"
#include "hires_timer.hpp"
#include "rand.hpp"

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
    int64_t appTimeoutSeconds = -1;
    bool bDrawBackbuffer = false;
    int64_t tmStart = 0;
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

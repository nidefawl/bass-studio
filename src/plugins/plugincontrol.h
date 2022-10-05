#pragma once
#include <list>
#include <vector>
#include <set>
#include "types.h"
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
#include "plugin.h"
#include "gui/plugin/pluginviewcontainers.h"
#include "commands.h"

struct NVGcontext;
class guibase;
class guictr_base;
class guictxtmenu_base;

class PluginControl : public AppCtrl {
    std::shared_ptr<PluginViewContainers> view;
    bool firstInit = true;
    DAW::UI::CommandManager commandMgr;
public:
    explicit PluginControl(AppCtrl* parent, std::shared_ptr<PluginViewContainers> view);
    ~PluginControl() override;
    static PluginControl* get();

    void mouseMoved(ivec2 mousePos, ivec2 deltaPos, int kbmods) override;
    void menuCommand(const menucmd_t& command) override;
    void onTick() override;
    bool initAppWindow(window_main* window, NVGcontext* nanovg) override;
    void startApp() override;
    void destroy() override;
    void relayout(int32_t w, int32_t h) override;
    bool processGlobalKeyevent(KeyEvent& event) override;
    bool mouseDownPre() override;

    void initApp(const std::vector<String>& args) override;
    void render(NVGcontext* nanovgCtxt, int32_t x, int32_t y, int32_t w, int32_t h, float ratio) override;

    void onGuiOpen();
    void onGuiClose();
    void onSetParameter(int32_t index, float value);
};

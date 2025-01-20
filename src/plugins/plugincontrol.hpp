#pragma once
#include <list>
#include <vector>
#include <set>
#include "types.hpp"
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
#include "plugin.hpp"
#include "gui/plugin/pluginviewcontainers.hpp"
#include "commands.hpp"

struct NVGcontext;
class guibase;
class guictr_base;
class guictxtmenu_base;

class PluginControl final : public AppCtrl {
    std::shared_ptr<PluginViewContainer> view;
    bool firstInit = true;
    DAW::UI::CommandManager commandMgr;
public:
    explicit PluginControl(AppCtrl* parent, std::shared_ptr<PluginViewContainer> view);
    ~PluginControl() override;
    static PluginControl* get();

    void mouseMoved(ivec2 mousePos, ivec2 deltaPos, KeyboardMods kbmods) override;
    bool menuCommand(const menucmd_t& command) override;
    void onTick() override;
    bool initAppWindow(window_main* window, NVGcontext* nanovg) override;
    void startApp() override;
    void destroy() override;
    void relayout(int32_t w, int32_t h) override;
    bool mouseDownPre() override;

    void initApp(const std::vector<String>& args) override;
    void render(NVGcontext* nanovgCtxt, int32_t x, int32_t y, int32_t w, int32_t h, float ratio) override;

    void onGuiOpen();
    void onGuiClose();
    void onSetParameter(int32_t index, float value);
};

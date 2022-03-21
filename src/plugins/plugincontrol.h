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

struct NVGcontext;
class guibase;
class guictr_base;
class guictxtmenu_base;
class AudioEffect;

class PluginControl : public AppCtrl {
    std::shared_ptr<PluginViewContainers> view;
    bool firstInit = true;

public:
    explicit PluginControl(std::shared_ptr<PluginViewContainers> view);
    ~PluginControl() override;
    static PluginControl* get();
    void focusReceived() override{};
    void focusLost() override{};

    void mouseMoved(ivec2 mousePos, ivec2 deltaPos) override;
    void menuCommand(menucmd_t command) override;
    void onTick() override;
    bool initAppWindow(window_main* window, NVGcontext* nanovg) override;
    void startApp() override;
    void destroy() override;
    void relayout(int32_t w, int32_t h) override;
    bool processGlobalKeyevent(KeyEvent& event) override;
    bool mouseDownPre() override;

    void initApp(const std::vector<String>& args) override;

    void onGuiOpen(AudioEffect* eff);
    void onGuiClose(AudioEffect* eff);
    void onSetParameter(int32_t index, float value);
};

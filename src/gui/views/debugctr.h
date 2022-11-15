#pragma once
#include "host/daw/mainctrl.h"
#include "math/vec.h"
#include "str_util.h"
#include "gui/controls/knob.h"
#include "gui/container/container.h"
#include "gui/controls/button.h"
#include <vector>


class gui_ctr_debug final : public guictr_base {
    struct ctr_debug_impl_t;
    ctr_debug_impl_t* const impl;
public:
    static constexpr int BTN_FONT_SIZE = 16;

    enum class DebugCtrType : int32_t {
        TYPE_0,
        DEBUG_APPCTRL,
        TYPE_2
    };

private:
    const DebugCtrType dgbCtrType;
    std::vector<String> g_debugStrings;

public:
    gui_ctr_debug(create_ctr_t ctxt, DebugCtrType debugCtrType);
    ~gui_ctr_debug() override;
    void render(NVGcontext* vg) override;
    void layout() override;
    void addStr(String str) {
        g_debugStrings.push_back(std::move(str));
    }
    void buttonClicked(guibase* button) override;
    void onTick(AppCtrl* ctrl) override;
};

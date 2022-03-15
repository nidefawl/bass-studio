#pragma once
#include "gui/gui.h"
#include "gui/container/container.h"

class gui_shaderview_impl_t;
class gui_shaderview : public guictr_base {
    gui_shaderview_impl_t* impl = nullptr;

public:
    gui_shaderview();
    ~gui_shaderview() override;
    void prerender(NVGcontext* vg) override;
    void render(NVGcontext* vg) override;
    void onTick(AppCtrl* appctrl) override;
};

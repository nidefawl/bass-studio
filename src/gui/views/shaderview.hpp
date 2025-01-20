#pragma once
#include "gui/dropdown/dropdown_preset_tree.hpp"
#include "gui/gui.hpp"
#include "gui/container/container.hpp"
class guictr_shader_preset_control;
class gui_shaderview_impl_t;
class gui_shaderview final : public guictr_base {
    gui_shaderview_impl_t* impl = nullptr;

    guictr_shader_preset_control* presetControl;
    int32_t inputHeight = HEIGHT_DEFAULT_INPUT;
public:
    gui_shaderview();
    ~gui_shaderview() override;
    void prerender(NVGcontext* vg) override;
    void render(NVGcontext* vg) override;
    void onTick(AppCtrl* appctrl) override;
    void layout() override;
    void buttonClicked(guibase* button) override;
};

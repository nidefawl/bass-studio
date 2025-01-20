#pragma once
#include "gui/dialog/dialog.hpp"

class gui_asyc_progress final : public guidialog_base {
    guibutton btnCancel;
    String desc;
public:
    gui_asyc_progress();
    ~gui_asyc_progress() override;
    void determineSize(ivec2& prefSize) override;
    void layout() override;
    void buttonClicked(guibase* button) override;
    void render(NVGcontext* vg) override;
    void onTick(AppCtrl* ctrl) override;
};
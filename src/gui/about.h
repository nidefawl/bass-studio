#pragma once
#include "math/vec.h"
#include "str_util.h"
#include "knob.h"
#include "guicontainer.h"
#include "guicontextmenu_base.h"
#include "button.h"
#include <vector>
#include "dialog.h"

class guidialog_about : public guidialog_base {
    guibutton btnClose;

public:
    guidialog_about();
    ~guidialog_about() {
        removeGuis();
    }
    void render(NVGcontext* vg) override;
    void layout() override;
    void buttonClicked(guibase* button) override;
};

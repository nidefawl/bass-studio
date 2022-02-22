#pragma once
#include "math/vec.h"
#include "str_util.h"
#include "knob.h"
#include "guicontainer.h"
#include "guicontextmenu_base.h"
#include "button.h"
#include <vector>
#include <tuple>
#include "dialog.h"

class guidialog_about : public guidialog_base {
    using AboutLine = std::tuple<String, String>;
    using DetailedAbout = std::tuple<const char*, const char*>;
    guibutton btnClose;
    std::vector<DetailedAbout> strings2;
    std::vector<AboutLine> strings;
public:
    guidialog_about();
    ~guidialog_about() override {
        removeGuis();
    }
    void render(NVGcontext* vg) override;
    void layout() override;
    void buttonClicked(guibase* button) override;
};

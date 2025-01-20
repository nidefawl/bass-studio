#pragma once
#include "math/vec.hpp"
#include "str_util.hpp"
#include "gui/controls/knob.hpp"
#include "gui/container/container.hpp"
#include "gui/contextmenu/contextmenu_base.hpp"
#include "gui/controls/button.hpp"
#include <vector>
#include <tuple>
#include "dialog.hpp"

class guidialog_about final : public guidialog_base {
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

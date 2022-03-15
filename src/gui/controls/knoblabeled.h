#pragma once
#include <vector>
#include <memory>
#include <functional>
#include "math/vec.h"
#include "str_util.h"
#include "color_util.h"
#include "gui/gui.h"
#include "gui/container/container.h"
#include "gui/plugin/pluginviewcontainers.h"
#include "gui/controls/knob.h"


class guiknob_labeled_base : public guiknob {
public:
    const int button_inset = 10;
    std::function<String(float)> fnGetDisplayValue;

protected:
    int labelHeight     = 0;
    int valueHeight     = 0;
    String valueDisplay = "  ";

public:
    guiknob_labeled_base(const bool _renderBackground = true, const bool _isSlider = false) : guiknob(_renderBackground, _isSlider) {
    }
    ~guiknob_labeled_base() override = default;
    void setDisplayValue(float f) override {
        if (fnGetDisplayValue) {
            valueDisplay = fnGetDisplayValue(f);
        }
    }
    void layout() override;
    void render(NVGcontext* vg) override;
};

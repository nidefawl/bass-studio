#pragma once
#include "gui/container/container.h"
#include "gui/controls/knoblabeled.h"
#include "keyboard.h"
#include "gui/controls/inputfield.h"
#include "textfield.h"


class gui_color_pick : public guictr_base {
    guiknob_labeled_base knH;
    guiknob_labeled_base knS;
    guiknob_labeled_base knL;
    guiknob_labeled_base knA;
    gui_input_filtered hexInput;
    NVGcolor nvgColor{ 1.0f, 1.0f, 1.0f, 1.0f };
    uint32_t colorU32     = 0xFFFFFFFFU;
    NVGcolor* ptrNvgColor = nullptr;
    uint32_t* ptrColorU32 = nullptr;
    void setHSL_(float h, float s, float v, float a);

public:
    std::function<void(uint32_t)> fnSetValue;

public:
    gui_color_pick();
    void setHSL(float h, float s, float l, float a);
    void setU32(uint32_t rgba);
    void init();
    ~gui_color_pick() override {
        removeGuis();
    }
    void buttonClicked(guibase* button) override;
    void layout() override;
    void setRefU32(uint32_t* ptrU32);
    void setRefNvg(NVGcolor* ptrNvg);
    void render(NVGcontext* vg) override;
    void handleRightClick(MouseEvent& evt) override;
    NVGcolor getNvg() {
        return nvgColor;
    }
    uint32_t getU32() {
        return colorU32;
    }
};

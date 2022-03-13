#pragma once
#include <nanovg.h>
#include "color_util.h"
#include "str_util.h"
#include "seq_util.h"
#include "math/seq_math.h"
#include "gui.h"
#include "guicolors.h"
#include "event.h"
#include "button.h"


class guidropdownbase : public guibutton {
public:
    enum dropdown_field_selectitem {
        SELECT_IDX,
        SELECT_NEXT,
        SELECT_PREVIOUS,
        SELECT_FIRST,
        SELECT_LAST,
    };
    void render(NVGcontext* vg) override;
    void handleDraggedRelease(MouseEvent& evt) override {
        if (parent)
            parent->buttonClicked(this);
    }
    bool handleMouseScroll(MouseEvent& evt, double xoffset, double yoffset) override;
    void handleRightClick(MouseEvent& evt) override {
    }
    bool handleKeyInput(KeyEvent& kevt) override;
    bool handleCharInput(unsigned int codepoint) override {
        return false;
    }
    virtual void select(dropdown_field_selectitem req, uint32_t idxOffset);
    virtual uint32_t getSelectIndex() {
        return 0xFFFFFFFF;
    }
    virtual uint32_t getLastIndex() {
        return 0;
    }
    virtual void setSelectedIndex(uint32_t) {
    }
    virtual String getString() = 0;
};

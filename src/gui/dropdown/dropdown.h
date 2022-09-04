#pragma once
#include <nanovg.h>
#include "color_util.h"
#include "str_util.h"
#include "seq_util.h"
#include "math/seq_math.h"
#include "gui/gui.h"
#include "guicolors.h"
#include "event.h"
#include "gui/controls/button.h"


class guidropdownbase : public guibutton {
public:
    enum dropdown_field_selectitem {
        SELECT_IDX,
        SELECT_NEXT,
        SELECT_PREVIOUS,
        SELECT_FIRST,
        SELECT_LAST,
    };
    guidropdownbase() : guibutton() {
        setFlag(FLG_RENDER_BACKGROUND_INSET, true);
        setFlag(FLG_BG_SHADING, true);
    }
    void render(NVGcontext* vg) override;
    void handleDraggedRelease(MouseEvent& evt) override {
        if (parent)
            parent->buttonClicked(this);
    }
    bool handleMouseScroll(MouseEvent& evt, double xoffset, double yoffset) override;
    void handleRightClick(MouseEvent& evt) override {
    }
    bool handleKeyInput(KeyEvent& kevt) override;
    bool handleCharInput(uint32_t codepoint) override {
        return false;
    }
    virtual void select(dropdown_field_selectitem req, int32_t idxOffset);
    virtual int32_t getSelectIndex() {
        return -1;
    }
    virtual int32_t getLastIndex() {
        return 0;
    }
    virtual void setSelectedIndex(int32_t idx) {
    }
    virtual String getString() = 0;
};

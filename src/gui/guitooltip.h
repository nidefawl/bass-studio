#pragma once
#include "math/vec.h"
#include "gui.h"
#include "guicontextmenu_base.h"
#include "guicontextmenu.h"
#include "guicontainer.h"
#include "gui/textfield.h"
#include "table.h"

#include <memory>
#include <numeric>

#define FONT_SIZE_TOOLTIP_TITLE 18
#define FONT_SIZE_TOOLTIP_BIG 15
#define FONT_SIZE_TOOLTIP 16

template<typename T>
class guitooltip : public guictxtmenu {
protected:
    T* ptr;
    bool hadMouseFocus = false;
    Table::tbl table;
    gui_textfield textField;

public:
    guitooltip(T* _ptr) : guictxtmenu(), ptr(_ptr) {
        add(&textField);
        setBackgroundRendered(true);
        setBackgroundRenderedInset(false);
//        setSnapSides(ivec4(1));
        textField.setVisible(false);
        scrollbarOutside = true;
        maxHeight        = 220;
    }
    ~guitooltip() override {
        removeGuis();
    }
    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
        if (contains(mpos)) {
            if (evt.type == MouseHitType::MOUSE_LEFT || evt.type == MouseHitType::MOUSE_RIGHT)
                hadMouseFocus = true;
            evt.requestFocus(this);
            return true;
        }
        return false;
    }
    bool isTransient() override {
        return !hadMouseFocus;
    }
    void clicked(int _id) override {
        closeContextMenu();
    }
    void onTick(AppCtrl* appctrl) override {
        layout();
    }
    void layout() override;
    void render(NVGcontext* vg) override {
        if (isBackgroundRendered()) {
            renderBackground(vg);
        }
        if (!setScissorTransform(vg)) {
            return;
        }
        setFont(vg, FONT_SIZE_TOOLTIP_TITLE, G_WHITE, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
        Table::DrawTableNVG(table, vg, theme, ivec2(INSET_TABLE), getSizeContent() - ivec2(INSET_TABLE << 1), FONT_SIZE_TOOLTIP);
        if (textField.isVisible()) {
            textField.render(vg);
        }
    }
};

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


template<typename T>
class guitooltip : public guictxtmenu {
protected:
    
    T* ptr;
    bool hadMouseFocus = false;
    Table::tbl table;
    gui_textfield textField;

public:
    static constexpr int FONT_SIZE_TOOLTIP_TITLE = 18;
    static constexpr int FONT_SIZE_TOOLTIP_BIG = 15;
    static constexpr int FONT_SIZE_TOOLTIP = 16;
    
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
    void setContent();
    void layout() override {
        table.tableWidth = 50;
        table.rowHeight = FONT_SIZE_TOOLTIP + INSET_TABLE_CELL_PADDING * 2;
        table.rows.clear();
        table.colSizes.clear();
        table.titleCols.clear();
        setContent();
        size.y = table.rows.size() * table.rowHeight;
        size = ivec2(table.tableWidth, table.rows.size() * table.rowHeight) + ivec2(INSET_TABLE << 1);
        Table::AdjustColSizes(table);
    }
    void render(NVGcontext* vg) override {
        if (isBackgroundRendered()) {
            renderBackground(vg);
        }
        if (!setScissorTransform(vg)) {
            return;
        }

        setFont(vg, FONT_SIZE_TOOLTIP_TITLE, THEMECOL_TEXT, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
        Table::DrawTableNVG(table, vg, theme, ivec2(INSET_TABLE), getSizeContent() - ivec2(INSET_TABLE << 1), FONT_SIZE_TOOLTIP);
        if (textField.isVisible()) {
            textField.render(vg);
        }
    }
};

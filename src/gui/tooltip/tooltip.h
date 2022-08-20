#pragma once
#include "basectrl.h"
#include "guicolors.h"
#include "math/vec.h"
#include "gui/gui.h"
#include "gui/contextmenu/contextmenu_base.h"
#include "gui/contextmenu/contextmenu.h"
#include "gui/container/container.h"
#include "gui/controls/textfield.h"
#include "gui/table/table.h"

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
    
    guitooltip(T* _ptr) : ptr(_ptr) {
        add(&textField);
        padding = 1;
        margin = padding;
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
    bool isTransient() const override {
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
        fontSize = G_FONT_SCALE(theme->getFloat(GuiConstant::CONST_FONT_SIZE_TABLE));
        determine_table_string_width strw(parentCtrl, theme, fontSize, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
        table.strW = &strw;
        table.tableWidth = 50;
        table.rowHeight = fontSize + INSET_TABLE_CELL_PADDING * 2;
        table.rows.clear();
        table.colSizes.clear();
        table.titleCols.clear();
        setContent();
        size.y = table.rows.size() * table.rowHeight;
        float rowWidth = 0.0f;
        for (auto& col : table.colSizes) {
            rowWidth += col;
        }
        table.tableWidth = math::max(table.tableWidth, rowWidth);
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

        setFont(vg, fontSize, THEMECOL_TEXT, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
        Table::DrawTableNVG(table, vg, theme, ivec2(INSET_TABLE), getSizeContent() - ivec2(INSET_TABLE << 1), fontSize);
        if (textField.isVisible()) {
            textField.render(vg);
        }
    }
};

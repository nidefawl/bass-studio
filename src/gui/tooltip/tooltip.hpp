#pragma once
#include "basectrl.hpp"
#include "gui/container/container_layout_types.hpp"
#include "guicolors.hpp"
#include "guiconstant.hpp"
#include "math/vec.hpp"
#include "gui/gui.hpp"
#include "gui/contextmenu/contextmenu_base.hpp"
#include "gui/contextmenu/contextmenu.hpp"
#include "gui/container/container.hpp"
#include "gui/controls/textfield.hpp"
#include "gui/table/table.hpp"
#include "saferef.hpp"

#include <memory>
#include <numeric>


template<typename T>
class guitooltip : public guictxtmenu {
protected:
    
    SafeRef<guibase> ref;
    bool hadMouseFocus = false;
    Table::tbl table;
    gui_textfield textField;
    int32_t tablePadding = 0;
public:
    
    explicit guitooltip(T* _ptr) : ref(_ptr->toRef()) {
        add(&textField);
        padding = 0;
        margin = padding;
        textField.setVisible(false);
        scrollbarOutside = true;
        maxHeight        = 220;
    }
    ~guitooltip() override {
        removeGuis();
    }
    T* getInstanceOrNull() {
        auto p = safeRefGet(ref);
        if (p) {
            return static_cast<T*>(p);
        }
        return nullptr;
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
    bool clickedElement(ctxtmenu_entry* e, int _id) override {
        closeContextMenu();
        return true;
    }
    void onTick(AppCtrl* appctrl) override {
        layout();
    }
    void setContent();
    void layout() override {
        fontSize = theme->getFloat(GuiConstant::CONST_FONT_SIZE_TOOLTIP);
        // table padding is 10% of font size, clamped to 1-10
        tablePadding = math::clamp(math::roundfS32(fontSize * 0.1f), 2, 10);
        determine_table_string_width strw(parentCtrl, theme, fontSize, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
        table.strW = &strw;
        table.tableWidth = 0;
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
        size = ivec2(table.tableWidth, table.rows.size() * table.rowHeight) + ivec2(tablePadding * 2, 0);
        Table::AdjustColSizes(table);
    }

    void render(NVGcontext* vg) override {
        if (!isRenderableSizeAndContext(vg))
            return;
        if (isBackgroundRendered()) {
            renderBackground(vg);
        }
        if (!setScissorTransform(vg)) {
            return;
        }

        setFont(vg, fontSize, THEMECOL_TEXT, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
        Table::DrawTableNVG(table, vg, theme, ivec2(tablePadding, 0), getSizeContent() - ivec2(tablePadding * 2, 0), fontSize);
        if (textField.isVisible()) {
            textField.render(vg);
        }
    }
};

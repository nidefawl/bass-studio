#include "draggedfiles.hpp"
#include "host/daw/mainctrl.h"

void gui_dragged_files::handleDraggedMove(MouseEvent& evt) {
    if (bIsExternal) {
        return;
    }
    
    auto daw        = dawCtrl->getDaw();
    auto& clipboard = daw->getDragDropClip();
    if (clipboard.state == dragdrop_file::STATE_LOADED) {
        dawCtrl->filesDropMove(evt.mousepos, evt.kbmods);
    }
}

void gui_dragged_files::handleDraggedRelease(MouseEvent& evt) {
    if (bIsExternal) {
        return;
    }
    auto daw        = dawCtrl->getDaw();
    auto& clipboard = daw->getDragDropClip();
    if (clipboard.state == dragdrop_file::STATE_LOADED) {
        dawCtrl->filesDropFinal(evt.mousepos, evt.kbmods);
    }
}

void gui_dragged_files::setFiles(const std::vector<String>& list) {
    table.tableWidth  = 200 - (INSET_TABLE << 1);
    table.titleHeight = HEIGHT_ENTRY;
    table.rowHeight   = HEIGHT_ENTRY;
    table.rows.clear();
    auto tStr = Table::tblString{ .str = StringFormat("Files (%zu)", list.size()) };
    table.rows.push_back(Table::tbl_row_t{ { tStr } });
    for (auto& s : list) {
        Table::tbl_row_t row;
        row.cols.emplace_back(s);
        table.rows.push_back(row);
    }
    Table::AdjustColSizes(table);
    size = ivec2(table.tableWidth, table.rows.size() * table.rowHeight) + ivec2(INSET_TABLE << 1);
}

void gui_dragged_files::renderDragged(NVGcontext* vg, ivec2 mousepos, ivec2 dragOffset) {
    mousepos -= pos;
    mousepos.x -= size.x / 2;
    nvgTranslate(vg, mousepos.x, mousepos.y);
    drawBackground(vg, theme, pos, size, 0, false);
    ivec2 inset                    = { 2, 2 };
    UIFont::font_instance instance = theme->getFont(UIFont::FONT_DEFAULT);
    UIFont::bindFont(vg, instance);
    nvgFillColor(vg, THEMECOL_TEXT);
    Table::DrawTableNVG(this->table, vg, theme, pos + inset, size - inset * 2, HEIGHT_ENTRY - 4);
}

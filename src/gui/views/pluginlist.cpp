#include "pluginlist.h"
#include "gui/table/table.h"

template<>
void guitooltip<gui_vstpluginlist_entry>::setContent() {
    using Table::tblint;
    using Table::tblString;
    table.tableWidth = 80;
    auto entry = ptr->getEntry();
    table.rows.push_back({ { tblString{ entry.path } } });
    determine_string_width strw(parentCtrl, theme);
    for (auto str : {&entry.path}) {
        auto widthLabel = strw.getStringWidth(*str, table.rowHeight, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        table.tableWidth = math::max(table.tableWidth, (widthLabel + INSET_TABLE_CELL_PADDING * 3) * 1.05f);
    }
}

guictxtmenu_base* gui_vstpluginlist_entry::getTooltip(AppCtrl* appctrl) {
    auto tooltip = new guitooltip<gui_vstpluginlist_entry>(this);
    return tooltip;
}
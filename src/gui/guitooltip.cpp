#include "guitooltip.h"
#include "table.h"
#include "str_util.h"

#include <vector>

using Table::table_entry_t;
using Table::tbl;
using Table::tbl_row_t;
using Table::tblfloat;
using Table::tblint;
using Table::tblstr;
using Table::tblString;

template<>
void guitooltip<String>::layout() {
    size.x          = 220;
    table.rowHeight = FONT_SIZE_TOOLTIP + INSET_TABLE_CELL_PADDING * 2;
    table.rows.clear();
    table.titleCols.clear();
    table.colSizes.clear();
//    row1.cols.push_back();
    using tbl_rows = std::vector<table_entry_t>;
    {
        tbl_rows vec{ tblstr{ "value" }, tblString{ *ptr } };
        table.rows.push_back(tbl_row_t{ vec });
    }
    Table::AdjustColSizes(table, getSizeContent() - ivec2(INSET_TABLE << 1));
    size.y = table.rows.size() * table.rowHeight;
}

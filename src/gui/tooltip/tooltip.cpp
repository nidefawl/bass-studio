#include "tooltip.hpp"
#include "gui/table/table.hpp"
#include "str_util.hpp"

#include <vector>

using Table::table_entry_t;
using Table::tbl;
using Table::tbl_row_t;
using Table::tblfloat;
using Table::tblint;
using Table::tblstr;
using Table::tblString;

// template<>
// void guitooltip<String>::setContent() {
//     auto ptr = getInstanceOrNull();
//     if (!ptr) {
//         return;
//     }
//     using tbl_rows = std::vector<table_entry_t>;
//     table.tableWidth = 140;
//     tbl_rows vec{ tblString{ *ptr } };
//     table.rows.push_back(tbl_row_t{ vec });
// }

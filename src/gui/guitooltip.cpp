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
void guitooltip<String>::setContent() {
    using tbl_rows = std::vector<table_entry_t>;
    table.tableWidth = 140;
    tbl_rows vec{ tblstr{ "value" }, tblString{ *ptr } };
    table.rows.push_back(tbl_row_t{ vec });
}

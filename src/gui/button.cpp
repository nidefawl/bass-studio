#include "button.h"
#include "gui.h"
#include "guicontainer.h"
#include "guitooltip.h"


using Table::tbl;
using Table::tbl_row_t;
using Table::table_entry_t;
using Table::tblint;
using Table::tblfloat;
using Table::tblstr;
using Table::tblString;

template <>
void guitooltip<guibuttonbase>::layout()  {
	size.x = 140;
	table.rowHeight = FONT_SIZE_TOOLTIP+INSET_TABLE_CELL_PADDING*2;
	table.rows.clear();
	table.titleCols.clear();
	table.colSizes.clear();
//	row1.cols.push_back();
	using tbl_rows = std::vector<table_entry_t>;
	{

		tbl_row_t row{};
		row.cols.push_back(tblString{ptr->label});
		table.rows.push_back(row);
	}
	Table::AdjustColSizes(table, getSizeContent()-ivec2(INSET_TABLE<<1));
	size.y = table.rows.size()*table.rowHeight;
}

guictxtmenu_base* guibuttonbase::getTooltip(AppCtrl* appctrl) {
	if (!label.empty()) {

		auto tooltip = new guitooltip<guibuttonbase>(this); //why does casting m_clip to (clip_t*) break the ptr?
		return tooltip;
	}
//	appctrl->openContextMenu(tooltip, appctrl->m_mousePos);
	return nullptr;
}

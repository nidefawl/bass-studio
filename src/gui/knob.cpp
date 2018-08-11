#include "knob.h"
#include "gui.h"
#include "guitooltip.h"
#include "str_util.h"
#include "table.h"
#include "logging.h"


template <>
void guitooltip<guiknob>::layout()  {
	size.x = 80;
	table.rowHeight = FONT_SIZE_TOOLTIP+INSET_TABLE_CELL_PADDING*2;
	table.rows.clear();
	table.titleCols.clear();
	table.colSizes.clear();
//	row1.cols.push_back();
	using tbl_rows = std::vector<table_entry_t>;
	{
		tbl_rows vec{tblstr{"value"}, tblfloat{ptr->getValue()}};
		table.rows.push_back(tbl_row_t{vec});
	}
	adjustColSizes(table, getSizeContent()-ivec2(INSET_TABLE<<1));
	size.y = table.rows.size()*table.rowHeight;
}

guictxtmenu_base* guiknob::getTooltip(AppCtrl* appctrl) {
	auto tooltip = new guitooltip<guiknob>(this); //why does casting m_clip to (clip_t*) break the ptr?
	return tooltip;
//	appctrl->openContextMenu(tooltip, appctrl->m_mousePos);
}

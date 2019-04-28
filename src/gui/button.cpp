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
	{

		tbl_row_t row{};
		row.cols.push_back(tblString{ptr->label});
		table.rows.push_back(row);
	}
	Table::AdjustColSizes(table, getSizeContent()-ivec2(INSET_TABLE<<1));
	size.y = table.rows.size()*table.rowHeight;
}
void guibuttonbase::renderButtonLabel(NVGcontext* vg, int32_t stateFlags) {
	if (drawFn || str.length()) {
		nvgSave(vg);
		setScissorTransform(vg);

		ivec2 renderPos(0);
		if (str.length() > 0) {
			//			nvgDawText(vg, this, pos, size, str, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
			int fontScale = math::round((this->fontSize > 0 ? this->fontSize : size.y) * fFontScale);
			GuiColor::constant_t c = (stateFlags & FLG_ENBL) ? GuiColor::COL_LABEL_ACTIVE : GuiColor::COL_LABEL_INACTIVE;
			NVGcolor color = theme->getColor(c);
			setFont(vg, fontScale, color, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
			float lineh = 0;
			nvgTextMetrics(vg, NULL, NULL, &lineh);
			//TODO: move this into layout (needs vg ctxt tho)
			auto getNvgMultiLineTextBounds = [](NVGcontext* vg, String s, float maxLineWidth, float* bounds) -> void {
				auto* btncstr = StringAsCStr(s);
				NVGtextRow rows[16];
				int nrows;
				float lineh = 0;
				float textBoundsX = 0;
				float textBoundsY = 0;
				nvgTextMetrics(vg, NULL, NULL, &lineh);
				if ((nrows = nvgTextBreakLines(vg, btncstr, nullptr, maxLineWidth, rows, 16))) {
					for (int i = 0; i < nrows; i++) {
						NVGtextRow* row = &rows[i];
						textBoundsX = math::max(textBoundsX, row->width);
						textBoundsY += lineh/* (vg) state->lineHeight*/;
					}
				}
				bounds[0] = textBoundsX;
				bounds[1] = textBoundsY;

			};
			renderPos.y += G_FONT_MIDDLE_OFFSET(size.y);
			if (str.find('\n') != String::npos) {
				float bounds[2];
				getNvgMultiLineTextBounds(vg, str, size.x, bounds);
				renderPos.x = size.x/2.0f - ( bounds[0]/2.0f );
				renderPos.y = size.y/2.0f - ( bounds[1]/2.0f );
				renderPos.y += lineh/2.0f;
			}
			nvgTextBox(vg, 0, renderPos.y, size.x, StringAsCStr(str), NULL);

		}

		if (drawFn) {
			drawFn(vg, renderPos, size, getBackgroundColor(getStateFlags()), drawParm, isEnabled());
		}
		nvgRestore(vg);
	}
}
guictxtmenu_base* guibuttonbase::getTooltip(AppCtrl* appctrl) {
	if (!label.empty()) {

		auto tooltip = new guitooltip<guibuttonbase>(this); //why does casting m_clip to (clip_t*) break the ptr?
		return tooltip;
	}
//	appctrl->openContextMenu(tooltip, appctrl->m_mousePos);
	return nullptr;
}

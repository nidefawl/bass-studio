#include "table.h"
#include <memory>
#include <numeric>
#include <vector>
#include <nanovg.h>
#include "math/seq_math.h"
#include "str_util.h"
#include "theme.h"
#include "event.h"
#include "gui.h"
#include "assert_dbg.h"

namespace Table {

void AdjustColSizes(tbl& table, vec2 size) {
	int maxCols = 0;
	for (tbl_row_t& row : table.rows) {
		maxCols = math::max((int)row.cols.size(), maxCols);
	}
	if ((int)table.colSizes.size() != maxCols) {
		table.colSizes.resize(maxCols);
	}
	if (maxCols > 0) {
		int colWidth = std::round(size.x/(float)maxCols);
		for (int i = 0; i < maxCols; i++) {
			table.colSizes[i] = colWidth;
		}
	}
}
void drawTbl(const table_ctxt_t& ctxt, const tblint& obj) {
	const vec2& pos = ctxt.pos;
	const vec2& size = ctxt.size;
	nvgTextAlign(ctxt.vg, NVG_ALIGN_RIGHT|NVG_ALIGN_BOTTOM);
	nvgText(ctxt.vg, pos.x+size.x-INSET_TABLE_CELL_PADDING, pos.y+size.y-INSET_TABLE_CELL_PADDING, StringAsCStr(StringFormat((obj.format?obj.format:"%d"), obj.i)), nullptr);
}
void drawTbl(const table_ctxt_t& ctxt, const tblstr& obj) {
	const vec2& pos = ctxt.pos;
	const vec2& size = ctxt.size;
	bool right = (obj.flags&1);
	nvgTextAlign(ctxt.vg, (right?NVG_ALIGN_RIGHT:NVG_ALIGN_LEFT)|NVG_ALIGN_BOTTOM);
	nvgText(ctxt.vg, pos.x+(right?size.x-INSET_TABLE_CELL_PADDING:INSET_TABLE_CELL_PADDING), pos.y+size.y-INSET_TABLE_CELL_PADDING, obj.str, nullptr);
}
void drawTbl(const table_ctxt_t& ctxt, const tblString& obj) {
	const vec2& pos = ctxt.pos;
	const vec2& size = ctxt.size;
	bool right = (obj.flags&1);
	nvgTextAlign(ctxt.vg, (right?NVG_ALIGN_RIGHT:NVG_ALIGN_LEFT)|NVG_ALIGN_BOTTOM);
	nvgText(ctxt.vg, pos.x+(right?size.x-INSET_TABLE_CELL_PADDING:INSET_TABLE_CELL_PADDING), pos.y+size.y-INSET_TABLE_CELL_PADDING, StringAsCStr(obj.str), nullptr);
}
template <>
void drawTbl(const table_ctxt_t& ctxt, const tbltype<glm::ivec2>& obj) {
	const vec2& pos = ctxt.pos;
	const vec2& size = ctxt.size;
	nvgTextAlign(ctxt.vg, NVG_ALIGN_RIGHT|NVG_ALIGN_BOTTOM);
	nvgText(ctxt.vg, pos.x+size.x-INSET_TABLE_CELL_PADDING, pos.y+size.y-INSET_TABLE_CELL_PADDING, StringAsCStr(StringFormat((obj.format?obj.format:"%d %d"), obj.t.x, obj.t.y)), nullptr);
}
template <>
void drawTbl(const table_ctxt_t& ctxt, const tbltype<glm::ivec3>& obj) {
	const vec2& pos = ctxt.pos;
	const vec2& size = ctxt.size;
	nvgTextAlign(ctxt.vg, NVG_ALIGN_RIGHT|NVG_ALIGN_BOTTOM);
	nvgText(ctxt.vg, pos.x+size.x-INSET_TABLE_CELL_PADDING, pos.y+size.y-INSET_TABLE_CELL_PADDING, StringAsCStr(StringFormat((obj.format?obj.format:"%d %d %d"), obj.t.x, obj.t.y, obj.t.z)), nullptr);
}
template <>
void drawTbl(const table_ctxt_t& ctxt, const tbltype<glm::ivec4>& obj) {
	const vec2& pos = ctxt.pos;
	const vec2& size = ctxt.size;
	nvgTextAlign(ctxt.vg, NVG_ALIGN_RIGHT|NVG_ALIGN_BOTTOM);
	nvgText(ctxt.vg, pos.x+size.x-INSET_TABLE_CELL_PADDING, pos.y+size.y-INSET_TABLE_CELL_PADDING, StringAsCStr(StringFormat((obj.format?obj.format:"%d %d %d %d"), obj.t.x, obj.t.y, obj.t.z, obj.t.w)), nullptr);
}
template <typename T>
void drawTbl(const table_ctxt_t& ctxt, const tbltype<T>& obj) {
	const vec2& pos = ctxt.pos;
	const vec2& size = ctxt.size;
	nvgTextAlign(ctxt.vg, NVG_ALIGN_LEFT|NVG_ALIGN_BOTTOM);
	nvgText(ctxt.vg, pos.x+INSET_TABLE_CELL_PADDING, pos.y+size.y-INSET_TABLE_CELL_PADDING, "NOT IMPLEMENTED", nullptr);
}
void drawTbl(const table_ctxt_t& ctxt, const String& obj) {
	const vec2& pos = ctxt.pos;
	const vec2& size = ctxt.size;
	nvgTextAlign(ctxt.vg, NVG_ALIGN_LEFT|NVG_ALIGN_BOTTOM);
	nvgText(ctxt.vg, pos.x+INSET_TABLE_CELL_PADDING, pos.y+size.y-INSET_TABLE_CELL_PADDING, StringAsCStr(obj), nullptr);
}

void drawTbl(const table_ctxt_t& ctxt, const tblfloat& obj) {
	const vec2& pos = ctxt.pos;
	const vec2& size = ctxt.size;
	nvgTextAlign(ctxt.vg, NVG_ALIGN_RIGHT|NVG_ALIGN_BOTTOM);
	nvgText(ctxt.vg, pos.x+size.x-INSET_TABLE_CELL_PADDING, pos.y+size.y-INSET_TABLE_CELL_PADDING, StringAsCStr(StringFormat("%f", obj.f)), nullptr);
}
void drawTbl(const table_ctxt_t& ctxt, const int& obj) {
	const vec2& pos = ctxt.pos;
	const vec2& size = ctxt.size;
	nvgFontSize(ctxt.vg, ctxt.fontSize);
	nvgTextAlign(ctxt.vg, NVG_ALIGN_RIGHT|NVG_ALIGN_BOTTOM);
	nvgText(ctxt.vg, pos.x+size.x-INSET_TABLE_CELL_PADDING, pos.y+size.y-INSET_TABLE_CELL_PADDING, StringAsCStr(StringFormat("%d", obj)), nullptr);
}
void drawTbl(const table_ctxt_t& ctxt, const float& obj) {
	const vec2& pos = ctxt.pos;
	const vec2& size = ctxt.size;
	nvgTextAlign(ctxt.vg, NVG_ALIGN_RIGHT|NVG_ALIGN_BOTTOM);
	nvgText(ctxt.vg, pos.x+size.x-INSET_TABLE_CELL_PADDING, pos.y+size.y-INSET_TABLE_CELL_PADDING, StringAsCStr(StringFormat("%f", obj)), nullptr);

}
void drawTbl(const table_ctxt_t& ctxt, const glm::ivec2& obj) {
	const vec2& pos = ctxt.pos;
	const vec2& size = ctxt.size;
	nvgTextAlign(ctxt.vg, NVG_ALIGN_RIGHT|NVG_ALIGN_BOTTOM);


	String strX = StringFormat("X %d", obj.x);
	String strY = StringFormat("Y %d", obj.y);
	int w = 100;
//	if(strX.length() > 5) {
//		w = 180;
//	}
	nvgText(ctxt.vg,
			pos.x+size.x-(w+INSET_TABLE_CELL_PADDING)*1-INSET_TABLE_CELL_PADDING,
			pos.y+size.y-INSET_TABLE_CELL_PADDING, StringAsCStr(strX), nullptr);
	nvgText(ctxt.vg,
			pos.x+size.x-(w+INSET_TABLE_CELL_PADDING)*0-INSET_TABLE_CELL_PADDING,
			pos.y+size.y-INSET_TABLE_CELL_PADDING, StringAsCStr(strY), nullptr);
}
void drawTbl(const table_ctxt_t& ctxt, const glm::ivec4& obj) {
	const vec2& pos = ctxt.pos;
	const vec2& size = ctxt.size;
	nvgTextAlign(ctxt.vg, NVG_ALIGN_RIGHT|NVG_ALIGN_BOTTOM);

	const char* pref[4] = {"X", "Y", "Z", "W"};
	int w = 50;
	for (int i = 0; i < 4; i++) {
		String strX = StringFormat("%s %d", pref[i], obj[i]);
		nvgText(ctxt.vg,
				pos.x+size.x-(w+INSET_TABLE_CELL_PADDING)*(3-i)-INSET_TABLE_CELL_PADDING,
				pos.y+size.y-INSET_TABLE_CELL_PADDING, StringAsCStr(strX), nullptr);
	}
}

table_entry_t& GetCell(tbl& table, int32_t x, int32_t y) {
	dbgassert(y >= 0 && y < table.rows.size());
	tbl_row_t& rowRef = table.rows[y];
	dbgassert(x >= 0 && x < rowRef.cols.size());
	return rowRef.cols[x];
}

bool GetCellClicked(tbl& table, const guitheme_t* theme, glm::vec2 mouse, glm::ivec2& idx, glm::ivec2& screenPos, glm::ivec2& screenSize) {
	idx.x = -1;
	idx.y = -1;
	float tableHeight = table.rows.size()*table.rowHeight;
	if (mouse.y >= 0 && mouse.y < tableHeight) {
		idx.y = (mouse.y - (table.titleCols.size()?table.titleHeight:0))/table.rowHeight;
		screenPos.y = idx.y*table.rowHeight;
		screenSize.y = table.rowHeight;
		int numCols = table.colSizes.size();
		float xPos = 0;
		for (int xCol = 0; xCol < numCols; xCol++) {
			float xColW = table.colSizes[xCol];
			if (mouse.x >= xPos && mouse.x < xPos+xColW) {
				screenPos.x = xPos;
				screenSize.x = xColW;
				idx.x = xCol;
				return true;
			}
			xPos+=xColW;
		}

	}
	return false;
}
void DrawTableNVG(tbl& table, NVGcontext* vg, guitheme_t* theme, vec2 pos, vec2 size, float fontSize) {
	int nTitleCols = table.titleCols.size();
	table_ctxt_t ctxt = {vg, theme, pos, size, fontSize};

	int nContentRows = table.rows.size();
	bool renderColWise = nContentRows > 0;
	if (renderColWise) {
		ctxt.pos = pos;
		int nCols = table.rows[0].cols.size();
		int nContentRows = table.rows.size();
		for (int xCol = 0; xCol < nCols; xCol++) {
			float colSizeX = table.colSizes[xCol];
			ctxt.pos.y = pos.y;
			nvgSave(vg);
			nvgIntersectScissor(vg, ctxt.pos.x, ctxt.pos.y, ctxt.pos.x+colSizeX, ctxt.pos.y+size.y);
			if (nTitleCols) {
				ctxt.size = ivec2(colSizeX, table.titleHeight);
				nvgFontSize(ctxt.vg, fontSize);
				tableDrawEntry(ctxt, table.titleCols[xCol]);
				pos.y+=table.titleHeight;
			}
			for (int yRow = 0; yRow < nContentRows; yRow++) {
				tbl_row_t& row = table.rows[yRow];
				ctxt.size = ivec2(colSizeX, table.rowHeight);
				nvgFontSize(ctxt.vg, fontSize);
				tableDrawEntry(ctxt, row.cols[xCol]);
				ctxt.pos.y+=table.rowHeight;
			}
			ctxt.pos.x+=colSizeX;
			nvgRestore(vg);
		}
	} else {
		if (nTitleCols) {
			ctxt.pos = pos;
			for (int xCol = 0; xCol < nTitleCols; xCol++) {
				ctxt.size = ivec2(table.colSizes[xCol], table.titleHeight);
				nvgFontSize(ctxt.vg, fontSize);
				tableDrawEntry(ctxt, table.titleCols[xCol]);
				ctxt.pos.x+=table.colSizes[xCol];
			}
			pos.y+=table.titleHeight;
		}
		ctxt.pos = pos;
		for (int yRow = 0; yRow < nContentRows; yRow++) {
			tbl_row_t& row = table.rows[yRow];
			int nCols = row.cols.size();
			ctxt.pos.x = pos.x;
			for (int xCol = 0; xCol < nCols; xCol++) {
				float x = table.colSizes[xCol];
				ctxt.size = ivec2(table.colSizes[xCol], table.rowHeight);
				nvgFontSize(ctxt.vg, fontSize);
				tableDrawEntry(ctxt, row.cols[xCol]);
				ctxt.pos.x+=x;
			}
			ctxt.pos.y+=table.rowHeight;
		}
	}
	float height = ctxt.pos.y;
	ctxt.pos = pos;
	ctxt.pos.x += (int)std::accumulate(table.colSizes.begin(), table.colSizes.end(), 0.0f);
	for (int yRow = 0; yRow < nContentRows-1; yRow++) {
		ctxt.pos.y+=table.rowHeight;
		nvgBeginPath(vg);
		nvgMoveTo(vg, pos.x, ctxt.pos.y);
		nvgLineTo(vg, ctxt.pos.x, ctxt.pos.y);
		nvgStrokeColor(vg, theme->getColor(GuiColor::COL_LINE_SEPERATOR));
		nvgStroke(vg);
	}
	int nMaxCols = table.colSizes.size();
	ctxt.pos = pos;
	for (int xCol = 0; xCol < nMaxCols-1; xCol++) {
		ctxt.pos.x+=table.colSizes[xCol];
		nvgBeginPath(vg);
		nvgMoveTo(vg, ctxt.pos.x, pos.y);
		nvgLineTo(vg, ctxt.pos.x, pos.y+height);
		nvgStrokeColor(vg, theme->getColor(GuiColor::COL_LINE_SEPERATOR));
		nvgStroke(vg);
	}
}

}

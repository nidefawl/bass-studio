#include "table.h"
#include "gui.h"
#include <nanovg.h>
#include <glm/glm.hpp>
#include <memory>
#include <numeric>
#include <vector>

using glm::vec2;
void adjustColSizes(tbl& table, vec2 size) {
	int maxCols = 0;
	for (tbl_row_t& row : table.rows) {
		maxCols = std::max((int)row.cols.size(), maxCols);
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
	nvgTextAlign(ctxt.vg, NVG_ALIGN_LEFT|NVG_ALIGN_BOTTOM);
	nvgText(ctxt.vg, pos.x+INSET_TABLE_CELL_PADDING, pos.y+size.y-INSET_TABLE_CELL_PADDING, StringAsCStr(obj.str), nullptr);
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
	String str = StringFormat("%d %d", obj.x, obj.y);
	nvgText(ctxt.vg, pos.x+size.x-INSET_TABLE_CELL_PADDING, pos.y+size.y-INSET_TABLE_CELL_PADDING, StringAsCStr(str), nullptr);
}
void draw(tbl& table, NVGcontext* vg, vec2 pos, vec2 size, float fontSize) {
	int nTitleCols = table.titleCols.size();
	table_ctxt_t ctxt = {vg, pos, size, fontSize};

	if (nTitleCols) {
		ctxt.pos = pos;
		for (int xCol = 0; xCol < nTitleCols; xCol++) {
			ctxt.size = ivec2(table.colSizes[xCol], table.titleHeight);
			nvgFontSize(ctxt.vg, fontSize);
			drawTbl(ctxt, table.titleCols[xCol]);
			ctxt.pos.x+=table.colSizes[xCol];
		}
		pos.y+=table.titleHeight;
	}
	int nContentRows = table.rows.size();
	ctxt.pos = pos;
	for (int yRow = 0; yRow < nContentRows; yRow++) {
		tbl_row_t& row = table.rows[yRow];
		int nCols = row.cols.size();
		ctxt.pos.x = pos.x;
		for (int xCol = 0; xCol < nCols; xCol++) {
			float x = table.colSizes[xCol];
			ctxt.size = ivec2(table.colSizes[xCol], table.rowHeight);
			nvgFontSize(ctxt.vg, fontSize);
			drawTbl(ctxt, row.cols[xCol]);
			ctxt.pos.x+=x;
		}
		ctxt.pos.y+=table.rowHeight;
	}
	float height = ctxt.pos.y;
	ctxt.pos = pos;
	ctxt.pos.x += (int)std::accumulate(table.colSizes.begin(), table.colSizes.end(), 0.0f);
	for (int yRow = 0; yRow < nContentRows-1; yRow++) {
		ctxt.pos.y+=table.rowHeight;
		nvgBeginPath(vg);
		nvgMoveTo(vg, pos.x, ctxt.pos.y);
		nvgLineTo(vg, ctxt.pos.x, ctxt.pos.y);
		nvgStrokeColor(vg, g_guiColors[COL_LINE_SEPERATOR]);
		nvgStroke(vg);
	}
	int nMaxCols = table.colSizes.size();
	ctxt.pos = pos;
	for (int xCol = 0; xCol < nMaxCols-1; xCol++) {
		ctxt.pos.x+=table.colSizes[xCol];
		nvgBeginPath(vg);
		nvgMoveTo(vg, ctxt.pos.x, pos.y);
		nvgLineTo(vg, ctxt.pos.x, pos.y+height);
		nvgStrokeColor(vg, g_guiColors[COL_LINE_SEPERATOR]);
		nvgStroke(vg);
	}
}

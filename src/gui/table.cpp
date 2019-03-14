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
template <typename T>
void drawTbl(const table_ctxt_t& ctxt, const tbltyperef<T>& obj) {
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
	String str = StringFormat("%d %d", obj.x, obj.y);
	nvgText(ctxt.vg, pos.x+size.x-INSET_TABLE_CELL_PADDING, pos.y+size.y-INSET_TABLE_CELL_PADDING, StringAsCStr(str), nullptr);
}
bool getCellClicked(tbl& table, guitheme_t* theme, glm::vec2 mouse, glm::ivec2& res) {
	res.x = -2;
	res.y = -2;
	int nTitleCols = table.titleCols.size();
	auto colClicked = [nTitleCols, mouse, table]() {
		float xPos = 0;
		for (int xCol = 0; xCol < nTitleCols; xCol++) {
			if (mouse.x >= xPos && mouse.x < xPos+table.colSizes[xCol]) {
				return xCol;
			}
			xPos+=table.colSizes[xCol];
		}
		if (mouse.x >= xPos) {
			return nTitleCols-1;
		}
		return -1;
	};
	res.x = colClicked();
	if (nTitleCols && mouse.y < table.titleHeight) {
		res.y = -1;
	} else {
		int idx = (mouse.y - (nTitleCols?table.titleHeight:0))/table.rowHeight;
		res.y = idx;
	}
	return false;
}
void draw(tbl& table, NVGcontext* vg, guitheme_t* theme, vec2 pos, vec2 size, float fontSize) {
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
				drawTbl(ctxt, table.titleCols[xCol]);
				pos.y+=table.titleHeight;
			}
			for (int yRow = 0; yRow < nContentRows; yRow++) {
				tbl_row_t& row = table.rows[yRow];
				ctxt.size = ivec2(colSizeX, table.rowHeight);
				nvgFontSize(ctxt.vg, fontSize);
				drawTbl(ctxt, row.cols[xCol]);
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
				drawTbl(ctxt, table.titleCols[xCol]);
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
				drawTbl(ctxt, row.cols[xCol]);
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
		nvgStrokeColor(vg, theme->getColor(COL_LINE_SEPERATOR));
		nvgStroke(vg);
	}
	int nMaxCols = table.colSizes.size();
	ctxt.pos = pos;
	for (int xCol = 0; xCol < nMaxCols-1; xCol++) {
		ctxt.pos.x+=table.colSizes[xCol];
		nvgBeginPath(vg);
		nvgMoveTo(vg, ctxt.pos.x, pos.y);
		nvgLineTo(vg, ctxt.pos.x, pos.y+height);
		nvgStrokeColor(vg, theme->getColor(COL_LINE_SEPERATOR));
		nvgStroke(vg);
	}
}

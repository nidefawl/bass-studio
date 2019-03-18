#pragma once
#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <memory>
#include <numeric>
#include <vector>
#include "str_util.h"
#include "theme.h"
#include "event.h"
#include "saferef.h"

struct guitheme_t;
struct NVGcontext;
class guibase;
namespace GuiColor {
struct constant_t;
}

#define INSET_TABLE_CELL_PADDING 3
#define INSET_TABLE 1

namespace Table {

struct table_ctxt_t {
	NVGcontext* vg;
	guitheme_t* theme;
	glm::vec2 pos;
	glm::vec2 size;
	float fontSize;
//	int align;
};
class click_type_handler;
struct click_ctxt_t {
	guibase* gui;
	click_type_handler* callback;
	MouseEvent evt;
};
class table_entry_t {
public:
	template <typename T>
	table_entry_t(T x) : self_(std::make_shared<model<T>>(std::move(x))) {
	}
	friend void tableDrawEntry(const table_ctxt_t& ctxt, const table_entry_t& x) {
		x.self_->drawTblImpl(ctxt);
	}
	friend void tableCellClicked(const click_ctxt_t& ctxt, const table_entry_t& x) {
		x.self_->cellClickedImpl(ctxt);
	}
private:
	struct concept_t {
		virtual ~concept_t() = default;
		virtual void drawTblImpl(const table_ctxt_t&) const = 0;
		virtual void cellClickedImpl(const click_ctxt_t&) const = 0;
	};
	template <typename T>
	struct model final : concept_t {
		model(T x) : data_(std::move(x)) { }
		void drawTblImpl(const table_ctxt_t& ctxt)  const override {
			drawTbl(ctxt, data_);
		}
		void cellClickedImpl(const click_ctxt_t& ctxt)  const override {
			cellClicked(ctxt, data_);
		}
		T data_;
	};
	std::shared_ptr<const concept_t> self_;
};
struct tbl_row_t {
	std::vector<table_entry_t> cols;
};
struct tbl {
	float titleHeight;
	float rowHeight;
	std::vector<float> colSizes;
	std::vector<table_entry_t> titleCols;
	std::vector<tbl_row_t> rows;
};

void DrawTableNVG(tbl& table, NVGcontext* vg, guitheme_t* theme, glm::vec2 pos, glm::vec2 size, float fontSize);
table_entry_t& GetCell(tbl& table, int32_t x, int32_t y);
bool GetCellClicked(tbl& table, const guitheme_t* theme, glm::vec2 mouse, glm::ivec2& idx, glm::ivec2& cellPos, glm::ivec2& cellSize);
void AdjustColSizes(tbl& table, glm::vec2 size);

struct tblString {
	String str;
	int flags = 0;
};
struct tblstr {
	const char* str = nullptr;
	int flags = 0;
};
struct tblint {
	int64_t i;
	const char* format = nullptr;
};
template <typename T>
struct tbltype {
	T t;
	const char* format = nullptr;
};
template <typename T>
struct tbltyperef {
	T& t;
	const char* format = nullptr;
};
template <typename T>
struct tbltypesaferef {
	SafeRef<guibase> saferef;
	T& t;
	const char* format = nullptr;
};
template <typename T>
inline void drawTbl(const table_ctxt_t& ctxt, T& obj) {
	drawTbl(ctxt, const_cast<const T&>(obj));
}
template <typename T>
void drawTbl(const table_ctxt_t& ctxt, const tbltype<T>& obj);
template <typename T>
inline void drawTbl(const table_ctxt_t& ctxt, const tbltyperef<T>& obj) {
	drawTbl(ctxt, const_cast<const T&>(obj.t));
//	const vec2& pos = ctxt.pos;
//	const vec2& size = ctxt.size;
//	nvgTextAlign(ctxt.vg, NVG_ALIGN_LEFT|NVG_ALIGN_BOTTOM);
//	nvgText(ctxt.vg, pos.x+INSET_TABLE_CELL_PADDING, pos.y+size.y-INSET_TABLE_CELL_PADDING, "NOT IMPLEMENTED", nullptr);
}
template <typename T>
inline void drawTbl(const table_ctxt_t& ctxt, const tbltypesaferef<T>& obj) {
	if (safeRefOk(obj.saferef)) {
		drawTbl(ctxt, const_cast<const T&>(obj.t));
	}
}
struct tblfloat {
	float f;
};
void drawTbl(const table_ctxt_t& ctxt, const tblfloat& obj);
void drawTbl(const table_ctxt_t& ctxt, const tblString& obj);
void drawTbl(const table_ctxt_t& ctxt, const tblstr& obj);
void drawTbl(const table_ctxt_t& ctxt, const tblint& obj);
void drawTbl(const table_ctxt_t& ctxt, const int& obj);
void drawTbl(const table_ctxt_t& ctxt, const float& obj);
void drawTbl(const table_ctxt_t& ctxt, const String& obj);
void drawTbl(const table_ctxt_t& ctxt, const glm::ivec2& obj);
template <typename T>
inline void cellClicked(const click_ctxt_t& ctxt, T& obj) {

}
//class click_type_handler {
//public:
//	virtual void onClickNotImplemented(const click_ctxt_t& ctxt) = 0;
//	virtual void onClick(const click_ctxt_t& ctxt, glm::ivec2& value) = 0;
//	virtual void onClick(const click_ctxt_t& ctxt, NVGcolor& value) = 0;
//	virtual void onClick(const click_ctxt_t& ctxt, guitheme_t* theme, GuiColor::constant_t constant) = 0;
//	virtual void onClick(const click_ctxt_t& ctxt, guitheme_t* theme, GuiConstant::constant_t constant) = 0;
//	virtual ~click_type_handler() {
//
//	}
//};
//template <typename T>
//inline void cellClicked(const click_ctxt_t& ctxt, const tbltyperef<T>& obj) {
//	if (ctxt.callback) {
//		ctxt.callback->onClickNotImplemented(ctxt);
//	}
//}
//template <typename T>
//inline void cellClicked(const click_ctxt_t& ctxt, const tbltypesaferef<T>& obj) {
//	if (safeRefOk(obj.saferef)) {
//		cellClicked(ctxt, obj.t);
//	}
//}
//template <>
//inline void cellClicked(const click_ctxt_t& ctxt, const tbltypesaferef<glm::ivec2>& obj) {
//	if (safeRefOk(obj.saferef)) {
//		if (ctxt.callback) {
//			ctxt.callback->onClick(ctxt, obj.t);
//		}
//	}
//}
//template <>
//inline void cellClicked(const click_ctxt_t& ctxt, const tbltyperef<NVGcolor>& obj) {
//	if (ctxt.callback) {
//		ctxt.callback->onClick(ctxt, obj.t);
//	}
//}
//template <>
//inline void cellClicked(const click_ctxt_t& ctxt, const tbltypesaferef<NVGcolor>& obj) {
//	if (safeRefOk(obj.saferef)) {
//		if (ctxt.callback) {
//			ctxt.callback->onClick(ctxt, obj.t);
//		}
//	}
//}
}

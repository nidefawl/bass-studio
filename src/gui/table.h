#pragma once
#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <memory>
#include <numeric>
#include <vector>
#include "str_util.h"
#include "theme.h"

struct NVGcontext;
struct table_ctxt_t {
	NVGcontext* vg;
	guitheme_t* theme;
	glm::vec2 pos;
	glm::vec2 size;
	float fontSize;
//	int align;
};
class table_entry_t {
public:
	template <typename T>
	table_entry_t(T x) : self_(std::make_shared<model<T>>(std::move(x))) {
	}
	friend void drawTbl(const table_ctxt_t& ctxt, const table_entry_t& x) {
		x.self_->drawTbl_(ctxt);
	}
private:
	struct concept_t {
		virtual ~concept_t() = default;
		virtual void drawTbl_(const table_ctxt_t&) const = 0;
	};
	template <typename T>
	struct model final : concept_t {
		model(T x) : data_(std::move(x)) { }
		void drawTbl_(const table_ctxt_t& ctxt)  const override {
			drawTbl(ctxt, data_);
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

#define INSET_TABLE_CELL_PADDING 3
#define INSET_TABLE 1
struct guitheme_t;
void draw(tbl& table, NVGcontext* vg, guitheme_t* theme, glm::vec2 pos, glm::vec2 size, float fontSize);
bool getCellClicked(tbl& table, guitheme_t* theme, glm::vec2 mouse, glm::ivec2& res);
void adjustColSizes(tbl& table, glm::vec2 size);

struct tblstr {
	tblstr(const char* chr) : str(chr)  {
	}
	tblstr(String str) : str(std::move(str)) {
	}
	String str;
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
void drawTbl(const table_ctxt_t& ctxt, const tbltype<T>& obj);
template <typename T>
void drawTbl(const table_ctxt_t& ctxt, const tbltyperef<T>& obj);
struct tblfloat {
	float f;
};
void drawTbl(const table_ctxt_t& ctxt, const tblfloat& obj);
void drawTbl(const table_ctxt_t& ctxt, const tblstr& obj);
void drawTbl(const table_ctxt_t& ctxt, const tblint& obj);
void drawTbl(const table_ctxt_t& ctxt, const int& obj);
void drawTbl(const table_ctxt_t& ctxt, const float& obj);
void drawTbl(const table_ctxt_t& ctxt, const String& obj);
void drawTbl(const table_ctxt_t& ctxt, const glm::ivec2& obj);


#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <glm/geometric.hpp>
#include <memory>
#include <numeric>


#include "gui.h"
#include "guicontextmenu.h"
#include "guicontainer.h"
#include "gui/textfield.h"
#include "table.h"

#include "guicontainer.h"
#include "../gui/guiscrollcontainer.h"
#include "debugproperties.h"

struct guiproperties_t {
	SafeRef<guibase> safeRef;
};


#define FONT_SIZE_TOOLTIP_TITLE 24
#define FONT_SIZE_TOOLTIP 20

template <typename T>
class guiproperties_table : public debugproperties {
protected:
	T* ptr;
	bool hadMouseMovement = false;
	tbl table;
	gui_textfield textField;
public:
	guiproperties_table(T* _ptr) : debugproperties(), ptr(_ptr) {
		add(&textField);
		textField.setVisible(false);
		padding = 0;
		margin = 0;
//		scrollbarOutside=true;
//		maxHeight = 220;
	}
	~guiproperties_table() {
		remove(&textField);
	}
	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
		hadMouseMovement = true;
		if (contains(mpos)) {
			evt.requestFocus(this);
			return true;
		}
		return false;
	}
//	bool isTransient() override {
//		return true;
//	}
//	bool canClose() override {
//		return hadMouseMovement && !parentCtrl->isMouseInside();
//	}
	virtual void clicked(int _id) {
		parentCtrl->closePopup();
		parentCtrl->closeContextMenu();
	}
	virtual void handleDraggedBegin(MouseEvent& evt) override {
		ivec2 local = evt.relMousepos;
		ivec2 tableMin = ivec2(INSET_TABLE);
		ivec2 tableMax = tableMin + getSizeContent()-ivec2(INSET_TABLE<<1);
		if (local.x >= tableMin.x && local.y >= tableMin.y && local.x < tableMax.x && local.y < tableMax.y) {
			ivec2 res(-3);
			getCellClicked(table, theme, local-tableMin, res);
			if (res.x >= 0 && res.y >= 0) {
				my_printf("Clicked %d %d\n", res.x, res.y);
			}
		}
		return;
	}
	void onTick(AppCtrl* appctrl) {
		layout();
	}
	void layout();
	void render(NVGcontext* vg) {
		if (!setScissorTransformContainer(vg)) {
			return;
		}
		ivec2 mSize = this->size;
		ivec2 csSize = this->getSizeContent();
		setFont(vg, FONT_SIZE_TOOLTIP_TITLE, G_WHITE, NVG_ALIGN_LEFT|NVG_ALIGN_BOTTOM);
		draw(table, vg, theme, ivec2(INSET_TABLE), getSizeContent()-ivec2(INSET_TABLE<<1), FONT_SIZE_TOOLTIP);
//		ivec2 tSize = getSizeContent()-ivec2(INSET_TABLE<<1);
//		nvgBeginPath(vg);
//		nvgRect(vg, INSET_TABLE, INSET_TABLE, tSize.x, tSize.y);
//		nvgFillColor(vg, nvgRGBAf(1.0f, 0.0f, 1.0f, 0.4f));
//		nvgFill(vg);
//		nvgBeginPath(vg);
//		nvgRect(vg, INSET_TABLE+tSize.x-30, INSET_TABLE, 15, tSize.y);
//		nvgFillColor(vg, nvgRGBAf(0.0f, 1.0f, 1.0f, 0.4f));
//		nvgFill(vg);
		if (textField.isVisible()) {
			textField.render(vg);
		}
	}
	void setDebugPropertyHandle(void *ptr) override;
	void determineSize() override {

	}
};

template <>
void guiproperties_table<guiproperties_t>::layout()  {
	if (size.x == 0)
		size.x = 450;
	table.rowHeight = FONT_SIZE_TOOLTIP+INSET_TABLE_CELL_PADDING*2;
	table.rows.clear();
	table.titleCols.clear();
	table.colSizes.clear();
	guibase* ref = ptr->safeRef.handler ? ptr->safeRef.handler->safeRefGet(ptr->safeRef.refId) : nullptr;
	if (ref)
	{
		table.rows.push_back({{tblstr{"self"}, tbltype<SafeRef<guibase>>{ptr->safeRef, nullptr}}});
		table.rows.push_back({{tblstr{"pos"}, tbltype<glm::ivec2>{ref->pos, nullptr}}});
		table.rows.push_back({{tblstr{"size"}, tbltype<glm::ivec2>{ref->size, nullptr}}});
		SafeRef<guibase> safeRef;
		if (ref->parent) {
			safeRef = ref->parent->makeSafeRef();
			table.rows.push_back({{tblstr{"parent"}, tbltype<SafeRef<guibase>>{safeRef, nullptr}}});
		} else {
			table.rows.push_back({{tblstr{"parent"}, tblstr{"<null>"}}});
		}
//		table.rows.push_back({{tblstr{"tracklink"}, tblint{(int64_t)ptr->effect->getTrackLink(), "%12x"}}});
//		table.rows.push_back({{tblstr{"bIsSetup"}, tblint{ptr->effect->bIsSetup}}});
//		table.rows.push_back({{tblstr{"bIsEnabled"}, tblint{ptr->effect->bIsEnabled}}});
//		table.rows.push_back({{tblstr{"PARAM_ENABLE"}, tblfloat{ptr->effect->getParamValue(PARAM_ENABLE)}}});
	} else {
		my_printf("went away\n", 0);
	}
	ivec2 tableSize = getSizeContent()-ivec2(INSET_TABLE<<1);
	adjustColSizes(table, tableSize);
	if (table.colSizes.size() == 2) {
		table.colSizes[0] = std::max(100.0f, std::min(250.0f, 0.25f*tableSize.x));
		table.colSizes[1] = tableSize.x - table.colSizes[0];
	}
	size.y = table.rows.size()*table.rowHeight;

}
template <>
void guiproperties_table<guiproperties_t>::onTick(AppCtrl* appctrl) {
//	guibase* ref = ptr->safeRef.handler ? ptr->safeRef.handler->safeRefGet(ptr->safeRef.refId) : nullptr;
//	auto ptrNew = appctrl->getGuiFocused();
//	if (ref != ptrNew) {
//		if (ptrNew) {
//			ptr->safeRef = ptrNew->makeSafeRef();
//		} else {
//			ptr->safeRef = SafeRef<guibase>();
//		}
//		layout();
//	}
}
template <>
void guiproperties_table<guiproperties_t>::setDebugPropertyHandle(void *vPtr)  {
	guibase* ref = ptr->safeRef.handler ? ptr->safeRef.handler->safeRefGet(ptr->safeRef.refId) : nullptr;
	guibase* pGui = static_cast<guibase*>(vPtr);
	if (ref != pGui) {
		if (pGui) {
			ptr->safeRef = pGui->makeSafeRef();
		} else {
			ptr->safeRef = SafeRef<guibase>();
		}
		layout();
		this->onChildLayoutChanged(this);
	}
}
template <>
void drawTbl(const table_ctxt_t& ctxt, const tbltyperef<NVGcolor>& obj) {
	const vec2& pos = ctxt.pos;
	const vec2& size = ctxt.size;
	nvgTextAlign(ctxt.vg, NVG_ALIGN_RIGHT|NVG_ALIGN_BOTTOM);
	int32_t colorRgb = nvgToRGB(obj.t);
	String strColorHex = StringFormat(obj.format, colorRgb);
	int sizeQuad = size.y-INSET_TABLE_CELL_PADDING*2;

	nvgText(ctxt.vg, pos.x+size.x-INSET_TABLE_CELL_PADDING*2-sizeQuad, pos.y+size.y-INSET_TABLE_CELL_PADDING, StringAsCStr(strColorHex), nullptr);

	nvgBeginPath(ctxt.vg);
	nvgRect(ctxt.vg, pos.x+size.x-INSET_TABLE_CELL_PADDING-sizeQuad, pos.y+INSET_TABLE_CELL_PADDING,
			sizeQuad, sizeQuad);
	nvgFillColor(ctxt.vg, obj.t);
	nvgFill(ctxt.vg);
	nvgFillColor(ctxt.vg, G_WHITE);
}
template <>
void guiproperties_table<guitheme_t>::layout()  {
	//	size.x = 250;
		table.rowHeight = FONT_SIZE_TOOLTIP+INSET_TABLE_CELL_PADDING*2;
		table.rows.clear();
		table.titleCols.clear();
		table.colSizes.clear();
		if (ptr)
		{

			table.rows.push_back({{tblstr{"colorBg"}, tbltyperef<NVGcolor>{ptr->colorBg, "%08X"}}});
			table.rows.push_back({{tblstr{"colorBgHover"}, tbltyperef<NVGcolor>{ ptr->colorBgHover, "%08X"}}});
			table.rows.push_back({{tblstr{"colorBgPressed"}, tbltyperef<NVGcolor>{ptr->colorBgPressed, "%08X"}}});
			table.rows.push_back({{tblstr{"colorBgFocused"}, tbltyperef<NVGcolor>{ptr->colorBgFocused, "%08X"}}});
			table.rows.push_back({{tblstr{"colorBgDisabled"}, tbltyperef<NVGcolor>{ptr->colorBgDisabled, "%08X"}}});
			table.rows.push_back({{tblstr{"colorBgFrameBase"}, tbltyperef<NVGcolor>{ptr->colorBgFrameBase, "%08X"}}});
			table.rows.push_back({{tblstr{"colorBgFrameOutline"}, tbltyperef<NVGcolor>{ptr->colorBgFrameOutline, "%08X"}}});
			table.rows.push_back({{tblstr{"colorBgFrameHighlight"}, tbltyperef<NVGcolor>{ptr->colorBgFrameHighlight, "%08X"}}});
			for (int i = 0; i < NUM_GUI_COLORS; i++) {
				table.rows.push_back({{tblstr{StringFormat("guiColors[%d]", i)}, tbltyperef<NVGcolor>{ptr->guiColors[i], "%08X"}}});
			}

		} else {
			my_printf("went away\n", 0);
		}
		ivec2 tableSize = getSizeContent()-ivec2(INSET_TABLE<<1);
		adjustColSizes(table, tableSize);
		table.colSizes[0] = std::max(100.0f, std::min(250.0f, 0.25f*tableSize.x));
		table.colSizes[1] = tableSize.x - table.colSizes[0];

		size.y = table.rows.size()*table.rowHeight;
}
template <>
void guiproperties_table<guitheme_t>::onTick(AppCtrl* appctrl) {

}
template <>
void guiproperties_table<guitheme_t>::setDebugPropertyHandle(void *vPtr) {
	ptr = static_cast<guitheme_t*>(vPtr);
}

template <>
void drawTbl(const table_ctxt_t& ctxt, const tbltype<SafeRef<guibase>>& obj) {
	const vec2& pos = ctxt.pos;
	const vec2& size = ctxt.size;
	nvgTextAlign(ctxt.vg, NVG_ALIGN_RIGHT|NVG_ALIGN_BOTTOM);
	guibase* ref = obj.t.handler ? obj.t.handler->safeRefGet(obj.t.refId) : nullptr;
	if (ref)
	{
		String strAddr = StringFormat("0x%6X (refId %d)", (int64_t)ref, obj.t.refId);
		String className = ref->getClassName();
		nvgText(ctxt.vg, pos.x+size.x-INSET_TABLE_CELL_PADDING, pos.y+size.y-INSET_TABLE_CELL_PADDING, StringAsCStr(StringFormat((obj.format?obj.format:"%s@%s"), StringAsCStr(className), StringAsCStr(strAddr))), nullptr);

	} else {
		nvgText(ctxt.vg, pos.x+size.x-INSET_TABLE_CELL_PADDING, pos.y+size.y-INSET_TABLE_CELL_PADDING, StringAsCStr(StringFormat("<null> RefId %d", obj.t.refId)), nullptr);

	}
}
guitheme_t* getDefaultTheme();
guictr_base* makeCtrTheme() {
	guictr_scrollbar* ctr = new guictr_scrollbar(new guiproperties_table<guitheme_t>(getDefaultTheme()));
	ctr->maxHeight = -1;
	return ctr;
}
static guiproperties_table<guiproperties_t>* firstInstance = nullptr;
debugproperties* getPropertiesTable(){
	return firstInstance;
}
debugproperties* makeCtrProperties2() {
	return new guiproperties_table<guiproperties_t>(new guiproperties_t());
}
guictr_base* makeCtrProperties() {
	auto* ptr = new guiproperties_table<guiproperties_t>(new guiproperties_t());
	if (!firstInstance)
		firstInstance = ptr;
	return ptr;
}

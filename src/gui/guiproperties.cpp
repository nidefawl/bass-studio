#include <glm/geometric.hpp>
#include <nanovg.h>
#include <vector>
#include <memory>
#include <numeric>



#include "str_util.h"
#include "saferef.h"
#include "seq_util.h"
#include "color_util.h"
#include "event.h"
#include "mouse.h"
#include "gui.h"
#include "guicolors.h"
#include "guicontainer.h"
#include "guiconstant.h"
#include "guicontextmenu_base.h"
#include "guicontextmenu.h"
#include "textfield.h"
#include "button.h"
#include "guicolorpick.h"
#include "table.h"
#include "theme.h"
#include "thememgr.h"

#include "guicontainer.h"
#include "guiscrollcontainer.h"
#include "guicolorpick.h"
#include "dropdown.h"
#include "debugproperties.h"
#include "guiinputfield.h"
#include "logging.h"

namespace Table {

	class click_type_handler {
	public:
		virtual void onClickNotImplemented(const click_ctxt_t& ctxt) = 0;
		virtual void onClick(const click_ctxt_t& ctxt, glm::ivec2& value) = 0;
		virtual void onClick(const click_ctxt_t& ctxt, NVGcolor& value) = 0;
		virtual void onClick(const click_ctxt_t& ctxt, guitheme_t* theme, GuiColor::constant_t constant) = 0;
		virtual void onClick(const click_ctxt_t& ctxt, guitheme_t* theme, GuiConstant::constant_t constant) = 0;
		virtual ~click_type_handler() {

		}
	};
	template <typename T>
	struct tbltypesaferef {
		SafeRef<guibase> saferef;
		T& t;
		const char* format = nullptr;
	};
	struct tbltype_gui_flags {
		SafeRef<guibase> saferef;
		int mask;
	};

	template <typename T>
	inline void cellClicked(const click_ctxt_t& ctxt, const tbltyperef<T>& obj) {
		if (ctxt.callback) {
			ctxt.callback->onClickNotImplemented(ctxt);
		}
	}
	template <typename T>
	inline void cellClicked(const click_ctxt_t& ctxt, const tbltypesaferef<T>& obj) {
		if (safeRefOk(obj.saferef)) {
			cellClicked(ctxt, obj.t);
		}
	}
	template <>
	inline void cellClicked(const click_ctxt_t& ctxt, const tbltypesaferef<glm::ivec2>& obj) {
		if (safeRefOk(obj.saferef)) {
			if (ctxt.callback) {
				ctxt.callback->onClick(ctxt, obj.t);
			}
		}
	}
	template <>
	inline void cellClicked(const click_ctxt_t& ctxt, const tbltyperef<NVGcolor>& obj) {
		if (ctxt.callback) {
			ctxt.callback->onClick(ctxt, obj.t);
		}
	}
	template <>
	inline void cellClicked(const click_ctxt_t& ctxt, const tbltypesaferef<NVGcolor>& obj) {
		if (safeRefOk(obj.saferef)) {
			if (ctxt.callback) {
				ctxt.callback->onClick(ctxt, obj.t);
			}
		}
	}
}
using namespace Table;

struct guiproperties_t {
	SafeRef<guibase> safeRef;
};


#define FONT_SIZE_TOOLTIP_TITLE 24
#define FONT_SIZE_TOOLTIP 20

template <typename T>
class guiproperties_table : public debugproperties {
protected:
	struct cellclicked_t {
		ivec2 idx{-1,-1};
		ivec2 pos{0};
		ivec2 size{0};
	};
	T* ptr;
	Table::tbl table;
	gui_textfield textField;
	gui_numberinput_field numberInput;
	gui_color_pick colorPick;
	bool mouseDown = false;
	cellclicked_t lastClicked;
	std::vector<guibase*> controls;
	int32_t number;
	const bool isGlobalInstance;
public:
	guiproperties_table(T* _ptr, bool _isGlobalInstance)
		: debugproperties(),
		  ptr(_ptr),
		  numberInput(nullptr),
		  isGlobalInstance(_isGlobalInstance)
	{
		setBackgroundRendered(true);
		setBackgroundRenderedInset(false);
		setSnapSides(ivec4(1));
		addControl(&textField);
		addControl(&numberInput);
		addControl(&colorPick);

		textField.fnFocus = [this](MouseHitEvt& evt, bool focused) {
			if (!focused) {
				setActiveControl(nullptr);
			}
		};
		numberInput.getField().fnFocus = [this](MouseHitEvt& evt, bool focused) {
			if (!focused) {
				setActiveControl(nullptr);
			}
		};
		padding = 0;
		margin = 0;
//		scrollbarOutside=true;
//		maxHeight = 220;
	}
	~guiproperties_table() {
		removeGuis();
	}
	void addControl(guibase* g) {
		controls.push_back(g);
		g->setVisible(false);
		add(g);
	}
	void setActiveControl(guibase* g) {
		assert(!g || isControl(g));
		for (guibase* g2 : controls) {
			g2->setVisible(g2 == g);
		}
		if (g) {
			this->parentCtrl->focusGui(g);
		}
	}
	bool isControl(guibase* g) {
		return STL_CONTAINS(controls, g);
	}
	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
		validateReferences();
		if (this->contains(mpos)) {
			ivec2 localMouse = this->toContainerSpace(mpos);
			for (guibase* gui : guis) {
				if (!gui->isVisible())
					continue;
				if (gui->mouseHitTest(localMouse, evt)) {
					return true;
				}
			}
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
	}
	virtual void handleDraggedBegin(MouseEvent& evt) override {
		validateReferences();
		lastClicked = cellclicked_t();
		ivec2 local = evt.relMousepos;
		ivec2 tableMin = ivec2(INSET_TABLE);
		ivec2 tableMax = tableMin + getSizeContent()-ivec2(INSET_TABLE<<1);
		if (local.x >= tableMin.x && local.y >= tableMin.y && local.x < tableMax.x && local.y < tableMax.y) {
			mouseDown = true;
			GetCellClicked(table, theme, local-tableMin, lastClicked.idx, lastClicked.pos, lastClicked.size);
			onCellClicked(lastClicked, evt);
		}
		if (lastClicked.idx.x < 0 || lastClicked.idx.y < 0) {
			setActiveControl(nullptr);
		}
	}
	virtual void handleDraggedMove(MouseEvent& evt) {
		validateReferences();
	}
	virtual void handleDraggedRelease(MouseEvent& evt) {
		validateReferences();
		if (mouseDown) {
			mouseDown = false;
			onCellClicked(lastClicked, evt);
			lastClicked = cellclicked_t();
		}
	}
	void onCellClicked(cellclicked_t cell, MouseEvent& evt) {
		if (cell.idx.x >= 0 && cell.idx.y >= 0) {
			if (evt.type == MouseEventType::M_EVT_BTN_DOWN) {
				table_entry_t& tableCell = GetCell(table, cell.idx.x, cell.idx.y);
				class click_handler_t : public click_type_handler {
					guiproperties_table* const table;
					cellclicked_t& clickedcell;
					MouseEvent& evt;
				public:
					click_handler_t(guiproperties_table* _table, cellclicked_t& _clickedcell, MouseEvent& _evt)
						: click_type_handler(), table(_table), clickedcell(_clickedcell), evt(_evt) {

					}
					void click() {
					}
					virtual void onClickNotImplemented(const click_ctxt_t& ctxt) override {
						table->setActiveControl(nullptr);
					}
					void onClick(const click_ctxt_t& ctxt, glm::ivec2& value) override {
						click();
						int32_t posRight = clickedcell.pos.x+clickedcell.size.x-100;
						bool wasRightSide = evt.relMousepos.x > posRight;
						gui_numberinput_field& numberInput = table->numberInput;
						numberInput.size = clickedcell.size;
						if (wasRightSide) {
							numberInput.pos = ivec2(posRight, clickedcell.pos.y);
							numberInput.size.x = 100;
							numberInput.setRef(&value.y);
						} else {
							numberInput.pos = ivec2(posRight-100, clickedcell.pos.y);
							numberInput.size.x = 100;
							numberInput.setRef(&value.x);
						}
						table->setActiveControl(&numberInput);
						evt.guiDragged = &numberInput;
						numberInput.handleDraggedBegin(evt);
					}
					void onClick(const click_ctxt_t& ctxt, guitheme_t* theme, GuiConstant::constant_t constant) override {
						click();
						gui_numberinput_field& numberInput = table->numberInput;
						table->number = theme->get(constant);
						numberInput.setRef(&table->number);
						numberInput.pos = clickedcell.pos;
						numberInput.size = clickedcell.size;
						BaseCtrl* const ctrl = table->parentCtrl;
						numberInput.fnValueEditChanged = [theme, constant, ctrl](gui_numberinput_field*,int32_t rgba) {
							theme->set(constant, rgba);
							if (ctrl)
								ctrl->relayout();
						};
						numberInput.fnClamp = [](int32_t i) {
							return i > 1000 ? 1000 : i < 1 ? 1 : i;
						};
						table->setActiveControl(&numberInput);
						evt.guiDragged = &numberInput;
						numberInput.handleDraggedBegin(evt);
					}
					void onClick(const click_ctxt_t& ctxt, guitheme_t* theme, GuiColor::constant_t constant) override {
						click();
						gui_color_pick* color = new gui_color_pick();
						color->size = {480, 240};
						color->pos = {0, 0};
						//color->setRefNvg(&value);
						color->setInt32(theme->getColorInt32(constant));
						color->layout();
						color->fnSetValue = [theme, constant](int32_t rgba) {
							theme->setColor(constant, rgba);
						};
						guictxtmenu_base* ctxtMenu = new guictxtmenu_base();
						ctxtMenu->size = color->size;
						ctxtMenu->add(color);
						ctxtMenu->layout();
						ctxtMenu->canTakeInputFocus = true;
						assert(!ctxtMenu->isBackgroundRendered());
						ctxtMenu->setBackgroundRendered(false);
						table->parentCtrl->openContextMenu(ctxtMenu, evt.mousepos);
						table->setActiveControl(nullptr);
						assert(!ctxtMenu->isBackgroundRendered());
					}

					void onClick(const click_ctxt_t& ctxt, NVGcolor& value) override {
						click();
						gui_color_pick* color = new gui_color_pick();
						color->size = { 480, 240 };
						color->pos = { 0, 0 };
						color->setRefNvg(&value);
						color->setInt32(nvgToRGBA(value));
						color->layout();
						guictxtmenu_base* ctxtMenu = new guictxtmenu_base();
						ctxtMenu->size = color->size;
						ctxtMenu->add(color);
						ctxtMenu->layout();
						ctxtMenu->canTakeInputFocus = true;
						assert(!ctxtMenu->isBackgroundRendered());
						ctxtMenu->setBackgroundRendered(false);
						table->parentCtrl->openContextMenu(ctxtMenu, evt.mousepos);
						table->setActiveControl(nullptr);
						assert(!ctxtMenu->isBackgroundRendered());
					}
				};
				click_handler_t handler( this, cell, evt );
				const click_ctxt_t ctxt = {this, &handler, evt};
				tableCellClicked(ctxt, tableCell);
			}
		} else {
			setActiveControl(nullptr);
		}
	}
	void onTick(AppCtrl* appctrl) {
//		layout();
	}
	void layout();
	void render(NVGcontext* vg);
	void validateReferences() {

	}
	void renderDefault(NVGcontext* vg) {
		if (isBackgroundRendered()){
			renderBackground(vg);
		}
		if (!setScissorTransformContainer(vg)) {
			return;
		}
		setFont(vg, FONT_SIZE_TOOLTIP_TITLE, G_WHITE, NVG_ALIGN_LEFT|NVG_ALIGN_BOTTOM);
		Table::DrawTableNVG(table, vg, theme, ivec2(INSET_TABLE), getSizeContent()-ivec2(INSET_TABLE<<1), FONT_SIZE_TOOLTIP);
		for (guibase* ctrl : controls) {
			if (ctrl->isVisible()) {
				ctrl->render(vg);
			}
		}
	}
	void setDebugPropertyHandle(void *ptr) override;
	void determineSize(glm::ivec2& prefSize) override {

	}
};


template<typename T>
void addPropertiesFromGui(T& gui, Table::tbl* table);
template<>
void addPropertiesFromGui(guibase& gui, Table::tbl* table) {
	SafeRef<guibase> ref = gui.makeSafeRef();
	std::vector<tbl_row_t>& rows = table->rows;
	rows.push_back({{tblstr{"this"}, ref}});
	rows.push_back({{tblstr{"pos"}, tbltypesaferef<glm::ivec2>{ref, gui.pos, nullptr}}});
	rows.push_back({{tblstr{"size"}, tbltypesaferef<glm::ivec2>{ref, gui.size, nullptr}}});

	rows.push_back({{tblstr{"FLG_VISIBLE"}, tbltype_gui_flags{ref, FLG_VISIBLE}}});
	rows.push_back({{tblstr{"FLG_RENDER_BACKGROUND"}, tbltype_gui_flags{ref, FLG_RENDER_BACKGROUND}}});
	rows.push_back({{tblstr{"FLG_RENDER_BACKGROUND_INSET"}, tbltype_gui_flags{ref, FLG_RENDER_BACKGROUND_INSET}}});
	rows.push_back({{tblstr{"FLG_ENBL"}, tbltype_gui_flags{ref, FLG_ENBL}}});
	rows.push_back({{tblstr{"FLG_HVRD"}, tbltype_gui_flags{ref, FLG_HVRD}}});
	rows.push_back({{tblstr{"FLG_FOC"}, tbltype_gui_flags{ref, FLG_FOC}}});
	rows.push_back({{tblstr{"FLG_ACT"}, tbltype_gui_flags{ref, FLG_ACT}}});
	rows.push_back({{tblstr{"FLG_DRG"}, tbltype_gui_flags{ref, FLG_DRG}}});
	rows.push_back({{tblstr{"FLG_HAS_COLOR_BG"}, tbltype_gui_flags{ref, FLG_HAS_COLOR_BG}}});

	if (gui.parent) {
		SafeRef<guibase> parentSafeRef = gui.parent->makeSafeRef();
		rows.push_back({{tblstr{"parent"}, parentSafeRef}});
	} else {
		rows.push_back({{tblstr{"parent"}, tblstr{"<null>"}}});
	}
	String strTheme = gui.theme->name+StringFormat("[%7X]", (int64_t)gui.theme);
	rows.push_back({{tblstr{"theme"}, tblString{strTheme, 1}}});
	rows.push_back({{tblstr{"theme2"}, tblString{strTheme, 1}}});
}

template <>
void guiproperties_table<guiproperties_t>::layout()  {
	if (size.x == 0)
		size.x = 450;
	table.rowHeight = FONT_SIZE_TOOLTIP+INSET_TABLE_CELL_PADDING*2;
	table.rows.clear();
	table.titleCols.clear();
	table.colSizes.clear();
	guibase* ref = safeRefGet(ptr->safeRef);
	if (ref)
	{
		ref->addProperties(&table);
	}
	ivec2 tableSize = getSizeContent()-ivec2(INSET_TABLE<<1);
	AdjustColSizes(table, tableSize);
	if (table.colSizes.size() == 2) {
		table.colSizes[0] = math::max(100.0f, math::min(250.0f, 0.25f*tableSize.x));
		table.colSizes[1] = tableSize.x - table.colSizes[0];
	}
	size.y = table.rows.size()*table.rowHeight+table.rowHeight;

}
template <>
void guiproperties_table<guiproperties_t>::render(NVGcontext* vg)  {
	renderDefault(vg);
}
template <>
void guiproperties_table<guiproperties_t>::validateReferences()  {
	guibase* ref = safeRefGet(ptr->safeRef);
	if (!ref) {
		setActiveControl(nullptr);
		table.rows.clear();
		table.titleCols.clear();
		table.colSizes.clear();
	}
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
	guibase* ref = safeRefGet(ptr->safeRef);
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
struct tbltype_theme_color {
	guitheme_t* theme;
	GuiColor::constant_t constant;
};
struct tbltype_theme_constant {
	guitheme_t* theme;
	GuiConstant::constant_t constant;
};

namespace Table {
void drawColor(NVGcontext* vg, ivec2 pos, ivec2 size, int32_t rgba) {
	nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BOTTOM);
	String strColorHex = StringFormat("%08X", rgba);
	int sizeQuad = size.y - INSET_TABLE_CELL_PADDING * 2;

	nvgText(vg, pos.x + size.x - INSET_TABLE_CELL_PADDING * 2 - sizeQuad, pos.y + size.y - INSET_TABLE_CELL_PADDING, StringAsCStr(strColorHex), nullptr);

	nvgBeginPath(vg);
	nvgRect(vg, pos.x + size.x - INSET_TABLE_CELL_PADDING - sizeQuad, pos.y + INSET_TABLE_CELL_PADDING,
		sizeQuad, sizeQuad);
	nvgFillColor(vg, rgbaToNvg(rgba));
	nvgFill(vg);
	nvgFillColor(vg, rgbaToNvg(-1));

}
template <typename T>
inline void drawTbl(const table_ctxt_t& ctxt, const tbltypesaferef<T>& obj) {
	if (safeRefOk(obj.saferef)) {
		drawTbl(ctxt, const_cast<const T&>(obj.t));
	}
}

template <>
void drawTbl(const table_ctxt_t& ctxt, const SafeRef<guibase>& obj) {
	const vec2& pos = ctxt.pos;
	const vec2& size = ctxt.size;
	nvgTextAlign(ctxt.vg, NVG_ALIGN_RIGHT|NVG_ALIGN_BOTTOM);
	guibase* ref = safeRefGet(obj);
	if (ref)
	{
		String strAddr = StringFormat("0x%6X (refId %d)", (int64_t)ref, obj.refId);
		String className = ref->getClassName();
		nvgText(ctxt.vg, pos.x+size.x-INSET_TABLE_CELL_PADDING, pos.y+size.y-INSET_TABLE_CELL_PADDING, StringAsCStr(StringFormat("%s [%s]", StringAsCStr(className), StringAsCStr(strAddr))), nullptr);

	} else {
		nvgText(ctxt.vg, pos.x+size.x-INSET_TABLE_CELL_PADDING, pos.y+size.y-INSET_TABLE_CELL_PADDING, StringAsCStr(StringFormat("<null> [%d]", obj.refId)), nullptr);

	}
}

template <>
void drawTbl(const table_ctxt_t& ctxt, const tbltype_gui_flags& obj) {
	const vec2& pos = ctxt.pos;
	const vec2& size = ctxt.size;
	guibase* ref = safeRefGet(obj.saferef);
	if (ref)
	{
		int flags = ref->getStateFlags();
		const char* strState = (flags&obj.mask) != 0 ? "1" : "0";
		nvgTextAlign(ctxt.vg, NVG_ALIGN_RIGHT|NVG_ALIGN_BOTTOM);
		nvgText(ctxt.vg, pos.x+size.x-INSET_TABLE_CELL_PADDING, pos.y+size.y-INSET_TABLE_CELL_PADDING, strState, nullptr);
	}
}
template <>
void drawTbl(const table_ctxt_t& ctxt, NVGcolor& obj) {
	drawColor(ctxt.vg, ctxt.pos, ctxt.size, nvgToRGBA(obj));
}
template <>
void drawTbl(const table_ctxt_t& ctxt, const tbltyperef<NVGcolor>& obj) {
	drawColor(ctxt.vg, ctxt.pos, ctxt.size, nvgToRGBA(obj.t));
}
template <>
void drawTbl(const table_ctxt_t& ctxt, const tbltype_theme_color& obj) {
	auto t = obj.theme->getColorInt32(obj.constant);
	drawColor(ctxt.vg, ctxt.pos, ctxt.size, t);
}
template <>
inline void cellClicked(const click_ctxt_t& ctxt, const tbltype_theme_color& obj) {
	ctxt.callback->onClick(ctxt, obj.theme, obj.constant);
}
template <>
void drawTbl(const table_ctxt_t& ctxt, const tbltype_theme_constant& obj) {
	int32_t t = obj.theme->get(obj.constant);
	drawTbl(ctxt, t);
}
template <>
inline void cellClicked(const click_ctxt_t& ctxt, const tbltype_theme_constant& obj) {
	ctxt.callback->onClick(ctxt, obj.theme, obj.constant);
}
}
template <>
void guiproperties_table<guitheme_t>::layout()  {
	//	size.x = 250;
		table.rowHeight = FONT_SIZE_TOOLTIP+INSET_TABLE_CELL_PADDING*2;
		table.rows.clear();
		table.titleCols.clear();
		table.colSizes.clear();
		table.rows.push_back({{tblstr{"this"}, tblint{(int64_t)ptr, "%08X"}}});
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
			auto add = [this](tblstr&& x, const auto& y) {
				table.rows.push_back({{x, y}});
			};
			std::vector<GuiColor::constant_t> vec = GuiColor::getAllConstants();
			for (auto _constant : vec) {
				add(tblstr{ _constant.name }, tbltype_theme_color{ ptr, _constant });
			}
			std::vector<GuiConstant::constant_t> vec2 = GuiConstant::getAllConstants();
			for (auto _constant2 : vec2) {
				add(tblstr{ _constant2.name }, tbltype_theme_constant{ ptr, _constant2 });
			}
		} else {
		}
		ivec2 tableSize = getSizeContent()-ivec2(INSET_TABLE<<1);
		AdjustColSizes(table, tableSize);
	if (table.colSizes.size() == 2) {
		table.colSizes[0] = math::max(220.0f, math::min(450.0f, 0.25f*tableSize.x));
		table.colSizes[1] = tableSize.x - table.colSizes[0];
	}

		size.y = table.rows.size()*table.rowHeight;
		ivec2 padTL = paddingTL(padding);
		ivec2 padBR = paddingBR(padding);
		size -= (padTL + padBR);
}
template <>
void guiproperties_table<guitheme_t>::render(NVGcontext* vg)  {
	renderDefault(vg);
}
template <>
void guiproperties_table<guitheme_t>::onTick(AppCtrl* appctrl) {

}
template <>
void guiproperties_table<guitheme_t>::setDebugPropertyHandle(void *vPtr) {
	ptr = static_cast<guitheme_t*>(vPtr);
	layout();
}



class guidropdown_selecttheme_ctxt : public guictxtmenu {
	guitheme_mgr* themeMgr;
	std::vector<String> strThemeNames;
public:
	guidropdown_selecttheme_ctxt(guitheme_mgr* _themeMgr) : themeMgr(_themeMgr) {
		this->size.x = 120;
		this->fontSize = FONT_SIZE_CTXT_SMALL;
		this->paddingV = 0;
		int32_t idx = 0;
		_themeMgr->getThemeNames(strThemeNames);
		for (auto str : strThemeNames) {
			my_printf("added %s\n", StringAsCStr(str));
			addEntry(new ctxtmenu_entry(str, idx));
			idx++;
		}
		my_printf("added %d themes \n", idx);
	}
	void clicked(int _id) {
		if (_id >= 0 && _id < strThemeNames.size()) {
			themeMgr->setThemeName(strThemeNames[_id]);
			parentCtrl->closePopup();
		}
	}
};
class guidropdown_selecttheme : public guidropdownbase {
public:
	guidropdown_selecttheme() :
		guidropdownbase() {
	}
	String getString() {
		guitheme_mgr* themeMgr = this->parentCtrl->getThemeMgr();
		return themeMgr->getThemeName();
	}
	virtual void handleDraggedRelease(MouseEvent& evt) {
		guitheme_mgr* themeMgr = this->parentCtrl->getThemeMgr();
		guictxtmenu_base *popup = new guidropdown_selecttheme_ctxt(themeMgr);
		popup->size = size;
		popup->setFontSize(size.y);
		this->parentCtrl->openContextMenu(popup, toScreenSpace(ivec2(0, size.y))-popup->pos+ivec2(1));
	}
};
class guictr_theme_settings : public guictr_base {

	guiproperties_table<guitheme_t> themeProperties;
	guictr_scrollbar scrollContainer;
	guidropdown_selecttheme selectTheme;
	guibutton buttonAdd;
	guibutton buttonRemove;
	guibutton buttonSave;
public:
	guictr_theme_settings() : guictr_base(), themeProperties(nullptr, false), scrollContainer(), selectTheme() {
		padding = 0;
		margin = 0;
		buttonAdd.setText("+");
		buttonRemove.setText("-");
		buttonSave.setText("save");
		add(&scrollContainer);
		add(&selectTheme);
		add(&buttonSave);
		add(&buttonRemove);
		add(&buttonAdd);
		scrollContainer.add(&themeProperties);
		scrollContainer.maxHeight = -1;
	}
	~guictr_theme_settings() {
		removeGuis();
	}
	void render(NVGcontext* vg) {
		if (!setScissorTransform(vg)) {
			return;
		}
		for (auto c : guis) {
			nvgSave(vg);
			c->render(vg);
			nvgRestore(vg);
		}
		//int colorIdx = 0;
		//auto renderDebugF = [](NVGcontext* vg, guibase* gui, NVGcolor color) {
		//	nvgBeginPath(vg);
		//	nvgRect(vg, gui->pos.x, gui->pos.y, gui->size.x, gui->size.y);
		//	nvgFillColor(vg, color);
		//	nvgFill(vg);
		//};
		//static NVGcolor dbgcolorsa[5] = {
		//	nvgRGBA(255, 0, 0, 55),
		//	nvgRGBA(0, 255, 0, 55),
		//	nvgRGBA(0, 0, 255, 55),
		//	nvgRGBA(255, 0, 255, 55),
		//	nvgRGBA(255, 255, 0, 55)
		//};

		//for (guibase* g : guis) {
		//	//renderDebugF(vg, g, dbgcolorsa[colorIdx++ % 5]);
		//}
	}
	virtual void buttonClicked(guibase* button) {
		if (button == &buttonAdd) {
			guitheme_mgr* thememgr = parentCtrl->getThemeMgr();
			thememgr->saveCurrentAsNewTheme("User");
			this->parentCtrl->relayout();
		}
		if (button == &buttonRemove) {
			guitheme_mgr* thememgr = parentCtrl->getThemeMgr();
			thememgr->removeTheme(thememgr->getRef());
			this->parentCtrl->relayout();
		}
		if (button == &buttonSave) {
			guitheme_mgr* thememgr = parentCtrl->getThemeMgr();
			thememgr->saveThemes();
		}
	}
	void layout() override {
		ivec2 size = getSizeContent();
		int32_t hTop = HEIGHT_DEFAULT_INPUT;
		buttonAdd.size = { hTop, hTop };
		buttonRemove.size = { hTop, hTop };
		buttonSave.size = { hTop*3, hTop };
		buttonSave.pos = { size.x - buttonSave.size.x, 0 };
		buttonRemove.pos = { buttonSave.left() - buttonRemove.size.x, 0 };
		buttonAdd.pos = { buttonRemove.left() - buttonAdd.size.x, 0 };
		selectTheme.pos = {0, 0};
		selectTheme.size = { buttonAdd.left(), hTop};
		scrollContainer.pos = {0, hTop};
		scrollContainer.size = {size.x, size.y-hTop};
		scrollContainer.determineSize(scrollContainer.size);

		for (auto c : guis) {
			c->layout();
		}
	}
	virtual void setControl(BaseCtrl* parentCtrl) {
		guictr_base::setControl(parentCtrl);
		guitheme_mgr* thememgr = parentCtrl->getThemeMgr();
		themeProperties.setDebugPropertyHandle(&thememgr->getRef());
	}
};
guictr_base* makeCtrTheme() {
	guictr_theme_settings* ctr = new guictr_theme_settings();
	return ctr;
}

std::vector<guiproperties_table<guiproperties_t>*> propTableInstances;
void setDebugPropertyHandle(void* ptr) {
	for (auto* instance : propTableInstances) {
		instance->setDebugPropertyHandle(ptr);
	}
}
debugproperties* makeUniquePropertiesCtr() {
	return new guiproperties_table<guiproperties_t>(new guiproperties_t(), false);
}
guictr_base* makeCtrProperties() {
	auto* ptr = new guiproperties_table<guiproperties_t>(new guiproperties_t(), true);
	propTableInstances.push_back(ptr);
	return ptr;
}

template <>
guiproperties_table<guiproperties_t>::~guiproperties_table() {
	removeGuis();
	if (isGlobalInstance) {
		bool b = removeEntry(propTableInstances, this);
		assert(b);
	}
}

#include "guiplugin.h"
#include <nanovg.h>
#include <memory>
#include "str_util.h"
#include "logging.h"
#include "event.h"
#include "keyboard.h"
#include "edithistory.h"
#include "renderresources.h"

#include "gui.h"
#include "button.h"
#include "knob.h"
#include "list.h"
#include "table.h"
#include "guicolors.h"
#include "guiconstant.h"
#include "theme.h"
#include "guitooltip.h"
#include "pluginviewcontainers.h"
#include "guicontainer.h"
#include "guicontextmenu.h"
#include "guicontextmenu_base.h"
#include "guicontextmenu_daw.h"
#include "pluginctr.h"
#include "pluginlist.h"
#include "trackcontent.h"

#include "basectrl.h"

#include "../host/mainctrl.h"
#include "../host/vst_host.h"
#include "../host/plugin/base_plugin.h"
#include "../host/plugin/internal_plugin.h"
#include "../host/plugin/vst_plugin.h"
#include "../host/plugin/vst_plugin_handles.h"
#include "../host/vst_window.h"
#include "automatable.h"
#include "debugproperties.h"



using Table::tbl;
using Table::tbl_row_t;
using Table::table_entry_t;
using Table::tblint;
using Table::tblfloat;
using Table::tblstr;
using Table::tblString;

void setDraggedPluginsUI(guictr_dragged_plugins& gui, plugin_selection& sel);

guiplugin::~guiplugin() {
	remove(&buttonLayout);
	remove(&buttonDelete);
	remove(&buttonBypass);
	remove(&buttonSave);
	remove(&meter);
	for (auto g : guiButtons) {
		dbgassert(!stl_contains(guis, g));
	}
}
void guiplugin::addGuiBtn(guibuttontoggle* btn)  {
	guiButtons.push_back(btn);
	add(btn);
}

void guiplugin::render(NVGcontext* vg) {
	renderBase(vg);
	for (guibase* gui : guis) {
		if (!gui->isVisible())
			continue;
		if (gui->size.x < 0) {
			log_printf("gui size x %d %s\n", gui->size.x, StringAsCStr(gui->label));
			continue;
		}
		gui->render(vg);
	}
}
void guiplugin::prerender(NVGcontext* vg) {
	guictr_base::prerender(vg);
	if (effect->getParam(PARAM_ENABLE)) {
		auto at = effect->getRegisteredAutomation(PARAM_ENABLE);
		if (at && at->isAutomated()) {
			buttonBypass.colorActive = GuiColor::COL_AUTOMATED;
		} else {
			buttonBypass.colorActive = GuiColor::COL_BTN_BG_BYPASS_ACTIVE;
		}
	}
}
void guiplugin::determineSize(ivec2& prefSize) {
	if (layoutMode == 1) {
		//		dbgassert(module->getAudioStage());
		//
			const int32_t hpt = theme->get(GuiConstant::CONST_PLUGIN_TITLE_HEIGHT);
		//		int32_t meterW = math::max(16, (int32_t)(theme->get(GuiConstant::CONST_METER_WIDTH)*hpt/32.0));
		//		prefSize.x = hpt+ctr.size.x+meterW;
			prefSize.x = hpt;
	} else {

		prefSize.x = prefSize.y;
	}
}
void guiplugin::buttonClicked(guibase* _button) {
	if (_button == &buttonLayout) {
		layoutMode = (layoutMode+1)%2;
		isHorizontalTitle = layoutMode == 0;
		buttonLayout.icon = layoutMode == 0 ? ICON_ARR_RIGHT : ICON_ARR_DOWN;
		parent->onChildLayoutChanged(this);
		return;
	}
	if (_button == &buttonBypass) {
    	ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
    	float f = effect->getParamValue(PARAM_ENABLE);
    	float f2 = f < 0.5 ? 1 : 0;
		effect->deactivateAutomation(PARAM_ENABLE);
    	effect->setParamValue(PARAM_ENABLE, f2, FLG_PAR_UPDATE_USER);
    	effect->postSetParameter(PARAM_ENABLE, f, f2, FLG_PAR_UPDATE_USER);

	}
	if (_button == &buttonDelete) {
    	removePlugin(effect);
	}
}
bool guiplugin::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
	if (contains(mpos)) {
		if (evt.type == MouseHitType::MOUSE_DRAGDROP_OBJECT) {
			return false;
		}
		ivec2 localMouse = this->toContainerSpace(mpos);
		for (guibase* gui : guis) {
			if (!gui->isVisible())
				continue;
			if (gui->mouseHitTest(localMouse, evt)) {
				return true;
			}
		}
		if (isShift(evt.kbmods)) {
			if (MainCtrl::get()->getPluginSel().pluginCtr != this->parent) {
				return true;
			}
		}
		evt.requestFocus(this);
		return true;
	}
	return false;
}
guiplugin::guiplugin(effectbase* _effect)
: guictr_base(),
  effect(_effect),
  meter(&_effect->meter) {
	padding = 0;
	margin = 0;
	text[0] = 0;
	buttonBypass.setLabel("Bypass");
	buttonBypass.colorActive = GuiColor::COL_BTN_BG_BYPASS_ACTIVE;
	buttonBypass.icon = ICON_BYPASS;
	buttonBypass.getState = [_effect]() {
		return _effect->getParamValue(PARAM_ENABLE)>0;
	};
//	buttonBypass.setTint(0x80c040);
	buttonDelete.setLabel("Remove");
	buttonDelete.icon = ICON_CLOSE;
	static bool closeEnabled = true;
	buttonDelete.state = &closeEnabled;
	buttonLayout.icon = ICON_ARR_RIGHT;
	buttonLayout.setLabel("Hide");
	buttonSave.icon = ICON_SAVE;
	buttonSave.setLabel("Save");
	add(&meter);
	addGuiBtn(&buttonBypass);
	addGuiBtn(&buttonLayout);
	addGuiBtn(&buttonDelete);
	addGuiBtn(&buttonSave);
//	buttonDelete.setTint(0x404040);
}
void guiplugin::rightClicked(MouseEvent& evt, guibase* button) {
	int32_t clickedParamIdx = -1;
	if (button == &this->buttonBypass) {
		clickedParamIdx = PARAM_ENABLE;
	}
	if (clickedParamIdx != -1) {
		auto* ctxt = new guictxtmenu_at_param(effect, clickedParamIdx);
		MainCtrl::get()->openContextMenu(ctxt, evt.mousepos);
	}

}
void guiplugin::layout() {
	const int32_t hpt = theme->get(GuiConstant::CONST_PLUGIN_TITLE_HEIGHT);
	int buttonSize = hpt * 0.8;
	int32_t inset1 = (hpt - buttonSize) / 2;
	ivec2 btnPos = {inset1, inset1};
	buttonLayout.pos = btnPos;
	btnPos[isHorizontalTitle?0:1] += buttonSize;
	for (auto btn : guiButtons) {
		btn->size = {buttonSize, buttonSize};
		btn->setRadius(hpt/3.f);
		if (btn == &buttonLayout) {
			continue;
		}
		if (btn == &buttonDelete) {
			continue;
		}
		btn->pos = btnPos;
		btnPos[isHorizontalTitle?0:1] += buttonSize;
	}
	if (isHorizontalTitle) {
		buttonDelete.pos = {size.x - buttonDelete.size.x - inset1, inset1};
	} else {
		buttonDelete.pos = {inset1, size.y - buttonDelete.size.y - inset1};
	}


	int32_t meterW = math::max(16, (int32_t)(theme->get(GuiConstant::CONST_METER_WIDTH)*hpt/32.0));
	ivec2 contentS;
	ivec2 contentP;
	if (isHorizontalTitle) {
		contentP = ivec2(0, hpt);
		contentS = ivec2(size.x - meterW, 		size.y - hpt);
		titlePosX = btnPos.x;
	} else {
		contentP = ivec2(hpt, 0);
		contentS = ivec2(size.x - hpt - meterW, size.y );
		titlePosX = buttonDelete.top();
	}
	meter.pos = ivec2(size.x - meterW, hpt);
	meter.size = ivec2(meterW, size.y - hpt);
	layoutModule(contentP, contentS, inset1);
	for (auto btn : guis) {
		btn->layout();
	}
}
void guiplugin::renderBase(NVGcontext* vg) {
	if (!setScissorTransformContainer(vg)) {
		return;
	}
	renderFrameBase(vg);
	int flags = parentCtrl->isCtrOrChildFocused(this) ? FLAG_FOCUSED : 0;
	if (isSelected()) {
		flags |= FLAG_SELECTED;
	}
	renderTitleBar(vg, size, this->text, GuiConstant::CONST_PLUGIN_TITLE_HEIGHT, titlePosX, flags, isHorizontalTitle);
	renderFrameOutline(vg);
}

void guiplugin::handleDraggedMove(MouseEvent& evt) {
	hasDragged = false;
	if (isSelected()) {
		auto& sel = MainCtrl::get()->getPluginSel();
		if (sel.hasSelection()) {
			setDraggedPluginsUI(sel.pluginCtr->dragged, sel);
			MainCtrl::get()->setDragged(&sel.pluginCtr->dragged);
			hasDragged = true;
		}
	} else {
		MainCtrl::get()->objectDragMove(this, evt);
	}
}
void guiplugin::handleDraggedRelease(MouseEvent& evt) {
	MainCtrl::get()->objectDragRelease(this, evt);
	if (hasDragged) {
		assert(0);
		return;
	}
	if (isSelected()) {
		static_cast<guictr_plugins*>(this->parent)->onSelected(evt, this);
	}
}
void guiplugin::handleDraggedBegin(MouseEvent& evt) {
	hasDragged = false;
	if (!isSelected()) {
//		hasDragged = true;
		static_cast<guictr_plugins*>(this->parent)->onSelected(evt, this);
	}
}

//enum action_plugin_ctr {
//	SELECTALL, DELETE, CUT, COPY, PASTE, DUPLICATE
//};
//bool handlePluginCtrCommand(action_plugin_ctr action);
debugproperties* makeUniquePropertiesCtr();
class guictxtmenu_plugin : public guictxtmenu {
public:
	static constexpr int CMD_SHOW_AUTOMATION = 1;
	static constexpr int CMD_SHOW_PARAM_LIST = 2;
	static constexpr int CMD_DUPLICATE = 3;
	static constexpr int CMD_DELETE = 4;
	static constexpr int CMD_COPY = 5;
	static constexpr int CMD_CUT = 6;
	static constexpr int CMD_PASTE = 7;
	effectbase* const effect;
	guictxtmenu_plugin(effectbase* _effect) : effect(_effect) {
		this->size.x = 260;
		addEntry(new ctxtmenu_entry("Show all automation", CMD_SHOW_AUTOMATION));
		addEntry(new ctxtmenu_entry("Show parameter list", CMD_SHOW_PARAM_LIST));
		addEntry(new ctxtmenu_splitter());
		addEntry(new ctxtmenu_entry("Copy", CMD_COPY));
		addEntry(new ctxtmenu_entry("Cut", CMD_CUT));
		addEntry(new ctxtmenu_entry("Paste", CMD_PASTE));
		addEntry(new ctxtmenu_entry("Duplicate", CMD_DUPLICATE));
		addEntry(new ctxtmenu_entry("Delete", CMD_DELETE));
	}
	void clicked(int _id) {
		ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
		if (_id == CMD_SHOW_PARAM_LIST) {
//			class guitable_params : public guictr_base {
//				gui_ctr_
//				guitable_params() {
//
//				}
//
//				void renderBackground(NVGcontext* vg) {
//					dbgassert(isBackgroundRendered());
//					bool focused = parentCtrl->isCtrOrChildFocused(this);
//					drawBackground(vg, theme, getPosContent(), getSizeContent(), margin, focused, isBackgroundRenderedInset());
//				}
//				virtual void render(NVGcontext* vg)
//				{
//					guictr_base::render(vg);
//				}
//			};
//			setDebugPropertyHandle(this);
			auto* gui = effect->getGui();
			if (gui) {
				debugproperties* dbgPropertiesCtrPopup = makeUniquePropertiesCtr();
				guictxtmenu_base* ctxtMenu = new guictxtmenu_base();
				ctxtMenu->size = {240, 480};
				ctxtMenu->add(static_cast<guibase*>(dbgPropertiesCtrPopup));
				dbgPropertiesCtrPopup->setDebugPropertyHandle(nullptr);
				ivec2 wndPos{0};
				this->parentCtrl->window->getPos(&wndPos);
				closeContextMenu();
				MainCtrl::get()->openContextMenu(ctxtMenu, wndPos, 2);
				dbgPropertiesCtrPopup->setDebugPropertyHandle(gui);
				assert(ctxtMenu->parentCtrl);
				ctxtMenu->parentCtrl->window->getPos(&wndPos);
				dbgPropertiesCtrPopup->setDebugPropertyHandle(gui);
				return;
			}
		}
		if (_id == CMD_DELETE) {
			my_printf("CMD_DELETE %s\n", StringAsCStr(effect->sName));
			handlePluginCtrCommand(action_plugin_ctr::PLUGINS_DELETE);
		}
		if (_id == CMD_COPY) {
			my_printf("CMD_COPY %s\n", StringAsCStr(effect->sName));
			handlePluginCtrCommand(action_plugin_ctr::PLUGINS_COPY);
		}
		if (_id == CMD_CUT) {
			my_printf("CMD_CUT %s\n", StringAsCStr(effect->sName));
			handlePluginCtrCommand(action_plugin_ctr::PLUGINS_CUT);
		}
		if (_id == CMD_PASTE) {
			my_printf("CMD_PASTE %s\n", StringAsCStr(effect->sName));
			handlePluginCtrCommand(action_plugin_ctr::PLUGINS_PASTE);
		}
		if (_id == CMD_PASTE) {
			my_printf("CMD_COPY %s\n", StringAsCStr(effect->sName));
			handlePluginCtrCommand(action_plugin_ctr::PLUGINS_COPY);
		}
		if (_id == CMD_DUPLICATE) {
			my_printf("CMD_DUPLICATE %s\n", StringAsCStr(effect->sName));
			handlePluginCtrCommand(action_plugin_ctr::PLUGINS_DUPLICATE);
		}
		if (_id == CMD_SHOW_AUTOMATION) {
			auto tr = effect->getTrack();
			gui_track_automationlane* gtr_at = NULL;
			if (tr) {
				tr->hideTrack = false;
				tr->hideSubtracks = false;
				tr->audio->updateStoreLoadSubtracks();
				auto trCtr = MainCtrl::getGuiTrackCtr();

				std::vector<int32_t> automated;
				effect->getAutomated(automated);
				for (int32_t param : automated) {
					auto lane = trCtr->addAutomationLane(tr, effect, param, true);
					if (!gtr_at) {
						gtr_at= lane;
					}
				}
			}
			if (gtr_at) {
				MainCtrl::getGuiTrackCtr()->layout();
				MainCtrl::getGuiTrackCtr()->updateVisibleTrackContents();
				MainCtrl::getGuiTrackCtr()->scrollTo(gtr_at);
			}
		}
		closeContextMenu();
	}
};
void guiplugin::handleRightClick(MouseEvent& evt) {
	handleDraggedBegin(evt);
	const int32_t hpt = theme->get(GuiConstant::CONST_PLUGIN_TITLE_HEIGHT);
	bool b = false;
	if (isHorizontalTitle) {
		b = evt.relMousepos.y < hpt;
	} else {
		b = evt.relMousepos.x < hpt;
	}
	if (b) {
		MainCtrl::get()->openContextMenu(new guictxtmenu_plugin(effect), evt.mousepos);
	}

}
void guiplugin::dragMoveOn(guibase* target, ivec2 mousepos) {
	target->pluginDragMove(this, mousepos);
}
void guiplugin::dragReleaseOn(guibase* target, ivec2 mousepos) {
	target->pluginDragRelease(this, mousepos);
}

bool guiplugin::focusEvent(MouseHitEvt& evt, bool focused) {
	return true;
}
void guiplugin::setControl(BaseCtrl* parentCtrl) {
	guictr_base::setControl(parentCtrl);
}
guibase* guiplugin::getDraggedControl() {
	return this;
}
bool guiplugin::isSelected() {
	dbgassert(this->parentCtrl);
	if (!this->parentCtrl->guiCtrFocused) {
		return false;
	}
	auto& sel = MainCtrl::get()->getPluginSel();
	if (!sel.hasSelection())
		return false;
	if (sel.pluginCtr == this->parent) {
		if (this->effect->getSlot() >= sel.firstSelection &&
				this->effect->getSlot() <= sel.lastSelection) {
			return isChildOf(this->parentCtrl->guiCtrFocused);
		}
		return false;
	}
	if (this->parent) {
		return this->parent->isSelected();
	}
	return false;
}

template <>
void guitooltip<guiplugin>::layout()  {
	size.x = 250;
	table.rowHeight = FONT_SIZE_TOOLTIP+INSET_TABLE_CELL_PADDING*2;
	table.rows.clear();
	table.titleCols.clear();
	table.colSizes.clear();
	{
		table.rows.push_back({{tblstr{"track"}, tblint{(int64_t)ptr->effect->getTrack(), "%12x"}}});
		table.rows.push_back({{tblstr{"tracklink"}, tblint{(int64_t)ptr->effect->getTrackLink(), "%12x"}}});
		table.rows.push_back({{tblstr{"bIsSetup"}, tblint{ptr->effect->bIsSetup}}});
		table.rows.push_back({{tblstr{"bIsEnabled"}, tblint{ptr->effect->bIsEnabled}}});
		table.rows.push_back({{tblstr{"PARAM_ENABLE"}, tblfloat{ptr->effect->getParamValue(PARAM_ENABLE)}}});
	}
	Table::AdjustColSizes(table, getSizeContent()-ivec2(INSET_TABLE<<1));
	size.y = table.rows.size()*table.rowHeight;
}

guictxtmenu_base* guiplugin::getTooltip(AppCtrl* appctrl) {
	auto tooltip = new guitooltip<guiplugin>(this);
	return tooltip;
}


class gui_plugin_paramlist_entry : public gui_list_entry {

	const float spacing = INSET_TITLE;
public:
	effectbase* const effect;
	automatable_param_t* const entry;
	guiknob knobTest;
	gui_plugin_paramlist_entry(effectbase* _effect, automatable_param_t* _entry)
		: gui_list_entry(),
		  effect(_effect),
		  entry(_entry),
		  knobTest(false)
	{
		icon = 0;
		knobTest.setAutomationRef(effect, entry->idx);
		knobTest.setAutomationHandlers();
		knobTest.setParent(this);
	}
	void handleRightClick(MouseEvent& evt) override {
		MainCtrl::get()->openContextMenu(new guictxtmenu_at_param(effect, entry->idx), evt.mousepos);
	}
	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
		if (this->contains(mpos)) {
			if (evt.type != MouseHitType::MOUSE_RIGHT)
			{
				if (knobTest.mouseHitTest(mpos, evt)) {
					return true;
				}
			}
			evt.requestFocus(this);
			return true;
		}
		return false;
	}
	void dragMoveOn(guibase* target, ivec2 mousepos) override {
	}
	void dragReleaseOn(guibase* target, ivec2 mousepos) override {
	}
	virtual void setControl(BaseCtrl* parentCtrl) {
		guibase::setControl(parentCtrl);
		knobTest.setControl(parentCtrl);
	}
	virtual void setParent(guibase* parent) override {
		guibase::setParent(parent);
		dbgassert(knobTest.parent == this);
	}
	String getText() override {
		return entry->label;
	}
	void layout() {
		knobTest.pos = pos + ivec2(spacing);
		knobTest.size = ivec2(size.y, size.y) - ivec2(spacing*2);
	}
	virtual void render(NVGcontext* vg) {
		MainCtrl* ctrl = MainCtrl::get();
		float rowHeight = size.y;
		float x = knobTest.right()+spacing;
		if (ctrl->isCtrOrChildFocused(this)) {
			nvgBeginPath(vg);
			nvgRect(vg, pos.x, pos.y, size.x, size.y);
			nvgFillColor(vg, theme->getColor(GuiColor::COL_BG_DRKER));
			nvgFill(vg);
		}
		nvgTranslate(vg, pos.x, pos.y);
		if (rowHeight > 32) {
			setFont(vg, (int) (rowHeight*0.4), G_WHITE, G_TITLE_ALIGN);
			nvgText(vg, x, rowHeight*0.25, StringAsCStr(getText()), NULL);
			String sValue = effect->getParamValueDisplay(entry->idx);
			nvgText(vg, x, rowHeight*0.5 + rowHeight*0.25, StringAsCStr(sValue), NULL);
		} else {
			setFont(vg, (int) (rowHeight * 0.8), G_WHITE, G_TITLE_ALIGN);
			nvgText(vg, x, rowHeight / 2, StringAsCStr(getText()), NULL);
		}
		nvgTranslate(vg, -pos.x, -pos.y);

		knobTest.render(vg);
	}
};

void drawImage(NVGcontext* vg, int image, float alpha,
		float sx, float sy, float sw, float sh, // sprite location on texture
		float x, float y, float w, float h); // position and size of the sprite rectangle on screen
class guivstplugin_preview : public guictr_base {
	vstplugin* const plugin;
	guivstplugin* const guivst;
	int tex = -1;
	ivec2 sizeTex;
public:
	guivstplugin_preview(vstplugin* _plugin, guivstplugin* _guivst)
	: guictr_base(), plugin(_plugin), guivst(_guivst), sizeTex{0,0}
	{
		padding = 0;
		margin = 0;
	}
	~guivstplugin_preview() {

	}
	void determineSize(ivec2& prefSize) override {
//		if (sizeTex.x && sizeTex.y) {
//			prefSize = sizeTex;
//		}
	}
	int nFrame = 0;
	void prerender(NVGcontext* vg) override {
		//TODO: resource management
//		if (nFrame++<20)
//			return;
//		nFrame = 0;
		auto window = plugin->window;
		if (window) {
//			window->captureWindowFrame();
			auto& frame = window->capturedFrame;
			if (frame.w && frame.h) {
				if (tex > 0 && (frame.w != sizeTex.x || frame.h != sizeTex.y)) {
					nvgDeleteImage(vg, tex);
					tex = -1;
				}
				if (tex < 0) {
					tex = nvgCreateImageRGBA(vg, frame.w, frame.h, 0, (const unsigned char*)nullptr);
					sizeTex = { frame.w, frame.h };
				}
//				std::vector<uint8_t> tmpData = frame.bytes;
////				tmpData.resize(frame.w*frame.h*4);
//				for (int _x = 0; _x < frame.w; _x++) {
//
//					for (int _y = 0; _y < frame.h; _y++) {
//						int idx = _x*frame.h+_y;
//						tmpData[idx*4+0] = 0xff;
////						tmpData[idx*4+1] = 0xff;
////						tmpData[idx*4+2] = 0xff;
//						tmpData[idx*4+3] = 0xff;
//					}
//				}
				nvgUpdateImage(vg, tex, frame.bytes.data());
			} else if (tex > 0) {
				nvgDeleteImage(vg, tex);
				tex = -1;
				sizeTex = { frame.w, frame.h };
			}

		}
	}
	void render(NVGcontext* vg) override {
		if (isBackgroundRendered()) {
			renderBackground(vg);
		}
		if (!setScissorTransform(vg)) {
			return;
		}
		if (tex > 0) {
			ivec2 sizePrev;
			if (sizeTex.x > sizeTex.y) {
				sizePrev.x = size.x;
				sizePrev.y = (int)((sizeTex.y/(float)sizeTex.x)*sizePrev.x);
			} else {
				sizePrev.y = size.y;
				sizePrev.x = (int)((sizeTex.x/(float)sizeTex.y)*sizePrev.y);
			}
			drawImage(vg, tex, 1.0f, 0, 0, sizeTex.x, sizeTex.y, 0, 0, sizePrev.x, sizePrev.y);
		}
		for (auto c : guis) {
			nvgSave(vg);
			c->render(vg);
			nvgRestore(vg);
		}
	}
};
guivstplugin::guivstplugin(vstplugin * _vst)
  : guiplugin(_vst), vst(_vst), dropdownProgram(_vst)
{
	params.setRowHeight(48);
	buttonOpenEditor.icon = ICON_ADJUST;
	buttonOpenEditor.state = &_vst->bEditOpen;
	buttonOpenEditor.setParent(this);
	buttonOpenEditor.colorActive = GuiColor::COL_BTN_BG_SHOW_ACTIVE;
	addGuiBtn(&buttonOpenEditor);
	params.setParent(this);
	dropdownProgram.setParent(this);
	std::vector<automatable_param_t*> sortedParams;
	_vst->getSortedParams(sortedParams);
	std::vector<gui_list_entry*> listEntries;
	listEntries.reserve(sortedParams.size());
    std::for_each(sortedParams.begin(), sortedParams.end(), [&listEntries, _vst](auto* param) {
		if (param->internalIdx >= 0)
			listEntries.push_back(new gui_plugin_paramlist_entry(_vst, param));
    });
	params.setList(listEntries);
//	ctrPreview = new guivstplugin_preview(this->vst, this);
//	viewCtrs.push_back(ctrPreview);
}

guivstplugin::~guivstplugin() {
	remove(&buttonOpenEditor);
	if (ctrPreview) {
		delete ctrPreview;
	}
}
void guivstplugin::setControl(BaseCtrl* parentCtrl) {
	guiplugin::setControl(parentCtrl);
	params.setControl(parentCtrl);
	dropdownProgram.setControl(parentCtrl);
	for (auto* ctr : viewCtrs) {
		ctr->setControl(parentCtrl);
	}
}

void guivstplugin::prerender(NVGcontext* vg) {
	guiplugin::prerender(vg);
	for (auto* ctr : viewCtrs) {
		assert(!ctr->parent);
		ctr->prerender(vg);
	}
}
void guivstplugin::determineSize(glm::ivec2& prefSize) {
	if (layoutMode == 1) {
		guiplugin::determineSize(prefSize);
		return;
	}
	sizeCtrs = {0, size.y};
	if (viewCtr) {
		ivec2 sizeCtr;
		viewCtr->getFixedSize(&sizeCtr.x, &sizeCtr.y);
		sizeCtr.x = (int)((sizeCtr.x/(float)sizeCtr.y)*size.y);
		sizeCtr.y = size.y;
		viewCtr->layout(sizeCtr.x, sizeCtr.y);
		sizeCtrs.x += sizeCtr.x;
	}
	if (ctrPreview) {
		ivec2 sizeCtr{prefSize.y, prefSize.y};
		ctrPreview->determineSize(sizeCtr);
		sizeCtr.x = (int)((sizeCtr.x/(float)sizeCtr.y)*size.y);
		sizeCtr.y = size.y;
		ctrPreview->size = sizeCtr;
		sizeCtrs.x += sizeCtr.x;
	}
	prefSize.y = math::max(sizeCtrs.y, prefSize.y);
	prefSize.x += sizeCtrs.x;
}
void guivstplugin::render(NVGcontext* vg) {
	renderBase(vg);
	for (auto* btn : guiButtons) {
		if (btn->isVisible())
			btn->render(vg);
	}
	if (layoutMode != 1) {
		for (auto* ctr : viewCtrs) {
			nvgSave(vg);
			if (ctr->isVisible())
				ctr->render(vg);
			nvgRestore(vg);
		}
		if (meter.isVisible())
			meter.render(vg);
		if (dropdownProgram.isVisible()) {
			dropdownProgram.render(vg);
		}
		if (params.isVisible()) {
			if (params.isBackgroundRendered()){
				params.renderBackground(vg);
			}
			params.render(vg);
		}
	}
}
bool guivstplugin::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
	if (evt.type == MouseHitType::MOUSE_DRAGDROP_OBJECT) {
		return false;
	}
	if (contains(mpos)) {
		ivec2 localMouse = this->toContainerSpace(mpos);
		for (auto* btn : guiButtons) {
			if (btn->isVisible() && btn->mouseHitTest(localMouse, evt)) {
				return true;
			}
		}
		for (auto* ctr : viewCtrs) {
			if (ctr->mouseHitTest(localMouse, evt)) {
				return true;
			}
		}
		if (params.isVisible() && params.mouseHitTest(localMouse, evt)) {
			return true;
		}
		if (dropdownProgram.isVisible() && dropdownProgram.mouseHitTest(localMouse, evt)) {
			return true;
		}
		if (isShift(evt.kbmods)) {
			if (MainCtrl::get()->getPluginSel().pluginCtr != this->parent) {
				return true;
			}
		}
		evt.requestFocus(this);
		return true;
	}
	return false;
}
void guivstplugin::buttonClicked(guibase* _button) {
	guiplugin::buttonClicked(_button);
	if (_button == &buttonOpenEditor) {
		if (vst->bEditOpen) {
			vst->close();
		} else {
			vst->show();
		}
	}
	dropdownProgram.setVisible(layoutMode == 0 && vst->programNames.size());
	params.setVisible(layoutMode == 0);
	meter.setVisible(layoutMode == 0);
}
void guivstplugin::layoutModule(ivec2 pos, ivec2 contentS, int32_t inset1) {
	const int32_t hpt = theme->get(GuiConstant::CONST_PLUGIN_TITLE_HEIGHT);
//	buttonOpenEditor.size = buttonBypass.size;
//	buttonOpenEditor.setRadius(buttonBypass.radius);
//	buttonOpenEditor.pos.y = inset1;
//	buttonOpenEditor.pos.x = buttonBypass.right();
//	titlePosX = buttonOpenEditor.right();
	int32_t insetCtrls = INSET_TITLE;
	int rowHeight = 64;
	while (contentS.y < rowHeight * 8 && rowHeight > 8) {
		rowHeight -= 4;
	}
	dropdownProgram.setVisible(layoutMode == 0 && vst->programNames.size());
	if (dropdownProgram.isVisible()) {

		int hDropDown = hpt*0.7;
		int paramsW = contentS.x - sizeCtrs.x;
		params.setRowHeight(rowHeight);
		params.pos = ivec2(insetCtrls, insetCtrls + hpt+hDropDown);
		params.size = ivec2(paramsW, contentS.y-hDropDown) - ivec2(insetCtrls*2);
		params.layout();
		dropdownProgram.pos = ivec2(insetCtrls*2, insetCtrls+hpt);
		dropdownProgram.size = ivec2(paramsW, hDropDown) - ivec2(insetCtrls*4, 0);
		dropdownProgram.layout();
	} else {

		int paramsW = contentS.x - sizeCtrs.x;
		params.setRowHeight(rowHeight);
		params.pos = ivec2(insetCtrls, insetCtrls + hpt);
		params.size = ivec2(paramsW, contentS.y) - ivec2(insetCtrls*2);
		params.layout();
	}
	if (viewCtrs.size()) {
		int left = params.right() + INSET_TITLE;
		for (auto* ctr : viewCtrs) {
			ctr->pos = ivec2(left, 0) + ivec2(insetCtrls, insetCtrls + hpt);
			ivec2 prefSizeCtr = ivec2(ctr->size.x, contentS.y) - ivec2(insetCtrls*2);
			ctr->determineSize(prefSizeCtr);
			ctr->size = prefSizeCtr;
			ctr->layout();
			left = ctr->right() + INSET_TITLE;
		}
	}
}


guidropdown_select_program::guidropdown_select_program(vstplugin *_plugin) :
		plugin(_plugin) {
	this->size.x = 120;
	this->fontSize = FONT_SIZE_CTXT_SMALL;
	this->paddingV = 0;
	int32_t idx = 0;
	for (auto str : plugin->programNames) {
		addEntry(new ctxtmenu_entry(str, idx));
		idx++;
	}
}

void guidropdown_select_program::clicked(int _id) {
	closeContextMenu();
	if (_id >= 0 && _id < plugin->programNames.size()) {
		String s = plugin->programNames[_id];
		plugin->dispatch(effSetProgram, 0, _id, 0, 0);
	}
}

String guidropdownprogram::getString() {
	char buf[1024];
	memset(buf, 0, sizeof(buf));
	if (plugin->dispatch(effGetProgramName, 0, 0, buf, 0) && buf[0]) {
		return String(buf);
	}
	int n = plugin->dispatch(effGetProgram, 0, 0, 0, 0);
	if (n >= 0 && n < plugin->programNames.size()) {
		return plugin->programNames[n];
	}
	return "";
}

void guidropdownprogram::handleDraggedRelease(MouseEvent &evt) {
	if (plugin) {
		guictxtmenu_base *popup = new guidropdown_select_program(plugin);
		popup->size = size;
		popup->setFontSize(size.y);
		this->parentCtrl->openContextMenu(popup, toScreenSpace(ivec2(0, size.y)) - popup->pos + ivec2(1));
	}
}

template <>
void guitooltip<guivstplugin>::layout()  {
	size.x = 250;
	table.rowHeight = FONT_SIZE_TOOLTIP+INSET_TABLE_CELL_PADDING*2;
	table.rows.clear();
	table.titleCols.clear();
	table.colSizes.clear();
	{
		table.rows.push_back({{String("isSynth"), (int)ptr->vst->isSynth}});
		auto vst = ptr->vst;
		auto aeffect = vst->handle->aeffect;
		table.rows.push_back({{tblstr{"numInputs"}, tblint{aeffect->numInputs}}});
		table.rows.push_back({{tblstr{"numOutputs"}, tblint{aeffect->numOutputs}}});
		table.rows.push_back({{tblstr{"numParams"}, tblint{aeffect->numParams}}});
		table.rows.push_back({{tblstr{"numPrograms"}, tblint{aeffect->numPrograms}}});
		table.rows.push_back({{tblstr{"uniqueID"}, tblint{aeffect->uniqueID, "%8X"}}});
		table.rows.push_back({{tblstr{"version"}, tblint{aeffect->version}}});
		table.rows.push_back({{tblstr{"bIsEnabled"}, tblint{ptr->vst->bIsEnabled}}});
		table.rows.push_back({{tblstr{"PARAM_ENABLE"}, tblfloat{ptr->vst->getParamValue(PARAM_ENABLE)}}});
		table.rows.push_back({{tblstr{"flags"}, tblint{aeffect->flags}}});
		table.rows.push_back({{tblstr{"initialDelay"}, tblint{aeffect->initialDelay}}});
		table.rows.push_back({{tblstr{"magic"}, tblint{aeffect->magic}}});
		table.rows.push_back({{tblstr{"offQualities"}, tblint{aeffect->offQualities}}});
		table.rows.push_back({{tblstr{"realQualities"}, tblint{aeffect->realQualities}}});
		int n = 0;
		for (auto& in : vst->inputNames) {
			table.rows.push_back({{tblString{StringFormat("input[%d]", n)}, tblstr{StringAsCStr(in)}}});
			n++;
		}
		n = 0;
		for (auto& out : vst->outputNames) {
			table.rows.push_back({{tblString{StringFormat("output[%d]", n)}, tblstr{StringAsCStr(out)}}});
			n++;
		}

	}
	Table::AdjustColSizes(table, getSizeContent()-ivec2(INSET_TABLE<<1));
	size.y = table.rows.size()*table.rowHeight;
}

guictxtmenu_base* guivstplugin::getTooltip(AppCtrl* appctrl) {
	auto tooltip = new guitooltip<guivstplugin>(this);
	return tooltip;
}

template<typename T>
void addPropertiesFromGui(T& gui, Table::tbl* table);
template<>
void addPropertiesFromGui(guiplugin& gui, Table::tbl* table);
void guiplugin::addProperties(Table::tbl* table) {
	addPropertiesFromGui(*this, table);
}

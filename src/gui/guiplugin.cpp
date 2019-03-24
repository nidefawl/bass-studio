#include "guiplugin.h"
#include <nanovg.h>
#include <glm/glm.hpp>
#include <glm/vec2.hpp>
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
#include "guicontextmenu_daw.h"
#include "pluginctr.h"
#include "pluginlist.h"

#include "basectrl.h"

#include "../host/mainctrl.h"
#include "../host/vst_host.h"
#include "../host/plugin/base_plugin.h"
#include "../host/plugin/internal_plugin.h"
#include "../host/plugin/vst_plugin.h"
#include "automatable.h"

#include "leak_detect.h"



using glm::vec2;
using glm::ivec2;
using Table::tbl;
using Table::tbl_row_t;
using Table::table_entry_t;
using Table::tblint;
using Table::tblfloat;
using Table::tblstr;

void setDraggedPluginsUI(guictr_dragged_plugins& gui, plugin_selection& sel);

guiplugin::guiplugin(effectbase* _effect)
: guictr_base(),
  effect(_effect),
  meter(&_effect->meter) {
	padding = 0;
	margin = 0;
	text[0] = 0;
	buttonBypass.colorActive = GuiColor::COL_BTN_BG_BYPASS_ACTIVE;
	buttonBypass.icon = ICON_BYPASS;
	buttonBypass.getState = [_effect]() {
		return _effect->getParamValue(PARAM_ENABLE)>0;
	};
	buttonBypass.setParent(this);
//	buttonBypass.setTint(0x80c040);
	buttonDelete.icon = ICON_CLOSE;
	static bool closeEnabled = true;
	buttonDelete.state = &closeEnabled;
	buttonDelete.setParent(this);
//	buttonDelete.setTint(0x404040);
}
void guiplugin::layout() {
	const int32_t hpt = theme->get(GuiConstant::CONST_PLUGIN_TITLE_HEIGHT);
	int buttonSize = hpt * 0.8;
	int32_t inset1 = (hpt - buttonSize) / 2;
	buttonBypass.size = {buttonSize, buttonSize};
	buttonDelete.size = {buttonSize, buttonSize};
	buttonBypass.pos = {inset1, inset1};
	buttonDelete.pos = {size.x - buttonDelete.size.x - inset1, inset1};
	buttonBypass.setRadius(hpt/3.f);
	buttonDelete.setRadius(hpt/3.f);



	int32_t meterW = std::max(16, (int32_t)(theme->get(GuiConstant::CONST_METER_WIDTH)*hpt/32.0));
	ivec2 contentS;
	ivec2 contentP;
	if (isHorizontalTitle) {
		contentP = ivec2(0, hpt);
		contentS = ivec2(size.x - meterW, 		size.y - hpt);
		titlePosX = buttonBypass.right();
	} else {
		contentP = ivec2(hpt, 0);
		contentS = ivec2(size.x - hpt - meterW, size.y );
		titlePosX = 0;
	}
	meter.pos = ivec2(size.x - meterW, hpt);
	meter.size = ivec2(meterW, size.y - hpt);
	layoutModule(contentP, contentS, inset1);
	meter.layout();
	buttonDelete.layout();
	buttonBypass.layout();
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
	renderTitleBar(vg, this->text, GuiConstant::CONST_PLUGIN_TITLE_HEIGHT, titlePosX, flags, isHorizontalTitle);
	renderFrameOutline(vg);
}

void guiplugin::handleDraggedMove(MouseEvent& evt) {
	if (isSelected()) {
		auto& sel = MainCtrl::get()->getPluginSel();
		if (sel.hasSelection()) {
			setDraggedPluginsUI(sel.pluginCtr->dragged, sel);
			MainCtrl::get()->setDragged(&sel.pluginCtr->dragged);
			hasDragged = true;
		} else {
			hasDragged = false;
		}
	} else {
		hasDragged = false;
		MainCtrl::get()->objectDragMove(this, evt);
	}
}
void guiplugin::handleDraggedRelease(MouseEvent& evt) {
	MainCtrl::get()->objectDragRelease(this, evt);
	if (hasDragged) {
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
	buttonBypass.setControl(parentCtrl);
	buttonDelete.setControl(parentCtrl);
	meter.setControl(parentCtrl);
}
guibase* guiplugin::getDraggedControl() {
	return this;
}
bool guiplugin::isSelected() {
	assert(this->parentCtrl);
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
		knobTest.fnFocus = [this](MouseHitEvt& evt, bool focused) {focusEvent(evt, focused);};
		knobTest.setParent(this);
	}
    virtual bool focusEvent(MouseHitEvt& evt, bool focused) override {
    	if (focused)
    		MainCtrl::get()->showAutomation(effect->getTrack(), effect, entry->idx);
    	return true;
    }
	void handleRightClick(MouseEvent& evt) override {
		guictxtmenu_base* ctxt = new guictxtmenu_vstparam(effect, entry);
		MainCtrl::get()->openContextMenu(ctxt, evt.mousepos);
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
		assert(knobTest.parent == this);
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
		setFont(vg, (int) (rowHeight * 0.8), G_WHITE, G_TITLE_ALIGN);
		nvgText(vg, x, rowHeight / 2, StringAsCStr(getText()), NULL);
		nvgTranslate(vg, -pos.x, -pos.y);

		knobTest.render(vg);
	}
};

guivstplugin::guivstplugin(vstplugin * _vst)
  : guiplugin(_vst), vst(_vst)
{
	params.setRowHeight(48);
	buttonOpenEditor.icon = ICON_ADJUST;
	buttonOpenEditor.state = &_vst->bEditOpen;
	buttonOpenEditor.setParent(this);
	buttonOpenEditor.colorActive = GuiColor::COL_BTN_BG_SHOW_ACTIVE;
	params.setParent(this);
	meter.setParent(this);
	std::vector<gui_list_entry*> _newList;
	for (automatable_param_t& param : _vst->params) {
		if (param.internalIdx >= 0)
			_newList.push_back(new gui_plugin_paramlist_entry(_vst, &param));
	}
	params.setList(_newList);
}

guivstplugin::~guivstplugin() {
}
void guivstplugin::setControl(BaseCtrl* parentCtrl) {
	guiplugin::setControl(parentCtrl);
	buttonOpenEditor.setControl(parentCtrl);
	params.setControl(parentCtrl);
	for (auto* ctr : viewCtrs) {
		ctr->setControl(parentCtrl);
	}
}

void guivstplugin::determineSize(glm::ivec2& prefSize) {
	if (this->viewCtr) {
		this->viewCtr->getFixedSize(&sizeCtrs.x, &sizeCtrs.y);
		int width = (int)((sizeCtrs.x/(float)sizeCtrs.y)*size.y);
		sizeCtrs.x = width;
		sizeCtrs.y = size.y;
		prefSize.y = std::max(sizeCtrs.y, prefSize.y);
		prefSize.x += sizeCtrs.x;
	} else {
		sizeCtrs = {0, 0};
		// we accept whatever prefSize
	}
}
void guivstplugin::render(NVGcontext* vg) {
	renderBase(vg);
	buttonBypass.render(vg);
	buttonOpenEditor.render(vg);
	buttonDelete.render(vg);
	for (auto* ctr : viewCtrs) {
		nvgSave(vg);
		ctr->render(vg);
		nvgRestore(vg);
	}
	meter.render(vg);
	if (params.isBackgroundRendered()){
		params.renderBackground(vg);
	}
	params.render(vg);
}
bool guivstplugin::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
	if (evt.type == MouseHitType::MOUSE_DRAGDROP_OBJECT) {
		return false;
	}
	if (contains(mpos)) {
		ivec2 localMouse = this->toContainerSpace(mpos);
		if (buttonBypass.mouseHitTest(localMouse, evt)) {
			return true;
		}
		if (buttonOpenEditor.mouseHitTest(localMouse, evt)) {
			return true;
		}
		if (buttonDelete.mouseHitTest(localMouse, evt)) {
			return true;
		}
		for (auto* ctr : viewCtrs) {
			if (ctr->mouseHitTest(localMouse, evt)) {
				return true;
			}
		}
		if (params.mouseHitTest(localMouse, evt)) {
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
	if (_button == &buttonBypass) {
    	ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
    	float f = vst->getParamValue(PARAM_ENABLE);
    	float f2 = f > 0.5 ? 0 : 1;
    	vst->setParamValue(PARAM_ENABLE, f2, 2);
		vst->postSetParameter(PARAM_ENABLE, f, f2, 2);
	}
	if (_button == &buttonOpenEditor) {
		if (vst->bEditOpen) {
			vst->close();
		} else {
			vst->show();
		}
	}
	if (_button == &buttonDelete) {
    	removePlugin(vst);
	}
}
void guivstplugin::layoutModule(ivec2 pos, ivec2 contentS, int32_t inset1) {
	const int32_t hpt = theme->get(GuiConstant::CONST_PLUGIN_TITLE_HEIGHT);
	buttonOpenEditor.size = buttonBypass.size;
	buttonOpenEditor.setRadius(buttonBypass.radius);
	buttonOpenEditor.pos.y = inset1;
	buttonOpenEditor.pos.x = buttonBypass.right();
	titlePosX = buttonOpenEditor.right();
	int32_t insetCtrls = INSET_TITLE;
	int rowHeight = 64;
	while (contentS.y < rowHeight * 8 && rowHeight > 8) {
		rowHeight -= 4;
	}
	int paramsW = contentS.x - sizeCtrs.x;
	params.setRowHeight(rowHeight);
	params.pos = ivec2(insetCtrls, insetCtrls + hpt);
	params.size = ivec2(paramsW, contentS.y) - ivec2(insetCtrls*2);
	params.layout();
	if (viewCtrs.size()) {
		int left = params.right() + INSET_TITLE;
		for (auto* ctr : viewCtrs) {
			ctr->pos = ivec2(left, 0) + ivec2(insetCtrls, insetCtrls + hpt);
			ivec2 prefSizeCtr = ivec2(sizeCtrs.x, contentS.y) - ivec2(insetCtrls*2);
			ctr->determineSize(prefSizeCtr);
			ctr->size = prefSizeCtr;
			ctr->layout();
			left = ctr->right() + INSET_TITLE;
		}
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
		table.rows.push_back({{tblstr{"bIsEnabled"}, tblint{ptr->vst->bIsEnabled}}});
		table.rows.push_back({{tblstr{"PARAM_ENABLE"}, tblfloat{ptr->vst->getParamValue(PARAM_ENABLE)}}});
	}
	Table::AdjustColSizes(table, getSizeContent()-ivec2(INSET_TABLE<<1));
	size.y = table.rows.size()*table.rowHeight;
}

guictxtmenu_base* guivstplugin::getTooltip(AppCtrl* appctrl) {
	auto tooltip = new guitooltip<guivstplugin>(this);
	return tooltip;
}

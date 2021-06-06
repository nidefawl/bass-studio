#pragma once
#include <nanovg_min.h>
#include "math/vec.h"
#include "gui.h"
#include "event.h"
#include "str_util.h"
#include "color_util.h"
#include "button.h"
#include "list.h"
#include "table.h"
#include "guimeter.h"
#include "dropdown.h"
#include "guicontextmenu.h"

class effectbase;
class vstplugin;
class internalplugin;
class BaseCtrl;
class AppCtrl;
class PluginViewContainers;
class guictxtmenu_base;
class guiplugin : public guictr_base {
public:
	effectbase* const effect;
	String text;
	guibuttontoggle buttonBypass;
	guibuttontoggle buttonDelete;
	guibuttontoggle buttonLayout;
	guibuttontoggle buttonSave;
	gui_trackmeter<16000,2> meter;
	float titlePosX = 0;
	bool hasDragged=false;
	bool isHorizontalTitle=true;
	int layoutMode = 0;

	std::vector<guibuttontoggle*> guiButtons;
	guiplugin(effectbase* _effect);
	~guiplugin();
	void addGuiBtn(guibuttontoggle* btn);
	virtual void render(NVGcontext* vg) override;
	virtual void prerender(NVGcontext* vg) override;
	virtual void buttonClicked(guibase* _button);
	virtual bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt);
	virtual void layoutModule(ivec2 pos, ivec2 contentS, int32_t inset1) = 0;
	void determineSize(ivec2& prefSize) override;
	effectbase* getModule() {
		return effect;
	}
	virtual void renderDragged(NVGcontext* vg, ivec2 mousepos, ivec2 dragOffset) override {
		mousepos += dragOffset;
		mousepos -= pos;
		nvgTranslate(vg, mousepos.x, mousepos.y);
		render(vg);
	}
	virtual void renderBase(NVGcontext* vg);

	virtual void layout() override;

	void rightClicked(MouseEvent& evt, guibase* button) override;
	void handleRightClick(MouseEvent& evt) override;
	void handleDraggedBegin(MouseEvent& evt) override;
	void handleDraggedMove(MouseEvent& evt) override;
	void handleDraggedRelease(MouseEvent& evt) override;
	void dragMoveOn(guibase* target, ivec2 mousepos) override;
	void dragReleaseOn(guibase* target, ivec2 mousepos) override;
	void setTitle(String _text) {
		text = _text;
	}
	void setState(bool state) {
	}
	bool isDragMoveable() {
		return true;
	}
    virtual bool focusEvent(MouseHitEvt& evt, bool focused) override;

	virtual guictxtmenu_base* getTooltip(AppCtrl* appctrl) override;
	virtual guibase* getDraggedControl() override;
	virtual void setControl(BaseCtrl* parentCtrl) override;
	virtual bool isSelected() override;
	virtual void addProperties(Table::tbl* table);
};

class guidropdown_select_program : public guictxtmenu {
	effectbase* const plugin;
public:
	guidropdown_select_program(effectbase *_plugin);
	void clicked(int _id);
};
class guidropdownprogram : public guidropdownbase {
	effectbase* plugin = nullptr;
public:
	guidropdownprogram(effectbase* _plugin) : guidropdownbase(), plugin(_plugin) {

	}
	String getString();
	virtual void handleDraggedRelease(MouseEvent &evt);
	uint32_t getSelectIndex();
	uint32_t getLastIndex();
	void setSelectedIndex(uint32_t idx);
};
class guipluginview : public guiplugin {
public:
	guipluginview(effectbase * _effect);
	~guipluginview();
	effectbase* const effect;
	guidropdownprogram dropdownProgram;
	gui_list params; //TODO: use add() on control
	guibuttontoggle buttonOpenEditor; //TODO: use add() on controls
    guibuttontoggle buttonShowInlineGUI; // TODO: use add() on controls

	/* holds view controller for internal vstplugins with custom gui (non-steinberg api) */
	std::shared_ptr<PluginViewContainers> viewCtr;
	/* holds guictrs of internal vstplugins with custom gui (non-steinberg api) */
	std::vector<guictr_base*> viewCtrs;
	/* holds size for internal vstplugins with custom gui (non-steinberg api) */
	ivec2 sizeCtrs;

	guictr_base* ctrPreview = nullptr;

	void layoutModule(ivec2 pos, ivec2 contentS, int32_t inset1);
	void render(NVGcontext* vg) override;
	void buttonClicked(guibase* _button) override;
	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
	virtual void setControl(BaseCtrl* parentCtrl) override;
	void determineSize(ivec2& prefSize) override;
	void prerender(NVGcontext* vg) override;
};
class guivstplugin : public guipluginview {
public:
	guivstplugin(vstplugin * _vst);
	~guivstplugin();
	vstplugin* const vst;
	guictxtmenu_base* getTooltip(AppCtrl* appctrl) override;

};
class guiinternalpluginview : public guipluginview {
	internalplugin* const plugin;
public:
	guiinternalpluginview(internalplugin * _effect);
	~guiinternalpluginview();
	guictxtmenu_base* getTooltip(AppCtrl* appctrl) override;

};

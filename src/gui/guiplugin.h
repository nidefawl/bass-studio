#pragma once
#include <nanovg_min.h>
#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include "gui.h"
#include "event.h"
#include "str_util.h"
#include "color_util.h"
#include "button.h"
#include "list.h"
#include "guimeter.h"
#include "leak_detect.h"

using glm::vec2;
using glm::ivec2;

class effectbase;
class vstplugin;
class BaseCtrl;
class AppCtrl;
class PluginViewContainers;
class guictxtmenu_base;
class guiplugin : public guictr_base {
public:
	effectbase* const effect;
	String text;
	guibuttontoggle buttonBypass; //TODO: use add() on controls
	guibuttontoggle buttonDelete; //TODO: use add() on controls
	gui_trackmeter meter; //TODO: use add() on controls
	float titlePosX = 0;
	bool hasDragged=false;
	bool isHorizontalTitle=true;
	guiplugin(effectbase* _effect);
	virtual ~guiplugin() {
		my_printf("DSTR!\n",0);
	}
	virtual void render(NVGcontext* vg) = 0;
	virtual void buttonClicked(guibase* _button) = 0;
	virtual bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) = 0;
	virtual void layoutModule(ivec2 pos, ivec2 contentS, int32_t inset1) = 0;
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
	void handleDraggedBegin(MouseEvent& evt) override;
	void handleDraggedMove(MouseEvent& evt) override;
	void handleDraggedRelease(MouseEvent& evt) override;
	void dragMoveOn(guibase* target, ivec2 mousepos) override;
	void dragReleaseOn(guibase* target, ivec2 mousepos) override;
	void setTitle(String _text) {
		text = _text;
		my_printf("SET TITLE %s\n", StringAsCStr(text));
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
};
class guivstplugin : public guiplugin {
public:
	guivstplugin(vstplugin * _vst);
	~guivstplugin();
	vstplugin* const vst;
	gui_list params; //TODO: use add() on controls
	guibuttontoggle buttonOpenEditor; //TODO: use add() on controls
	PluginViewContainers* viewCtr = nullptr;
	std::vector<guictr_base*> viewCtrs;
	ivec2 sizeCtrs;
	void layoutModule(ivec2 pos, ivec2 contentS, int32_t inset1);
	void render(NVGcontext* vg) override;
	void buttonClicked(guibase* _button) override;
	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
	guictxtmenu_base* getTooltip(AppCtrl* appctrl) override;
	virtual void setControl(BaseCtrl* parentCtrl) override;
	void determineSize() override;
};

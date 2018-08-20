#pragma once
#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include "gui.h"
#include "guicolors.h"
#include "event.h"
#include "str_util.h"
#include "color_util.h"
#include "button.h"
#include "renderresources.h"
#include "list.h"
#include "guimeter.h"
#include "knob.h"
#include "leak_detect.h"

using glm::vec2;
using glm::ivec2;

class effectbase;
class vstplugin;

class guiplugin : public guictr_base {
public:
	effectbase* const effect;
	String text;
	guibuttontoggle buttonBypass;
	guibuttontoggle buttonDelete;
	gui_trackmeter meter;
	float titlePosX = 0;
	bool hasDragged=false;
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
	virtual void renderDragged(NVGcontext* vg, ivec2 mousepos) {
		mousepos -= pos;
		nvgTranslate(vg, mousepos.x, mousepos.y);
		render(vg);
	}
	virtual void renderBase(NVGcontext* vg);

	virtual void layout() override {
		int32_t meterW = 32;
		while (size.x < meterW * 16 && meterW > 16) {
			meterW -= 4;
		}
		int32_t inset1 = (HEIGHT_PLUGIN_TITLE - buttonBypass.size.y) / 2;
		ivec2 contentS(size.x - meterW, size.y-HEIGHT_PLUGIN_TITLE);
		ivec2 contentP(0, HEIGHT_PLUGIN_TITLE);
		buttonBypass.pos.y = inset1;
		buttonBypass.pos.x = inset1;
		buttonDelete.pos.y = inset1;
		buttonDelete.pos.x = size.x - buttonDelete.size.x - inset1;
		titlePosX = buttonBypass.right();
		layoutModule(contentP, contentS, inset1);
		meter.pos = ivec2(size.x - meterW, HEIGHT_PLUGIN_TITLE);
		meter.size = ivec2(meterW, contentS.y);
		meter.layout();
	}
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
	virtual bool isSelected() override;
	virtual guibase* getDraggedControl() override;
};
class guivstplugin : public guiplugin {
public:
	guivstplugin(vstplugin * _vst);
	~guivstplugin();
	vstplugin* const vst;
	gui_list params;
	guibuttontoggle buttonOpenEditor;
	void layoutModule(ivec2 pos, ivec2 contentS, int32_t inset1) {
		buttonOpenEditor.pos.y = inset1;
		buttonOpenEditor.pos.x = buttonBypass.right();
		titlePosX = buttonOpenEditor.right();
		int32_t insetCtrls = INSET_TITLE;
		int rowHeight = 64;
		while (contentS.y < rowHeight * 8 && rowHeight > 8) {
			rowHeight -= 4;
		}
		params.setRowHeight(rowHeight);
		params.pos = ivec2(insetCtrls, insetCtrls + HEIGHT_PLUGIN_TITLE);
		params.size = contentS - ivec2(insetCtrls*2);
		params.layout();
	}
	void render(NVGcontext* vg) override;
	void buttonClicked(guibase* _button) override;
	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
	guictxtmenu_base* getTooltip(AppCtrl* appctrl) override;
};

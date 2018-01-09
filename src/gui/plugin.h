#pragma once
#include <glm/vec2.hpp>
#include "gui.h"
#include "guicolors.h"
#include "event.h"
#include "str_util.h"
#include "color_util.h"
#include "button.h"
#include "renderresources.h"
#include "list.h"
#include "knob.h"
#include "leak_detect.h"

using glm::vec2;
using glm::ivec2;

class vstplugin;

class vstplugin;

class guiplugin : public guibase {
public:
	vstplugin* const vst;
	gui_list params;
	char text[MAX_STR_TITLE];
	guibuttontoggle buttonBypass;
	guibuttontoggle buttonOpenEditor;
	guibuttontoggle buttonDelete;
	guiplugin(vstplugin* _vst);
	~guiplugin() {
		my_printf("DSTR!\n",0);
	}
	virtual void renderDragged(NVGcontext* vg, ivec2 mousepos) {
		mousepos -= pos;
		nvgTranslate(vg, mousepos.x, mousepos.y);
		render(vg);
	}
	void render(NVGcontext* vg);
	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
	void setTitle(String wxtext) {
		const char* wxmb = StringAsCStr(wxtext);
		strncpy_s(this->text, MAX_STR_TITLE, wxmb, strlen(wxmb));
	}
	void layout() {
		int32_t inset1 = (HEIGHT_PLUGIN_TITLE - buttonBypass.size.y) / 2;
		buttonBypass.pos.y = inset1;
		buttonBypass.pos.x = inset1;
		buttonOpenEditor.pos.y = inset1;
		buttonOpenEditor.pos.x = buttonBypass.right();
		buttonDelete.pos.y = inset1;
		buttonDelete.pos.x = size.x - buttonDelete.size.x - inset1;
		int32_t insetCtrls = INSET_TITLE;
		int rowHeight = 64;
		while (size.y < rowHeight * 8 && rowHeight > 8) {
			rowHeight -= 4;
		}
		params.setRowHeight(rowHeight);
		params.pos = ivec2(insetCtrls, insetCtrls + HEIGHT_PLUGIN_TITLE);
		params.size = size - params.pos - ivec2(insetCtrls);
		params.layout();
	}
	void handleDraggedMove(MouseEvent& evt) override;
	void handleDraggedRelease(MouseEvent& evt) override;
	void dragMoveOn(guibase* target, ivec2 mousepos) override;
	void dragReleaseOn(guibase* target, ivec2 mousepos) override;
	void setState(bool state) {
	}
	void buttonClicked(guibase* _button);
	bool isDragMoveable() {
		return true;
	}
};

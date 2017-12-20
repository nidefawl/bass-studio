#pragma once
#include "str_util.h"
#include "color_util.h"
#include "guicontainer.h"

class gui_statusbar : public guictr_base {
public:
	char text[MAX_STR_STATUSBAR];
	gui_statusbar() : guictr_base() {
		text[0] = 0;
	}
	~gui_statusbar() {
	}
	void render(NVGcontext* vg) {
		guictr_base::renderBackground(vg);
		if (!setScissorTransform(vg)) {
			return;
		}
		if (this->text[0]) {
			setFont(vg, (int)(HEIGHT_PLUGIN_TITLE*0.8), G_BLACK, G_TITLE_ALIGN);
			nvgText(vg, INSET_TITLE, getSizeContent().y / 2, this->text, NULL);
		}
	}
	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
		if (contains(mpos)) {
			evt.requestFocus(this);
			return true;
		}
		return false;
	}
	void setTitleCstr(const char* cstr) {
		setTitle(String(cstr));
	}
	void setTitle(String wxtext) {
		const char* wxmb = StringAsCStr(wxtext);
		strncpy_s(this->text, MAX_STR_TITLE, wxmb, strlen(wxmb));
	}
};

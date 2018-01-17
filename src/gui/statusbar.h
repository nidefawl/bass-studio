#pragma once
#include "str_util.h"
#include "color_util.h"
#include "guicontainer.h"

class gui_statusbar : public guictr_base {
public:
	String text;
	gui_statusbar() : guictr_base() {
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
			nvgText(vg, INSET_TITLE, getSizeContent().y / 2, StringAsCStr(text), NULL);
		}
	}
	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
		if (contains(mpos)) {
			evt.requestFocus(this);
			return true;
		}
		return false;
	}
	void setTitle(String _text) {
		text = _text;
	}
};

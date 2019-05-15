#include "dialog.h"
#include "button.h"

#include <functional>

class guidialog_cb_yes_no : public guidialog_base {
	int TITLE_FONT_SIZE = 24;
	int BTN_FONT_SIZE = 16;
	guibutton btnYes;
	guibutton btnNo;
public:
	std::function<void(int)> cb;
	String message;
public:
	guidialog_cb_yes_no() : guidialog_base(ivec2(360, 140)) {
		btnYes.setText("Yes");
		btnNo.setText("No");
		add(&btnYes);
		add(&btnNo);
		btnYes.setFontSize(BTN_FONT_SIZE);
		btnNo.setFontSize(BTN_FONT_SIZE);
		setBackgroundRendered(true);
		dbgassert(getAllocId() > 0); // make sure we are heap allocated
	}
	~guidialog_cb_yes_no() {
		remove(&btnNo);
		remove(&btnYes);
	}
	void layout() {
		int sizeW = getSizeContent().x;
		int sizeH = getSizeContent().y/2;
		int y = sizeH;
		int wYes = 100;
		int wNo = wYes*0.7;
		int hBtn = 32;
		btnYes.pos = { sizeW/3-wYes/2, (sizeH-hBtn)/2 +y};
		btnYes.size = { wYes, hBtn };
		btnNo.pos = { sizeW*2/3-wNo/2, (sizeH-hBtn)/2 +y};
		btnNo.size = { wNo, hBtn };
	}
	void buttonClicked(guibase* button) {
		int n = button == &btnYes ? 1 : 0;
		cb(n);
	}
	void render(NVGcontext* vg) {
		if (isBackgroundRendered()){
			renderBackground(vg);
		}
		if (!setScissorTransform(vg)) {
			return;
		}
		for (auto c : guis) {
			nvgSave(vg);
			c->render(vg);
			nvgRestore(vg);
		}
		int sizeH = getSizeContent().y/4;
		setFont(vg, G_FONT_SCALE(TITLE_FONT_SIZE), G_WHITE, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
		nvgText(vg, TITLE_FONT_SIZE*2, sizeH, message.c_str(), nullptr);
	}
};

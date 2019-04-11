#include "about.h"
#include "math/vec.h"
#include "str_util.h"
#include "button.h"
#include "guicontainer.h"
#include "guicontextmenu_base.h"
#ifndef __TIMESTAMP__
#define __TIMESTAMP__ ""
#endif

constexpr int ID_BTN_CLOSE = 1;
constexpr int TITLE_FONT_SIZE = 32;
constexpr int TEXT_FONT_SIZE = 26;
constexpr int BTN_FONT_SIZE = 16;
guidialog_about::guidialog_about() : guidialog_base(ivec2{440, 560}) {
	setBackgroundRendered(true);
	add(&btnClose);
	btnClose.id = ID_BTN_CLOSE;
	btnClose.setText("Close");
	btnClose.setFontSize(BTN_FONT_SIZE);
	setLabel("About");
}
void guidialog_about::render(NVGcontext* vg) {
	if (isBackgroundRendered()){
		renderBackground(vg);
	}
	if (!setScissorTransform(vg)) {
		return;
	}

	using AboutLine = std::tuple<String, String>;
	using std::make_tuple;
	std::vector<AboutLine> strings;
	String str;
	strings.emplace_back(String("Compiled: "), String(__TIMESTAMP__));

	int x = 0;
	int xRight = getSizeContent().x;
	int y = 0;
	float lineh;
	setFont(vg, TITLE_FONT_SIZE, G_WHITE, NVG_ALIGN_TOP | NVG_ALIGN_LEFT);
	nvgTextMetrics(vg, NULL, NULL, &lineh);
	y+=lineh;
	nvgText(vg, x, 0, StringAsCStr(label), NULL);
	setFont(vg, TEXT_FONT_SIZE, G_WHITE, NVG_ALIGN_TOP | NVG_ALIGN_LEFT);
	nvgTextMetrics(vg, NULL, NULL, &lineh);
	y+=lineh;
	int yPosLines = y;
	for (AboutLine& t : strings) {
		nvgText(vg, x, y, StringAsCStr(std::get<0>(t)), NULL);
		y += lineh;
	}
	nvgTextAlign(vg, NVG_ALIGN_TOP | NVG_ALIGN_RIGHT);
	y = yPosLines;
	for (AboutLine& t : strings) {
		nvgText(vg, xRight, y, StringAsCStr(std::get<1>(t)), NULL);
		y += lineh;
	}
	for (auto c : guis) {
		nvgSave(vg);
		c->render(vg);
		nvgRestore(vg);
	}
}
void guidialog_about::layout() {
	ivec2 cs = getSizeContent();
	int32_t size = 32;
	btnClose.size = ivec2(size * 4, size);
	btnClose.pos = ivec2(cs.x - btnClose.size.x, cs.y - btnClose.size.y);
	for (auto gui : guis) {
		gui->layout();
	}
}

void guidialog_about::buttonClicked(guibase* button) {
	switch (button->id) {
	case ID_BTN_CLOSE:
		closeContextMenu();
		break;

	}
}

#pragma once
#include <vector>
#include "event.h"
#include <functional>
#include "math/vec.h"
#include "gui.h"
#include "guicolors.h"
#include "guicontextmenu_base.h"
#include "guicontextmenu.h"
#include "basectrl.h"

class ctxtmenu_color_select : public ctxtmenu_entry {
public:
	const int WH = 16;
	const int ROWS = COLOR_PALETTE_ROWS;
	const int COLS = COLOR_PALETTE_COLS;
	const int pad = 5;
	const int padCell = 3;
	ctxtmenu_color_select(String _title, int _id)
	: ctxtmenu_entry(_title, _id)
	{
		this->id = _id;
		this->title = _title;
		this->width = pad * 2 + (WH + padCell) * COLS - padCell;
	}
	void layout(ivec2 size, int32_t _fontSize) override {
		this->fontSize = _fontSize;
		height = _fontSize + pad * 2 + (WH + padCell) * ROWS - padCell;
	}
	void render(ivec2 ctxtSize, NVGcontext* vg, int idx, ivec2 mouse) {
		setFont(vg, this->fontSize, G_WHITE, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
		nvgText(vg, leftOffset(), y+this->fontSize/2, StringAsCStr(title), NULL);
		nvgFontSize(vg, this->fontSize-4);
		int focusIdx = -1;
		int y = this->y+this->fontSize+pad-4;
		for (int col = 0; col < COLS; col++) {
			int cX = pad+(WH+padCell)*col;
			for (int row = 0; row < ROWS; row++) {
				int idx = col*ROWS+row;
				int cY = pad+(WH+padCell)*row;
				if (mouse.y >= y + cY && mouse.y < y + cY + WH && mouse.x >= cX && mouse.x < cX + WH) {
					focusIdx = idx;
				}
				nvgBeginPath(vg);
				nvgRect(vg, cX, y+cY, WH, WH);
				nvgFillColor(vg, g_colorPalette[idx]);
				nvgFill(vg);
				nvgStrokeColor(vg, G_WHITE);
				nvgStrokeWidth(vg, 0.5f);
				nvgStroke(vg);
			}
		}
		if (focusIdx >= 0) {
			int row = focusIdx%ROWS;
			int col = focusIdx/ROWS;
			int cX = pad+(WH+padCell)*col;
			int cY = pad+(WH+padCell)*row;
			int extent = 4;
			nvgBeginPath(vg);
			nvgRect(vg, cX-extent, y+cY-extent, WH+extent*2, WH+extent*2);
			nvgFillColor(vg, g_colorPalette[focusIdx]);
			nvgFill(vg);
			nvgStrokeColor(vg, G_WHITE);
			nvgStrokeWidth(vg, 1.0f);
			nvgStroke(vg);
		}
	}
	bool contains(ivec2& ctxtSize, ivec2& mouse) {
		return mouse.y >= y && mouse.y < y + height && mouse.x >= 0 && mouse.x < ctxtSize.x;
	}
	int getClicked(ivec2& ctxtSize, ivec2& mouse) {
		if (contains(ctxtSize, mouse)) {
			int y = this->y+this->fontSize+pad-4;
			for (int col = 0; col < COLS; col++) {
				int cX = pad+(WH+padCell)*col;
				for (int row = 0; row < ROWS; row++) {
					int idx = col*ROWS+row;
					int cY = pad+(WH+padCell)*row;
					if (mouse.y >= y + cY && mouse.y < y + cY + WH && mouse.x >= cX && mouse.x < cX + WH) {
						return this->id+idx;
					}
				}
			}
		}
		return -1;
	}
};
class guictxtmenu_colorpalette : public guictxtmenu {
public:
    std::function<void(int32_t)> callback = nullptr;
	guictxtmenu_colorpalette() {
		ctxtmenu_color_select* colorSelect = new ctxtmenu_color_select("Pick Color", 100);
		addEntry(colorSelect);
	}
	void clicked(int _id) {
		if (_id >= 100) {
			_id -= 100;
			int32_t col = colorPalette[_id];
			my_printf("col: %08X\n", col);
			if (callback) {
				callback(col);
			}
		}
		parentCtrl->closePopup();
	}
};

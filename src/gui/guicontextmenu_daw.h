#pragma once
#include <vector>
#include "math/vec.h"
#include "event.h"
#include "gui.h"
#include "guicontextmenu_base.h"
#include "guicontextmenu.h"
#include "guicontextmenu_color.h"
#include "trackctr.h"
#include "track.h"
#include "track_impl.h"
#include "guicolors.h"
//#include "clipeditor.h"

class ctxtmenu_time_select : public ctxtmenu_entry {
	struct _time_sel_entry {
		int id;
		int x;
		int y;
		int w;
		String name;
	};
	scaled_grid& grid;
	std::vector<_time_sel_entry> entries;
public:
	const int pad = 10;
	bool fixed = false;
	const int inset = 5;
	ctxtmenu_time_select(scaled_grid& _grid, String _title, int _id)
	: ctxtmenu_entry(_title, _id),
	  grid(_grid)
	{
		this->id = _id;
		this->title = _title;
	}
	void layout(ivec2 size, int32_t _fontSize) override {
		this->fontSize = _fontSize;
		this->height = (int32_t) round(_fontSize*1.1f);
		const int h = this->fontSize;
		layoutE(size.x, fixed?5:3);
		if (fixed) {

			_time_sel_entry& off = entries.back();
			off.x = inset;
			off.y += h;
			this->height = off.y+h;
		}
	}
	void layoutE(int tw, int perRow) {
		const int h = this->fontSize;
		int iX = inset;
		int iY = h+2;
		int elW = (tw-inset*2)/perRow;
		for (_time_sel_entry& e : entries) {
			this->height = iY+h;
			e.x = iX;
			e.y = iY;
			e.w = elW;
			iX += e.w;
			if (iX >= tw-inset*2) {
				iX = inset;
				iY += h;
			}
		}
	}
	void initAdaptive() {
		fixed = false;
		entries.push_back({GRID_WIDEST, 0, 0, 0, "Widest"});
		entries.push_back({GRID_WIDE, 0, 0, 0, "Wide"});
		entries.push_back({GRID_MED, 0, 0, 0, "Medium"});
		entries.push_back({GRID_NARROW, 0, 0, 0, "Narrow"});
		entries.push_back({GRID_NARROWEST, 0, 0, 0+15, "Narrowest"});
	}
	void initFixed() {
		fixed = true;
		entries.push_back({GRID_8BAR, 0, 0, 0, "8 Bars"});
		entries.push_back({GRID_4BAR, 0, 0, 0, "4 Bars"});
		entries.push_back({GRID_2BAR, 0, 0, 0, "2 Bars"});
		entries.push_back({GRID_1BAR, 0, 0, 0, "1 Bar"});
		entries.push_back({GRID_1_2, 0, 0, 0, "1/2"});
		entries.push_back({GRID_1_4, 0, 0, 0, "1/4"});
		entries.push_back({GRID_1_8, 0, 0, 0, "1/8"});
		entries.push_back({GRID_1_16, 0, 0, 0, "1/16"});
		entries.push_back({GRID_1_32, 0, 0, 0, "1/32"});
		entries.push_back({GRID_OFF, 0, 0, 0, "Off"});
	}
	void render(ivec2 ctxtSize, NVGcontext* vg, int idx, ivec2 mouse) {
		const int h = this->fontSize;
		UTIL_setFont(vg, theme, h, G_WHITE, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
		nvgText(vg, leftOffset(), y+h/2, StringAsCStr(title), NULL);
		nvgFontSize(vg, this->fontSize-4);
		int n = 0;
		for (_time_sel_entry& e : entries) {
			if (mouse.y >= y+e.y && mouse.y < y+e.y + h && mouse.x >= e.x && mouse.x < e.x+e.w) {
				nvgBeginPath(vg);
				nvgRect(vg, e.x, y+e.y+2, e.w, h-4);
				nvgFillColor(vg, theme->getColor(GuiColor::COL_CTXTMNU_HILIGHT));
				nvgFill(vg);
			}
			if ((!grid.grid_dens.enabled && e.id == GRID_OFF) ||
				(grid.grid_dens.enabled && (grid.grid_dens.isfixed == fixed) &&
					((!fixed && n == grid.grid_dens.dynamicDensity)
				 ||  ( fixed && n == grid.grid_dens.fixedBars     ))) ) {
				nvgBeginPath(vg);
				nvgCircle(vg, e.x+10, y+e.y+h/2, 4);
				nvgFillColor(vg, theme->getColor(GuiColor::COL_CTXTMNU_OUTLINE));
				nvgFill(vg);
			}
			nvgFillColor(vg, G_WHITE);
			nvgText(vg, e.x+20, y+e.y+h/2, StringAsCStr(e.name), NULL);
			n++;
		}
	}
	bool contains(ivec2& ctxtSize, ivec2& mouse) {
		return mouse.y >= y && mouse.y < y + height && mouse.x >= 0 && mouse.x < ctxtSize.x;
	}
	int getClicked(ivec2& ctxtSize, ivec2& mouse) {
		if (contains(ctxtSize, mouse)) {
			int n = fixed ? 10 : 0;
			const int h = this->fontSize;
			for (_time_sel_entry& e : entries) {
				if (mouse.y >= y+e.y && mouse.y < y+e.y + h && mouse.x >= 0 && mouse.x < e.x+e.w) {
					return n+100;
				}
				n++;
			}
		}
		return -1;
	}
};

class guictxtmenu_clip : public guictxtmenu {
	ctxtmenu_color_select* sel;
	clip_t* const m_clip;
public:
	guictxtmenu_clip(clip_t* const _clip) : m_clip(_clip) {
		this->size.x = 120;
		sel = new ctxtmenu_color_select("Pick Color", 100);
		addEntry(sel);
	}
	void clicked(int _id) {
		if (_id >= sel->id) {
			_id -= sel->id;
			int32_t col = colorPalette[_id];
			if (m_clip) {
				m_clip->rgb = col;
			}
		}
		closeContextMenu();
	}
};
class guictxtmenu_notrack : public guictxtmenu {
private:
	int idxImport;
public:
	guictxtmenu_notrack() {
		this->size.x = 190;
		int i = 0;
		for (; i < NUM_TRACK_TYPES; i++) {
			addEntry(new ctxtmenu_entry(StringFormat("Insert %s Track", TrackTypeToName(i)), i));
		}
		idxImport = i;
		addEntry(new ctxtmenu_entry("Import Track", i++));
	}
	void clicked(int _id);
};


class guictxtmenu_at_param : public guictxtmenu {
	automatable_t* const atl;
	int32_t const paramIdx;
public:
	guictxtmenu_at_param(automatable_t* _atl, int32_t _paramIdx);
	void clicked(int _id);
};

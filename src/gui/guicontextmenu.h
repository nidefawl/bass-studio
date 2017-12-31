#pragma once
#include <vector>
#include "mainctrl.h"
#include "gui.h"
#include "guicolors.h"
#include "clipeditor.h"

class ctxtmenu_entry {
public:
	String title;
	int width = -1;
	int height = 0;
	int id = 0;
	int y = 0;
	int fontSize = 0;
	ctxtmenu_entry(String _title, int _id) {
		this->id = _id;
		this->title = _title;
	}
	virtual ~ctxtmenu_entry() {

	}
	virtual void layout(ivec2 size, int32_t _fontSize) {
		this->fontSize = _fontSize;
		this->height = (int32_t) round(_fontSize*1.1f);
	}
	int leftOffset() {
		return (int32_t) round(this->fontSize/2.4f);
	}
	virtual void render(ivec2 ctxtSize, NVGcontext* vg, int idx, ivec2 mouse) {
		if (contains(ctxtSize, mouse)) {
			nvgBeginPath(vg);
			nvgRect(vg, 0, y, ctxtSize.x, height);
			nvgFillColor(vg, g_guiColors[COL_CTXTMNU_HILIGHT]);
			nvgFill(vg);
		}
		setFont(vg, this->fontSize, G_WHITE, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
		nvgText(vg, leftOffset(), y + height / 2, StringAsCStr(title), NULL);
	}
	bool contains(ivec2& ctxtSize, ivec2& mouse) {
		return mouse.y >= y && mouse.y < y + height && mouse.x >= 0 && mouse.x < ctxtSize.x;
	}
	virtual int getClicked(ivec2& ctxtSize, ivec2& mouse) {
		if (contains(ctxtSize, mouse)) {
			return id;
		}
		return -1;
	}
};
class ctxtmenu_splitter : public ctxtmenu_entry {
public:
	ctxtmenu_splitter()
		: ctxtmenu_entry("-", -1)
	{
	}
	void render(ivec2 ctxtSize, NVGcontext* vg, int idx, ivec2 mouse) {
		nvgBeginPath(vg);
		nvgMoveTo(vg, 0, y+height/2);
		nvgLineTo(vg, ctxtSize.x, y+height/2);
		nvgStrokeColor(vg, g_guiColors[COL_CTXTMNU_OUTLINE]);
		nvgStrokeWidth(vg, 1.0f);
		nvgStroke(vg);
	}
	void layout(ivec2 size, int32_t _fontSize) override {
		this->fontSize = _fontSize;
		this->height = ((int32_t) round(_fontSize*1.1f)) / 2;
	}
	bool contains(ivec2& ctxtSize, ivec2& mouse) {
		return false;
	}
};


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
	}
	void layout(ivec2 size, int32_t _fontSize) override {
		this->fontSize = _fontSize;
		width = pad * 2 + (WH + padCell) * COLS - padCell;
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
			e.w += elW;
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
		setFont(vg, h, G_WHITE, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
		nvgText(vg, leftOffset(), y+h/2, StringAsCStr(title), NULL);
		nvgFontSize(vg, this->fontSize-4);
		int n = 0;
		for (_time_sel_entry& e : entries) {
			if (mouse.y > y+e.y && mouse.y < y+e.y + h && mouse.x >= e.x && mouse.x < e.x+e.w) {
				nvgBeginPath(vg);
				nvgRect(vg, e.x, y+e.y+2, e.w, h-4);
				nvgFillColor(vg, g_guiColors[COL_CTXTMNU_HILIGHT]);
				nvgFill(vg);
			}
			if ((!grid.grid_dens.enabled && e.id == GRID_OFF) ||
				(grid.grid_dens.enabled && (grid.grid_dens.isfixed == fixed) &&
					((!fixed && n == grid.grid_dens.dynamicDensity)
				 ||  ( fixed && n == grid.grid_dens.fixedBars     ))) ) {
				nvgBeginPath(vg);
				nvgCircle(vg, e.x+10, y+e.y+h/2, 4);
				nvgFillColor(vg, g_guiColors[COL_CTXTMNU_OUTLINE]);
				nvgFill(vg);
			}
			nvgFillColor(vg, G_WHITE);
			nvgText(vg, e.x+20, y+e.y+h/2, StringAsCStr(e.name), NULL);
			n++;
		}
	}
	bool contains(ivec2& ctxtSize, ivec2& mouse) {
		return mouse.y > y && mouse.y < y + height && mouse.x >= 0 && mouse.x < ctxtSize.x;
	}
	int getClicked(ivec2& ctxtSize, ivec2& mouse) {
		if (contains(ctxtSize, mouse)) {
			int n = fixed ? 10 : 0;
			const int h = this->fontSize;
			for (_time_sel_entry& e : entries) {
				if (mouse.y > y+e.y && mouse.y < y+e.y + h && mouse.x >= 0 && mouse.x < e.x+e.w) {
					return n+100;
				}
				n++;
			}
		}
		return -1;
	}
};

class guictxtmenu_base : public guibase {
protected:
	std::vector<ctxtmenu_entry*> entries;
	int paddingV = 2;
	int fontSize = FONT_SIZE_CTXT;
public:
	~guictxtmenu_base() {
		for (ctxtmenu_entry* e : entries) {
			delete e;
		}
	}
	void add(ctxtmenu_entry* entry) {
		entries.push_back(entry);
	}
	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
		return contains(mpos);
	}
	virtual void clicked(int _id) {
		MainCtrl::get()->closeContextMenu();
	}
	bool mouseDown(ivec2 mousePos) {
		if (contains(mousePos)) {
			ivec2 mouse = toContainerSpace(mousePos);
			for (ctxtmenu_entry* e : entries) {
				int n = e->getClicked(size, mouse);
				if (n >= 0) {
					clicked(n);
					return true;
				}
			}
		}
		MainCtrl::get()->closeContextMenu();
		return false;
	}
	void layout() {
		//TODO: figure out string width here to make life easier laying out context menus
		int y = paddingV;
		for (ctxtmenu_entry* e : entries) {
			e->layout(size, fontSize);
			e->y = y;
			y += e->height + paddingV;
			size.x = std::max(size.x, e->width);
		}
		size.y = y;
	}

	void render(NVGcontext* vg) {
		int idx = 0;
		ivec2 mouse = ContextCtrl::get()->mousepos;
		mouse = toContainerSpace(mouse);
		for (ctxtmenu_entry* e : entries) {
			e->render(size, vg, idx, mouse);
			idx++;
		}
	}
};


class guictxtmenu_clip : public guictxtmenu_base {
	ctxtmenu_color_select* sel;
	clip_t* const m_clip;
public:
	guictxtmenu_clip(clip_t* const _clip) : m_clip(_clip) {
		this->size.x = 120;
		sel = new ctxtmenu_color_select("Pick Color", 100);
		add(sel);
		layout();
	}
	void clicked(int _id) {
		if (_id >= sel->id) {
			_id -= sel->id;
			int32_t col = colorPalette[_id];
			if (m_clip) {
				m_clip->rgb = col;
			}
		}
		MainCtrl::get()->closeContextMenu();
	}
};
class guictxtmenu_trackcontent : public guictxtmenu_base {
public:
	int32_t trackid;
	guictxtmenu_trackcontent(int32_t _trackid) {
		this->trackid = _trackid;
		this->size.x = 320;
		MainCtrl* ctrl = MainCtrl::get();
		track_t* tr = ctrl->getTrackId(this->trackid);
		if (MainCtrl::get()->cursor.selRange && tr && tr->type == TRACK_TYPE_MIDI) {
			auto newClip = new ctxtmenu_entry("Create clip", 20);
			add(newClip);
			add(new ctxtmenu_splitter());
		}
		scaled_grid& grid = MainCtrl::get()->getGrid();
		auto adaptive = new ctxtmenu_time_select(grid, "Adaptive Grid", 0);
		adaptive->initAdaptive();
		add(adaptive);
		auto fixed = new ctxtmenu_time_select(grid, "Fixed Grid", 0);
		fixed->initFixed();
		add(fixed);
		layout();
	}
	void clicked(int _id) {
		MainCtrl* ctrl = MainCtrl::get();
		scaled_grid& grid = ctrl->getGrid();
		if (_id == 20) {
			Cursor cursor = MainCtrl::get()->cursor.getLeftAligned();
			if (cursor.selRange) {
				track_t* tr = ctrl->getTrackId(this->trackid);
				if (tr && tr->type == TRACK_TYPE_MIDI) {
					clip_t* cl = new clip_t(StringFormat("%s Clip", StringAsCStr(tr->name)));
					cl->time = cursor.cursorPos;
					cl->len = cursor.selRange;
					cl->loopStart = 0;
					cl->loopLen = cl->len;
					tr->getMidi().addClipSort(cl);
				}
			}
		}
		else if (_id == 110+9) { // OFF
			grid.grid_dens.enabled = false;
		} else if (_id >= 110) {
			grid.grid_dens.enabled = true;
			grid.grid_dens.fixedBars = _id - 110;
			grid.grid_dens.isfixed = true;
		} else {
			grid.grid_dens.enabled = true;
			grid.grid_dens.dynamicDensity = _id - 100;
			grid.grid_dens.isfixed = false;
		}
//		ctrl->updateVisibleTrackContents();
		MainCtrl::get()->updateGrid();

		MainCtrl::get()->closeContextMenu();
	}
};
class guictxtmenu_track : public guictxtmenu_base {
public:
	int32_t trackid;
	ctxtmenu_color_select* sel;
	guictxtmenu_track(int32_t _trackid) {
		this->trackid = _trackid;
		this->size.x = 120;
		sel = new ctxtmenu_color_select("Pick Color", 100);
		add(new ctxtmenu_entry("Delete track", 0));
		add(new ctxtmenu_splitter());
		add(sel);
		layout();
	}
	void clicked(int _id) {
		if (_id >= sel->id) {
			_id -= sel->id;
			int32_t col = colorPalette[_id];
			track_t* tr = MainCtrl::get()->getTrackId(trackid);
			if (tr) {
				tr->rgb = col;
			}
		} else if (_id == 0) {
			MainCtrl::get()->removeTrackId(trackid);
		}
		MainCtrl::get()->closeContextMenu();
	}
};
class guictxtmenu_notrack : public guictxtmenu_base {
public:
	guictxtmenu_notrack() {
		this->size.x = 190;
		for (int i = 0; i < NUM_TRACK_TYPES; i++) {
			add(new ctxtmenu_entry(StringFormat("Insert %s Track", TrackTypeToName(i)), i));
		}
		layout();
	}
	void clicked(int _id) {
		MainCtrl::get()->insertNewTrack(-1, _id);
		MainCtrl::get()->closeContextMenu();
	}
};

class guictxtmenu_colorpalette : public guictxtmenu_base {
public:
	guictxtmenu_colorpalette() {
		ctxtmenu_color_select* colorSelect = new ctxtmenu_color_select("Pick Color", 0);
		add(colorSelect);
		layout();
	}
	void clicked(int _id) {
		if (_id >= 100) {
			_id -= 100;
			int32_t col = colorPalette[_id];
			my_printf("col: %08X\n", col);
		}
		MainCtrl::get()->closeContextMenu();
	}
};

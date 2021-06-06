#pragma once
#include <stdint.h>
#include <nanovg_min.h>
#include <vector>
#include "guiglobals.h"
#include "str_util.h"
#define NUM_GUI_COLORS 255
namespace GuiColor {
//int32_t getNextId();
struct constant_t {
	int32_t idx;
	const char* name;
	int32_t defValue;
	constant_t();
	constant_t(const char* _name, int32_t _defValue);
};
std::vector<constant_t> getAllConstants();
constant_t getConstantById(int32_t id);
constant_t getConstantByName(String name);
extern constant_t COL_BASE_BG;
extern constant_t COL_BASE_BG_HOVER;
extern constant_t COL_BASE_BG_PRESSED;
extern constant_t COL_BASE_BG_FOCUSED;
extern constant_t COL_BASE_BG_DISABLED;
extern constant_t COL_BASE_BG_FRAME_BASE;
extern constant_t COL_BASE_BG_FRAME_BRIGHT;
extern constant_t COL_BASE_BG_FRAME_OUTLINE;
extern constant_t COL_BASE_BG_FRAME_HIGHLIGHT;
extern constant_t COL_BASE_BG_STROKE;
extern constant_t COL_WHITE;
extern constant_t COL_BLACK;
extern constant_t COL_LEVEL_IND_GREEN;
extern constant_t COL_LEVEL_IND_GREEN_DRK;
extern constant_t COL_LEVEL_IND_GREEN_DRKER;
extern constant_t COL_LEVEL_IND_YELLOW;
extern constant_t COL_LEVEL_IND_YELLOW_DRK;
extern constant_t COL_LEVEL_IND_YELLOW_DRKER;
extern constant_t COL_GRID_DRK;
extern constant_t COL_GRID_BRT;
extern constant_t COL_LINE_BAR;
extern constant_t COL_LINE_QRT;
extern constant_t COL_LINE_XTH;
extern constant_t COL_BG_DRK;
extern constant_t COL_BG_BRT;
extern constant_t COL_LINE_SEPERATOR;
extern constant_t COL_CTXTMNU_OUTLINE;
extern constant_t COL_CTXTMNU_BG;
extern constant_t COL_CTXTMNU_HILIGHT;
extern constant_t COL_GUI_STROKE;
extern constant_t COL_BG_DRK_FOCUSED;
extern constant_t COL_NODES_EDGE;
extern constant_t COL_NOTE;
extern constant_t COL_NOTE_PLAYING;
extern constant_t COL_NOTE_ARP;
extern constant_t COL_NOTE_MUTE;
extern constant_t COL_NOTE_OUTLINE;
extern constant_t COL_NOTE_TEXT;
extern constant_t COL_BG_SELECTEDTRACK;
extern constant_t COL_BG_SELECTEDTRACK_TITLE;
extern constant_t COL_BG_DRKER;
extern constant_t COL_BG_DRKER2;
extern constant_t COL_GUI_HANDLE;
extern constant_t COL_GUI_HANDLE_FOCUSED;
extern constant_t COL_CLEAR_COLOR;
extern constant_t COL_LABEL_ACTIVE;
extern constant_t COL_LABEL_INACTIVE;
extern constant_t COL_KNOB;
extern constant_t COL_KNOB_IND;
extern constant_t COL_BTN_SOLO_BG_ENABLED;
extern constant_t COL_BTN_SOLO_BG_DISABLED;
extern constant_t COL_AUTOMATED;
extern constant_t COL_PLUG_TITLE;
extern constant_t COL_PLUG_TITLE_SELECTED;
extern constant_t COL_PLUG_TITLE_FOCUSED;
extern constant_t COL_ON;
extern constant_t COL_OFF;
}


#define NVG_KAPPA90 0.5522847493f	// Length proportional to radius of a cubic bezier handle for 90deg arcs.
#define PT1 (r*(1-NVG_KAPPA90))


struct NVGcolor;
#define COLOR_PALETTE_ROWS 4
#define COLOR_PALETTE_COLS 15
#define COLOR_PALETTE_LEN (COLOR_PALETTE_COLS*COLOR_PALETTE_ROWS)
extern uint32_t colorPalette[COLOR_PALETTE_LEN];
extern NVGcolor g_colorPalette[COLOR_PALETTE_LEN];

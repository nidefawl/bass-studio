#pragma once
#include <stdint.h>


#define RENDER_DBG_BRD 0
#define MAX_STR_TITLE 256
#define MAX_STR_STATUSBAR 2048

#define CLAMP(x) (x > 255 ? 255 : (x < 0 ? 0 : x))
#define CMUL(x,y) CLAMP((int)(x*y))
//#define G_S1 66
//#define G_S2 CMUL(G_S1, 1.33)
//#define G_S3 CMUL(G_S1, 1.66)
//#define G_S4 CMUL(G_S1, 2.2)
#define G_R(x) x
#define G_G(x) x
#define G_B(x) x
#define G_RND 2.0
#define G_STROKE 1.0
#define GUI_COLOR_HEX(x) (0xff000000|(x<<16)|(x<<8)|x)
#define GUI_COLOR(x) nvgRGBA(G_R(x), G_G(x), G_B(x), 255)
#define GUI_COLORA(x, a) nvgRGBA(G_R(x), G_G(x), G_B(x), a)
#define GUI_COLORRGB(r, g, b, a) nvgRGBA(G_R(r), G_G(g), G_B(b), a)
#define GUI_COLOR_HEXA(x, a) (((a)<<24)|((x)<<16)|((x)<<8)|(x))

#define INSET_TITLE 4
#define INSET_TRACK_CONTENT 2
#define INSET_CLIP_CONTENT 2
#define INSET_CTR_SPACING 4
//#define HEIGHT_PLUGIN_TITLE 24
#define HEIGHT_DEFAULT_INPUT 30
//#define HEIGHT_TRACK_TITLE (24+INSET_TRACK_CONTENT*2)
#define HEIGHT_CLIP_TITLE 24
#define FONT_SIZE_CTXT 24
#define FONT_SIZE_CTXT_SMALL 18
#define DRAG_RANGE 10
#define TRACK_MIN_HEIGHT 2
#define TRACK_MAX_HEIGHT 128
#define TRACK_MAX_HEIGHT_SUB 12
#define TRACK_MIN_HEIGHT_SUB 1
//#define TRACK_HEIGHT_STEP HEIGHT_TRACK_TITLE
#define TRACK_HEIGHT_SPACING 2
#define TRACK_HEIGHT_SPACING_HALF 1
#define FLG_VISIBLE 1
#define FLG_RENDER_BACKGROUND 2
#define FLG_ENBL 4
#define FLG_HVRD 8
#define FLG_FOC 16
#define FLG_ACT 32
#define FLG_DRG 64
#define CTR_SPACING 8
#define CONTENT_INSET 14

#define G_PLUGIN_TITLE_HEIGHT 0x1000
#define G_TRACK_HEIGHT_STEP 0x1001
#define G_HEIGHT_TRACK_TITLE 0x1002
#define G_WHITE GUI_COLOR(255)
#define G_BLACK GUI_COLOR(0)
#define G_PURPLE_HEX 0xEF62DF
#define G_BLUE2_HEX 0x62EFDF
#define G_PURPLE rgbToNvg(G_PURPLE_HEX)
#define G_BLUE2 rgbToNvg(G_BLUE2_HEX)
#define G_BLUE GUI_COLORRGB(0, 0xdd, 0xff, 255)
#define G_GREEN GUI_COLORRGB(30, 255, 30, 255)
#define G_GREEN_DRK GUI_COLORRGB(10, 160, 10, 255)
#define G_GREEN_DRKER GUI_COLORRGB(5, 120, 5, 255)
#define G_YELLOW GUI_COLORRGB(255, 255, 30, 255)
#define G_YELLOW_DRK GUI_COLORRGB(160, 160, 10, 255)
#define G_YELLOW_DRKER GUI_COLORRGB(120, 120, 5, 255)
#define G_FONT_MIDDLE_OFFSET(x) (x/2.0f+1)
#define G_FONT_SCALE(x) (x)
#define G_MOVE_HIGHLIGHT nvgRGBA(255, 0, 0, 192)
#define G_BUTTON nvgRGBA(128, 192, 64, 255)
#define G_BUTTON_DISABLED GUI_COLOR(80)
#define G_BUTTON_INACTIVE GUI_COLOR(100)
#define G_TITLE_ALIGN NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE
#define G_SELECTION nvgRGBA(2, 2, 2, 96)

#include "str_util.h"
#include <vector>
#define NUM_GUI_COLORS 255
namespace GuiColor {
//int32_t getNextId();
struct constant_t {
	int32_t idx;
	String name;
	int32_t defValue;
	constant_t();
	constant_t(const char* _name, int32_t _defValue);
};
std::vector<constant_t> getAllConstants();
extern constant_t G_WHITE;
extern constant_t G_BLACK;
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
extern constant_t COL_NOTE;
extern constant_t COL_NOTE_PLAYING;
extern constant_t COL_NOTE_ARP;
extern constant_t COL_NOTE_MUTE;
extern constant_t COL_NOTE_OUTLINE;
extern constant_t COL_NOTE_TEXT;
extern constant_t COL_BG_SELECTEDTRACK;
extern constant_t COL_BG_DRKER;
extern constant_t COL_BG_DRKER2;
extern constant_t COL_BG_DRK_SELECTED;
extern constant_t COL_CLEAR_COLOR;
extern constant_t COL_LABEL_ACTIVE;
extern constant_t COL_LABEL_INACTIVE;
}
namespace GuiColor {
extern constant_t COL_KNOB;
extern constant_t COL_KNOB_IND;
extern constant_t COL_AUTOMATED;
}


#define NVG_KAPPA90 0.5522847493f	// Length proportional to radius of a cubic bezier handle for 90deg arcs.
#define PT1 (r*(1-NVG_KAPPA90))


struct NVGcolor;
#define COLOR_PALETTE_ROWS 4
#define COLOR_PALETTE_COLS 15
#define COLOR_PALETTE_LEN (COLOR_PALETTE_COLS*COLOR_PALETTE_ROWS)
extern uint32_t colorPalette[COLOR_PALETTE_LEN];
extern NVGcolor g_colorPalette[COLOR_PALETTE_LEN];
extern NVGcolor g_guiColors[];

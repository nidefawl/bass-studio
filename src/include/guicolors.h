#pragma once
#include <stdint.h>


#define RENDER_DBG_BRD 0
#define MAX_STR_TITLE 256
#define MAX_STR_STATUSBAR 2048

#define CLAMP(x) (x > 255 ? 255 : (x < 0 ? 0 : x))
#define CMUL(x,y) CLAMP((int)(x*y))
#define G_S1 50
#define G_S2 CMUL(G_S1, 1.33)
#define G_S3 CMUL(G_S1, 1.66)
#define G_S4 CMUL(G_S1, 2.2)
#define G_R(x) x
#define G_G(x) x
#define G_B(x) x
#define G_RND 2.0
#define G_STROKE 1.0
#define GUI_COLOR(x) nvgRGBA(G_R(x), G_G(x), G_B(x), 255)
#define GUI_COLORA(x, a) nvgRGBA(G_R(x), G_G(x), G_B(x), a)
#define GUI_COLORRGB(r, g, b, a) nvgRGBA(G_R(r), G_G(g), G_B(b), a)
#define INSET_TITLE 4
#define INSET_TRACK_CONTENT 2
#define INSET_CLIP_CONTENT 2
#define HEIGHT_PLUGIN_TITLE 30
#define HEIGHT_TRACK_TITLE 30
#define HEIGHT_CLIP_TITLE 24
#define FONT_SIZE_CTXT 22
#define G_WHITE GUI_COLOR(255)
#define G_BLACK GUI_COLOR(0)
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

#define COL_GRID_DRK 0
#define COL_GRID_BRT 1
#define COL_LINE_BAR 2
#define COL_LINE_QRT 3
#define COL_LINE_XTH 4
#define COL_BG_DRK 5
#define COL_BG_BRT 6
#define COL_LINE_SEPERATOR 7
#define COL_CTXTMNU_OUTLINE 8
#define COL_CTXTMNU_BG 9
#define COL_CTXTMNU_HILIGHT 10
#define COL_GUI_STROKE 11
#define COL_BG_DRK_FOCUSED 12
#define COL_NOTE 13
#define COL_NOTE_PLAYING 19
#define COL_NOTE_OUTLINE 14
#define COL_NOTE_TEXT 15
#define COL_BG_SELECTEDTRACK 16
#define COL_BG_DRKER 17
#define COL_BG_DRKER2 18

#define CTR_SPACING 8
#define CONTENT_INSET 14

#define NVG_KAPPA90 0.5522847493f	// Length proportional to radius of a cubic bezier handle for 90deg arcs.
#define PT1 (r*(1-NVG_KAPPA90))


struct NVGcolor;
#define COLOR_PALETTE_ROWS 9
#define COLOR_PALETTE_COLS 15
#define COLOR_PALETTE_LEN (COLOR_PALETTE_COLS*COLOR_PALETTE_ROWS)
extern uint32_t colorPalette[COLOR_PALETTE_LEN];
extern NVGcolor g_colorPalette[COLOR_PALETTE_LEN];
extern NVGcolor g_guiColors[];

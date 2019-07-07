#pragma once
#include <stdint.h>
#include <nanovg_min.h>


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
#define FLG_RENDER_BACKGROUND (FLG_VISIBLE<<1)
#define FLG_RENDER_BACKGROUND_INSET (FLG_RENDER_BACKGROUND<<1)
#define FLG_ENBL (FLG_RENDER_BACKGROUND_INSET<<1)
#define FLG_HVRD (FLG_ENBL<<1)
#define FLG_FOC (FLG_HVRD<<1)
#define FLG_ACT (FLG_FOC<<1)
#define FLG_DRG (FLG_ACT<<1)
#define FLG_HAS_COLOR_BG (FLG_DRG<<1)
#define FLG_CANFOCUS (FLG_HAS_COLOR_BG<<1)
#define FLG_RENDER_LABEL (FLG_CANFOCUS<<1)
#define FLG_VERTICAL_LABEL (FLG_RENDER_LABEL<<1)
#define FLG_IMPL_SPEC1 (FLG_VERTICAL_LABEL<<1)
#define FLG_IMPL_SPEC2 (FLG_RENDER_LABEL<<1)
#define CTR_SPACING 8
#define CONTENT_INSET 14

#define G_WHITE (theme->getColor(GuiColor::COL_WHITE))
#define G_BLACK (theme->getColor(GuiColor::COL_BLACK))
//#define G_GREEN GUI_COLORRGB(30, 255, 30, 255)
//#define G_GREEN_DRK GUI_COLORRGB(10, 160, 10, 255)
//#define G_GREEN_DRKER GUI_COLORRGB(5, 120, 5, 255)
#define G_GREEN (theme->getColor(GuiColor::COL_LEVEL_IND_GREEN))
#define G_GREEN_DRK (theme->getColor(GuiColor::COL_LEVEL_IND_GREEN_DRK))
#define G_GREEN_DRKER (theme->getColor(GuiColor::COL_LEVEL_IND_GREEN_DRKER))

#define G_YELLOW (theme->getColor(GuiColor::COL_LEVEL_IND_YELLOW))
#define G_YELLOW_DRK (theme->getColor(GuiColor::COL_LEVEL_IND_YELLOW_DRK))
#define G_YELLOW_DRKER (theme->getColor(GuiColor::COL_LEVEL_IND_YELLOW_DRKER))
#define G_FONT_MIDDLE_OFFSET(x) (x/2.0f+1)
#define G_FONT_SCALE(x) (math::round(x*theme->getFloat(GuiConstant::CONST_FONT_SCALE)))
#define G_MOVE_HIGHLIGHT nvgRGBA(255, 0, 0, 192)

#define G_TITLE_ALIGN NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE
#define G_SELECTION nvgRGBA(2, 2, 2, 96)

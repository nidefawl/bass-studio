#pragma once
#include "types.h"
#include <nanovg_min.h>

#define INSET_TITLE 4
#define INSET_TRACK_CONTENT 2
#define INSET_CLIP_CONTENT 2
#define INSET_CTR_SPACING 4

#define FONT_AUTOSCALE 0.75f

#define HEIGHT_DEFAULT_INPUT 30

#define CONTEXT_MENU_MIN_WIDTH 16
#define APP_MENU_MIN_WIDTH 16
#define FONT_SIZE_CTXT 24
#define FONT_SIZE_CTXT_SMALL 18
#define DRAG_RANGE 6
#define TRACK_MIN_HEIGHT 2
#define TRACK_MAX_HEIGHT 128
#define TRACK_MAX_HEIGHT_SUB 12
#define TRACK_MIN_HEIGHT_SUB 1

#define TRACK_HEIGHT_SPACING 2
#define TRACK_HEIGHT_SPACING_HALF 1

#define SPLITTER_HANDLE_SIZE 6

#define CTR_SPACING 8
#define CONTENT_INSET 14

#define THEMECOL_TEXT (theme->getColor(GuiColor::COL_TEXT))
#define THEMECOL_WHITE (theme->getColor(GuiColor::COL_WHITE))
#define G_BLACK (theme->getColor(GuiColor::COL_BLACK))

#define G_GREEN (theme->getColor(GuiColor::COL_LEVEL_IND_GREEN))
#define G_GREEN_DRK (theme->getColor(GuiColor::COL_LEVEL_IND_GREEN_DRK))
#define G_GREEN_DRKER (theme->getColor(GuiColor::COL_LEVEL_IND_GREEN_DRKER))

#define G_YELLOW (theme->getColor(GuiColor::COL_LEVEL_IND_YELLOW))
#define G_YELLOW_DRK (theme->getColor(GuiColor::COL_LEVEL_IND_YELLOW_DRK))
#define G_YELLOW_DRKER (theme->getColor(GuiColor::COL_LEVEL_IND_YELLOW_DRKER))
#define G_FONT_MIDDLE_OFFSET(x) (x / 2.0f)
#define G_FONT_SCALE(x) ( math::froundf(x * theme->getFloat(GuiConstant::CONST_FONT_SCALE)) )

#define G_TITLE_ALIGN NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE

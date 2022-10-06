#pragma once
#include "types.h"
#include <nanovg_min.h>
#include <vector>
#include "guiglobals.h"
#include "str_util.h"

namespace GuiColor {
    struct constant_t {
        uint32_t idx;
        const char* name;
        uint32_t defValue;
        constant_t() noexcept;
        constant_t(const char* _name, uint32_t _defValue) noexcept;
    };
    std::vector<constant_t> getAllConstants();
    constant_t getConstantById(uint32_t id);
    constant_t getConstantByName(const String& name);

    inline bool operator==(const constant_t& lhs, const constant_t& rhs) { return lhs.idx == rhs.idx; }
    inline bool operator<(const constant_t& lhs, const constant_t& rhs) { return lhs.idx < rhs.idx; }

    extern constant_t COL_AUTOMATED;
    extern constant_t COL_KNOB_HIGHLIGHT;
    extern constant_t COL_KNOB_HIGHLIGHT_BACKGROUND;
    extern constant_t COL_KNOB_MODULATED;
    extern constant_t COL_MODULATION_SATURATED;
    extern constant_t COL_BASE_BG_DISABLED;
    extern constant_t COL_BASE_BG_FOCUSED;
    extern constant_t COL_BASE_BG_FRAME_BASE;
    extern constant_t COL_BASE_BG_FRAME_BRIGHT;
    extern constant_t COL_BASE_BG_FRAME_HIGHLIGHT;
    extern constant_t COL_BASE_BG_FRAME_OUTLINE;
    extern constant_t COL_BASE_BG_HOVER;
    extern constant_t COL_BASE_BG_PRESSED;
    extern constant_t COL_BASE_BG_STROKE;
    extern constant_t COL_BASE_BG;
    extern constant_t COL_BG_BRT;
    extern constant_t COL_BG_DRK_FOCUSED;
    extern constant_t COL_BG_DRK;
    extern constant_t COL_BG_DRKER;
    extern constant_t COL_BG_DRKER2;
    extern constant_t COL_BG_SELECTEDTRACK_TITLE;
    extern constant_t COL_BG_SELECTEDTRACK;
    extern constant_t COL_BG_WIDGET;
    extern constant_t COL_BLACK;
    extern constant_t COL_BTN_BG_BYPASS_ACTIVE;
    extern constant_t COL_BTN_BG_DEFAULT_ACTIVE;
    extern constant_t COL_BTN_BG_DEFAULT_INACTIVE;
    extern constant_t COL_BTN_BG_SHOW_ACTIVE;
    extern constant_t COL_BTN_LOAD_DEF_PLUGINS;
    extern constant_t COL_BTN_RECORD_ARM_BG;
    extern constant_t COL_BTN_SOLO_BG_DISABLED;
    extern constant_t COL_BTN_SOLO_BG_ENABLED;
    extern constant_t COL_BTN_SOLO_BG_PARENT;
    extern constant_t COL_CLEAR_COLOR;
    extern constant_t COL_CLIP_NOTE_MUTED;
    extern constant_t COL_CLIP_NOTE_OVERLAP;
    extern constant_t COL_CLIP_NOTE;
    extern constant_t COL_CLIP_OUTLINE;
    extern constant_t COL_CLIPEDITOR_SHARP;
    extern constant_t COL_CTXTMNU_HILIGHT;
    extern constant_t COL_CTXTMNU_OUTLINE;
    extern constant_t COL_DRAGDROPMOVE_HIGHLIGHT;
    extern constant_t COL_FOLD_BUTTON;
    extern constant_t COL_GRID_BRT;
    extern constant_t COL_GRID_DRK;
    extern constant_t COL_GUI_HANDLE_FOCUSED;
    extern constant_t COL_GUI_HANDLE;
    extern constant_t COL_GUI_STROKE;
    extern constant_t COL_INVALID_INPUT;
    extern constant_t COL_KNOB_BG;
    extern constant_t COL_KNOB_IND;
    extern constant_t COL_KNOB;
    extern constant_t COL_LABEL_ACTIVE;
    extern constant_t COL_LABEL_AUTOMATION_TRACK;
    extern constant_t COL_LABEL_CONTAINER;
    extern constant_t COL_LABEL_INACTIVE;
    extern constant_t COL_LEVEL_IND_GREEN_DRK;
    extern constant_t COL_LEVEL_IND_GREEN_DRKER;
    extern constant_t COL_LEVEL_IND_GREEN;
    extern constant_t COL_LEVEL_IND_YELLOW_DRK;
    extern constant_t COL_LEVEL_IND_YELLOW_DRKER;
    extern constant_t COL_LEVEL_IND_YELLOW;
    extern constant_t COL_LINE_BAR;
    extern constant_t COL_LINE_QRT;
    extern constant_t COL_LINE_SEPERATOR;
    extern constant_t COL_LINE_XTH;
    extern constant_t COL_LOOPHANDLES;
    extern constant_t COL_NODES_EDGE;
    extern constant_t COL_NOTE_ARP;
    extern constant_t COL_NOTE_MOUSE;
    extern constant_t COL_NOTE_MUTE;
    extern constant_t COL_NOTE_OUTLINE;
    extern constant_t COL_NOTE_PLAYING;
    extern constant_t COL_NOTE_REALTIME;
    extern constant_t COL_NOTE_SELECTED;
    extern constant_t COL_NOTE_TEXT;
    extern constant_t COL_NOTE;
    extern constant_t COL_OFF;
    extern constant_t COL_ON;
    extern constant_t COL_PIANOROLL_BLACK;
    extern constant_t COL_PIANOROLL_STROKE;
    extern constant_t COL_PIANOROLL_WHITE;
    extern constant_t COL_PLAYHEAD_OUTLINE;
    extern constant_t COL_PLAYHEAD;
    extern constant_t COL_PLUG_TITLE_FOCUSED;
    extern constant_t COL_PLUG_TITLE_SELECTED;
    extern constant_t COL_PLUG_TITLE;
    extern constant_t COL_PLUGIN_VIEW_FRAME;
    extern constant_t COL_SELECTION_BACKGROUND;
    extern constant_t COL_TEXT;
    extern constant_t COL_TEXTBOX_TEXT_DISABLED;
    extern constant_t COL_TEXTBOX_TEXT_MARKED;
    extern constant_t COL_TEXTBOX_TEXT;
    extern constant_t COL_WHITE;
}// namespace GuiColor


struct NVGcolor;
#define COLOR_PALETTE_ROWS 4
#define COLOR_PALETTE_COLS 15
#define COLOR_PALETTE_LEN (COLOR_PALETTE_COLS * COLOR_PALETTE_ROWS)
extern uint32_t colorPalette[COLOR_PALETTE_LEN];
extern uint32_t* colorOnlyPalette;
extern uint32_t colorOnlyPaletteLen;
extern NVGcolor g_colorPalette[COLOR_PALETTE_LEN];

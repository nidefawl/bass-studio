#pragma once
#include "types.h"
#include <nanovg_min.h>
#include "str_util.h"
#include <vector>

namespace GuiConstant {

    struct constant_t {
        int32_t idx;
        const char* name;
        int32_t defValue;
        int rangeMin = 1;
        int rangeMax = 1000;
        constant_t() noexcept;
        constant_t(const char* _name, int32_t _defValue) noexcept;
        constant_t(const char* _name, int32_t _defValue, int rangeMin, int rangeMax) noexcept;
        constant_t& setMinMax(int rangeMin, int rangeMax) noexcept;
    };

    std::vector<constant_t> getAllConstants();
    constant_t getConstantById(int32_t id);
    constant_t getConstantByName(String name);

    extern constant_t CONST_PLUGIN_TITLE_HEIGHT;
    extern constant_t CONST_TRACK_HEIGHT_STEP;
    extern constant_t CONST_METER_WIDTH;
    extern constant_t CONST_FIXED_TITLE_HEIGHT;
    extern constant_t CONST_FONT_SCALE;
    extern constant_t CONST_NODES_SCALE;
    extern constant_t CONST_FONT_SIZE_CONTEXT_MENU;
    extern constant_t CONST_FONT_SIZE_TABLE;
    extern constant_t CONST_LAYOUT_MARGIN;
    extern constant_t CONST_FONT_SIZE_CTR_LABEL;
    extern constant_t CONST_ROW_HEIGHT;

    extern constant_t CONST_CLIPEDITOR_HANDLES_STROKE_WIDTH;

    extern constant_t CONST_PIANOROLL_STROKE_WIDTH;
    extern constant_t CONST_NOTE_RENDER_MODE;

    extern constant_t CONST_SMALL_LABEL_HEIGHT;

    extern constant_t CONST_GUI_FRAME_STROKE_WIDTH;
    extern constant_t CONST_GUI_INSET_WIDGET_BG;

    extern constant_t CONST_MIXER_WIDTH;
    extern constant_t CONST_TRACK_IO_WIDTH;
    extern constant_t CONST_TRACK_CONTROLS_WIDTH;

}// namespace GuiConstant

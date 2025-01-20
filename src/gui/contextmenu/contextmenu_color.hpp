#pragma once
#include <vector>
#include "event.hpp"
#include <functional>
#include "math/vec.hpp"
#include "gui/gui.hpp"
#include "guicolors.hpp"
#include "contextmenu_base.hpp"
#include "contextmenu.hpp"
#include "basectrl.hpp"

class ctxtmenu_color_select final : public ctxtmenu_entry {
public:
    const int WH      = 16;
    const int ROWS    = COLOR_PALETTE_ROWS;
    const int COLS    = COLOR_PALETTE_COLS;
    const int pad     = 5;
    const int padCell = 3;
    ctxtmenu_color_select(String _title, int _id)
        : ctxtmenu_entry(_title, _id) {
        this->id    = _id;
        this->width = pad * 2 + (WH + padCell) * COLS - padCell;
    }

    void layout(ivec2, float _fontSize, determine_string_width& strw) override {
        this->fontSize = _fontSize;
        auto tableSize = pad * 2 + (WH + padCell) * ROWS - padCell;
        height         = math::roundfS32(_fontSize) + tableSize;
    }

    void render(ivec2, NVGcontext* vg, int idx, ivec2 mouse) override {
        renderTextLabel(vg,
                        vec2(leftOffset(), y + fontSize * 0.5f),
                        vec2(width, height),
                        title,
                        theme,
                        fontSize,
                        theme->getColor(GuiColor::COL_TEXT),
                        NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        int focusIdx = -1;
        int y        = this->y + this->fontSize + pad - 4;
        for (int col = 0; col < COLS; col++) {
            int cX = pad + (WH + padCell) * col;
            for (int row = 0; row < ROWS; row++) {
                int colorIdx = col * ROWS + row;
                int cY  = pad + (WH + padCell) * row;
                if (mouse.y >= y + cY && mouse.y < y + cY + WH && mouse.x >= cX && mouse.x < cX + WH) {
                    focusIdx = colorIdx;
                }
                nvgBeginPath(vg);
                nvgRect(vg, cX, y + cY, WH, WH);
                nvgFillColor(vg, g_colorPalette[colorIdx]);
                nvgFill(vg);
                nvgStrokeColor(vg, THEMECOL_WHITE);
                nvgStrokeWidth(vg, 0.5f);
                nvgStroke(vg);
            }
        }
        if (focusIdx >= 0) {
            int row    = focusIdx % ROWS;
            int col    = focusIdx / ROWS;
            int cX     = pad + (WH + padCell) * col;
            int cY     = pad + (WH + padCell) * row;
            int extent = 4;
            nvgBeginPath(vg);
            nvgRect(vg, cX - extent, y + cY - extent, WH + extent * 2, WH + extent * 2);
            nvgFillColor(vg, g_colorPalette[focusIdx]);
            nvgFill(vg);
            nvgStrokeColor(vg, THEMECOL_WHITE);
            nvgStrokeWidth(vg, 1.0f);
            nvgStroke(vg);
        }
    }

    bool contains(ivec2& ctxtSize, ivec2& mouse) const override {
        return mouse.y >= y && mouse.y < y + height && mouse.x >= 0 && mouse.x < ctxtSize.x;
    }

    int getClicked(ivec2& ctxtSize, ivec2& mouse) override {
        if (contains(ctxtSize, mouse)) {
            int y = this->y + this->fontSize + pad - 4;
            for (int col = 0; col < COLS; col++) {
                int cX = pad + (WH + padCell) * col;
                for (int row = 0; row < ROWS; row++) {
                    int idx = col * ROWS + row;
                    int cY  = pad + (WH + padCell) * row;
                    if (mouse.y >= y + cY && mouse.y < y + cY + WH && mouse.x >= cX && mouse.x < cX + WH) {
                        return this->id + idx;
                    }
                }
            }
        }
        return -1;
    }
};
class guictxtmenu_colorpalette final : public guictxtmenu {
public:
    std::function<void(uint32_t)> callback = nullptr;
    guictxtmenu_colorpalette() {
        auto* colorSelect = new ctxtmenu_color_select("Pick Color", 100);
        addEntry(colorSelect);
    }
    bool clickedElement(ctxtmenu_entry* e, int _id) override {
        if (_id >= 100) {
            _id -= 100;
            uint32_t col = colorPalette[_id];
            if (callback) {
                callback(col);
            }
        }
        return guictxtmenu::clickedElement(e, _id);
    }
};

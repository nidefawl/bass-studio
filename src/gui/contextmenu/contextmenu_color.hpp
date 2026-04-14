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
    int cellSize      = 16;
    int centerOffset  = 0;
    const int ROWS    = COLOR_PALETTE_ROWS;
    const int COLS    = COLOR_PALETTE_COLS;
    const int pad     = 5;
    const int padCell = 3;
    ctxtmenu_color_select(String _title, int _id)
        : ctxtmenu_entry(_title, _id) {
        this->id = _id;
    }

    void layout(ivec2 menuSize, float _fontSize, determine_string_width& strw) override {
        this->fontSize = _fontSize;
        
        // Calculate cell size from available menu width
        int availableWidth = menuSize.x - pad * 2;
        cellSize = (availableWidth - padCell * (COLS - 1)) / COLS;
        cellSize = std::max(cellSize, 4); // Minimum size to ensure visibility
        
        // Calculate dimensions based on dynamic cell size
        this->width = pad * 2 + (cellSize + padCell) * COLS - padCell;
        int tableHeight = pad * 2 + (cellSize + padCell) * ROWS - padCell;
        height = math::roundfS32(_fontSize) + tableHeight;
        
        // Center grid horizontally
        centerOffset = (menuSize.x - this->width) / 2;
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
            int cX = centerOffset + pad + (cellSize + padCell) * col;
            for (int row = 0; row < ROWS; row++) {
                int colorIdx = col * ROWS + row;
                int cY  = pad + (cellSize + padCell) * row;
                if (mouse.y >= y + cY && mouse.y < y + cY + cellSize && mouse.x >= cX && mouse.x < cX + cellSize) {
                    focusIdx = colorIdx;
                }
                nvgBeginPath(vg);
                nvgRect(vg, cX, y + cY, cellSize, cellSize);
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
            int cX     = centerOffset + pad + (cellSize + padCell) * col;
            int cY     = pad + (cellSize + padCell) * row;
            int extent = 4;
            nvgBeginPath(vg);
            nvgRect(vg, cX - extent, y + cY - extent, cellSize + extent * 2, cellSize + extent * 2);
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
                int cX = centerOffset + pad + (cellSize + padCell) * col;
                for (int row = 0; row < ROWS; row++) {
                    int idx = col * ROWS + row;
                    int cY  = pad + (cellSize + padCell) * row;
                    if (mouse.y >= y + cY && mouse.y < y + cY + cellSize && mouse.x >= cX && mouse.x < cX + cellSize) {
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

#pragma once
#include "math/vec.h"
#include "gui/gui.h"

struct dragdrop_target_indicator_t {
    enum drop_type {
        none,
        slot_line_vertical,
        target_line,
        target_area
    };
    drop_type type = none;
    int slotIdx    = -1;
    guibase* dst   = nullptr;
    ivec2 targetPos{ -1, -1 };
    String desc    = "";
    void reset() {
        *this = { none, -1, nullptr, { -1, -1 }, "" };
    }
};

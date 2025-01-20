#pragma once
#include "math/vec.hpp"
#include "gui/gui.hpp"
#include "saferef.hpp"

struct dragdrop_target_indicator_t {
    enum drop_type {
        none,
        slot_line_vertical,
        target_line,
        target_area
    };
    drop_type type = none;
    int slotIdx    = -1;
    SafeRef<guibase> target;
    ivec2 targetPos{ -1, -1 };
    String desc    = "";
    void reset() {
        *this = { none, -1, {}, { -1, -1 }, "" };
    }
    static dragdrop_target_indicator_t TargetArea(guibase* dst) {
        return { target_area, -1, dst ? dst->toRef() : SafeRef<guibase>{}, { -1, -1 }, "" };
    }
};

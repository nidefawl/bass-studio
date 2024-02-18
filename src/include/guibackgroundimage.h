#pragma once
#include "math/vec.h"
#include "str_util.h"
#include "nanovg/nanovg_min.h"

class guictr_base;

struct container_background_image {
    enum position_t : uint8_t {
        left = 0, top = 0, center = 1, middle = 1, right = 2, bottom = 2
    };
    enum layout_t : uint8_t {
        position, fill, contain, cover
    };
    String path;
    layout_t layout = cover;
    position_t verticalPos = middle;
    position_t horizontalPos = center;
    vec2 scale = vec2(1.0f);
    bool scaleAbsolute = false;
    void render(guictr_base* ctr, NVGcontext* vg) const;
};
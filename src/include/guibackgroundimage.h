#pragma once
#include "types.h"
#include "math/vec.h"
#include "str_util.h"
#include <nanovg_min.h>
#include "str_util.h"
#include <vector>

class guictr_base;

struct container_background_image {
    enum position_t : uint8_t {
        left = 0, top = 0, center = 1, middle = 1, right = 2, bottom = 2
    };
    enum layout_t : uint8_t {
        position, fill, contain, cover, repeat
    };
    String path;
    layout_t layout = cover;
    position_t verticalPos = middle;
    position_t horizontalPos = center;
    vec2 scale = vec2(1.0f);
    bool scaleAbsolute = false;
    uint32_t rgba = 0xFFFFFFFF;
    void render(guictr_base* ctr, NVGcontext* vg) const;
};

namespace GuiBackgroundImage {

    struct constant_t {
        uint32_t idx;
        const char* name;
        constant_t() noexcept;
        explicit constant_t(const char* _name) noexcept;
    };

    std::vector<constant_t> getAllConstants();
    constant_t getConstantById(uint32_t id);
    constant_t getConstantByName(const String& name);

    extern constant_t BG_TRACKEDITOR_1;
    extern constant_t BG_TRACKEDITOR_2;
    extern constant_t BG_NOTEEDITOR_1;
    extern constant_t BG_EQUALIZER_1;
    extern constant_t BG_TRACKEDITOR_MIXERS_1;
    extern constant_t BG_TRACK_MIXER_1;
    extern constant_t BG_MIXER_1;
} // namespace GuiBackgroundImage
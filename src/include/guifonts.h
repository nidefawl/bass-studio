#pragma once
#include "types.h"
#include <nanovg_min.h>
#include <vector>
#include "str_util.h"

namespace UIFont {
    struct font_instance {
        String name         = "";
        int fontInstanceIdx = -1;
    };
    struct font_type_t {
        int32_t idx;
        const char* name;
        const char* defValue;
        font_type_t() noexcept;
        font_type_t(const char* _name, const char* _defValue) noexcept;
    };
    std::vector<font_type_t> getAllConstants();
    font_type_t getConstantById(int32_t id);
    font_type_t getConstantByName(const String& name);
    extern const font_type_t FONT_DEFAULT;
    extern const font_type_t FONT_LABEL;
    extern const font_type_t FONT_TEXFIELD;
    extern const font_type_t FONT_CONTEXT_MENU;
    extern const font_type_t FONT_DECIMAL;
    void bindFont(NVGcontext* ctx, UIFont::font_instance font);

    String getFontName(int fontInstanceIdx);
}// namespace UIFont

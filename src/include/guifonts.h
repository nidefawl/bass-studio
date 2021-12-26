#pragma once
#include <stdint.h>
#include <nanovg_min.h>
#include <vector>
#include "guiglobals.h"
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
        font_type_t();
        //	font_type_t(const font_type_t&) = default;
        //	font_type_t& operator=(const font_type_t&) = default;
        font_type_t(const char* _name, const char* _defValue);
    };
    std::vector<font_type_t> getAllConstants();
    font_type_t getConstantById(int32_t id);
    font_type_t getConstantByName(String name);
    extern font_type_t FONT_DEFAULT;
    extern font_type_t FONT_LABEL;
    extern font_type_t FONT_TEXFIELD;
    extern font_type_t FONT_CONTEXT_MENU;
    extern font_type_t FONT_DECIMAL;
    void bindFont(NVGcontext* ctx, UIFont::font_instance font);

    String getFontName(int fontInstanceIdx);
}// namespace UIFont

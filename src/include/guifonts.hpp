#pragma once
#include "types.hpp"
#include <nanovg_min.h>
#include <vector>
#include "str_util.hpp"

struct guitheme_t;

namespace UIFont {
    struct font_instance {
        String name = "";
        int fontInstanceIdx = -1;
    };
    struct font_type_t {
        uint32_t idx;
        const char* name;
        const char* defValue;
        font_type_t() noexcept;
        font_type_t(const char* _name, const char* _defValue) noexcept;
    };
    std::vector<font_type_t> getAllConstants();
    font_type_t getConstantById(uint32_t id);
    font_type_t getConstantByName(const String& name);
    extern const font_type_t FONT_DEFAULT;
    extern const font_type_t FONT_LABEL;
    extern const font_type_t FONT_TEXTFIELD;
    extern const font_type_t FONT_CONTEXT_MENU;
    extern const font_type_t FONT_DECIMAL;
    extern const font_type_t FONT_TEST;
    String getFontName(int fontInstanceIdx);
}// namespace UIFont

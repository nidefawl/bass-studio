#pragma once
#include <memory>
#include <vector>
#include <nanovg_min.h>
#include <unordered_map>
#include "guicolors.h"
#include "guiconstant.h"
#include "guifonts.h"
#include "str_util.h"
#include "guibackgroundimage.h"

NVGcolor rgbaToNvg(uint32_t color);

struct guitheme_t {
    String name;
    bool isDefault = false;
    std::vector<NVGcolor> vecNVGColors;
    std::unordered_map<uint32_t, uint32_t> mapColors;
    std::unordered_map<uint32_t, int32_t> mapProperties;
    std::unordered_map<uint32_t, UIFont::font_instance> mapFonts;
    std::unordered_map<uint32_t, container_background_image> mapBackgrounds;
    struct guitheme_override_state_t;
    std::shared_ptr<guitheme_override_state_t> pOverrideState;
    guitheme_t();
    guitheme_t(const guitheme_t&)     = default;
    ~guitheme_t()                     = default;
    guitheme_t(guitheme_t&&) noexcept = default;
    guitheme_t& operator=(const guitheme_t&) = default;
    guitheme_t& operator=(guitheme_t&&) noexcept = default;

    void initTheme();
    NVGcolor getBgColor(int32_t flags) const;
    NVGcolor getBgStrokeColor(int32_t flags) const;
    NVGcolor getFrameColorOutline() const;
    NVGcolor getFrameColorBase() const;
    NVGcolor getFrameColorHighlight() const;
    NVGcolor getFrameColorBright() const;
    NVGcolor& getColorRef(GuiColor::constant_t _constant);
    NVGcolor getColor(GuiColor::constant_t _constant) const;
    NVGcolor getContrastColor(GuiColor::constant_t _constant) const;
    uint32_t getColorInt32(GuiColor::constant_t _constant);
    UIFont::font_instance getFont(UIFont::font_type_t _fonttype) const;
    const container_background_image* getBackgroundImage(GuiBackgroundImage::constant_t _constant) const;
    void setBackgroundImage(GuiBackgroundImage::constant_t _constant, const container_background_image& s);
    void clearBackgroundImage(GuiBackgroundImage::constant_t _constant);
    void bindFonts();
    UIFont::font_instance setFont(UIFont::font_type_t _fonttype, String s);
    void setColor(GuiColor::constant_t _constant, uint32_t _value);
    void setColorsFrom(const guitheme_t& _otherTheme);
    int32_t get(GuiConstant::constant_t _constant) const;
    float getFloat(GuiConstant::constant_t _constant) const;
    void set(GuiConstant::constant_t _constant, int32_t _value);
    void updateAnimation();
    void pingConstant(GuiColor::constant_t _constant);
    void pingConstant(GuiConstant::constant_t);
    void endPing();
    /* experimental */
    void setThemeBaseColor(const NVGcolor& col, vec3 hueSatBrMixIntensity);
};

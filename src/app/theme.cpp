#include <algorithm>
#include <nanovg.h>
#include <vector>
#include "theme.hpp"
#include "logging.hpp"
#include "math/vec.hpp"
#include "math/seq_math.hpp"
#include "color_util.hpp"
#include "guicolors.hpp"
#include "guiconstant.hpp"
#include "renderresources.hpp"
#include "gui/gui.hpp"
#include "assert_dbg.h"
#include <glm/vec3.hpp>
#include <glm/gtx/color_space.hpp>

bool nvgColorEqual(NVGcolor a, NVGcolor b) {
#define F_EPS 0.000001f
    if (fabs(a.r - b.r) > F_EPS) return false;
    if (fabs(a.g - b.g) > F_EPS) return false;
    if (fabs(a.b - b.b) > F_EPS) return false;
    if (fabs(a.a - b.a) > F_EPS) return false;
    return true;
}


struct guitheme_t::guitheme_override_state_t {
    int32_t overrideState = 0;
    int32_t animationTime = 0;
    GuiColor::constant_t _overrideColorConstant;
    int32_t _overrideColorValue = 0;
    GuiConstant::constant_t _overrideConstantConstant;
    int32_t _overrideConstantValue = 0;
    void updateAnimation() {
        if (this->overrideState) {
            this->animationTime++;
            if (this->animationTime > 15) {
                this->overrideState ^= 1 << 4;
                this->animationTime = 0;
            }
        }
    }
    void pingConstant(GuiColor::constant_t _constant, int32_t colorContrastPing) {
        overrideState          = 1;
        _overrideColorConstant = _constant;
        _overrideColorValue    = colorContrastPing;
    }
    void endPing() { overrideState = 0; }
    void pingConstant(GuiConstant::constant_t _constant) {
        overrideState             = 2;
        _overrideConstantConstant = _constant;
        _overrideConstantValue    = 5;
    }
};
guitheme_t::guitheme_t() {
    this->pOverrideState = std::make_shared<guitheme_t::guitheme_override_state_t>();
    initTheme();
}

void guitheme_t::initTheme() {
    vecNVGColors.clear();
    mapColors.clear();
    mapProperties.clear();
    mapFonts.clear();
    uint32_t maxIdx = 0;

    std::vector<GuiColor::constant_t> v = GuiColor::getAllConstants();
    for (auto c : v) {
        mapColors[c.idx]    = c.defValue;
        maxIdx = math::max(maxIdx, c.idx);
    }
    dbgassert(maxIdx == mapColors.size());
    dbgassert(maxIdx < 300);
    vecNVGColors.resize(maxIdx+1);
    for (auto c : v) {
        vecNVGColors[c.idx] = rgbaToNvg(c.defValue);
    }

    std::vector<GuiConstant::constant_t> v2 = GuiConstant::getAllConstants();
    for (auto c : v2) {
        mapProperties[c.idx] = c.defValue;
    }

    auto v3 = UIFont::getAllConstants();
    for (auto c : v3) {
        mapFonts[c.idx] = UIFont::font_instance{c.defValue};
    }
}

NVGcolor& guitheme_t::getColorRef(GuiColor::constant_t _constant) {
    dbgassert(_constant.idx < this->vecNVGColors.size());
#ifndef NDEBUG
    // Make sure the 2 are in sync
    auto it = mapColors.find(_constant.idx);
    if (it != mapColors.end()) {
        dbgassert(nvgColorEqual(this->vecNVGColors[_constant.idx], rgbaToNvg(it->second)));
    }
#endif
    return this->vecNVGColors[_constant.idx];
}
NVGcolor guitheme_t::getColor(GuiColor::constant_t _constant) const {
    dbgassert(_constant.idx < this->vecNVGColors.size());
    if (pOverrideState->overrideState == 1 && pOverrideState->_overrideColorConstant.idx == _constant.idx) {
        return rgbaToNvg(this->pOverrideState->_overrideColorValue);
    }
#ifndef NDEBUG
    // Make sure the 2 are in sync
    auto it = mapColors.find(_constant.idx);
    if (it != mapColors.end()) {
        dbgassert(nvgColorEqual(this->vecNVGColors[_constant.idx], rgbaToNvg(it->second)));
    }
#endif
    return this->vecNVGColors[_constant.idx];
}
NVGcolor guitheme_t::getContrastColor(GuiColor::constant_t _constant) const {
    if (pOverrideState->overrideState == 1 && pOverrideState->_overrideColorConstant.idx == _constant.idx) {
        return getContrastFontColor(this->pOverrideState->_overrideColorValue);
    }
    dbgassert(_constant.idx < this->vecNVGColors.size());
    return getContrastFontColor(nvgToRGBA(this->vecNVGColors[_constant.idx]));
}
uint32_t guitheme_t::getColorInt32(GuiColor::constant_t _constant) {
    if (pOverrideState->overrideState == 1 && pOverrideState->_overrideColorConstant.idx == _constant.idx) {
        return this->pOverrideState->_overrideColorValue;
    }
    auto it = mapColors.find(_constant.idx);
    if (it == mapColors.end()) {
        // Make sure the 2 are in sync
        dbgassert(nvgColorEqual(this->vecNVGColors[_constant.idx], rgbaToNvg(_constant.defValue)));
        return _constant.defValue;
    }
    // Make sure the 2 are in sync
    dbgassert(nvgColorEqual(this->vecNVGColors[_constant.idx], rgbaToNvg(mapColors[_constant.idx])));
    return mapColors[_constant.idx];
}
void guitheme_t::setColor(GuiColor::constant_t _constant, uint32_t _value) {
    if (isDefault) return;
    dbgassert(_constant.idx < this->vecNVGColors.size());
    mapColors[_constant.idx]          = _value;
    this->vecNVGColors[_constant.idx] = rgbaToNvg(_value);
}

float guitheme_t::getFloat(GuiConstant::constant_t _constant) const {
    return get(_constant) / _constant.floatScale;
}
int32_t guitheme_t::get(GuiConstant::constant_t _constant) const {
    auto it = mapProperties.find(_constant.idx);
    if (it == mapProperties.end()) {
        return _constant.defValue;
    }
    const int32_t val = mapProperties.at(_constant.idx);
    dbgassert(val >= 0 && val <= 10000);
    return val;
}
UIFont::font_instance guitheme_t::getFont(UIFont::font_type_t _fonttype) const {
    auto it = mapFonts.find(_fonttype.idx);
    if (it == mapFonts.end()) {
        return UIFont::font_instance{_fonttype.defValue};
    }
    return mapFonts.at(_fonttype.idx);

}
void guitheme_t::bindFont(NVGcontext* ctx, UIFont::font_type_t _fonttype) const {
    UIFont::font_instance& font = mapFonts.at(_fonttype.idx);
    RenderResources::NvgFonts& fonts = RenderResources::perContextFonts[ctx];
    if (font.fontInstanceIdx == -1) {
        font.fontInstanceIdx = -2;
        int i                = 0;
        for (auto& f : fonts.fontsLoaded) {
            if (f.name == font.name) {
                font.fontInstanceIdx = i;
                break;
            }
            i++;
        }
    }
    if (font.fontInstanceIdx >= 0) {
        auto& bindFont = fonts.fontsLoaded[font.fontInstanceIdx];
        if (bindFont.nvgId < 0) {
            auto& fontsInstalled = RenderResources::fontsInstalled;
            auto itFont = std::find_if(fontsInstalled.begin(), fontsInstalled.end(), [&font](const auto& f) {
                return f.name == font.name;
            });
            if (itFont == fontsInstalled.end()) {
                log_lf(Log::L_WARN, "Font '%s' not found\n", StringAsCStr(font.name));
                // try binding second font if loaded
                if (fonts.fontsLoaded.size() > 1) {
                    bindFont.nvgId = fonts.fontsLoaded[1].nvgId;
                }
                nvgFontFaceId(ctx, bindFont.nvgId);
                return;
            }
            const String& fontPath = itFont->path;
            if (itFont->isEmbedded) {
                auto it = RenderResources::resources.find(fontPath);
                if (it != RenderResources::resources.end()) {
                    bindFont.nvgId = nvgCreateFontMem(ctx, StringAsCStr(font.name), it->second.data(), it->second.size(), 0);
                }
            }
            if (bindFont.nvgId < 0) {
                bindFont.nvgId = nvgCreateFont(ctx, StringAsCStr(font.name), StringAsCStr(fontPath));
            }
            if (bindFont.nvgId < 0) {
                log_lf(Log::L_WARN, "Failed to load font '%s' from '%s'\n", StringAsCStr(font.name), StringAsCStr(fontPath));
                // try binding second font if loaded
                if (fonts.fontsLoaded.size() > 1) {
                    bindFont.nvgId = fonts.fontsLoaded[1].nvgId;
                } else {
                    font.fontInstanceIdx = -2;
                }
                nvgFontFaceId(ctx, bindFont.nvgId);
                return;
            }
            if (RenderResources::emojiFont.nvgId >= 0) {
                nvgAddFallbackFontId(ctx, bindFont.nvgId, RenderResources::emojiFont.nvgId);
            }
        }
        nvgFontFaceId(ctx, bindFont.nvgId);
    }
}
void guitheme_t::bindFonts() {
    for (auto& mapFont : mapFonts) {
        UIFont::font_type_t c = UIFont::getConstantById(mapFont.first);
        if (c.idx == 0) continue;
        mapFont.second.fontInstanceIdx = -1;
    }
}
UIFont::font_instance guitheme_t::setFont(UIFont::font_type_t _fonttype, String s) {
    mapFonts[_fonttype.idx] = UIFont::font_instance{s};
    return mapFonts[_fonttype.idx];
}
void guitheme_t::set(GuiConstant::constant_t _constant, int32_t _value) {
    mapProperties[_constant.idx] = _value;
}

NVGcolor guitheme_t::getBgColor(int32_t flags) const {
    if (!(flags & FLG_ENBL)) {
        return getColor(GuiColor::COL_BASE_BG_DISABLED);
    }
    if (flags & FLG_DRG) {
        return getColor(GuiColor::COL_BASE_BG_PRESSED);
    }
    return getColor(GuiColor::COL_BASE_BG);
}

NVGcolor guitheme_t::getBgStrokeColor(int32_t flags) const {
    if (flags & FLG_FOC) {
        return getColor(GuiColor::COL_BASE_BG_FOCUSED);
    }
    if (flags & FLG_HVRD) {
        return getColor(GuiColor::COL_BASE_BG_HOVER);
    }
    return getColor(GuiColor::COL_BASE_BG_STROKE);
}

NVGcolor guitheme_t::getFrameColorOutline() const {
    return getColor(GuiColor::COL_BASE_BG_FRAME_OUTLINE);
}
NVGcolor guitheme_t::getFrameColorBase() const {
    return getColor(GuiColor::COL_BASE_BG_FRAME_BASE);
}
NVGcolor guitheme_t::getFrameColorHighlight() const {
    return getColor(GuiColor::COL_BASE_BG_FRAME_HIGHLIGHT);
}
NVGcolor guitheme_t::getFrameColorBright() const {
    return getColor(GuiColor::COL_BASE_BG_FRAME_BRIGHT);
}

void guitheme_t::updateAnimation() {
    this->pOverrideState->updateAnimation();
}
void guitheme_t::pingConstant(GuiColor::constant_t _constant) {
    int32_t colorContrastPing = 0;
    if (this->mapColors.count(_constant.idx)) {
        colorContrastPing = 0xFF000000 | getContrastFontColoru32(this->mapColors.at(_constant.idx));
    }
    this->pOverrideState->pingConstant(_constant, colorContrastPing);
}
void guitheme_t::pingConstant(GuiConstant::constant_t _constant) {
    this->pOverrideState->pingConstant(_constant);
}
void guitheme_t::endPing() {
    this->pOverrideState->endPing();
}
void guitheme_t::setThemeBaseColor(const NVGcolor& col, vec3 hueSatBrMixIntensity) {
    if (isDefault) return;
    static const std::array colorConstants = {
        GuiColor::COL_AUTOMATED,
        GuiColor::COL_AUTOMATED_INACTIVE,
        GuiColor::COL_BASE_BG,
        GuiColor::COL_BASE_BG_FOCUSED,
        GuiColor::COL_BG_BRT,
        GuiColor::COL_BG_DRK,
        GuiColor::COL_BG_DRKER,
        GuiColor::COL_BG_SELECTEDTRACK,
        GuiColor::COL_BG_SELECTEDTRACK_TITLE,
        GuiColor::COL_BTN_BG_SHOW_ACTIVE,
        GuiColor::COL_CLEAR_COLOR,
        GuiColor::COL_CLIP_NOTE,
        GuiColor::COL_FOLD_BUTTON,
        GuiColor::COL_GRID_BRT,
        GuiColor::COL_GRID_DRK,
        GuiColor::COL_GUI_HANDLE,
        GuiColor::COL_KNOB,
        GuiColor::COL_KNOB_HIGHLIGHT,
        GuiColor::COL_KNOB_HIGHLIGHT_BACKGROUND,
        GuiColor::COL_KNOB_IND,
        GuiColor::COL_OFF,
        GuiColor::COL_PLUG_TITLE,
        GuiColor::COL_PLUG_TITLE_FOCUSED,
        GuiColor::COL_PLUG_TITLE_SELECTED,
        GuiColor::COL_SELECTION_BACKGROUND,
        GuiColor::COL_SHAPE_CURVE,
    };
    auto hsvToSet = glm::hsvColor(glm::vec3(col.r, col.g, col.b));
    for (auto c : colorConstants) {
        auto& col = mapColors[c.idx];
        auto vec = colorHex(col);
        auto hsv = glm::hsvColor(vec3(vec.r, vec.g, vec.b));
        auto newSat = hsv.g * (1.0f + (hsvToSet.g - 0.5f));
        auto newBr  = hsv.b * 1.0f + (hsvToSet.b - 0.5f);
        hsv.r = hsv.r + (hsvToSet.r - hsv.r) * hueSatBrMixIntensity.r;
        hsv.g = hsv.g + (newSat - hsv.g) * hueSatBrMixIntensity.g;
        hsv.b = hsv.b + (newBr - hsv.b) * hueSatBrMixIntensity.b;
        hsv.g = math::clamp(hsv.g, 0.0f, 1.0f);
        hsv.b = math::clamp(hsv.b, 0.0f, 1.0f);
        auto rgb = glm::rgbColor(hsv);
        // copy over alpha 
        setColor(c, vec4ToRgbU32({rgb.r, rgb.g, rgb.b, vec.a}));
    }
}
void guitheme_t::setColorsFrom(const guitheme_t& _otherTheme) {
    for (auto& it : _otherTheme.mapColors) {
        setColor(GuiColor::getConstantById(it.first), it.second);
    }
}


const container_background_image* guitheme_t::getBackgroundImage(GuiBackgroundImage::constant_t _constant) const {
    auto it = mapBackgrounds.find(_constant.idx);
    if (it == mapBackgrounds.end()) {
        return nullptr;
    }
    return &it->second;
}

void guitheme_t::clearBackgroundImage(GuiBackgroundImage::constant_t _constant) {
    auto it = mapBackgrounds.find(_constant.idx);
    if (it != mapBackgrounds.end()) {
        mapBackgrounds.erase(it);
    }
}

void guitheme_t::setBackgroundImage(GuiBackgroundImage::constant_t _constant, const container_background_image& s) {
    mapBackgrounds[_constant.idx] = s;
}

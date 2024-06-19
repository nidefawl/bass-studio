#include <algorithm>
#include <nanovg.h>
#include <nanovg_min.h>
#include <typeinfo>
#include <utility>
#include "appconfig.h"
#include "assert_dbg.h"
#include "gui/tooltip/tooltip.h"
#include "guiglobals.h"
#include "math/vec.h"
#include "math/seq_math.h"
#include "color_util.h"
#include "basectrl.h"
#include "gui.h"
#include "gui/controls/button.h"
#include "platform.h"
#include "theme.h"
#include "saferef.h"
#include "seq_util.h"
#include "guicolors.h"
#include "guiconstant.h"
#include "gui/properties/properties_table.h"
#include "gui/contextmenu/contextmenu_base.h"
#include "renderresources.h"
#include "thread.h"
#include "util/debug_alloc.h"
#include "guifonts.h"
#include "host/daw/mainctrl.h"

NVGcolor g_colorPalette[COLOR_PALETTE_LEN];


const NVGcolor dbgcolorsArray[8] = {
        nvgRGBA(255, 0, 0, 55),
        nvgRGBA(0, 255, 0, 55),
        nvgRGBA(0, 0, 255, 55),
        nvgRGBA(255, 127, 0, 55),
        nvgRGBA(0, 255, 127, 55),
        nvgRGBA(127, 0, 255, 55),
        nvgRGBA(255, 127, 255, 55),
        nvgRGBA(255, 255, 0, 55)
};

void initColor() {
    const int ROWS = COLOR_PALETTE_ROWS;
    const int COLS = COLOR_PALETTE_COLS;

    int greyCols = 2;
    for (int col = 0; col < COLS; col++) {
        for (int row = 0; row < ROWS; row++) {
            int idx = col * ROWS + row;
            switch (col) {
                case 0:
                    g_colorPalette[idx] = nvgHSL(0, 0, math::clamp(row * 0.4f / (float) (ROWS - 1), 0.0f, 1.0f));
                    break;
                case 1:
                    if (row == ROWS - 1) {
                        g_colorPalette[idx] = rgbaToNvg(0xffffffff);
                    } else {
                        g_colorPalette[idx] = nvgHSL(0, 0, math::clamp(0.4f + (row + 1) * 0.2f / (float) (ROWS - 1), 0.0f, 1.0f));
                    }

                    break;
                default:
                    g_colorPalette[idx] =
                            nvgHSL((col - greyCols) / ((float) COLS - greyCols) + row * 0.002f,
                                   row == 0   ? 0.85f
                                   : row == 1 ? 0.65f
                                   : row == 2 ? 0.65f
                                              : 0.8f,
                                   row == 0   ? 0.15f
                                   : row == 1 ? 0.3f
                                   : row == 2 ? 0.5f
                                              : 0.65f);
                    break;
            }
            colorPalette[idx] = nvgToRGBA(g_colorPalette[idx]);
        }
    }

}

void UTIL_setFont(NVGcontext* vg, const guitheme_t* const theme, float size, NVGcolor color, int alignment) {
    nvgFontSize(vg, size);
    UIFont::font_instance instance = theme->getFont(UIFont::FONT_DEFAULT);
    UIFont::bindFont(vg, instance);
    nvgFillColor(vg, color);
    nvgTextAlign(vg, alignment);
}
float textWidth(NVGcontext* vg, const String& str) {
    float bounds[4]{ 0 };
    nvgTextBounds(vg, 0, 0, StringAsCStr(str), nullptr, bounds);
    return bounds[2] - bounds[0];// maxX - minX;
}

float determine_string_width::getStringWidth(const String& text, float fontSize, int alignment) {
    NVGcontext* vg = ctrl->vg;
    float fontSizeScaled = fontSize;
    if (theme) {
        UIFont::font_instance instance = theme->getFont(UIFont::FONT_DEFAULT);
        UIFont::bindFont(vg, instance);
        fontSizeScaled = fontSize * theme->getFloat(GuiConstant::CONST_FONT_SCALE);
    }
    nvgFontSize(vg, fontSizeScaled);
    if (alignment) {
        nvgTextAlign(vg, alignment);
    }
    float bounds[4]{ 0 };
    nvgTextBounds(vg, 0, 0, StringAsCStr(text), nullptr, bounds);
    return bounds[2] - bounds[0];// maxX - minX;
}

float determine_table_string_width::getStringWidth(const String& text) {
    NVGcontext* vg = ctrl->vg;
    float fontSizeScaled = fontSize;
    if (theme) {
        UIFont::font_instance instance = theme->getFont(UIFont::FONT_DEFAULT);
        UIFont::bindFont(vg, instance);
        fontSizeScaled = fontSize * theme->getFloat(GuiConstant::CONST_FONT_SCALE);
    }
    nvgFontSize(vg, fontSizeScaled);
    if (textAlignment) {
        nvgTextAlign(vg, textAlignment);
    }
    float bounds[4]{ 0 };
    nvgTextBounds(vg, 0, 0, StringAsCStr(text), nullptr, bounds);
    return bounds[2] - bounds[0] + INSET_TABLE_CELL_PADDING*2;// maxX - minX;
}
vec2 getTextLabelBounds(NVGcontext* vg,
                     const vec2& pos,
                     const String& text,
                     const guitheme_t* theme,
                     const float fontSize,
                     const int32_t alignment) {

    float fontSizeScaled = fontSize;
    if (theme) {
        UIFont::font_instance instance = theme->getFont(UIFont::FONT_DEFAULT);
        UIFont::bindFont(vg, instance);
        fontSizeScaled = fontSize * theme->getFloat(GuiConstant::CONST_FONT_SCALE);
    }
    nvgFontSize(vg, fontSizeScaled);
    nvgTextAlign(vg, alignment);
    float bounds[4]{ 0 };
    if (!text.empty()) {
        nvgTextBounds(vg, pos.x, pos.y, text.c_str(), &text.back() + 1, bounds);
    }
    return {bounds[2] - bounds[0], bounds[3] - bounds[1]};
}
float renderTextLabel(NVGcontext* vg,
                     const vec2& pos,
                     const vec2& bounds,
                     const String& text,
                     const guitheme_t* theme,
                     const float fontSize,
                     const NVGcolor color,
                     const int32_t alignment) {

    float fontSizeScaled = fontSize;
    if (theme) {
        UIFont::font_instance instance = theme->getFont(UIFont::FONT_DEFAULT);
        UIFont::bindFont(vg, instance);
        fontSizeScaled = fontSize * theme->getFloat(GuiConstant::CONST_FONT_SCALE);
    }
#if 0
    auto col = getContrastFontColorNvg(color);
    col.a *= 0.3f;
    nvgBeginPath(vg);
    vec2 offsetPos = pos;
    if (alignment&NVG_ALIGN_CENTER) {
        offsetPos.x -= bounds.x * 0.5f;
    }
    if (alignment&NVG_ALIGN_RIGHT) {
        offsetPos.x -= bounds.x * 1.0f;
    }
    if (alignment&NVG_ALIGN_MIDDLE) {
        offsetPos.y -= bounds.y * 0.5f;
    }
    if (alignment&NVG_ALIGN_BOTTOM) {
        offsetPos.y -= bounds.y * 1.0f;
    }
    nvgRect(vg, offsetPos.x, offsetPos.y, bounds.x, bounds.y);
    nvgFillColor(vg, col);
    nvgFill(vg);
#endif
    nvgTranslateZ(vg, -2.0f);
    nvgFontSize(vg, fontSizeScaled);
    nvgFillColor(vg, color);
    nvgTextAlign(vg, alignment);
    float f = 0.0f;
    if (!text.empty()) {
        f = nvgTextW(vg, pos.x, pos.y, bounds.x, text.c_str(), &text.back() + 1);
    }
    nvgTranslateZ(vg, 2.0f);
    return f;
}
float guibase::renderText(NVGcontext* vg,
                const vec2& pos,
                const vec2& bounds,
                const String& text,
                const float fontSize,
                const int32_t alignment) {
    const GuiColor::constant_t c = (this->flags & FLG_ENBL) ? GuiColor::COL_TEXT : GuiColor::COL_LABEL_INACTIVE;
    const auto fontSizeScaled = math::clamp<float>(fontSize > 4 ? fontSize : size.y, 4, 48);
    const int32_t align = alignment == 0 ? NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE : alignment;
    return renderTextLabel(vg, pos, bounds, text, theme, fontSizeScaled, theme->getColor(c), align);
}

void getNvgMultiLineTextBounds(NVGcontext* vg, const String& s, float maxLineWidth, float lineh, float* bounds) {
    NVGtextRow rows[16]{};
    int nrows;
    float textBoundsX = 0;
    float textBoundsY = 0;
    if (!s.empty()) {
        if ((nrows = nvgTextBreakLines(vg, s.c_str(), &s.back() + 1, maxLineWidth, rows, 16))) {
            for (int i = 0; i < nrows; i++) {
                NVGtextRow* row = &rows[i];
                textBoundsX     = math::max(textBoundsX, row->width);
                textBoundsY += lineh;
            }
        }
    }
    bounds[0] = textBoundsX;
    bounds[1] = textBoundsY;
};
float renderCenteredMultilineText(NVGcontext* vg, const guitheme_t* const theme, const String& str, float fontSize, GuiColor::constant_t c, ivec2 pos, ivec2 bounds) {

    float fontSizeScaled = fontSize;
    if (theme) {
        UIFont::font_instance instance = theme->getFont(UIFont::FONT_DEFAULT);
        UIFont::bindFont(vg, instance);
        fontSizeScaled = fontSize * theme->getFloat(GuiConstant::CONST_FONT_SCALE);
    }
    const NVGcolor color = theme->getColor(c);
    const auto alignment = NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE;

#if 0
    auto col = getContrastFontColorNvg(color);
    col.a *= 0.3f;
    col.r = 1.0f;
    nvgBeginPath(vg);
    nvgRect(vg, pos.x, pos.y, bounds.x, bounds.y);
    nvgFillColor(vg, col);
    nvgFill(vg);
#endif

    nvgFontSize(vg, fontSizeScaled);
    nvgFillColor(vg, color);
    nvgTextAlign(vg, alignment);

    float lineh = 0;
    nvgTextMetrics(vg, nullptr, nullptr, &lineh);


    vec2 vPos = pos;
    float textBounds[2];
    getNvgMultiLineTextBounds(vg, str, bounds.x, lineh, textBounds);
    if (textBounds[1] > bounds.y && fontSizeScaled > 6) {
        nvgFontSize(vg, fontSizeScaled - 6);
        getNvgMultiLineTextBounds(vg, str, bounds.x, lineh, textBounds);
    }
    vPos.y += bounds.y / 2.0f - (textBounds[1] / 2.0f);
    vPos.y += lineh / 2.0f;
    if (!str.empty()) {
        nvgTextBox(vg, vPos.x, vPos.y, bounds.x, str.c_str(), &str.back() + 1);
    }
    return textBounds[0];
}

void renderFrame(NVGcontext* vg, ivec2 posA, ivec2 posB) {
    float x = math::min(posA.x, posB.x);
    float y = math::min(posA.y, posB.y);
    float w = math::max(posA.x - posB.x, posB.x - posA.x);
    float h = math::max(posA.y - posB.y, posB.y - posA.y);
    renderDashedLineFrame(vg, x, y, w, h, 2.0f);
}

void renderGridLines(NVGcontext* vg, const guitheme_t* theme, const std::vector<grid_div>& gridList, const ivec2& size) {
    for (auto& g : gridList) {
        nvgBeginPath(vg);
        nvgMoveTo(vg, g.screenpos, 0);
        nvgLineTo(vg, g.screenpos, size.y);
        NVGcolor col;
        switch (g.color) {
            case 0:
                col = theme->getColor(GuiColor::COL_LINE_BAR);
                break;
            case 1:
                col = theme->getColor(GuiColor::COL_LINE_QRT);
                break;
            case 2:
            default:
                col = theme->getColor(GuiColor::COL_LINE_XTH);
                break;
        }
        nvgStrokeColor(vg, col);
        nvgStrokeWidth(vg, g.thickness);
        nvgStroke(vg);
    }
}

void renderDashedLineFrame(NVGcontext* vg, float x, float y, float w, float h, float thickness) {
    RenderResources::NvgImageTexture& image = RenderResources::imgDashedLine;

    float t = (getTimeMillis() % 2000LL) / 2000.0f;

    uint32_t texOffsetX = t * image.width;
    uint32_t texOffsetY = t * image.height;
    int32_t imageId     = image.perContextId[vg];
    NVGpaint paintDown  = nvgImagePattern(vg, 0, texOffsetY, image.width, image.height, 0, imageId, 1.0f);
    NVGpaint paintRight = nvgImagePattern(vg, texOffsetX, 0, image.width, image.height, (float) (M_PI * 0.5f), imageId, 1.0f);
    NVGpaint paintUp    = nvgImagePattern(vg, 0, image.height - texOffsetY, image.width, image.height, 0, imageId, 1.0f);
    NVGpaint paintLeft  = nvgImagePattern(vg, image.width - texOffsetX, 0, image.width, image.height, (float) (M_PI * 0.5f), imageId, 1.0f);

    nvgShapeAntiAlias(vg, 0);
    nvgStrokeWidth(vg, thickness);
    nvgBeginPath(vg);
    nvgMoveTo(vg, x, y);
    nvgLineTo(vg, x, y + h);
    nvgStrokePaint(vg, paintUp);
    nvgStroke(vg);
    nvgBeginPath(vg);
    nvgMoveTo(vg, x + w, y);
    nvgLineTo(vg, x + w, y + h);
    nvgStrokePaint(vg, paintDown);
    nvgStroke(vg);
    nvgBeginPath(vg);
    nvgMoveTo(vg, x, y);
    nvgLineTo(vg, x + w, y);
    nvgStrokePaint(vg, paintRight);
    nvgStroke(vg);
    nvgBeginPath(vg);
    nvgMoveTo(vg, x, y + h);
    nvgLineTo(vg, x + w, y + h);
    nvgStrokePaint(vg, paintLeft);
    nvgStroke(vg);
    nvgShapeAntiAlias(vg, USE_NANOVG_AA);
}
void drawPlaySymbol(NVGcontext* vg, ivec2& pos, ivec2& size, const NVGcolor& color, int drawParm, int drawParm2) {
    float inset = math::max(2.0f, size.x / 8.0f);
    float x1    = pos.x + inset;
    float y1    = pos.y + inset;
    float y2    = pos.y + size.y - inset;
    float x3    = pos.x + size.x - inset;
    float y3    = pos.y + size.y / 2.0f;
    nvgBeginPath(vg);
    nvgMoveTo(vg, x1, y1);
    nvgLineTo(vg, x1, y2);
    nvgLineTo(vg, x3, y3);
    nvgClosePath(vg);
    nvgFillColor(vg, getContrastFontColorNvg(color));
    nvgSetShapeExtents(vg, x1, y1, x3 - x1, y2 - y1);
    nvgFill(vg);
}
void drawCross(NVGcontext* vg, ivec2& pos, ivec2& size, const NVGcolor& color, int drawParm, int drawParm2) {
    float inset = math::max(math::min<float>(math::min(size.x, size.y), 4.0f), size.x / 3.0f);
    float x1    = pos.x + inset;
    float y1    = pos.y + inset;
    float y2    = pos.y + size.y - inset;
    float x2    = pos.x + size.x - inset;
    nvgBeginPath(vg);
    nvgMoveTo(vg, x1, y1);
    nvgLineTo(vg, x2, y2);
    nvgMoveTo(vg, x1, y2);
    nvgLineTo(vg, x2, y1);
    nvgStrokeWidth(vg, 2.0f);
    nvgStrokeColor(vg, (color));
    nvgStroke(vg);
}
void drawRecordSymbol(NVGcontext* vg, ivec2& pos, ivec2& size, const NVGcolor& color, int drawParm, int drawParm2) {
    float inset = math::max(2.0f, size.x / 8.0f);
    auto radius = (int) (size.y - inset * 2.0) / 2.5;
    nvgBeginPath(vg);
    nvgCircle(vg, pos.x + size.x / 2.0f, pos.y + size.y / 2.0f, radius);
    nvgClosePath(vg);
    nvgFillColor(vg, !!drawParm2 ? rgbToNvg(0xFFDD3333) : rgbToNvg(0xFF884444));
    //    nvgSetShapeExtents(vg, pos.x, pos.y, size.x, size.y);
    nvgFill(vg);
}
void drawTri(NVGcontext* vg, float x, float y, float h, const int dir, const NVGcolor& color, const NVGcolor& strokeColor, float strokeWidth) {
    float x1 = x;
    float y1 = y;
    float y2 = y + h / 2.0f;
    float y3 = y + h;
    float x3 = dir == 1 ? x - h / 1.41f : x + h / 1.41f;
    nvgBeginPath(vg);
    //    nvgRect(vg, x1, y1, h, h);
    nvgMoveTo(vg, x1, y1);
    nvgLineTo(vg, x3, y2);
    nvgLineTo(vg, x1, y3);
    nvgClosePath(vg);
    nvgFillColor(vg, color);
    nvgSetShapeExtents(vg, x1, y1, x3 - x1, y3 - y1);
    nvgFill(vg);
    if (strokeWidth > 0) {
        nvgStrokeColor(vg, strokeColor);
        nvgStrokeWidth(vg, strokeWidth);
        nvgStroke(vg);
    }
}

void drawTintedImage(NVGcontext* vg, int image, float alpha,
               float sx, float sy, float sw, float sh,// sprite location on texture
               float x, float y, float w, float h, const NVGcolor& rgba)    // position and size of the sprite rectangle on screen
{
    float ax, ay;
    int iw, ih;
    NVGpaint img;

    nvgImageSize(vg, image, &iw, &ih);

    // Aspect ration of pixel in x an y dimensions. This allows us to scale
    // the sprite to fill the whole rectangle.
    ax = w / sw;
    ay = h / sh;

    img = nvgImagePattern(vg, x - sx * ax, y - sy * ay, (float) iw * ax, (float) ih * ay,
                          0, image, alpha);
    nvgBeginPath(vg);
    nvgRect(vg, x, y, w, h);
    img.innerColor = img.outerColor = rgba;
    nvgFillPaint(vg, img);
    nvgFill(vg);
}

NVGpaint imagePattern(NVGcontext* vg, int width, int ext, int imgId) {
    RenderResources::NvgImageTexture& image = RenderResources::imgIcons[imgId];
    return nvgImagePattern(vg, -ext, -ext, width + ext * 2, width + ext * 2, 0, image.perContextId[vg], 1.0f);
}

void drawIconColored(NVGcontext* vg, const ivec2& size, RenderResources::NvgImageTexture* image, NVGcolor color, int32_t extImg) {
    const int32_t iconW   = math::min(size.x, size.y);
    NVGpaint paintIcon    = nvgImagePattern(vg, -extImg, -extImg, iconW + extImg * 2, iconW + extImg * 2, 0, image->perContextId[vg], 1.0f);
    paintIcon.innerColor = paintIcon.outerColor = color;
    nvgBeginPath(vg);
    nvgRect(vg, -extImg, -extImg, iconW + extImg * 2, iconW + extImg * 2);
    nvgFillPaint(vg, paintIcon);
    nvgFill(vg);
}

void drawIcon(NVGcontext* vg, const ivec2& size, RenderResources::NvgImageTexture* image, int32_t extImg) {
    drawIconColored(vg, size, image, nvgRGBAf(1, 1, 1, 1), extImg);
}

void drawTextureSymbol(NVGcontext* vg, ivec2& pos, ivec2& size, const NVGcolor& color, int drawParm, int drawParm2) {
    int32_t inset = 3;
    //  iconPos.y -= inset;
    int32_t extImg = 2;
    int32_t iconW  = math::min(size.x, size.y);
    ivec2 offset   = ivec2(math::max(0, (size.x - iconW) / 2), math::max(0, (size.y - iconW) / 2));
    ivec2 iconPos  = pos + inset + offset;
    iconW -= inset * 2;
    if (iconW <= 0) {
        return;
    }

    /* nvgSave(vg);
    nvgTranslate(vg, iconPos.x, iconPos.y);
    nvgBeginPath(vg);
    nvgRect(vg, 0, 0, iconW, iconW);
    nvgFillColor(vg, NVGcolor{1,0,1,0.75f});
    nvgFill(vg);
    nvgRestore(vg); */

    NVGpaint paintIcon = imagePattern(vg, iconW, extImg, drawParm);
    nvgTranslate(vg, iconPos.x, iconPos.y);
    nvgBeginPath(vg);
    nvgRect(vg, -extImg, -extImg, iconW + extImg * 2, iconW + extImg * 2);
    nvgFillPaint(vg, paintIcon);
    nvgFill(vg);
    nvgTranslate(vg, -iconPos.x, -iconPos.y);

    if (drawParm2 > -1) {
        extImg       = 2;
        iconW        = 18;
        int32_t extW = iconW + extImg * 2;
        paintIcon    = imagePattern(vg, iconW, extImg, drawParm2);
        ivec2 ledPos = ivec2(pos.x + 6 - iconW / 2, pos.y + size.y - 6 - iconW / 2);
        nvgTranslate(vg, ledPos.x, ledPos.y);
        nvgBeginPath(vg);
        nvgRect(vg, -extImg, -extImg, extW, extW);
        nvgFillPaint(vg, paintIcon);
        nvgFill(vg);
        nvgGlobalCompositeOperation(vg, NVG_LIGHTER);
        if (drawParm2 == IMG_LED) {
            paintIcon = imagePattern(vg, iconW, extImg, IMG_LED_GLOW);
            nvgBeginPath(vg);
            nvgRect(vg, -extImg, -extImg, extW, extW);
            nvgFillPaint(vg, paintIcon);
            nvgFill(vg);
        }
        nvgGlobalCompositeOperation(vg, NVG_SOURCE_OVER);
        nvgTranslate(vg, -ledPos.x, -ledPos.y);
    }
}

void drawStopSymbol(NVGcontext* vg, ivec2& pos, ivec2& size, const NVGcolor& color, int drawParm, int drawParm2) {
    float inset = math::max(2.0f, size.x / 8.0f);
    nvgBeginPath(vg);
    nvgRect(vg, pos.x + inset, pos.y + inset, size.x - inset * 2.0f, size.y - inset * 2.0f);
    nvgFillColor(vg, getContrastFontColorNvg(color));
    nvgFill(vg);
}

void drawSquareInset(NVGcontext* vg, ivec2& pos, ivec2& size, const NVGcolor& color, int drawParm, int drawParm2) {
    float inset = math::max(4.0f, size.x / 10.0f);
    nvgBeginPath(vg);
    nvgRect(vg, pos.x + inset, pos.y + inset, size.x - inset * 2.0f, size.y - inset * 2.0f);
    nvgStrokeColor(vg, getContrastFontColorNvg(color));
    nvgStrokeWidth(vg, 1.0f);
    nvgStroke(vg);
}

void drawLoadingIcon(NVGcontext* vg, ivec2& pos, ivec2& size, const NVGcolor& color, int drawParm, int drawParm2) 
{
    int32_t inset = 3;
    int32_t extImg = 0;
    int32_t iconW  = math::min(size.x, size.y);
    ivec2 offset   = ivec2(math::max(0, (size.x - iconW)), math::max(0, (size.y - iconW) / 2));
    ivec2 iconPos  = pos + inset + offset;
    iconW -= inset * 2;
    auto time = getTimeMillisF();
    // 2 iterations per second
    float t = fmod(time, 2000.0f) / 2000.0f;
    float a = 2.0f * M_PI * t;
    RenderResources::NvgImageTexture& image = RenderResources::imgIcons[drawParm];

    /* nvgSave(vg);
    nvgTranslate(vg, iconPos.x, iconPos.y);
    nvgBeginPath(vg);
    nvgRect(vg, 0, 0, iconW, iconW);
    nvgFillColor(vg, NVGcolor{1,0,1,0.75f});
    nvgFill(vg);
    nvgRestore(vg); */

    NVGpaint paintIcon = nvgImagePattern(vg, -extImg, -extImg, iconW + extImg * 2, iconW + extImg * 2, 0, image.perContextId[vg], 1.0f);
    nvgSave(vg);
    nvgTranslate(vg, iconPos.x + iconW*0.5f, iconPos.y + iconW*0.5f);
    nvgRotate(vg, a);
    nvgTranslate(vg, -iconW*0.5f, -iconW*0.5f);
    nvgBeginPath(vg);
    nvgRect(vg, -extImg, -extImg, iconW + extImg * 2, iconW + extImg * 2);
    nvgFillPaint(vg, paintIcon);
    nvgFill(vg);
    nvgRestore(vg);
}

void drawWaveform(NVGcontext* vg, vec2 pos, vec2 size, int32_t shape, NVGcolor color, float strokeWidth) {
    using DAW::Shape::ShapeWaveform;
    /* pass 0 is fill, pass 1 is stroke */
    for (int pass = 0; pass < 2; ++pass) {
        nvgBeginPath(vg);
        switch (shape) {
            case ShapeWaveform::SHAPE_SINE:
            case ShapeWaveform::SHAPE_SINE_INV:
                for (int i = 0; i < size.x; ++i) {
                    float x = pos.x + i;
                    float v = size.y * (0.5f + sinf(float(i * M_PI) * 2.0f / size.x) * 0.5f);
                    float y = pos.y + (shape == ShapeWaveform::SHAPE_SINE ? v : size.y - v);
                    if (i == 0) {
                        nvgMoveTo(vg, x, y);
                    } else {
                        nvgLineTo(vg, x, y);
                    }
                }
                break;
            case ShapeWaveform::SHAPE_SQUARE: {
                nvgMoveTo(vg, pos.x, pos.y + size.y);
                nvgLineTo(vg, pos.x, pos.y + 0);
                nvgLineTo(vg, pos.x + size.x * 0.5f, pos.y + 0);
                nvgLineTo(vg, pos.x + size.x * 0.5f, pos.y + size.y);
                nvgLineTo(vg, pos.x + size.x, pos.y + size.y);
                break;
            }
            case ShapeWaveform::SHAPE_SQUARE_INV: {
                nvgMoveTo(vg, pos.x, pos.y + size.y);
                nvgLineTo(vg, pos.x + size.x * 0.5f, pos.y + size.y);
                nvgLineTo(vg, pos.x + size.x * 0.5f, pos.y + 0);
                nvgLineTo(vg, pos.x + size.x, pos.y + 0);
                nvgLineTo(vg, pos.x + size.x, pos.y + size.y);
                break;
            }
            case ShapeWaveform::SHAPE_PULSE: {
                float f = 1.0f / 6.0f;
                nvgMoveTo(vg, pos.x, pos.y + size.y);
                nvgLineTo(vg, pos.x, pos.y + 0);
                nvgLineTo(vg, pos.x + size.x * f, pos.y + 0);
                nvgLineTo(vg, pos.x + size.x * f, pos.y + size.y);
                nvgLineTo(vg, pos.x + size.x, pos.y + size.y);
                break;
            }
            case ShapeWaveform::SHAPE_PULSE_INV: {
                /* 66% PWM */
                float f = 1.0f / 6.0f;
                nvgMoveTo(vg, pos.x, pos.y + size.y);
                nvgLineTo(vg, pos.x + size.x * f, pos.y + size.y);
                nvgLineTo(vg, pos.x + size.x * f, pos.y + 0);
                nvgLineTo(vg, pos.x + size.x, pos.y + 0);
                nvgLineTo(vg, pos.x + size.x, pos.y + size.y);
                break;
            }
            case ShapeWaveform::SHAPE_SAW: {
                nvgMoveTo(vg, pos.x, pos.y + size.y);
                nvgLineTo(vg, pos.x, pos.y);
                nvgLineTo(vg, pos.x + size.x, pos.y + size.y);
                break;
            }
            case ShapeWaveform::SHAPE_SAW_INV: {
                nvgMoveTo(vg, pos.x, pos.y + size.y);
                nvgLineTo(vg, pos.x + size.x, pos.y);   
                nvgLineTo(vg, pos.x + size.x, pos.y + size.y);
                break;
            }
            case ShapeWaveform::SHAPE_TRIANGLE:
            case ShapeWaveform::SHAPE_TRIANGLE_INV: {
                bool bInv = shape == ShapeWaveform::SHAPE_TRIANGLE_INV;
                nvgMoveTo(vg, pos.x, pos.y + (bInv ? 0 : size.y));
                nvgLineTo(vg, pos.x + size.x * 0.5f, pos.y + (bInv ? size.y : 0));
                nvgLineTo(vg, pos.x + size.x, pos.y + (bInv ? 0 : size.y));
                break;
            }
            default:
                break;
        }
        if (pass == 0) {
            auto fillColor = color;
            fillColor.a = 0.3;
            nvgClosePath(vg);
            nvgFillColor(vg, fillColor);
            nvgFillCustomPar(vg, -2);
            nvgFill(vg);
        } else {
            nvgStrokeWidth(vg, strokeWidth);
            nvgStrokeColor(vg, color);
            nvgSetShapeExtents(vg, pos.x, pos.y, size.x, size.y);
            nvgStroke(vg);
        }
    }
}

GuiColor::constant_t guibase::getLabelColor() const {
    if (isFlag(FLG_HAS_COLOR_BG) && theme) {
        auto colBgU32 = theme->getColorInt32(getBackgroundColor());
        auto lum = getLuminance(colBgU32);
        if (lum > 0.179) {
            return GuiColor::COL_BLACK;
        }
        return GuiColor::COL_WHITE;
    }
    return (getStateFlags() & FLG_ENBL) ? GuiColor::COL_LABEL_ACTIVE : GuiColor::COL_LABEL_INACTIVE;
}


void guibase::renderWidgetBorder(NVGcontext* vg, int32_t flags) const {
    if (flags & FLG_RENDER_BACKGROUND) {
        renderWidgetBorderPosSize(vg, flags, pos, size);
    }
}

void guibase::renderWidgetBorderPosSize(NVGcontext* vg, int32_t flags, ivec2 pos, ivec2 size) const {
    int n       = theme->get(GuiConstant::CONST_GUI_INSET_WIDGET_BG);
    if (n != 0) {
        nvgBeginPath(vg);
        nvgRect(vg, pos.x, pos.y, size.x, size.y);
        nvgFillColor(vg, theme->getBgStrokeColor(flags));
        nvgFill(vg);
    }
    auto bgPos  = pos + ivec2(n);
    auto bgSize = size - ivec2(n * 2);
    if (bgSize.x > 0 && bgSize.y > 0) {
        NVGcolor bgColor = theme->getColor(getBackgroundColor());
        nvgBeginPath(vg);
        nvgRect(vg, bgPos.x, bgPos.y, bgSize.x, bgSize.y);
        nvgFillColor(vg, bgColor);
        if (flags & FLG_RENDER_BACKGROUND_INSET) {
            nvgFillCustomPar(vg, (flags&FLG_BG_SHADING)?-2:-1);
        }
        nvgFill(vg);
    }
}

void guibase::setGuiType(gui_type guiType) {
    this->guiType = guiType;
    getContainerLabel(guiType, this->label);
}

void guibase::handleMouseDownBegin(MouseEvent& evt) {
    if (evt.button == 0) {
        handleDraggedBegin(evt);
    } else if (evt.button == 1) {
        handleRightClick(evt);
    } else if (evt.button > 1) {
#if BUILD_DAW_HOST
        guictr_properties_table* dbgPropertiesCtrPopup = guictr_properties_table::MakeUniquePropertiesCtr();
        guictxtmenu_base* ctxtMenu = new guictxtmenu_base();
        ctxtMenu->setBackgroundRendered(true);
        ctxtMenu->size = { 640, 480 };
        ctxtMenu->add(static_cast<guibase*>(dbgPropertiesCtrPopup));
        dbgPropertiesCtrPopup->setDebugPropertyHandle(this);
        // dbgPropertiesCtrPopup->setTheme(theme);
        // dbgPropertiesCtrPopup->layout();
        // dbgPropertiesCtrPopup->theme = nullptr;
        this->parentCtrl->openContextMenu(ctxtMenu, evt.mousepos);
        dbgPropertiesCtrPopup->setDebugPropertyHandle(this);
        dbgPropertiesCtrPopup->layout();
        setGlobalDebugPropertyHandle(this);
#endif
    }
}

bool guibase::isChildOf(guibase* g) {
    if (this == g) return true;
    return parent && parent->isChildOf(g);
}

void guibase::setFont(NVGcontext* vg, float size, NVGcolor color, int alignment) {
    dbgassert(theme);
    UTIL_setFont(vg, theme, size, color, alignment);
}

void guibase::setTheme(guitheme_t* theme) {
    this->theme = theme;
}

void guibuttontoggle::render(NVGcontext* vg) {
    if (!isRenderableSizeAndContext(vg)) {
        return;
    }
    vec2 cen = vec2(size / 2);
    cen.x += pos.x;
    cen.y += pos.y;
    int32_t state              = getStateFlags();
    GuiColor::constant_t color = GuiColor::COL_BTN_BG_DEFAULT_INACTIVE;
    if (getState()) {
        color = colorActive;
    }
    nvgBeginPath(vg);
    nvgCircleFast(vg, cen.x, cen.y, radius);
    nvgFillColor(vg, theme->getColor(color));
    nvgFill(vg);
    nvgStrokeColor(vg, theme->getBgStrokeColor(state));
    nvgStrokeWidth(vg, theme->getFloat(GuiConstant::CONST_GUI_FRAME_STROKE_WIDTH));
    nvgStroke(vg);
    auto automationState = getFlags() & 0x00700000;
    if (automationState & FLG_IS_AUTOMATED) {
        auto automationColor = GuiColor::COL_AUTOMATED;
        if (automationState & FLG_IS_AUTOMATION_INACTIVE) {
            automationColor = GuiColor::COL_AUTOMATED_INACTIVE;
        }
        nvgStrokeColor(vg, theme->getColor(automationColor));
        nvgStrokeWidth(vg, theme->getFloat(GuiConstant::CONST_GUI_FRAME_STROKE_WIDTH) * 2.0f);
        nvgStroke(vg);
    }
    int icon = _getIcon();
    if (icon >= 0) {
        RenderResources::NvgImageTexture& image = RenderResources::imgIcons[icon];
        nvgTranslate(vg, pos.x, pos.y);
        drawIcon(vg, size, &image);
        nvgTranslate(vg, -pos.x, -pos.y);
    }

    /*nvgBeginPath(vg);
  nvgRect(vg, pos.x, pos.y, size.x, size.y);
  nvgFillColor(vg, c);
  nvgFill(vg);*/
}

bool guibase::isSelected() {
    return false;
}
bool guibase::hovered() const {
    return parentCtrl && toRef() == parentCtrl->getGuiOverRef();
}
bool guibase::pressed() const {
    return parentCtrl && toRef() == parentCtrl->getGuiDraggedRef();
}
bool guibase::focused() const {
    return parentCtrl && parentCtrl->isCtrOrChildFocused(this);
}
int32_t guibase::getStateFlags() const {
    int dynFlags = FLG_DRG | FLG_HVRD | FLG_FOC | FLG_VISIBLE | FLG_RENDER_BACKGROUND;
    int32_t flgs = this->flags & (~dynFlags);
    if (pressed()) {
        flgs |= FLG_DRG;
    }
    if (hovered()) {
        flgs |= FLG_HVRD;
    }
    if (focused()) {
        flgs |= FLG_FOC;
    }
    if (isVisible()) {
        flgs |= FLG_VISIBLE;
    }
    if (isBackgroundRendered()) {
        flgs |= FLG_RENDER_BACKGROUND;
    }
    return flgs;
}

void guibase::setControl(BaseCtrl* parentCtrl) {
    this->parentCtrl = parentCtrl;
#if BUILD_DAW_HOST
    if (parentCtrl) {
        dawCtrl = parentCtrl->getDawCtrl();
    }
#endif
    if (parentCtrl) {
        setTheme(parentCtrl->getTheme());
    }
}

void guibase::setParent(guibase* parent) {
    this->parent = parent;
    if (!parentCtrl && parent) {
        setTheme(parent->theme);
    }
}
String guibase::getClassName() const {
    return typeName(*this);
}

template<>
void guitooltip<guibase>::setContent() {
    auto ptr = getInstanceOrNull();
    if (!ptr) {
        return;
    }
    table.tableWidth = 140;
    auto cell = Table::tblString{ptr->getTooltipText()};
    if (table.strW) {
        table.tableWidth = table.strW->getStringWidth(cell.str);
    }
    Table::tbl_row_t row{{std::move(cell)}};
    table.rows.push_back(std::move(row));
}

guictxtmenu_base* guibase::getTooltip(AppCtrl* appctrl) {
    if (!getTooltipText().empty()) {
        auto tooltip = new guitooltip<guibase>(this);
        return tooltip;
    }
    return nullptr;
}

namespace DebugAlloc {
    Tracker<guibase> trackerGUIs;
    template<>
    Tracker<guibase>* getTracker() {
        return &trackerGUIs;
    }
}// namespace DebugAlloc
void printLeakedGuiBase() {
#ifdef TRACK_ALLOCATIONS_GUIBASE
    DebugAlloc::getTracker<guibase>()->onExit();
#endif
}
void guibase::renderDragged(NVGcontext* vg, ivec2 mousepos, ivec2 dragOffset) {
    //    mousepos += dragOffset;
    if (isDragRendered()) {

        mousepos -= pos;
        mousepos.x -= size.x / 2;
        nvgTranslate(vg, mousepos.x, mousepos.y);
        //    drawBackground(vg, theme, pos, size, 0, true, false);
        //    ivec2 inset = { 2, 2 };
        //    UIFont::font_instance instance = theme->getFont(UIFont::FONT_DEFAULT);
        //    UIFont::bindFont(vg, instance);
        //    nvgFillColor(vg, THEMECOL_TEXT);
        //    Table::DrawTableNVG(this->table, vg, theme, pos + inset, size - inset * 2, HEIGHT_ENTRY - 4);
        render(vg);
    }
}

template<>
void SafeRefStorage<guibase>::onPreDestroy() {
    size_t numRefsLeaked = 0;
    std::vector<String> someNames;
    for (auto& ref : refs) {
        if (ref->ptr) {
            log_lf(Log::L_WARN, "%s alive\n", ref->ptr->getClassName().c_str());
            ref->ptr->safeRef.handler = nullptr;
            ref->ptr = nullptr;
            numRefsLeaked++;
        }
    }
    if (numRefsLeaked > 0) {
        String name = typeName(*this);
        log_lf(Log::L_WARN, "%s: %zu refs still alive\n", name.c_str(), numRefsLeaked);
    }
}

guibase::guibase(gui_type guiType)
    : guiType(guiType)
{
    dbgassert(seqthreads::CurrentThreadType() == seqthreads::ThreadType::MainThread);

#ifdef TRACK_ALLOCATIONS_GUIBASE
    allocId = DebugAlloc::getTracker<guibase>()->objConstructor(this);
#endif
    makeSafeRef();
}
guibase::~guibase() {
    dbgassert(seqthreads::CurrentThreadType() == seqthreads::ThreadType::MainThread);
#ifdef TRACK_ALLOCATIONS_GUIBASE
    DebugAlloc::getTracker<guibase>()->objDestructor(this);
#endif
    if (safeRef.handler) {
        safeRef.handler->safeRefDestroy(safeRef.refId);
    }
}

SafeRef<guibase> guibase::makeSafeRef() {
    // dbgassert(parentCtrl);
    if (!safeRef.handler) {
        auto runtime = daw_tls::getTls().runtime;
        dbgassert(runtime);
        auto& storage = runtime->safeRefs;
        safeRef = /* SafeRef<guibase> */{ storage.safeRefCreate(this), &storage };
    }
    return safeRef;
}
#if !BUILD_DAW_HOST
void guibase::addProperties(Table::tbl* table) {
}
#else
template<typename T>
void addPropertiesFromGui(T& gui, Table::tbl* table);
template<>
void addPropertiesFromGui(guibase& gui, Table::tbl* table);
void guibase::addProperties(Table::tbl* table) {
    addPropertiesFromGui(*this, table);
}
#endif
#ifdef TRACK_ALLOCATIONS_GUIBASE
namespace DebugAlloc {
template<>
void printLeaked(int64_t allocCount, const std::vector<guibase*>& allocList, const std::unordered_map<guibase*, DebugAlloc::AllocInfo>& mapAllocInfo) {
    log_lf(Log::L_DEBUG, "allocations: %zd\n", allocCount);
    for (auto& tStar : allocList) {
        guibase* gui = tStar;
        log_lf(Log::L_DEBUG, "leaked %zd %s\n", gui->allocId, gui->getClassName().c_str());// add debug info to clip instance (track/time )
#if RECORD_ALLOC_STACKTRACES
        log_lf(Log::L_DEBUG, "leaked %012zX %zd allocated from:\n", reinterpret_cast<int64_t>(tStar), allocInfo.allocId);// add debug info to clip instance (track/time )
        for (auto& s : allocInfo.stacktrace) {
            log_lf(Log::L_DEBUG, "  %s\n", StringAsCStr(s));
        }
#endif
    }
}
}
#endif


float textlabel_dynamic_t::getScale() const {
    return math::clamp<float>(fontSize * dynamicFontScale, math::clamp(fontSize, 1.0f, 4.0f), math::max(2.0f, (size.y - 1.5f) * 0.95f));
}

void textlabel_dynamic_t::adjustWidth() {
    float delta = size.x - lastRenderWidthLabel;
    if (math::abs(delta) > 4.0f) {
        const float FONT_SCALE_MIN = 0.05f;
        const float FONT_SCALE_MAX = 2.0f;
        const float d = math::clamp(math::abs(delta) / 200.0f, 1.0f/64.0f, 1.0f/8.0f);
        if (delta > 0.0f) {
            dynamicFontScale = math::min(FONT_SCALE_MAX, dynamicFontScale + d);
        } else {
            dynamicFontScale = math::max(FONT_SCALE_MIN, dynamicFontScale - d);
        }
    }
}

void textlabel_dynamic_t::render(NVGcontext* vg, guitheme_t* theme, const String& label, const NVGcolor& fontColor) {
    if (size.x > 0 && size.y > 0) {
        float fontSize = getScale();
        if (fontSize >= 1.0) {
            auto offsetPos = pos;
            if (alignment & NVG_ALIGN_CENTER) {
                offsetPos.x += size.x * 0.5f;
            }
            if (alignment & NVG_ALIGN_RIGHT) {
                offsetPos.x += size.x * 1.0f;
            }
            if (alignment & NVG_ALIGN_MIDDLE) {
                offsetPos.y += size.y * 0.5f;
            }
            if (alignment & NVG_ALIGN_BOTTOM) {
                offsetPos.y += size.y * 1.0f;
            }
            lastRenderWidthLabel = renderTextLabel(vg, offsetPos, size, label, theme, fontSize, fontColor, alignment);
        }
    }
}

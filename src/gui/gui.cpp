#include <algorithm>
#include <nanovg.h>
#include <nanovg_min.h>
#include <typeinfo>
#include "assert_dbg.h"
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
#include "util/debug_alloc.h"
#include "guifonts.h"
#include "host/mainctrl.h"

NVGcolor g_colorPalette[COLOR_PALETTE_LEN];


NVGcolor dbgcolorsArray[8] = {
        nvgRGBA(255, 0, 0, 55),
        nvgRGBA(0, 255, 0, 55),
        nvgRGBA(0, 0, 255, 55),
        nvgRGBA(0, 127, 255, 55),
        nvgRGBA(255, 127, 0, 55),
        nvgRGBA(255, 0, 255, 55),
        nvgRGBA(255, 127, 255, 55),
        nvgRGBA(255, 255, 0, 55)
};
namespace GuiColor {
    void initConstants(int colorVal);
}
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
    float f = nvgTextW(vg, pos.x, pos.y, bounds.x, text.c_str(), &text.back() + 1);
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

void renderCenteredMultilineText(NVGcontext* vg, const guitheme_t* const theme, const String& str, float fontSize, GuiColor::constant_t c, ivec2 pos, ivec2 bounds) {

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

    auto getNvgMultiLineTextBounds = [lineh](NVGcontext* vg, String s, float maxLineWidth, float* bounds) -> void {
        auto* btncstr = StringAsCStr(s);
        NVGtextRow rows[16];
        int nrows;
        float textBoundsX = 0;
        float textBoundsY = 0;
        if ((nrows = nvgTextBreakLines(vg, btncstr, nullptr, maxLineWidth, rows, 16))) {
            for (int i = 0; i < nrows; i++) {
                NVGtextRow* row = &rows[i];
                textBoundsX     = math::max(textBoundsX, row->width);
                textBoundsY += lineh;
            }
        }
        bounds[0] = textBoundsX;
        bounds[1] = textBoundsY;
    };

    vec2 vPos = pos;
    float textBounds[2];
    getNvgMultiLineTextBounds(vg, str, bounds.x, textBounds);
    vPos.y += bounds.y / 2.0f - (textBounds[1] / 2.0f);
    vPos.y += lineh / 2.0f;

    nvgTextBox(vg, vPos.x, vPos.y, bounds.x, StringAsCStr(str), nullptr);
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

void drawImage(NVGcontext* vg, int image, float alpha,
               float sx, float sy, float sw, float sh,// sprite location on texture
               float x, float y, float w, float h)    // position and size of the sprite rectangle on screen
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
    drawIconColored(vg, size, image, nvgRGBAf(1,1,1,1), extImg);
}

void drawTextureSymbol(NVGcontext* vg, ivec2& pos, ivec2& size, const NVGcolor& color, int drawParm, int drawParm2) {
    int32_t inset = 3;
    //  iconPos.y -= inset;
    int32_t extImg = 2;
    int32_t iconW  = math::min(size.x, size.y);
    ivec2 offset   = ivec2(math::max(0, (size.x - iconW) / 2), math::max(0, (size.y - iconW) / 2));
    ivec2 iconPos  = pos + inset + offset;
    iconW -= inset * 2;
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

void guibase::renderWidgetBorder(NVGcontext* vg, int32_t flags) const {
    if (flags & FLG_RENDER_BACKGROUND) {
        renderWidgetBorderPosSize(vg, flags, pos, size);
    }
}

void guibase::renderWidgetBorderPosSize(NVGcontext* vg, int32_t flags, ivec2 pos, ivec2 size) const {
    nvgBeginPath(vg);
    nvgRect(vg, pos.x, pos.y, size.x, size.y);
    nvgFillColor(vg, theme->getBgStrokeColor(flags));
    nvgFill(vg);
    int n       = theme->get(GuiConstant::CONST_GUI_INSET_WIDGET_BG);
    auto bgPos  = pos + ivec2(n);
    auto bgSize = size - ivec2(n * 2);
    if (bgSize.x > 0 && bgSize.y > 0) {
        NVGcolor bgColor = theme->getColor(getBackgroundColor());
        nvgBeginPath(vg);
        nvgRect(vg, bgPos.x, bgPos.y, bgSize.x, bgSize.y);
        nvgFillColor(vg, bgColor);
        nvgFill(vg);
    }
}

guictr_properties_table* makeUniquePropertiesCtr();

void guibase::handleMouseDownBegin(MouseEvent& evt) {
    if (evt.button == 0) {
        handleDraggedBegin(evt);
    } else if (evt.button == 1) {
        handleRightClick(evt);
    } else if (evt.button > 1) {
#if BUILD_VSTHOST
        guictr_properties_table* dbgPropertiesCtrPopup = makeUniquePropertiesCtr();
        guictxtmenu_base* ctxtMenu = new guictxtmenu_base();
        ctxtMenu->size = { 640, 480 };
        ctxtMenu->add(static_cast<guibase*>(dbgPropertiesCtrPopup));
        dbgPropertiesCtrPopup->setDebugPropertyHandle(this);
        dbgPropertiesCtrPopup->setTheme(theme);
        dbgPropertiesCtrPopup->layout();
        dbgPropertiesCtrPopup->theme = nullptr;
        this->parentCtrl->openContextMenu(ctxtMenu, evt.mousepos);
        dbgPropertiesCtrPopup->setDebugPropertyHandle(this);
        dbgPropertiesCtrPopup->layout();
        setDebugPropertyHandle(this);
#endif
    }
}

guitheme_t* getDefaultTheme() {
    static guitheme_t theme;
    return &theme;
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
    return parentCtrl && this == parentCtrl->guiOver;
}
bool guibase::pressed() const {
    return parentCtrl && this == parentCtrl->guiDragged;
}
bool guibase::focused() const {
    return parentCtrl && this == parentCtrl->guiFocused;
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
#if BUILD_VSTHOST
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

namespace DebugAlloc {
    Tracker<guibase> trackerGUIs;
    template<>
    void printLeaked(int64_t allocId, int64_t allocCount, std::vector<guibase*>& allocList, std::unordered_map<int64_t, DebugAlloc::AllocInfo>& allocInfo) {
        log_printf("guibase allocations %lld\n", allocCount);
        for (auto gui : allocList) {
            log_printf("leaked %lld %s \n", gui->allocId, StringAsCStr(gui->getClassName()));// add debug info to clip instance (track/time )
        }
    }
    template<>
    Tracker<guibase>* getTracker() {
        return &trackerGUIs;
    }
}// namespace DebugAlloc
void printLeakedGuiBase() {
    DebugAlloc::getTracker<guibase>()->onExit();
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
guibase::guibase() {
    allocId = DebugAlloc::getTracker<guibase>()->objConstructor(this);
}
guibase::~guibase() {
    DebugAlloc::getTracker<guibase>()->objDestructor(this);
    BaseCtrl* ctrl = parentCtrl;
    if (ctrl) {
        ctrl->onGuiRemoved(this);
    }
    if (safeRef.handler) {
        safeRef.handler->safeRefDestroy(safeRef.refId);
    }
}

SafeRef<guibase> guibase::makeSafeRef() {
    dbgassert(parentCtrl);
    if (!safeRef.handler) {
        safeRef.handler = parentCtrl;
        safeRef.refId   = safeRef.handler->safeRefCreate(this);
    }
    return safeRef;
}
#if !BUILD_VSTHOST
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

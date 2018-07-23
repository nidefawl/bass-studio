#include <algorithm>
#include "color_util.h"
#include "gui.h"
#include "platform.h"
#include "seq_math.h"
#include "seq_util.h"
#include "leak_detect.h"

using std::min;
using std::max;
NVGcolor g_guiColors[24];
NVGcolor g_colorPalette[COLOR_PALETTE_LEN];

static NVGcolor dbgcolors[5] = {
	nvgRGBA(255, 0, 0, 55),
	nvgRGBA(0, 255, 0, 55),
	nvgRGBA(0, 0, 255, 55),
	nvgRGBA(255, 0, 255, 55),
	nvgRGBA(255, 255, 0, 55)
};

int colorVal = 57;
void initColor() {
	UNUSED(dbgcolors);
	for (int i = 0; i < (int)ARR_SIZE(colorPalette); i++) {
		g_colorPalette[i] = rgbToNvg(colorPalette[i]);
	}

	int c = colorVal;
	int c2 = max(5, c - 16);
	int c3 = min(255, c + 16);
	g_guiColors[COL_GRID_DRK] = GUI_COLORA(c, 255);
	g_guiColors[COL_GRID_BRT] = GUI_COLORA(c + 3, 255);
	g_guiColors[COL_LINE_BAR] = GUI_COLORA(c2, 255);
	g_guiColors[COL_LINE_QRT] = GUI_COLORA(c2 + 3, 255);
	g_guiColors[COL_LINE_XTH] = GUI_COLORA(c2 + 6, 255);
	g_guiColors[COL_LINE_SEPERATOR] = GUI_COLORA(c2 - 3, 255);
	g_guiColors[COL_BG_DRKER] = GUI_COLORA(max(0, c3-20), 255);
	g_guiColors[COL_BG_DRKER2] = GUI_COLORA(max(0, c3-40), 255);
	g_guiColors[COL_BG_DRK] = GUI_COLORA(c3, 255);
	g_guiColors[COL_BG_BRT] = GUI_COLORA(c3 + 20, 255);
	int c4 = max(5, c - 32);
	int c5 = max(5, c + 32);
	g_guiColors[COL_CTXTMNU_OUTLINE] = GUI_COLORA(255, 255);
	g_guiColors[COL_CTXTMNU_BG] = GUI_COLORA(c4, 255);
	g_guiColors[COL_CTXTMNU_HILIGHT] = GUI_COLORA(c5, 255);
	g_guiColors[COL_GUI_STROKE] = mulSatBright(g_guiColors[COL_GRID_DRK], 1.3f, 1.4f);
	g_guiColors[COL_BG_DRK_FOCUSED] = GUI_COLORA(c3+48, 255);

	g_guiColors[COL_NOTE] = rgbToNvg(0xff9933);
	g_guiColors[COL_NOTE_PLAYING] = rgbToNvg(0x33ff33);
	g_guiColors[COL_NOTE_ARP] = rgbToNvg(0x22bb22);
	g_guiColors[COL_NOTE_MUTE] = rgbToNvg(0x666666);
	g_guiColors[COL_NOTE_OUTLINE] = rgbToNvg(0);
	g_guiColors[COL_NOTE_TEXT] = rgbToNvg(33);
	g_guiColors[COL_BG_SELECTEDTRACK] = GUI_COLORA(c3 + 20, 80);
//	memset(g_guiColors, 0, sizeof(NVGcolor)*24);
	getDefaultTheme()->initDefaultTheme();
}

void setFont(NVGcontext* vg, float size, NVGcolor color, int alignment) {
	nvgFontSize(vg, size);
	nvgFontFace(vg, "sans");
	nvgFillColor(vg, color);
	nvgTextAlign(vg, alignment);
}
void renderText(NVGcontext* ctx, float x, float y, float maxWidth, const char* string)
{
	NVGtextRow rows[2];
	int nrows;
	if ((nrows = nvgTextBreakLines(ctx, string, NULL, maxWidth, rows, 2))) {
		NVGtextRow* row = &rows[0];
		if (row->width > maxWidth)
			return;
		float f = nvgText(ctx, x, y, row->start, row->end);
		if (nrows > 1 && (maxWidth-(f-x)) > 18) {
			nvgText(ctx, f, y, "...", NULL);
		}
	}
}

void renderDashedLineFrame(NVGcontext* vg, float x, float y, float w, float h, float thickness) {
	RenderResources::NvgImageTexture& image = RenderResources::imgDashedLine;
	uint64_t duration = 2000;
	float t = (getTimeMillis()%duration) / (float) duration;
	uint32_t texOffsetX = t*image.width;
	uint32_t texOffsetY = t*image.height;
	NVGpaint paintDown = nvgImagePattern(vg, 0, texOffsetY, image.width, image.height, 0, image.id, 1.0f);
	NVGpaint paintRight = nvgImagePattern(vg, texOffsetX, 0, image.width, image.height, M_PI*0.5f, image.id, 1.0f);
	NVGpaint paintUp = nvgImagePattern(vg, 0, image.height-texOffsetY, image.width, image.height, 0, image.id, 1.0f);
	NVGpaint paintLeft = nvgImagePattern(vg, image.width-texOffsetX, 0, image.width, image.height, M_PI*0.5f, image.id, 1.0f);

	nvgShapeAntiAlias(vg, 0);
	nvgStrokeWidth(vg, thickness);
	nvgBeginPath(vg);
	nvgMoveTo(vg, x, y);
	nvgLineTo(vg, x, y+h);
	nvgStrokePaint(vg, paintUp);
	nvgStroke(vg);
	nvgBeginPath(vg);
	nvgMoveTo(vg, x+w, y);
	nvgLineTo(vg, x+w, y+h);
	nvgStrokePaint(vg, paintDown);
	nvgStroke(vg);
	nvgBeginPath(vg);
	nvgMoveTo(vg, x, y);
	nvgLineTo(vg, x+w, y);
	nvgStrokePaint(vg, paintRight);
	nvgStroke(vg);
	nvgBeginPath(vg);
	nvgMoveTo(vg, x, y+h);
	nvgLineTo(vg, x+w, y+h);
	nvgStrokePaint(vg, paintLeft);
	nvgStroke(vg);
	nvgShapeAntiAlias(vg, 1);
}
void drawPlaySymbol(NVGcontext* vg, ivec2& pos, ivec2& size, const NVGcolor& color, int drawParm, int drawParm2) {
	float inset = max(2.0f, size.x/8.0f);
	float x1 = pos.x + inset;
	float y1 = pos.y + inset;
	float y2 = pos.y + size.y - inset;
	float x3 = pos.x + size.x - inset;
	float y3 = pos.y + size.y / 2.0f;
    nvgBeginPath(vg);
    nvgMoveTo(vg, x1, y1);
    nvgLineTo(vg, x1, y2);
    nvgLineTo(vg, x3, y3);
    nvgClosePath(vg);
    nvgFillColor(vg, getContrastFontColorNvg(color));
    nvgFill(vg);
}
void drawTri(NVGcontext* vg, float x, float y, float h, const int dir, const NVGcolor& color, const NVGcolor& strokeColor, float strokeWidth) {
	float x1 = x;
	float y1 = y;
	float y2 = y + h/2.0f;
	float y3 = y + h;
	float x3 = dir == 1 ? x - h/1.41f : x + h/1.41f;
    nvgBeginPath(vg);
//    nvgRect(vg, x1, y1, h, h);
    nvgMoveTo(vg, x1, y1);
    nvgLineTo(vg, x3, y2);
    nvgLineTo(vg, x1, y3);
    nvgClosePath(vg);
    nvgFillColor(vg, color);
    nvgFill(vg);
    if (strokeWidth > 0) {
        nvgStrokeColor(vg, strokeColor);
        nvgStrokeWidth(vg, strokeWidth);
        nvgStroke(vg);
    }
}
NVGpaint imagePattern(NVGcontext* vg, int width, int ext, int imgId) {
	RenderResources::NvgImageTexture& image = RenderResources::imgIcons[imgId];
	return nvgImagePattern(vg, -ext, -ext, width+ext*2, width+ext*2, 0, image.id, 1.0f);
}
void drawTextureSymbol(NVGcontext* vg, ivec2& pos, ivec2& size, const NVGcolor& color, int drawParm, int drawParm2) {
	int32_t inset = 3;
//	iconPos.y -= inset;
	int32_t extImg = 2;
	int32_t iconW = min(size.x, size.y);
	ivec2 offset = ivec2(max(0, (size.x-iconW)/2), max(0, (size.y-iconW)/2));
	ivec2 iconPos = pos + inset + offset;
	iconW -= inset*2;
	NVGpaint paintIcon = imagePattern(vg, iconW, extImg, drawParm);
	nvgTranslate(vg, iconPos.x, iconPos.y);
	nvgBeginPath(vg);
	nvgRect(vg, -extImg, -extImg, iconW+extImg*2, iconW+extImg*2);
	nvgFillPaint(vg, paintIcon);
	nvgFill(vg);
	nvgTranslate(vg, -iconPos.x, -iconPos.y);

	if (drawParm2 > -1) {
		int icon = drawParm2 == 0 ? IMG_LED_OFF : IMG_LED;
		extImg = 2;
		iconW = 18;
		int32_t extW = iconW+extImg*2;
		paintIcon = imagePattern(vg, iconW, extImg, icon);
		ivec2 ledPos = ivec2(pos.x+6-iconW/2, pos.y+size.y-6-iconW/2);
		nvgTranslate(vg, ledPos.x, ledPos.y);
		nvgBeginPath(vg);
		nvgRect(vg, -extImg, -extImg, extW, extW);
		nvgFillPaint(vg, paintIcon);
		nvgFill(vg);
		nvgGlobalCompositeOperation(vg, NVG_LIGHTER);
		if (icon == IMG_LED) {
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
	float inset = max(2.0f, size.x/8.0f);
	nvgBeginPath(vg);
	nvgRect(vg, pos.x+inset, pos.y+inset, size.x-inset*2.0f, size.y-inset*2.0f);
    nvgFillColor(vg, getContrastFontColorNvg(color));
    nvgFill(vg);
}
void drawAttachedBackground(NVGcontext* vg, ivec2 posInset, ivec2 sizeInset, int margin) {
	static const ivec2 borderThickness(6);
	posInset -= ivec2(margin);
	sizeInset += ivec2(margin) * 2;

	int topOffset = CTR_SPACING/2-1;
	float r = 4;
	float x = posInset.x;
	float y = posInset.y-topOffset;
	int w = sizeInset.x;
	int h = sizeInset.y+topOffset;
    nvgBeginPath(vg);
    nvgMoveTo(vg, x-r, y-1);
    nvgLineTo(vg, x-r, y);
    nvgBezierTo(vg, x-r+PT1, y, x, y+PT1, x, y+r);
    nvgLineTo(vg, x, y+h-r);
    nvgBezierTo(vg, x, y+h-PT1, x+PT1, y+h, x+r, y+h);
    nvgLineTo(vg, x+w-r, y+h);
    nvgBezierTo(vg, x+w-PT1, y+h, x+w, y+h-PT1, x+w, y+h-r);
    nvgLineTo(vg, x+w, y+r);
    nvgBezierTo(vg, x+w, y+PT1, x+w+r-PT1, y, x+w+r, y);
    nvgLineTo(vg, x+w+r, y-1);
    nvgClosePath(vg);


	nvgFillColor(vg, g_guiColors[COL_BG_DRK]);
	nvgFill(vg);
	posInset += borderThickness;
	sizeInset -= borderThickness * 2;
	nvgBeginPath(vg);
	nvgRect(vg, posInset.x, posInset.y, sizeInset.x, sizeInset.y);
	nvgFillColor(vg, g_guiColors[COL_BG_BRT]);
	nvgFill(vg);
}
void guibase::renderWidgetBorder(NVGcontext* vg, int32_t flags) {
	nvgBeginPath(vg);
	nvgRect(vg, pos.x, pos.y, size.x, size.y);
	nvgStrokeColor(vg, theme->getBgStrokeColor(flags));
	nvgStrokeWidth(vg, theme->getBgStrokeWidth(flags));
	nvgStroke(vg);
	nvgFillColor(vg, theme->getBgColor(flags));
	nvgFill(vg);


	//		nvgBeginPath(vg);
	//		nvgRect(vg, pos.x+1, pos.y+1, size.x-2, size.y-2);
	//		nvgStrokeColor(vg, g_guiColors[COL_GUI_STROKE]);
	//		nvgStrokeWidth(vg, 3);
	//		nvgStroke(vg);
	//		nvgFillColor(vg, g_guiColors[COL_BG_DRK]);
	//		nvgFill(vg);
}
guitheme_t* getDefaultTheme() {
	static guitheme_t theme(true);
	return &theme;
}

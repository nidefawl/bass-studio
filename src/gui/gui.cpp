#include <algorithm>
#include <typeinfo>
#include "math/seq_math.h"
#include "color_util.h"
#include "basectrl.h"
#include "gui.h"
#include "button.h"
#include "platform.h"
#include "theme.h"
#include "saferef.h"
#include "seq_util.h"
#include "guicolors.h"
#include "debugproperties.h"
#include "guicontextmenu_base.h"
#include "renderresources.h"
#include "util/debug_alloc.h"

namespace GuiColor {
constant_t COL_BTN_BG_DEFAULT_INACTIVE("COL_BTN_BG_DEFAULT_INACTIVE", 0xff202020);
constant_t COL_BTN_BG_DEFAULT_ACTIVE("COL_BTN_BG_DEFAULT_ACTIVE", 0xff404040);
constant_t COL_BTN_BG_BYPASS_ACTIVE("COL_BTN_BG_BYPASS_ACTIVE", 0xff80ABC0);
constant_t COL_BTN_BG_SHOW_ACTIVE("COL_BTN_BG_SHOW_ACTIVE", 0xff40ABC0);
}
//NVGcolor g_guiColors[NUM_GUI_COLORS];
NVGcolor g_colorPalette[COLOR_PALETTE_LEN];

static NVGcolor dbgcolors[5] = {
	nvgRGBA(255, 0, 0, 55),
	nvgRGBA(0, 255, 0, 55),
	nvgRGBA(0, 0, 255, 55),
	nvgRGBA(255, 0, 255, 55),
	nvgRGBA(255, 255, 0, 55)
};
namespace GuiColor {
void initConstants(int colorVal);
}
void initColor() {
	UNUSED(dbgcolors);
	for (int i = 0; i < (int)ARR_SIZE(colorPalette); i++) {
		colorPalette[i] |= 0xFF000000;
		g_colorPalette[i] = rgbaToNvg(colorPalette[i]);
	}
	GuiColor::initConstants(22);

//	initColorArr(g_guiColors, colorVal);
//	memset(g_guiColors, 0, sizeof(NVGcolor)*24);
	getDefaultTheme()->initTheme();

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
	int32_t imageId = image.perContextId[vg];
	NVGpaint paintDown = nvgImagePattern(vg, 0, texOffsetY, image.width, image.height, 0, imageId, 1.0f);
	NVGpaint paintRight = nvgImagePattern(vg, texOffsetX, 0, image.width, image.height, M_PI*0.5f, imageId, 1.0f);
	NVGpaint paintUp = nvgImagePattern(vg, 0, image.height-texOffsetY, image.width, image.height, 0, imageId, 1.0f);
	NVGpaint paintLeft = nvgImagePattern(vg, image.width-texOffsetX, 0, image.width, image.height, M_PI*0.5f, imageId, 1.0f);

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
	float inset = math::max(2.0f, size.x/8.0f);
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
	return nvgImagePattern(vg, -ext, -ext, width+ext*2, width+ext*2, 0, image.perContextId[vg], 1.0f);
}
void drawIcon(NVGcontext* vg, ivec2& size, RenderResources::NvgImageTexture* image) {
	const int32_t extImg = 2;
	const int32_t iconW = (int32_t)ceil(math::min(size.x, size.y));
	const int32_t renderW = iconW + extImg * 2;
	NVGpaint paintIcon = nvgImagePattern(vg, -extImg, -extImg, iconW + extImg * 2, iconW + extImg * 2, 0, image->perContextId[vg], 1.0f);
	nvgBeginPath(vg);
	nvgRect(vg, -extImg, -extImg, iconW + extImg * 2, iconW + extImg * 2);
	nvgFillPaint(vg, paintIcon);
	nvgFill(vg);
}
void drawTextureSymbol(NVGcontext* vg, ivec2& pos, ivec2& size, const NVGcolor& color, int drawParm, int drawParm2) {
	int32_t inset = 3;
//	iconPos.y -= inset;
	int32_t extImg = 2;
	int32_t iconW = math::min(size.x, size.y);
	ivec2 offset = ivec2(math::max(0, (size.x-iconW)/2), math::max(0, (size.y-iconW)/2));
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
	float inset = math::max(2.0f, size.x/8.0f);
	nvgBeginPath(vg);
	nvgRect(vg, pos.x+inset, pos.y+inset, size.x-inset*2.0f, size.y-inset*2.0f);
    nvgFillColor(vg, getContrastFontColorNvg(color));
    nvgFill(vg);
}
void drawAttachedBackground(NVGcontext* vg, const guitheme_t* theme, ivec2 posInset, ivec2 sizeInset, int margin) {
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


	nvgFillColor(vg, theme->getColor(GuiColor::COL_BG_DRK));
	nvgFill(vg);
	posInset += borderThickness;
	sizeInset -= borderThickness * 2;
	nvgBeginPath(vg);
	nvgRect(vg, posInset.x, posInset.y, sizeInset.x, sizeInset.y);
	nvgFillColor(vg, theme->getColor(GuiColor::COL_BG_BRT));
	nvgFill(vg);
}
void guibase::renderWidgetBorder(NVGcontext* vg, int32_t flags) const {
	renderWidgetBorderPosSize(vg, flags, pos, size);
}
void guibase::renderWidgetBorderPosSize(NVGcontext* vg, int32_t flags, ivec2 pos, ivec2 size) const {
	nvgBeginPath(vg);
	nvgRect(vg, pos.x, pos.y, size.x, size.y);
	nvgStrokeColor(vg, theme->getBgStrokeColor(flags));
	nvgStrokeWidth(vg, theme->getBgStrokeWidth(flags));
	nvgStroke(vg);
	auto color = getBackgroundColor(flags);
	nvgFillColor(vg, color);
	nvgFill(vg);

	//		nvgBeginPath(vg);
	//		nvgRect(vg, pos.x+1, pos.y+1, size.x-2, size.y-2);
	//		nvgStrokeColor(vg, theme->getColor(GuiColor::COL_GUI_STROKE));
	//		nvgStrokeWidth(vg, 3);
	//		nvgStroke(vg);
	//		nvgFillColor(vg, theme->getColor(GuiColor::COL_BG_DRK));
	//		nvgFill(vg);
}
debugproperties* makeUniquePropertiesCtr();
void guibase::handleMouseDownBegin(MouseEvent& evt) {
	if (evt.button == 0) {
		handleDraggedBegin(evt);
	} else if (evt.button == 1) {
		handleRightClick(evt);
	} else if (evt.button > 1) {
#ifdef BUILD_BUILTIN_EFFECT
		{

			setDebugPropertyHandle(this);

			debugproperties* dbgPropertiesCtrPopup = makeUniquePropertiesCtr();
			guictxtmenu_base* ctxtMenu = new guictxtmenu_base();
			ctxtMenu->size = {240, 480};
			ctxtMenu->add(static_cast<guibase*>(dbgPropertiesCtrPopup));
			dbgPropertiesCtrPopup->setDebugPropertyHandle(this);
			this->parentCtrl->openContextMenu(ctxtMenu, evt.mousepos);
		}
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

NVGcolor guibuttonbase::getBackgroundColor(int stateflags) const {
	int fl = FLG_HAS_COLOR_BG|FLG_ENBL;
	if ((stateflags&fl) == fl) {
		return theme->getColor(buttonColor);
	}
	return theme->getBgColor(stateflags);
}
void guibuttontoggle::render(NVGcontext* vg) {
	vec2 cen = vec2(size / 2);
	cen.x += pos.x;
	cen.y += pos.y;
	int32_t state = getStateFlags();
	GuiColor::constant_t color = GuiColor::COL_BTN_BG_DEFAULT_INACTIVE;
	if (state & FLG_ENBL) {
		color = colorActive;
	}
	nvgBeginPath(vg);
	nvgCircleFast(vg, cen.x, cen.y, radius);
	nvgFillColor(vg, theme->getColor(color));
	nvgFill(vg);
	nvgStrokeColor(vg, theme->getBgStrokeColor(state));
	nvgStrokeWidth(vg, theme->getBgStrokeWidth(state));
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
	return this == parentCtrl->guiOver;
}
bool guibase::pressed() const {
	return this == parentCtrl->guiDragged;
}
bool guibase::focused() const {
	return this == parentCtrl->guiFocused;
}
int32_t guibase::getStateFlags() const {
	int dynFlags = FLG_DRG|FLG_HVRD|FLG_FOC|FLG_ENBL|FLG_VISIBLE|FLG_RENDER_BACKGROUND;
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
	if (isEnabled()) {
		flgs |= FLG_ENBL;
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
	if (parentCtrl)
		this->theme = parentCtrl->getTheme();
}

void guibase::setParent(guibase* parent) {
	this->parent = parent;
	if (!parentCtrl && parent) {
		theme = parent->theme;
	}
}
NVGcolor guibase::getBackgroundColor(int stateflags) const {
	return theme->getBgColor(stateflags);
}
String guibase::getClassName() {
	return typeName(*this);
}


namespace {
	DebugAlloc::Tracker<guibase> tracker;
}

template<>
void DebugAlloc::Tracker<guibase>::throwUntrackked(guibase* g) {
	my_printf("guibase with allocId %lld was not tracked\n", g->id);
	assert(0);
}
template<>
void DebugAlloc::Tracker<guibase>::printLeaked() {
	my_printf("allocCount %lld\n", allocCount);
	for (auto it = allocList.begin(); it != allocList.end(); it++) {
		guibase* ctrl = *it;
		my_printf("leaked %lld %s \n", ctrl->id, StringAsCStr(ctrl->getClassName()));
	}
	allocList.clear();
	allocList.shrink_to_fit();
}
template<>
DebugAlloc::Tracker<guibase>* DebugAlloc::getTracker() {
	return &tracker;
}
void printLeakedGuiBase() {
	DebugAlloc::getTracker<guibase>()->printLeaked();
}
guibase::guibase()  {
	id = DebugAlloc::getTracker<guibase>()->objConstructor(this);
	if (id == 934) {

	}
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
	assert(parentCtrl);
	if (!safeRef.handler) {
		safeRef.handler = parentCtrl;
		safeRef.refId = safeRef.handler->safeRefCreate(this);
	}
	return safeRef;
}
#ifndef BUILD_BUILTIN_EFFECT
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

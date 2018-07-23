#include <nanovg.h>
#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include "basectrl.h"
#include "gui.h"
#include "guicolors.h"
#include "exceptions.h"
#include "mouse.h"
#include "event.h"
#include "guicontainer.h"

using glm::vec2;
using glm::ivec2;
using glm::vec4;
using glm::ivec4;


void guictr_base::renderTitleBarHorizontal(NVGcontext* vg, String text, float textOffsetX) {
	NVGcolor c;
	if (AppCtrl::get()->isCtrOrChildFocused(this)) {
		c = g_guiColors[COL_BG_DRK_FOCUSED];
	} else {
		c = g_guiColors[COL_BG_BRT];
	}
	ivec2 sizeContent = getSizeContent();
	nvgBeginPath(vg);
	nvgRoundedRectVarying(vg, 0, 0, sizeContent.x, HEIGHT_PLUGIN_TITLE, G_RND, G_RND, 0, 0);
	nvgFillColor(vg, c);
	nvgFill(vg);
	if (text[0]) {
		setFont(vg, (int)(HEIGHT_PLUGIN_TITLE*0.8), G_WHITE, G_TITLE_ALIGN);
		nvgText(vg, textOffsetX+INSET_TITLE, HEIGHT_PLUGIN_TITLE / 2, StringAsCStr(text), NULL);
	}
}
void guictr_base::renderFrameBase(NVGcontext* vg) {
	ivec2 sizeContent = getSizeContent();
	nvgBeginPath(vg);
	nvgRoundedRect(vg, 0, 0, sizeContent.x, sizeContent.y, G_RND);
	nvgFillColor(vg, GUI_COLOR(G_S2));
	nvgFill(vg);
}
void guictr_base::renderFrameOutline(NVGcontext* vg) {
	nvgBeginPath(vg);
	ivec2 sizeContent = getSizeContent();
	nvgRect(vg, 0, 0, sizeContent.x, sizeContent.y);
	nvgStrokeColor(vg, GUI_COLOR(G_S1));
	nvgStrokeWidth(vg, G_STROKE);
	nvgStroke(vg);
	ivec2 sizeInset = getSizeContent();
	nvgIntersectScissor(vg, 0, 0, sizeInset.x, sizeInset.y);
}
bool guictr_base::setScissorTransformContainer(NVGcontext* vg) {
	ivec2 posInset = getPosContent();
	ivec2 sizeInset = getSizeContent();
	if (sizeInset.y <= 0 || sizeInset.x <= 0) {
		return false;
	}
//	nvgBeginPath(vg);
//	nvgRect(vg, pos.x, pos.y, size.x, size.y);
//	nvgFillColor(vg, rgbfToNvg(0xff3300, 0.3f));
//	nvgFill(vg);
	nvgIntersectScissor(vg, pos.x, pos.y, size.x, size.y);
	nvgTranslate(vg, posInset.x, posInset.y);
	return true;
}

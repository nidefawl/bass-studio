#include <nanovg.h>
#include "math/vec.h"
#include "math/seq_math.h"
#include "guiglobals.h"
#include "guicolors.h"
#include "guiconstant.h"
#include "gui.h"
#include "guicontainer.h"
#include "basectrl.h"
#include "color_util.h"
#include "exceptions.h"
#include "mouse.h"
#include "event.h"
#include "renderresources.h"
#include "button.h"

namespace GuiColor {
constant_t COL_PLUG_TITLE("COL_PLUG_TITLE", 0xff151515);
constant_t COL_PLUG_TITLE_SELECTED("COL_PLUG_TITLE_SELECTED", 0xff353535);
constant_t COL_PLUG_TITLE_FOCUSED("COL_PLUG_TITLE_FOCUSED", 0xffff0000);
}
namespace GuiConstant {

constant_t CONST_ROUND("CONST_ROUND", 20);
}


void guictr_base::setControl(BaseCtrl* parentCtrl) {
	guibase::setControl(parentCtrl);
	for (guibase* g : guis) {
		g->setControl(parentCtrl);
	}
}
void guictr_base::setParent(guibase* parent) {
	guibase::setParent(parent);
	for (guibase* g : guis) {
		dbgassert(g->parent == this);
			g->setParent(this);
	}
}
void guictr_base::onRemove() {
//		removeGuis();
}
void guictr_base::onAdded() {
}
void guictr_base::render(NVGcontext* vg) {
	if (isBackgroundRendered()) {
		renderBackground(vg);
	}
	if (!setScissorTransform(vg)) {
		return;
	}
	for (auto c : guis) {
		nvgSave(vg);
		c->render(vg);
		nvgRestore(vg);
	}
}
void guictr_base::renderBackground(NVGcontext* vg) {
	dbgassert(isBackgroundRendered());
	bool focused = parentCtrl->isCtrOrChildFocused(this);
	drawBackground(vg, theme, getPosContent(), getSizeContent(), margin, focused, isBackgroundRenderedInset());
}
void guictr_base::renderFrameBase(NVGcontext* vg) {
	ivec2 sizeContent = getSizeContent();
	nvgBeginPath(vg);
	nvgRoundedRect(vg, 0, 0, sizeContent.x, sizeContent.y, theme->getFloat(GuiConstant::CONST_ROUND));
	nvgFillColor(vg, theme->getFrameColorBase());
	nvgFill(vg);
}
void guictr_base::renderFrameOutline(NVGcontext* vg) {
	nvgBeginPath(vg);
	ivec2 sizeContent = getSizeContent();
	nvgRect(vg, 0, 0, sizeContent.x, sizeContent.y);
	nvgStrokeColor(vg, theme->getFrameColorOutline());
	nvgStrokeWidth(vg, 2.0);
	nvgStroke(vg);
	ivec2 sizeInset = getSizeContent();
	nvgIntersectScissor(vg, 0, 0, sizeInset.x, sizeInset.y);
}

void guictr_base::renderTitleBar(NVGcontext* vg, String text, GuiConstant::constant_t& constantHeight, float textOffsetX, int flags, bool isHorizontalTitle) {
	NVGcolor c;
	if (flags & FLAG_SELECTED) {
		c = theme->getColor(GuiColor::COL_PLUG_TITLE_SELECTED);
	} else if (flags & FLAG_FOCUSED) {
		c = theme->getColor(GuiColor::COL_PLUG_TITLE_FOCUSED);
	} else {
		c = theme->getColor(GuiColor::COL_PLUG_TITLE);
	}

	//	ivec2 sizeContent = getSizeContent();
	float fRnd = theme->getFloat(GuiConstant::CONST_ROUND);
	const int32_t hpt = theme->get(constantHeight);
	if (hpt <= 0) {
		return;
	}
	nvgBeginPath(vg);
	float textMaxWidth;
	if (isHorizontalTitle) {
		nvgRoundedRectVarying(vg, 0, 0, size.x, hpt, fRnd, fRnd, 0, 0);
		textMaxWidth = size.x-INSET_TITLE*2;
		for (auto* gui : guis) {
			if (gui->top() < hpt && gui->bottom() > 0) {
				if (gui->left() > textOffsetX) {
					textMaxWidth = math::min<float>(textMaxWidth, gui->left()-INSET_TITLE*2);
				}
			}
		}
		textMaxWidth -= textOffsetX;
	} else {
		nvgRoundedRectVarying(vg, 0, 0, hpt, size.y, fRnd, fRnd, 0, 0);
		textMaxWidth = textOffsetX-INSET_TITLE*2;
		for (auto* gui : guis) {
			if (gui->left() < hpt && gui->right() > 0) {
				if (gui->bottom() < textOffsetX) {
					textMaxWidth = math::min<float>(textMaxWidth, (textOffsetX-gui->bottom())-INSET_TITLE*2);
				}
			}
		}
	}
	nvgFillColor(vg, c);
	nvgFill(vg);
	if (textMaxWidth+4 <= 0) {
		return;
	}
	if (text[0]) {
		if (isHorizontalTitle) {
			nvgSave(vg);
			nvgIntersectScissor(vg, textOffsetX + INSET_TITLE-1, 0, textMaxWidth+2, hpt);
			setFont(vg, (int) (hpt * 0.8), getContrastFontColorNvg(c), G_TITLE_ALIGN);
//			text = StringFormat("%d %d %d", (int32_t)(textOffsetX + INSET_TITLE), (int32_t)textMaxWidth, size.x);
			nvgText(vg, textOffsetX + INSET_TITLE, hpt / 2, StringAsCStr(text), NULL);
			nvgRestore(vg);
		} else {
			setFont(vg, (int) (hpt * 0.8), G_WHITE, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
			nvgSave(vg);
			nvgTranslate(vg, hpt / 2, textOffsetX);
			nvgRotate(vg, (float)(-M_PI / 2.0));
			nvgIntersectScissor(vg, INSET_TITLE*2-1, -hpt / 2, textMaxWidth, hpt);
			nvgText(vg, INSET_TITLE * 2, 0, StringAsCStr(text), NULL);
			nvgRestore(vg);
		}
	}
}
/*static*/
void guictr_base::drawInsetBackground(NVGcontext* vg, const guitheme_t* theme, ivec2 posInset, ivec2 sizeInset) {
	if (sizeInset.y > 0 && sizeInset.x > 0) {
		nvgBeginPath(vg);
		nvgRect(vg, posInset.x, posInset.y, sizeInset.x, sizeInset.y);
		nvgFillColor(vg, theme->getColor(GuiColor::COL_BG_BRT));
		nvgFill(vg);
	}
}
/*static*/
void guictr_base::drawBackground(NVGcontext* vg, const guitheme_t* theme, ivec2 posInset, ivec2 sizeInset, int margin, bool focused,
		bool drawInset) {
	static const ivec2 borderThickness(CTR_SPACING - 2);
	posInset -= ivec2(margin);
	sizeInset += ivec2(margin) * 2;
	if (sizeInset.y > 0 && sizeInset.x > 0) {
		nvgBeginPath(vg);
		nvgRoundedRect(vg, posInset.x, posInset.y, sizeInset.x, sizeInset.y, 4);
		NVGcolor bg = theme->getColor(GuiColor::COL_BG_DRK);
		if (focused) {
			bg = theme->getColor(GuiColor::COL_BG_DRK_FOCUSED);
		}
		nvgFillColor(vg, bg);
		nvgFill(vg);
		posInset += borderThickness;
		sizeInset -= borderThickness * 2;
		if (sizeInset.y > 0 && sizeInset.x > 0 && drawInset) {
			nvgBeginPath(vg);
			nvgRect(vg, posInset.x, posInset.y, sizeInset.x, sizeInset.y);
			nvgFillColor(vg, theme->getColor(GuiColor::COL_BG_BRT));
			nvgFill(vg);
		}
	}
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

bool guictr_base::setScissorTransform(NVGcontext* vg) {
	ivec2 posInset = getPosContent();
	ivec2 sizeInset = getSizeContent();
	if (sizeInset.y <= 0 || sizeInset.x <= 0) {
		return false;
	}
	int expand = 1;
	nvgIntersectScissor(vg, posInset.x-expand, posInset.y-expand, sizeInset.x+expand*2, sizeInset.y+expand*2);
	nvgTranslate(vg, posInset.x, posInset.y);
	return true;
}
void guictr_base::scissorClip(ivec2& vpos, ivec2& vsize) {
	ivec2 posTL = toParentSpace(vpos);
	ivec2 posBR = toParentSpace(vpos + vsize);
	ivec2 posCnt = getPosContent();
	ivec2 sizeCnt = getSizeContent();
	ivec2 posBRThis = posCnt+sizeCnt;
	vpos.x = math::max(posTL.x, posCnt.x);
	vpos.y = math::max(posTL.y, posCnt.y);
	vsize.x = math::min(posBR.x, posBRThis.x) - vpos.x;
	vsize.y = math::min(posBR.y, posBRThis.y) - vpos.y;
	if (parent != NULL) {
		parent->scissorClip(vpos, vsize);
	}
	vpos = toContainerSpace(vpos);
}


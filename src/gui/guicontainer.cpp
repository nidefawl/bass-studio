#include <nanovg.h>
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
		assert(g->parent == this);
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
	assert(isBackgroundRendered());
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
	nvgBeginPath(vg);
	if (isHorizontalTitle) {
		nvgRoundedRectVarying(vg, 0, 0, size.x, hpt, fRnd, fRnd, 0, 0);
	} else {
		nvgRoundedRectVarying(vg, 0, 0, hpt, size.y, fRnd, fRnd, 0, 0);
	}
	nvgFillColor(vg, c);
	nvgFill(vg);
	if (text[0]) {
		if (isHorizontalTitle) {
			setFont(vg, (int) (hpt * 0.8), getContrastFontColorNvg(c), G_TITLE_ALIGN);
			nvgText(vg, textOffsetX + INSET_TITLE, hpt / 2, StringAsCStr(text), NULL);
		} else {
			setFont(vg, (int) (hpt * 0.8), G_WHITE, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
			nvgSave(vg);
			nvgTranslate(vg, hpt / 2, textOffsetX);
			nvgRotate(vg, -M_PI / 2.0);
			//		nvgTranslate(vg, -HEIGHT_PLUGIN_TITLE, 0);
			//		nvgText(vg, 0, 0, StringAsCStr(this->text), NULL);
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
struct guictr_tabbed::tabbed_entry
{
	guibutton tabButton;
	guibase* tabCtr;
	bool active = false;
	tabbed_entry(guibase* _ctr, String title) : tabButton(), tabCtr(_ctr) {
		tabButton.setText(title);
		tabButton.setEnabledRef(&active);
	}
};
int32_t guictr_tabbed::getNumEntries() {
	return entries.size();
}
void guictr_tabbed::setActiveEntry(int32_t idx) {
	if (idx >= 0 && idx < entries.size()) {
		guictr_tabbed::tabbed_entry* entry = entries[idx];
		if (this->activeEntry) {
			this->activeEntry->active = false;
			this->removeUNCHECKED(this->activeEntry->tabCtr);
		}
		this->activeEntry = entry;
		this->activeEntry->active = true;
		this->add(this->activeEntry->tabCtr);
		if (this->parentCtrl) {
			this->layout();
		}
	}

}
void guictr_tabbed::buttonClicked(guibase* button) {
	auto it = std::find_if(entries.begin(), entries.end(), [button](const guictr_tabbed::tabbed_entry* entry) {
		return &entry->tabButton == button;
	});
	if (it != entries.end()) {
		size_t pos = it-entries.begin();
		setActiveEntry((int32_t) pos);
	}
}
guictr_tabbed::~guictr_tabbed() {
	for (tabbed_entry* entry : entries) {
		remove(&entry->tabButton);
		delete entry;
	}
	// only this->activeEntry->tabCtr should be in this cointainer
	// at this point. And it must be a valid pointer
	assert(guis.size() <= 1);
	removeGuis();
}
void guictr_tabbed::addEntry(guibase* ctr, String title) {
	guictr_tabbed::tabbed_entry* entry = new guictr_tabbed::tabbed_entry{ctr, title};
	guictr_base::add(&entry->tabButton);
	this->entries.push_back(entry);
}
void guictr_tabbed::render(NVGcontext* vg) {
	guictr_base::render(vg);
}
void guictr_tabbed::layout() {
	ivec2 csize = getSizeContent();
	int nEntries = entries.size();
	int sizePer = nEntries ? csize.x / entries.size() : csize.x;
	ivec2 sizeBar(0);
	for (tabbed_entry* entry : entries) {
		entry->tabButton.pos = ivec2(sizeBar.x, 0);
		entry->tabButton.size = ivec2(sizePer, HEIGHT_DEFAULT_INPUT);
		sizeBar.x = math::max(sizeBar.x, entry->tabButton.right()+INSET_CTR_SPACING);
		sizeBar.y = math::max(sizeBar.y, entry->tabButton.bottom()+INSET_CTR_SPACING);
		entry->tabButton.layout();
	}
	sizeContentTab = ivec2(csize.x, csize.y-sizeBar.y);
	for (tabbed_entry* entry : entries) {
		entry->tabCtr->pos = ivec2(0, sizeBar.y);
		entry->tabCtr->size = sizeContentTab;
		entry->tabCtr->determineSize(entry->tabCtr->size);
		entry->tabCtr->layout();
	}

}


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
#include "gui.h"
#include "guicontainer.h"
#include "button.h"

using glm::vec2;
using glm::ivec2;
using glm::vec4;
using glm::ivec4;


void guictr_base::renderTitleBarHorizontal(NVGcontext* vg, String text, float textOffsetX, int flags) {
	NVGcolor c;
	if (flags & FLAG_SELECTED) {
		c = theme->getColor(COL_BG_DRK_SELECTED);
	} else if (flags & FLAG_FOCUSED) {
		c = theme->getColor(COL_BG_DRK_FOCUSED);
	} else {
		c = theme->getColor(COL_BG_BRT);
	}
	ivec2 sizeContent = getSizeContent();
	const int32_t hpt = theme->get(G_PLUGIN_TITLE_HEIGHT);
	nvgBeginPath(vg);
	nvgRoundedRectVarying(vg, 0, 0, sizeContent.x, hpt, G_RND, G_RND, 0, 0);
	nvgFillColor(vg, c);
	nvgFill(vg);
	if (text[0]) {
		setFont(vg, (int)(hpt*0.8), G_WHITE, G_TITLE_ALIGN);
		nvgText(vg, textOffsetX+INSET_TITLE, hpt / 2, StringAsCStr(text), NULL);
	}
}
void guictr_base::render(NVGcontext* vg) {
	if (!setScissorTransform(vg)) {
		return;
	}
	for (auto c : guis) {
		nvgSave(vg);
		c->render(vg);
		nvgRestore(vg);
	}
}
void guictr_base::renderFrameBase(NVGcontext* vg) {
	ivec2 sizeContent = getSizeContent();
	nvgBeginPath(vg);
	nvgRoundedRect(vg, 0, 0, sizeContent.x, sizeContent.y, G_RND);
	nvgFillColor(vg, theme->getFrameColorBase());
	nvgFill(vg);
}
void guictr_base::renderFrameOutline(NVGcontext* vg) {
	nvgBeginPath(vg);
	ivec2 sizeContent = getSizeContent();
	nvgRect(vg, 0, 0, sizeContent.x, sizeContent.y);
	nvgStrokeColor(vg, theme->getFrameColorOutline());
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

struct guictr_tabbed::tabbed_entry
{
	guibutton tabButton;
	guictr_base* tabCtr;
	bool active = false;
	tabbed_entry(guictr_base* _ctr, String title) : tabButton(), tabCtr(_ctr) {
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
		if (this->parent) {
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
		guictr_base::remove(&entry->tabButton);
	}
	if (this->activeEntry) {
		this->remove(this->activeEntry->tabCtr);
	}
	for (tabbed_entry* entry : entries) {
		delete entry;
	}
	entries.clear();
}
void guictr_tabbed::addEntry(guictr_base* ctr, String title) {
	guictr_tabbed::tabbed_entry* entry = new guictr_tabbed::tabbed_entry{ctr, title};
	guictr_base::add(&entry->tabButton);
	this->entries.push_back(entry);
}
void guictr_tabbed::render(NVGcontext* vg) {
	if (!setScissorTransform(vg)) {
		return;
	}
	for (auto c : guis) {
		nvgSave(vg);
		c->render(vg);
		nvgRestore(vg);
	}
}
void guictr_tabbed::layout() {
	ivec2 csize = getSizeContent();
	int nEntries = entries.size();
	int sizePer = nEntries ? csize.x / entries.size() : csize.x;
	ivec2 sizeBar(0);
	for (tabbed_entry* entry : entries) {
		entry->tabButton.pos = ivec2(sizeBar.x, 0);
		entry->tabButton.size = ivec2(sizePer, HEIGHT_DEFAULT_INPUT);
		sizeBar.x = std::max(sizeBar.x, entry->tabButton.right()+INSET_CTR_SPACING);
		sizeBar.y = std::max(sizeBar.y, entry->tabButton.bottom()+INSET_CTR_SPACING);
		entry->tabButton.layout();
	}
	sizeContentTab = ivec2(csize.x, csize.y-sizeBar.y);
	for (tabbed_entry* entry : entries) {
		entry->tabCtr->pos = ivec2(0, sizeBar.y);
		entry->tabCtr->size = sizeContentTab;
		entry->tabCtr->determineSize();
		entry->tabCtr->layout();
	}

}

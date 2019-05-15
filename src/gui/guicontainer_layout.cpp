#include <nanovg.h>
#include "math/vec.h"
#include "math/seq_math.h"
#include "guiglobals.h"
#include "guicolors.h"
#include "guiconstant.h"
#include "gui.h"
#include "guicontainer.h"
#include "guicontainer_layout.h"
#include "basectrl.h"
#include "color_util.h"
#include "exceptions.h"
#include "mouse.h"
#include "event.h"
#include "renderresources.h"
#include "button.h"
#include "splitter.h"

struct guictr_tabbed::tabbed_entry
{
	guibutton tabButton;
	guibase* tabCtr;
	bool active = false;
	tabbed_entry(guibase* _ctr, String title) : tabButton(), tabCtr(_ctr) {
		tabButton.setText(title);
		tabButton.setEnabledRef(&active);
		tabButton.setFontScale(0.7f);
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
	if (parent) {
		parent->buttonClicked(button);
	}
}
guictr_tabbed::~guictr_tabbed() {
	for (tabbed_entry* entry : entries) {
		remove(&entry->tabButton);
		delete entry;
	}
	// only this->activeEntry->tabCtr should be in this cointainer
	// at this point. And it must be a valid pointer
	dbgassert(guis.size() <= 1);
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
	int csW = csize.x-insetMenuBar.x-INSET_CTR_SPACING;
	int sizePer = nEntries ? (csW) / nEntries : csW;
	ivec2 sizeBar(insetMenuBar);
	for (tabbed_entry* entry : entries) {
		entry->tabButton.pos = ivec2(sizeBar.x, insetMenuBar.y);
		entry->tabButton.size = ivec2(sizePer-INSET_CTR_SPACING/2, HEIGHT_DEFAULT_INPUT);
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

struct guictr_stacked::stacked_entry
{
	Splitter splitter;
	guibuttontoggle btnHideEntry;
	guictr_base* tabCtr;
	bool active = true;
	float splitterScale;
	stacked_entry(guictr_base* _ctr, String title) : splitter(0, 0.5), btnHideEntry(), tabCtr(_ctr) {
		splitterScale = splitter.getScale();
		btnHideEntry.setText(title);
		btnHideEntry.state = &active;
		btnHideEntry.setRadius(HEIGHT_DEFAULT_INPUT/2);
		btnHideEntry.getIcon = [this]{return active?ICON_ARR_DOWN:ICON_ARR_RIGHT;};
		btnHideEntry.pos = ivec2(INSET_CTR_SPACING, INSET_CTR_SPACING);
	}
};
int32_t guictr_stacked::getNumEntries() {
	return entries.size();
}
void guictr_stacked::toggleEntry(int32_t idx, int flags) {
	if (idx >= 0 && idx < entries.size()) {
		guictr_stacked::stacked_entry* entry = entries[idx];
		if ((flags&2) && entry->active) {
			return;
		}
		if (flags&1) {
			if (entry->active) {
				entry->splitterScale = entry->splitter.getScale();
			} else {
				entry->splitter.setScale(entry->splitterScale);
			}
		}
		entry->active = !entry->active;
		if (this->parentCtrl) {
			this->layout();
		}
	}

}
void guictr_stacked::buttonClicked(guibase* button) {
	auto it = std::find_if(entries.begin(), entries.end(), [button](const guictr_stacked::stacked_entry* entry) {
		return (&entry->btnHideEntry == button) || (button->parent == entry->tabCtr);
	});
	if (it != entries.end()) {
		size_t pos = it-entries.begin();
		toggleEntry((int32_t) pos, (&(*it)->btnHideEntry == button ? 1 : (2|1)));
		return;
	}
//	if (parent) {
//		parent->buttonClicked(button);
//	}
}
guictr_stacked::~guictr_stacked() {
	for (stacked_entry* entry : entries) {
		remove(entry->tabCtr);
		remove(&entry->splitter);
		entry->tabCtr->remove(&entry->btnHideEntry);
		delete entry;
	}
	dbgassert(guis.size() <= 1);
	removeGuis();
}
void guictr_stacked::addEntry(guictr_base* ctr, String title) {
	guictr_stacked::stacked_entry* entry = new guictr_stacked::stacked_entry{ctr, title};
	entry->splitter.notifyCtrl = this;
	ctr->add(&entry->btnHideEntry);
	guictr_base::add(ctr);
	guictr_base::add(&entry->splitter);
	this->entries.push_back(entry);
}
void guictr_stacked::render(NVGcontext* vg) {
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
void guictr_stacked::handleSplitterChanged(Splitter& splitter, float scale, int clampedAt) {
//	log_printf("splitter change %012X %f %d\n", (int64_t)&splitter, scale, clampedAt);
	auto it = std::find_if(entries.begin(), entries.end(), [addrOfSplitter=&splitter](const auto* entry) {
		return &entry->splitter == addrOfSplitter;
	});
	if (it != entries.end()) {
		auto pos = it-entries.begin();
		auto entry = *it;
		auto getEntryHeight = [this](int32_t idx, auto& entries, ivec2 csize) -> int32_t {
		//	ivec2 csize = getSizeContent();
			int32_t totalH = csize.y;
			int len = entries.size();
			for (int i = 0; i < len; i++) {
				auto* entry = entries[i];
				int32_t verticalHeight;
				if (entry->active || i == idx) {
					verticalHeight = entry->splitter.leftOrTop(totalH);
					if (i == len - 1) {
						verticalHeight = totalH;
					}
				} else {
					verticalHeight = getCollapsedCtrHeight(entry->tabCtr);
					if (i == len - 1) {
						verticalHeight = math::min(verticalHeight, totalH);
					}
				}
				if (idx == i) {
					return verticalHeight;
				}
				totalH -= verticalHeight;
			}
			return 0;
		};
		int32_t newHeight = getEntryHeight(pos, entries, getSizeContent());
		int32_t minSize = getCollapsedCtrHeight(entry->tabCtr);
		bool oldState = entry->active;
		bool newState = oldState;
		if (entry->active && newHeight < minSize+5) {
			newState = false;
		} else if (!entry->active && newHeight > minSize+10) {
			newState = true;
		}
//		log_printf("splitter change for %d, active: %d, splitterClamped: %d\n", pos, entry->active, oldState);
//		log_printf("newHeight: %d, minSize %d\n", newHeight, minSize);
		if (newState != oldState) {
			toggleEntry(pos, 0);
		} else {

			this->layout();
		}
		return;
	}
	dbgassert(0&&"entry not found");
}
int32_t guictr_stacked::getCollapsedCtrHeight(guictr_base* ctr) {
	ivec2 ctrPadding = ctr->getPadding();
	return ctrPadding.y + HEIGHT_DEFAULT_INPUT + INSET_CTR_SPACING*2;
}
void guictr_stacked::layout() {
	ivec2 csize = getSizeContent();
	ivec2 posOffset(0);
	int32_t totalH = csize.y;
	int len = entries.size();
	for (int i = 0; i < len; i++) {
		auto* entry = entries[i];
		int32_t verticalHeight;
		if (entry->active) {
			verticalHeight = entry->splitter.leftOrTop(totalH);
			if (i == len - 1) {
				verticalHeight = totalH;
			}
		} else {
			verticalHeight = getCollapsedCtrHeight(entry->tabCtr);
			if (i == len - 1) {
				verticalHeight = math::min(verticalHeight, totalH);
			}
		}
		entry->tabCtr->pos = posOffset;
		entry->tabCtr->size = {csize.x, verticalHeight};
		entry->splitter.pos = {0, entry->tabCtr->bottom()-Splitter::SPLITTER_LAYOUT_THICKNESS/2};
		entry->splitter.size = {csize.x, Splitter::SPLITTER_LAYOUT_THICKNESS};
		posOffset.y += verticalHeight;
		totalH -= verticalHeight;
		entry->splitter.layout();
		entry->tabCtr->layout();
	}
}

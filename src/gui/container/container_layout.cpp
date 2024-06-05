#include <nanovg.h>
#include "math/vec.h"
#include "math/seq_math.h"
#include "guiglobals.h"
#include "guicolors.h"
#include "guiconstant.h"
#include "gui/gui.h"
#include "container.h"
#include "container_layout.h"
#include "basectrl.h"
#include "color_util.h"
#include "exceptions.h"
#include "mouse.h"
#include "event.h"
#include "renderresources.h"
#include "gui/controls/button.h"
#include "gui/controls/splitter.h"
#include "host/daw/mainctrl.h"
#include "seq_util.h"

struct guictr_tabbed::tabbed_entry {
    guibutton tabButton;
    guibase* tabCtr;
    bool active = false;
    tabbed_entry(guibase* _ctr, String title) : tabButton(), tabCtr(_ctr) {
        tabButton.setText(title);
    }
};
int32_t guictr_tabbed::getNumEntries() {
    return CtrSize(entries);
}
void guictr_tabbed::setActiveEntry(int32_t idx) {
    if (idx >= 0 && idx < CtrSize(entries)) {
        guictr_tabbed::tabbed_entry* entry = entries[idx];
        if (this->activeEntry) {
            this->activeEntry->active = false;
            this->removeUNCHECKED(this->activeEntry->tabCtr);
        }
        this->activeEntry         = entry;
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
        size_t pos = it - entries.begin();
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
    guictr_tabbed::tabbed_entry* entry = new guictr_tabbed::tabbed_entry{ ctr, title };
    guictr_base::add(&entry->tabButton);
    this->entries.push_back(entry);
}

void guictr_tabbed::handleDraggedBegin(MouseEvent& evt) {
    hasDragged = false;
}
void guictr_tabbed::handleDraggedMove(MouseEvent& evt) {
    if (!hasDragged && (evt.dragDistance->x != 0 || evt.dragDistance->y != 0)) {
        parentCtrl->setDragged(this);
        hasDragged = true;
    }
}
void guictr_tabbed::render(NVGcontext* vg) {
    guictr_base::render(vg);
}
void guictr_tabbed::layout() {
    ivec2 csize  = getSizeContent();
    int nEntries = entries.size();
    int csW      = csize.x - insetMenuBar.x - INSET_CTR_SPACING;
    int sizePer  = nEntries ? (csW) / nEntries : csW;
    ivec2 sizeBar(insetMenuBar);
    for (tabbed_entry* entry : entries) {
        entry->tabButton.pos  = ivec2(sizeBar.x, insetMenuBar.y);
        entry->tabButton.size = ivec2(sizePer - INSET_CTR_SPACING / 2, HEIGHT_DEFAULT_INPUT);
        sizeBar.x             = math::max(sizeBar.x, entry->tabButton.right() + INSET_CTR_SPACING);
        sizeBar.y             = math::max(sizeBar.y, entry->tabButton.bottom() + INSET_CTR_SPACING);
        entry->tabButton.layout();
    }
    sizeContentTab = ivec2(csize.x, csize.y - sizeBar.y);
    for (tabbed_entry* entry : entries) {
        if (entry->active && entry->tabCtr->parent) {
            entry->tabCtr->pos  = ivec2(0, sizeBar.y);
            entry->tabCtr->size = sizeContentTab;
            entry->tabCtr->determineSize(entry->tabCtr->size);
            entry->tabCtr->layout();
        }
    }
}

struct guictr_stacked::stacked_entry {
    Splitter splitter;
    guibuttontoggle btnHideEntry;
    guictr_base* tabCtr;
    bool active = true;
    float splitterScale;
    stacked_entry(guictr_base* _ctr, String title)
        : splitter(0, 0.5),
        btnHideEntry(),
        tabCtr(_ctr)
    {
        splitterScale = splitter.getScale();
        btnHideEntry.setText(title);
        //TODO: mark as active
        btnHideEntry.setStateRef(&active);
        btnHideEntry.setRadius(HEIGHT_DEFAULT_INPUT / 2);
        btnHideEntry.getIcon = [this] { return active ? ICON_ARR_DOWN : ICON_ARR_RIGHT; };
        btnHideEntry.pos     = ivec2(INSET_CTR_SPACING, INSET_CTR_SPACING);
    }
};
int32_t guictr_stacked::getNumEntries() {
    return CtrSize(entries);
}
void guictr_stacked::toggleEntry(int32_t idx, int flags) {
    if (idx >= 0 && idx < CtrSize(entries)) {
        guictr_stacked::stacked_entry* entry = entries[idx];
        if ((flags & 2) && entry->active) {
            return;
        }
        if (flags & 1) {
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
        size_t pos = it - entries.begin();
        toggleEntry((int32_t) pos, (&(*it)->btnHideEntry == button ? 1 : (2 | 1)));
        return;
    }
    if (parent) {
       parent->buttonClicked(button);
    }
}
guictr_stacked::~guictr_stacked() {
    for (stacked_entry* entry : entries) {
        remove(entry->tabCtr);
        remove(&entry->splitter);
        // entry->tabCtr->remove(&entry->btnHideEntry);
        delete entry;
    }
    entries.clear();
    dbgassert(guis.size() <= 1);
    removeGuis();
}
void guictr_stacked::addEntry(guictr_base* ctr) {
    auto const entry = new guictr_stacked::stacked_entry{ ctr, ctr->getLabel() };
    // ctr->add(&entry->btnHideEntry);
    entry->splitter.setSplitterType(bVerticalLayout ? 0 : 1);
    guictr_base::add(ctr);
    entry->splitter.setCallback(this);
    guictr_base::add(&entry->splitter);
    this->entries.push_back(entry);
    updateSplitterPositions();
}
void guictr_stacked::setSplitters(const std::vector<float>& splitterPos) {
    dbgassert(splitterPos.size() <= entries.size());
    for (size_t i = 0; i < splitterPos.size(); ++i) {
        entries[i]->splitter.setScale(splitterPos.begin()[i]);
    }
    updateSplitterPositions();
}
void guictr_stacked::getSplitterPositions(std::vector<float>& splitterPos) {
    splitterPos.clear();
    for (auto* entry : entries) {
        splitterPos.push_back(entry->splitter.getScale());
    }
}
void guictr_stacked::updateSplitterPositions() {
    for (size_t i = 0; i < entries.size(); ++i) {
        auto min = i == 0 ? 0 : entries[i - 1]->splitter.getScale();
        auto max = i == entries.size() - 1 ? 1 : entries[i + 1]->splitter.getScale();
        entries[i]->splitter.setMinMax(min, max);
        entries[i]->splitter.setScale(entries[i]->splitter.getScaleClamped());
    }
    if (!entries.empty()) {
        entries.back()->splitter.setMinMax(1, 1);
        entries.back()->splitter.setScale(1.0f);
    }
}
void guictr_stacked::removeEntries() {
    for (auto* entry : entries) {
        remove(entry->tabCtr);
        remove(&entry->splitter);
        // entry->tabCtr->remove(&entry->btnHideEntry);
        delete entry;
    }
    entries.clear();
}
void guictr_stacked::renderBackground(NVGcontext* vg) {
    if (!isRenderableSizeAndContext(vg))
        return;
    // dbgassert(isBackgroundRendered());
    // bool focused = parentCtrl->isCtrOrChildFocused(this);
    float h = titleHeight;
    titleHeight = 0;
    drawBackground(vg, theme, getPosContent(), getSizeContent(), margin, isBackgroundRenderedInset());
    titleHeight = h;
    renderContainerLabel(vg);
    /* render debug background if gui flag 1<<16 is set */
    if ((this->id & (1 << 16)) && size.x > 0 && size.y > 0) {
        nvgBeginPath(vg);
        nvgRect(vg, pos.x, pos.y, size.x, size.y);
        nvgFillColor(vg, rgbaToNvg(0x7fff00ff));
        nvgFill(vg);
    }
}
void guictr_stacked::render(NVGcontext* vg) {
    guictr_base::render(vg);
}
void guictr_stacked::handleSplitterChanged(Splitter& splitter, float scale, int clampedAt) {
    updateSplitterPositions();
    this->layout();
}
ivec2 guictr_stacked::getContainerPos() {
    return toScreenSpace({getTitleHeight(), 0});
}
ivec2 guictr_stacked::getContainerSize() {
    return ivec2(size.x, size.y - getTitleHeight());
}
int32_t guictr_stacked::getCollapsedCtrHeight(guictr_base* ctr) {
    ivec2 ctrPadding = ctr->getPadding();
    return ctrPadding.y + HEIGHT_DEFAULT_INPUT + INSET_CTR_SPACING * 2;
}
void guictr_stacked::layout() {
    ivec2 csize = getSizeContent();
    ivec2 posOffset(0);
    auto totalS = csize;
    int len     = entries.size();
    int32_t prevS = 0;
    for (int i = 0; i < len; i++) {
        auto* entry = entries[i];
        int32_t s;
        if (entry->active) {
            s = entry->splitter.leftOrTop(totalS[bVerticalLayout]);
            if (i == len - 1) {
                s = totalS[bVerticalLayout];
            }
        } else {
            s = getCollapsedCtrHeight(entry->tabCtr);
            if (i == len - 1) {
                s = math::min(s, totalS[bVerticalLayout]);
            }
        }
        entry->tabCtr->pos   = posOffset;
        if (bVerticalLayout) {
            entry->tabCtr->size  = { csize.x, s - prevS };
            entry->splitter.pos  = { 0, entry->tabCtr->bottom() - Splitter::SPLITTER_LAYOUT_THICKNESS / 2 };
            entry->splitter.size = { csize.x, Splitter::SPLITTER_LAYOUT_THICKNESS };
            posOffset.y = entry->tabCtr->bottom();
        } else {
            entry->tabCtr->size  = { s - prevS, csize.y };
            entry->splitter.pos  = { entry->tabCtr->right() - Splitter::SPLITTER_LAYOUT_THICKNESS / 2, 0 };
            entry->splitter.size = { Splitter::SPLITTER_LAYOUT_THICKNESS, csize.y };
            posOffset.x = entry->tabCtr->right();
        }
        prevS = s;
        entry->splitter.layout();
        entry->tabCtr->layout();
    }
}

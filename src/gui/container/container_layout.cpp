#include <nanovg.h>
#include "gui/container/container.hpp"
#include "logging.hpp"
#include "math/vec.hpp"
#include "math/seq_math.hpp"
#include "guiglobals.hpp"
#include "guicolors.hpp"
#include "guiconstant.hpp"
#include "gui/gui.hpp"
#include "container.hpp"
#include "container_layout.hpp"
#include "basectrl.hpp"
#include "color_util.hpp"
#include "exceptions.hpp"
#include "mouse.hpp"
#include "event.hpp"
#include "renderresources.hpp"
#include "gui/controls/button.hpp"
#include "gui/controls/splitter.hpp"
#include "host/daw/mainctrl.hpp"
#include "seq_util.hpp"

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
    guibase* entry;
    float splitterScale = 0.5f;
    int32_t fixedSize = -1;
    explicit stacked_entry(guibase* _ctr)
        : splitter(0, 0.5),
        entry(_ctr)
    {
        splitterScale = splitter.getScale();
    }
};

int32_t guictr_stacked::getNumEntries() {
    return CtrSize(entries);
}

void guictr_stacked::buttonClicked(guibase* button) {
    if (parent) {
       parent->buttonClicked(button);
    }
}

void guictr_stacked::removeGuis() {
    for (stacked_entry* entry : entries) {
        remove(entry->entry);
        remove(&entry->splitter);
        // entry->tabCtr->remove(&entry->btnHideEntry);
        delete entry;
    }
    entries.clear();
    dbgassert(guis.size() <= 1);
    guictr_base::removeGuis();
}

guictr_stacked::~guictr_stacked() {
    removeGuis();
}

void guictr_stacked::addEntry(guibase* ctr) {
    auto const entry = new guictr_stacked::stacked_entry{ ctr };
    // ctr->add(&entry->btnHideEntry);
    entry->splitter.setSplitterType(bVerticalLayout ? 0 : 1);
    guictr_base::add(ctr);
    entry->splitter.setCallback(this);
    // add splitter of entry at idx - 1:
    if (!entries.empty()) {
        auto* prevEntry = entries.back();
        guictr_base::add(&prevEntry->splitter);
    }
    this->entries.push_back(entry);
    if (this->parent)
        updateSplitterPositions();
}

void guictr_stacked::setSplitters(const std::vector<float>& splitterPos) {
    dbgassert(splitterPos.size() <= entries.size());
    for (size_t i = 0; i < splitterPos.size(); ++i) {
        entries[i]->splitter.setScale(splitterPos.begin()[i]);
    }
    if (this->parent)
        updateSplitterPositions();
}

void guictr_stacked::setFixedHeight(int32_t idx, int32_t height) {
    if (idx >= 0 && idx < CtrSize(entries)) {
        entries[idx]->fixedSize = height;
    }
}

void guictr_stacked::getSplitterPositions(std::vector<float>& splitterPos) {
    splitterPos.clear();
    for (auto* entry : entries) {
        splitterPos.push_back(entry->splitter.getScale());
    }
}

void guictr_stacked::updateSplitterPositions() {
    if (!(this->size.x > 0 && this->size.y > 0) || entries.empty()) {
        return;
    }
    auto minSizePx = Splitter::SPLITTER_LAYOUT_THICKNESS * 3.0f;
    auto layoutCtrSize = math::max(32.0f, float(bVerticalLayout ? size.y : size.x));
    auto minScale = minSizePx / layoutCtrSize;
    auto numEntries = CtrSize(entries);
    if (entries.size() > 0) {
        entries.front()->splitter.setMin(minScale);
    }
    if (entries.size() > 1) {
        entries.back()->splitter.setMax(1.0f - minScale);
        entries.back()->splitter.setScale(1.0f);
    }
    for (int32_t i = 0; i < numEntries; ++i) {
        auto& entrySplitter = entries[i]->splitter;
        auto scalePrev = i == 0 ? minScale : entries[i - 1]->splitter.getScale();
        auto scaleNext = i == numEntries - 1 ? 1 - minScale : entries[i + 1]->splitter.getScale();
        if (i != 0)
            scalePrev = math::min(scalePrev + minScale, 1.0f - minScale);
        if (i != numEntries - 1)
            scaleNext = math::max(scaleNext - minScale, minScale);
        entrySplitter.setMin(scalePrev);
        entrySplitter.setMax(scaleNext);
        entrySplitter.setScale(entrySplitter.getScaleClamped());
    }
}

void guictr_stacked::removeEntries() {
    for (auto* entry : entries) {
        remove(entry->entry);
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
    drawBackground(vg, theme, getPosContent(), getSizeContent(), margin, isBackgroundRenderedInset());
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
    return {size.x, size.y - getTitleHeight()};
}

void guictr_stacked::layout() {
    auto titleHeight = getTitleHeight();
    const ivec2 csize = getSizeContent() - ivec2(0, titleHeight);
    const int32_t len = CtrSize(entries);
    int32_t prevS = 0;
    ivec2 posOffset(0, titleHeight);
    for (int32_t i = 0; i < len; i++) {
        auto* entry = entries[i];
        int32_t s = 0;
        if (entry->fixedSize < 0) {
            s = entry->splitter.leftOrTop(csize[bVerticalLayout]);
            if (i == len - 1) {
                s = csize[bVerticalLayout];
            }
        } else {
            s = prevS + entry->fixedSize;
            if (i == len - 1) {
                s = math::min(s, csize[bVerticalLayout]);
            }
        }
        entry->entry->pos   = posOffset;
        if (bVerticalLayout) {
            entry->entry->size  = { csize.x, s - prevS };
            entry->splitter.pos  = { 0, entry->entry->bottom() - Splitter::SPLITTER_LAYOUT_THICKNESS / 2 };
            entry->splitter.size = { csize.x, Splitter::SPLITTER_LAYOUT_THICKNESS };
            posOffset.y = entry->entry->bottom();
        } else {
            entry->entry->size  = { s - prevS, csize.y };
            entry->splitter.pos  = { entry->entry->right() - Splitter::SPLITTER_LAYOUT_THICKNESS / 2, 0 };
            entry->splitter.size = { Splitter::SPLITTER_LAYOUT_THICKNESS, csize.y };
            posOffset.x = entry->entry->right();
        }
        prevS = s;
        entry->splitter.layout();
        entry->entry->layout();
    }
}

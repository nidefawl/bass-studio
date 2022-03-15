#include "guicontainer_dnd_layout.h"
#include "basectrl.h"
#include "host/mainctrl.h"
#include "gui/container/guicontainer_layout_types.h"
#include "platform.h"
#include "fileio.h"

#include <cereal/cereal.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/polymorphic.hpp>

static const int32_t dropIndicatorWidth = 8;
class guictr_layout_entry_handle_button : public guibutton {
public:
    guictr_layout_entry_handle_button() : guibutton() {
    }
    void render(NVGcontext* vg) override {
        int32_t fl = getStateFlags();
        //renderWidgetBorder(vg, fl);
        renderButtonLabel(vg, fl);
    }
    void renderButtonLabel(NVGcontext* vg, int32_t stateFlags) {
        if (drawFn || str.length()) {
            nvgSave(vg);
            setScissorTransform(vg);

            if (drawFn) {
                ivec2 renderPos(0);
                GuiColor::constant_t buttonColor = (stateFlags & FLG_HVRD) ? GuiColor::COL_LABEL_ACTIVE : GuiColor::COL_LABEL_INACTIVE;
                drawFn(vg, renderPos, size, theme->getColor(buttonColor), drawParm, isEnabled());
            }
            nvgRestore(vg);
        }
    }
};
class guictr_layout_entry_handle : public guictr_base {
    guictr_layout_entry_handle_button btnClose;
    guictr_layout_entry* const parentCtr;
    guictr_base* const ctr;
    bool hasDragged = false;
    bool hasClicked = false;

public:
    guictr_layout_entry_handle(guictr_layout_entry* _parentCtr, guictr_base* _ctr) 
      : parentCtr(_parentCtr),
      ctr(_ctr) 
    {
        (void) ctr;
        add(&btnClose);
        btnClose.drawFn = drawCross;
        setBackgroundRendered(false);
        setBackgroundRenderedInset(false);
        setCanMouseHit(true);
        setLabel(_ctr->getLabel());
        padding = 0;
        margin  = 0;
    }
    ~guictr_layout_entry_handle() override {
        remove(&btnClose);
    }
    void buttonClicked(guibase* button) override {
        if (button == &btnClose) {
            std::shared_ptr<guictr_layout_entry> out;
            parentCtr->getContainerRef(out, true);
            parentCtrl->relayout();
        }
    }
    void layout() override {
        btnClose.size = ivec2(size.y);
        btnClose.pos  = ivec2(getSizeContent().x - btnClose.size.x, 0);
        for (auto gui: guis) {
            gui->layout();
        }
    }
    void handleDraggedBegin(MouseEvent& evt) override;
    void handleDraggedMove(MouseEvent& evt) override;
    void handleDraggedRelease(MouseEvent& evt) override;
    int32_t getStateFlags() const override {
        int32_t state = guictr_base::getStateFlags();
        //if (active()) {
        //state |= FLG_ACT;
        //}
        return state;
    }
    void render(NVGcontext* vg) override;
};


bool guictr_layout::setOverlayPos(i_ctr_drop_area* area, const dock_pos dockPos, ivec2 overlayPos, ivec2 overlaySize, int32_t dockPosOfffset, int32_t childContainerIndex) {
    ivec2 relPos  = overlayPos;
    ivec2 relSize = overlaySize;
    switch (dockPos) {
        case dock_pos::LEFT:
            relPos += ivec2(-dropIndicatorWidth / 2, 0);
            relSize = ivec2(overlaySize.x / 3 + dropIndicatorWidth, overlaySize.y);
            break;
        case dock_pos::RIGHT:
            relPos += ivec2(overlaySize.x * 2 / 3 - dropIndicatorWidth / 2, 0);
            relSize = ivec2(overlaySize.x / 3 + dropIndicatorWidth, overlaySize.y);
            break;
        case dock_pos::TOP:
            relPos += ivec2(0, -dropIndicatorWidth / 2);
            relSize = ivec2(overlaySize.x, overlaySize.y / 3 + dropIndicatorWidth);
            break;
        case dock_pos::BOTTOM:
            relPos += ivec2(0, overlaySize.y * 2 / 3 - dropIndicatorWidth / 2);
            relSize = ivec2(overlaySize.x, overlaySize.y / 3 + dropIndicatorWidth);
            break;
        case dock_pos::CENTER:
            relPos += ivec2(dropIndicatorWidth / 2, dropIndicatorWidth / 2);
            relSize = ivec2(overlaySize.x - dropIndicatorWidth, overlaySize.y - dropIndicatorWidth);
            break;
        case dock_pos::STACK:
            dbgassert(0);
            return false;
        default:
            dbgassert(0);
            return false;
            //layoutGui->validPreview = false;
            //layoutGui->boxes.clear();
            //return false;
    }
    area->dockPos             = dockPos;
    area->dockPosOffset       = 0;
    area->childContainerIndex = childContainerIndex;
    area->pos                 = toScreenSpace(relPos - paddingTL(padding));
    area->size                = math::maxvec2(ivec2(relSize), ivec2(10, 10));
    area->label               = "DockPos " + std::to_string(static_cast<int32_t>(area->dockPos)) + " of " + this->getLayoutCtrName();
    return true;
}
bool guictr_layout::setOverlayPosForTab(i_ctr_drop_area* area, const dock_pos dockPos, const int32_t dockOffset, const bool rightSideHandle) {
    ivec2 relPos  = ivec2(0);
    ivec2 relSize = size;
    dbgassert(dockPos == dock_pos::STACK);
    if (dockOffset > -1 && dockOffset <= entries.size()) {
        uint32_t dockIndex                                = dockOffset >= entries.size() ? entries.size() - 1U : static_cast<uint32_t>(dockOffset);
        const std::shared_ptr<guictr_layout_entry>& entry = entries[dockIndex];
        const guibase* entryHandle                        = entry.get()->getHandle();
        if (entryHandle) {
            if (rightSideHandle) {
                relPos = paddingTL(padding) + entryHandle->pos + ivec2(entryHandle->size.x - dropIndicatorWidth / 2, 0);
            } else {
                relPos = paddingTL(padding) + entryHandle->pos + ivec2(-dropIndicatorWidth / 2, 0);
            }
            relSize = ivec2(dropIndicatorWidth, entryHandle->size.y);
        } else {
            // not expected to be called, backup code path
            relPos  = paddingTL(padding) + entry.get()->getGui()->pos - ivec2(FONT_SIZE_CTXT_SMALL + dropIndicatorWidth / 2, 0);
            relSize = ivec2(dropIndicatorWidth, FONT_SIZE_CTXT_SMALL);
        }
        area->dockPos = dockPos;
        if (rightSideHandle) {
            area->dockPosOffset = dockOffset + 1;
        } else {
            area->dockPosOffset = dockOffset;
        }
        area->pos   = toScreenSpace(relPos - paddingTL(padding));
        area->size  = math::maxvec2(ivec2(relSize), ivec2(10, 10));
        area->label = "Tab Pos " + std::to_string(static_cast<int32_t>(area->dockPosOffset)) + " of " + this->getLayoutCtrName();
        return true;
    }
    return false;
}
i_ctr_drop_area* guictr_layout::makeDropArea(int32_t idx) {
    auto& vec = dragdropContainerAreaHelpers;
    while (vec.size() <= idx) {
        vec.push_back(std::make_shared<i_ctr_drop_area>(this));
    }
    vec[idx]->childContainerIndex = -1;
    vec[idx]->dockPosOffset       = -1;
    return vec[idx].get();
}
void guictr_layout::getOverlays(MouseEvent& evt, std::vector<std::weak_ptr<i_ctr_drop_area>>& vecHandles) {
    if (!this->entries.size()) {
        setOverlayPos(makeDropArea(0), dock_pos::CENTER, ivec2(0), size, -1, -1);
        vecHandles.push_back(dragdropContainerAreaHelpers[0]);
    } else {
        int32_t areaOffset = 0;
        for (int i = 0; i < entries.size(); i++) {
            if (entries[i]->hasHandle) {
                setOverlayPosForTab(makeDropArea(areaOffset), dock_pos::STACK, i, false);
                vecHandles.push_back(dragdropContainerAreaHelpers[areaOffset]);
                areaOffset++;
                // for non tabbed always emit droparea for left and right side of handle
                if ((size_t) i + 1 == entries.size() || this->ctrLayout != container_layout::TABBED) {
                    setOverlayPosForTab(makeDropArea(areaOffset), dock_pos::STACK, i, true);
                    vecHandles.push_back(dragdropContainerAreaHelpers[areaOffset]);
                    areaOffset++;
                }
            }
        }
        if (!parent && ctrLayout == container_layout::TABBED) {
            // return overlays for all 4 sides of each container entry that is not of type guictr_layout
            for (int j = 0; j < 4; j++) {
                setOverlayPos(makeDropArea(areaOffset), static_cast<dock_pos>(static_cast<int32_t>(dock_pos::LEFT) + j), vec2(0), size, -1, -1);
                vecHandles.push_back(dragdropContainerAreaHelpers[areaOffset]);
                areaOffset++;
            }
        }
        if (ctrLayout == container_layout::SPLIT_H || ctrLayout == container_layout::SPLIT_V || ctrLayout == container_layout::SOLE) {
            // return overlays for all 4 sides of each container entry that is not of type guictr_layout
            for (int i = 0; i < entries.size(); i++) {
                for (int j = 0; j < 4; j++) {
                    setOverlayPos(makeDropArea(areaOffset), static_cast<dock_pos>(static_cast<int32_t>(dock_pos::LEFT) + j), entries[i]->pos, entries[i]->size, -1, i);
                    vecHandles.push_back(dragdropContainerAreaHelpers[areaOffset]);
                    areaOffset++;
                }
            }
        }
        if (ctrLayout == container_layout::SPLIT_V) {
            //subdivide by attaching to top or bottom
            setOverlayPos(makeDropArea(areaOffset), dock_pos::TOP, ivec2(0), size, -1, -1);
            vecHandles.push_back(dragdropContainerAreaHelpers[areaOffset++]);
            setOverlayPos(makeDropArea(areaOffset), dock_pos::BOTTOM, ivec2(0), size, -1, -1);
            vecHandles.push_back(dragdropContainerAreaHelpers[areaOffset++]);

            //keep layout, add new child
            setOverlayPos(makeDropArea(areaOffset), dock_pos::LEFT, ivec2(0), size, -1, -1);
            vecHandles.push_back(dragdropContainerAreaHelpers[areaOffset++]);
            setOverlayPos(makeDropArea(areaOffset), dock_pos::RIGHT, ivec2(0), size, entries.size(), -1);
            vecHandles.push_back(dragdropContainerAreaHelpers[areaOffset++]);
        }
        if (ctrLayout == container_layout::SPLIT_H) {
            //subdivide by attaching to left and right
            setOverlayPos(makeDropArea(areaOffset), dock_pos::LEFT, ivec2(0), size, -1, -1);
            vecHandles.push_back(dragdropContainerAreaHelpers[areaOffset++]);
            setOverlayPos(makeDropArea(areaOffset), dock_pos::RIGHT, ivec2(0), size, -1, -1);
            vecHandles.push_back(dragdropContainerAreaHelpers[areaOffset++]);


            //keep layout, add new child
            setOverlayPos(makeDropArea(areaOffset), dock_pos::TOP, ivec2(0), size, -1, -1);
            vecHandles.push_back(dragdropContainerAreaHelpers[areaOffset++]);
            setOverlayPos(makeDropArea(areaOffset), dock_pos::BOTTOM, ivec2(0), size, entries.size(), -1);
            vecHandles.push_back(dragdropContainerAreaHelpers[areaOffset++]);
        }
    }
}


guictr_layout::guictr_layout() : guictr_base() {
    ctrType = CTR_TYPE_LAYOUT;
    //setBackgroundRendered(true);
    //setBackgroundRenderedInset(true);
    this->setCanMouseHit(true);
    margin  = 0;
    padding = 0;
    //padding = 6;
    //margin = padding-4;
}
void guictr_layout::layout() {
    ivec2 cs = getSizeContent();
    switch (this->ctrLayout) {
        case container_layout::SPLIT_V:
            cs = ivec2(size.x, cs.y);
            break;
        case container_layout::SPLIT_H:
            cs = ivec2(cs.x, size.y);
            break;
        default:
            break;
    }
    vec2 segSizeF    = vec2(cs);
    vec2 axis        = vec2(0);
    int32_t nEntries = math::max<int32_t>(1, entries.size());

    switch (this->ctrLayout) {
        case container_layout::TABBED:
            segSizeF = vec2(cs.x / (float) nEntries, cs.y);
            break;
        case container_layout::SOLE:
            break;
        case container_layout::SPLIT_V:
            segSizeF = vec2(cs.x / (float) nEntries, cs.y);
            axis.x   = 1.0f;
            break;
        case container_layout::SPLIT_H:
            segSizeF = vec2(cs.x, cs.y / (float) nEntries);
            axis.y   = 1.0f;
            break;
    }

    static const int32_t handleHeight  = 20;
    static const int32_t paddingHandle = 1;
    float tabW                         = segSizeF.x;
    int32_t entryIdx                   = 0;
    if (this->ctrLayout == container_layout::TABBED) {
        axis.x                          = 1;
        static const int32_t ctrPadding = 1;
        for (auto& entry: entries) {
            auto* gui       = entry->getGui();
            auto* guiHandle = entry->getHandle();
            entry->pos = gui->pos = ivec2(ctrPadding) + ivec2(0, handleHeight);
            entry->size = gui->size = math::maxvec2(cs - ivec2(ctrPadding * 2) - ivec2(0, handleHeight), ivec2(4, 4));
            if (guiHandle) {
                guiHandle->pos  = ivec2((float) entryIdx * vec2(tabW, 0) + vec2(paddingHandle, 0));
                guiHandle->size = ivec2(math::maxvec2f(vec2(tabW, handleHeight), vec2(4, 4)) - vec2(paddingHandle * 2, 0));
            }
            entryIdx++;
        }
    } else {
        for (auto& entry: entries) {
            int controlHeight               = entry->hasHandle ? handleHeight : 0;
            auto* gui                       = entry->getGui();
            auto* guiHandle                 = entry->getHandle();
            static const int32_t ctrPadding = guiHandle && entry->hasHandle ? 1 : 0;
            vec2 segPos                     = vec2((float) entryIdx * segSizeF * axis);
            if (this->ctrLayout == container_layout::SPLIT_V || this->ctrLayout == container_layout::SPLIT_H) {
                float curScale = 1.0f;
                if (splitters.size() > entryIdx) {
                    curScale = splitters[entryIdx]->getScale();
                }
                float prevScale = 0.0f;
                if (entryIdx - 1 >= 0) {
                    prevScale = splitters[entryIdx - 1]->getScale();
                }
                segSizeF = (curScale - prevScale) * vec2(cs) * axis + vec2(cs) * vec2(axis.y, axis.x);
                if (entryIdx - 1 >= 0) {
                    segPos = splitters[entryIdx - 1]->getScale() * vec2(cs) * axis;
                }
                if (this->ctrLayout == container_layout::SPLIT_V) {
                    tabW = segSizeF.x;
                }
            }
            entry->pos = gui->pos = ivec2(segPos + vec2(0, controlHeight) + vec2(ctrPadding));
            entry->size = gui->size = ivec2(math::maxvec2f(segSizeF - vec2(ctrPadding * 2) - vec2(0, controlHeight), vec2(4, 4)));
            if (guiHandle && entry->hasHandle) {
                guiHandle->pos  = ivec2(segPos + vec2(paddingHandle, 0));
                guiHandle->size = ivec2(vec2(tabW, controlHeight) - vec2(paddingHandle * 2, 0));
            }
            if (entryIdx > 0 && splitters.size() > entryIdx - 1) {
                ivec2 splitterPos  = entry->pos;
                ivec2 splitterSize = entry->size;
                if (guiHandle && entry->hasHandle) {
                    splitterPos  = guiHandle->pos;
                    splitterSize = entry->size + guiHandle->size;
                }
                splitters[entryIdx - 1]->pos  = splitterPos - ivec2(vec2(Splitter::SPLITTER_LAYOUT_THICKNESS/2) * axis);
                auto invAxis                  = ivec2(axis.y, axis.x);
                splitters[entryIdx - 1]->size = (splitterSize) *invAxis + ivec2(vec2(Splitter::SPLITTER_LAYOUT_THICKNESS) * axis);
            }
            entryIdx++;
        }
    }
    for (auto* gui: guis) {
        gui->determineSize(gui->size);
        //TODO: do not layout invisible (tabbed) entries
        gui->layout();
    }
    for (auto& entry: entries) {
        auto* gui = entry->getGui();
        gui->size = math::minvec2(gui->size, entry->size);
    }
}

void guictr_layout_entry_handle::handleDraggedBegin(MouseEvent& evt) {
    hasDragged = false;
    if (!hasClicked) {
        hasClicked = true;
        parent->buttonClicked(this);
    }
}
void guictr_layout_entry_handle::handleDraggedMove(MouseEvent& evt) {
    dbgassert(!hasDragged);
    float fDist = math::distvec2(ivec2(0, 0), *evt.dragDistance);
    if (!hasDragged && (fDist > 2.0f)) {

        parentCtrl->dragContainerBegin(evt, parentCtr);
        hasDragged = true;
    } else if (!hasDragged) {
        //parent->buttonClicked(this);
    }
}
void guictr_layout_entry_handle::handleDraggedRelease(MouseEvent& evt) {
    if (parent)
        parent->buttonClicked(this);
}
void guictr_layout_entry_handle::render(NVGcontext* vg) {
    if (!isVisible()) {
        log_printf("warning, skip rendering container with state !isVisible()\n");
        return;
    }
    if (isBackgroundRendered()) {
        renderBackground(vg);
    }
    if (!setScissorTransform(vg)) {
        return;
    }
    int32_t stateFlags = getStateFlags();
    nvgBeginPath(vg);
    nvgRect(vg, 0, 0, size.x, size.y);
    NVGcolor bg = theme->getColor(GuiColor::COL_BASE_BG);
    if (parentCtr->getGui()->isVisible()) {
        bg = theme->getColor(GuiColor::COL_BASE_BG_FOCUSED);
    }
    nvgFillColor(vg, bg);
    nvgFill(vg);
    String str = parentCtr->getGui()->label;
    if (str.length()) {

        ivec2 renderPos(size.y / 2, size.y / 2);
        if (str.length() > 0) {
            int fontScale = 12;
            GuiColor::constant_t c = (stateFlags & FLG_ENBL) ? GuiColor::COL_LABEL_ACTIVE : GuiColor::COL_LABEL_INACTIVE;
            NVGcolor color         = theme->getColor(c);
            UTIL_setFont(vg, theme, fontScale, color, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
            nvgText(vg, renderPos.x, renderPos.y, StringAsCStr(str), NULL);
        }
    }
    for (auto c: guis) {
        if (!c->isVisible()) {
            //log_printf("warning, skip rendering child container with state !isVisible()\n");
            continue;
        }
        if (c->size.x <= 0 || c->size.y <= 0) {
            log_printf("warning, skip rendering child container with size <= 0 0\n");
            continue;
        }
        {
            nvgSave(vg);
            c->render(vg);
            nvgRestore(vg);
        }
    }
}

void i_ctr_drop_area::render(NVGcontext* vg) {
    nvgBeginPath(vg);
    nvgRect(vg, pos.x, pos.y, size.x, size.y);
    auto handleColor = rgbaToNvg(0x3f00ff00);
    switch (this->dockPos) {
        case dock_pos::STACK:
            handleColor = rgbaToNvg(0x7fffff00);
            break;
        default:
            break;
    }
    nvgFillColor(vg, handleColor);
    nvgFill(vg);
    renderTextLabel(vg,
                    vec2(pos) + vec2(0, size.y/2.0),
                    size,
                    this->label,
                    nullptr,
                    20,
                    rgbaToNvg(0xff7fff7f),
                    NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
}

Splitter* guictr_layout::getSplitter(int32_t pos) {
    return splitters[pos].get();
}
template<typename T>
void updateSplitterMinMax(T& splitters) {
    float off          = 1.0f / (splitters.size() + 1);
    int numSplitters   = splitters.size();
    const float margin = off * 0.2f;
    for (int i = 0; i < numSplitters; ++i) {
        auto& splitter           = splitters[i];
        const float prevScalePos = (i - 1 >= 0 ? splitters[i - 1]->getScale() : 0.0f) + margin;
        const float nextScalePos = (i + 1 < numSplitters ? splitters[i + 1]->getScale() : 1.0f) - margin;
        splitter->setMinMax(prevScalePos, nextScalePos);
    }
}
std::vector<float> guictr_layout::getSplitterPositions() {
    std::vector<float> splitterPos;
    splitterPos.reserve(splitters.size());
    for (auto& splitter: splitters) {
        splitterPos.push_back(splitter->getScale());
    }
    return splitterPos;
}

void guictr_layout::setSplitterPositions(std::vector<float>& splitterPositons) {
    if (splitterPositons.size() == this->splitters.size()) {
        for (size_t i = 0; i < splitterPositons.size(); ++i) {
            splitters[i]->setScale(splitterPositons[i]);
        }
        updateSplitterMinMax(splitters);
    }
}

void guictr_layout::updateSplitters() {
    for (auto& splitter: splitters) {
        if (splitter->parent) {
            guictr_base::remove(splitter.get());
        }
    }
    splitters.clear();
    int numSplitters = entries.size() - 1;
    if (numSplitters <= 0)
        return;
    if (this->ctrLayout == container_layout::SPLIT_H || this->ctrLayout == container_layout::SPLIT_V) {
        int splitterLayout = ctrLayout == container_layout::SPLIT_V ? 1 : 0;
        float off          = 1.0f / (numSplitters + 1);
        for (int i = 0; i < numSplitters; ++i) {
            float splitPos =         off + i * off;
            auto splitter  = std::make_shared<Splitter>(splitterLayout, splitPos);
            splitters.push_back(splitter);
            splitter->setMinMax(splitPos - off * 0.8f, splitPos + off * 0.8f);
            splitter->setCallback(this);
        }
        updateSplitterMinMax(splitters);
        for (auto& splitter: splitters) {
            guictr_base::add(splitter.get());
        }
    }
}
void guictr_layout::handleSplitterChanged(Splitter& splitter, float scale, int clampedAt) {
    updateSplitterMinMax(splitters);
    layout();
}
ivec2 guictr_layout::getContainerSize() {
    return size;
}
void guictr_layout::updateVisible() {
    int32_t entryIdx = 0;
    for (auto& entry: entries) {
        entry->getGui()->setVisible(this->ctrLayout != container_layout::TABBED || entryIdx == this->activePosition);
        entryIdx++;
    }
}

bool guictr_layout::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
    if (this->contains(mpos)) {
        ivec2 localMouse = this->toContainerSpace(mpos);
        for (guibase* gui: handles) {
            if (!gui->isVisible())
                continue;

            if (gui->mouseHitTest(localMouse, evt)) {
                return true;
            }
        }
        for (auto& gui: splitters) {
            if (!gui->isVisible())
                continue;

            if (gui->mouseHitTest(localMouse, evt)) {
                return true;
            }
        }
        for (guibase* gui: guis) {
            if (!gui->isVisible())
                continue;

            if (gui->mouseHitTest(localMouse, evt)) {
                return true;
            }
        }
        if (evt.type == MouseHitType::MOUSE_DRAGDROP_OBJECT) {
            evt.requestFocus(this);
            return true;
        }
        if (canMouseHit()) {
            evt.requestFocus(this);
            return true;
        }
    }
    return false;
}

bool guictr_layout::getContainerRef(guictr_layout_entry* ctr, std::shared_ptr<guictr_layout_entry>& out, bool remove) {
    auto it = std::find_if(this->entries.begin(), this->entries.end(), [ctr](auto& e) {
        return ctr == e.get();
    });
    if (it == entries.end()) {
        //throw applogicexception(StringFormat("%s - attempt to remove non-present element", StringAsCStr(getClassName())));
        dbgassert(0);
        return false;
    }
    out = *it;
    if (!remove) {
        return true;
    }
    int32_t pos        = it - entries.begin();
    bool removedActive = getActivePosition() == pos;
    entries.erase(it);
    guictr_base::remove(ctr->getGui());
    auto* guiHandle = ctr->getHandle();
    if (guiHandle) {
        removeEntry(handles, guiHandle);
        guictr_base::remove(guiHandle);
    }
    ctr->parentLayoutContainer = nullptr;
    updateSplitters();
    if (removedActive && this->ctrLayout == container_layout::TABBED) {
        if (pos - 1 >= 0 || entries.empty()) {
            pos--;
        }
        //setActiveEntry(pos);
        this->activePosition = pos;
        updateVisible();
    }
    return true;
}

void guictr_layout::addEntry(std::shared_ptr<guictr_layout_entry> ctr, int32_t posOffset) {
    auto it = std::find(entries.begin(), entries.end(), ctr);
    if (it != entries.end()) {
        throw applogicexception(StringFormat("%s - attempt to add element twice", StringAsCStr(getClassName())));
    }
    auto insertPos = posOffset == -1 ? entries.begin() : (posOffset == -2 || posOffset >= entries.size() ? entries.end() : (entries.begin() + posOffset));
    entries.insert(insertPos, ctr);
    auto guiCtr = ctr->getGui();
    // a handle is present if the child is not a guictr_layout, or if this container is using tabbed layout
    ctr->hasHandle = dynamic_cast<guictr_layout*>(guiCtr) == nullptr || this->ctrLayout == container_layout::TABBED;
    guictr_base::add(guiCtr);
    //guiCtr->snapSides = ivec4(1);
    auto* guiHandle = ctr->getHandle();
    if (guiHandle && ctr->hasHandle) {
        guictr_base::add(guiHandle);
        handles.push_back(guiHandle);
    }
    ctr->parentLayoutContainer = this;
    updateSplitters();
}

bool guictr_layout::placeContainer(std::shared_ptr<guictr_layout_entry> ctr, i_ctr_drop_area* area) {

    // prevent dropping into self
    guibase* parent = this;
    while (parent) {
        if (parent == ctr.get()->getGui()) {
            return false;
        }
        parent = parent->parent;
    }

    dock_pos dockPos      = area->dockPos;
    int32_t dockPosOffset = area->dockPosOffset;
    auto updatedCtrLayout = dock_pos_to_container_layout(dockPos);

    std::shared_ptr<guictr_layout_entry> out;
    if (ctr->parentLayoutContainer != nullptr) {
        //undock from current container
        if (!ctr->getContainerRef(out, true)) {
            return false;
        }
    }
    auto it = std::find(entries.begin(), entries.end(), ctr);
    if (it != entries.end()) {
        throw applogicexception(StringFormat("%s - attempt to add element twice", StringAsCStr(getClassName())));
    }

    if (ctrLayout != updatedCtrLayout) {
        log_printf("Container layout changed to %d\n", static_cast<int>(updatedCtrLayout));
        setLayout(updatedCtrLayout);
    }
    if (dockPos == dock_pos::STACK) {
        if (dockPosOffset <= -1) {
            entries.insert(entries.begin(), ctr);
        } else if (dockPosOffset >= entries.size()) {
            entries.insert(entries.end(), ctr);
        } else {
            entries.insert(entries.begin() + dockPosOffset, ctr);
        }
    } else if (dockPos == dock_pos::RIGHT || dockPos == dock_pos::BOTTOM) {
        entries.insert(entries.end(), ctr);
    } else {
        entries.insert(entries.begin(), ctr);
    }

    auto guiCtr    = ctr->getGui();
    ctr->hasHandle = dynamic_cast<guictr_layout*>(guiCtr) == nullptr || this->ctrLayout == container_layout::TABBED;
    guictr_base::add(guiCtr);

    auto* guiHandle = ctr->getHandle();

    if (guiHandle && ctr->hasHandle) {
        guictr_base::add(guiHandle);
        handles.push_back(guiHandle);
    }
    ctr->parentLayoutContainer = this;
    if (this->ctrLayout == container_layout::TABBED) {
        int32_t newIdx       = indexOfCtr(entries, ctr);
        this->activePosition = newIdx;
        updateVisible();
    }
    updateSplitters();

    return true;
}
void guictr_layout::render(NVGcontext* vg) {
    if (!isVisible()) {
        log_printf("warning, skip rendering container with state !isVisible()\n");
        return;
    }
    if (isBackgroundRendered()) {
        renderBackground(vg);
    }
    if (parentCtrl->isDraggingContainer()) {
        nvgBeginPath(vg);
        nvgRect(vg, pos.x, pos.y, size.x, size.y);
        nvgStrokeWidth(vg, 1.0f);
        nvgStrokeColor(vg, THEMECOL_WHITE);
        nvgStroke(vg);
    }
    if (!setScissorTransform(vg)) {
        return;
    }
    if (this->id & (1 << 16)) {
        for (auto& h: handles) {
            nvgBeginPath(vg);
            nvgRect(vg, h->pos.x, h->pos.y, h->size.x, h->size.y);
            nvgFillColor(vg, rgbaToNvg(0x7f00ff00));
            nvgFill(vg);
        }
        for (auto& e: entries) {
            auto h = e->getGui();
            if (!h) continue;
            nvgBeginPath(vg);
            nvgRect(vg, h->pos.x, h->pos.y, h->size.x, h->size.y);
            nvgFillColor(vg, rgbaToNvg(0x7fffff00));
            nvgFill(vg);
        }
    }
    for (auto& entry: entries) {
        auto container = entry->getGui();
        if (!container || !container->isVisible())
            continue;
        if (container->size.x <= 0 || container->size.y <= 0) {
            log_printf("warning, skip rendering child container with size <= 0 0\n");
            continue;
        }
        {
            nvgSave(vg);
            nvgIntersectScissor(vg, entry->pos.x, entry->pos.y, entry->size.x, entry->size.y);
            container->render(vg);
            nvgRestore(vg);
        }
    }
    for (auto& handle: handles) {
        if (handle->size.x <= 0 || handle->size.y <= 0) {
            log_printf("warning, skip rendering child container with size <= 0 0\n");
            continue;
        }
        {
            nvgSave(vg);
            handle->render(vg);
            nvgRestore(vg);
        }
    }
    for (auto& splitter: splitters) {
        if (splitter->size.x <= 0 || splitter->size.y <= 0) {
            log_printf("warning, skip rendering child container with size <= 0 0\n");
            continue;
        }
        {
            nvgSave(vg);
            splitter->render(vg);
            nvgRestore(vg);
        }
    }
}
std::shared_ptr<guictr_layout_entry> guictr_layout::replaceContainerWith(guictr_base* ctr,
                                                                         std::shared_ptr<guictr_layout> newContainer) {
    std::shared_ptr<guictr_layout> retCtr;
    auto it = std::find_if(entries.begin(), entries.end(), [ctr](std::shared_ptr<guictr_layout_entry>& e) {
        return e->getGui() == ctr;
    });
    if (it == entries.end()) {
        throw applogicexception(StringFormat("%s - attempt to remove missing ctr", StringAsCStr(getClassName())));
    }


    std::shared_ptr<guictr_layout_entry> entry = *it;// copy for return
    guictr_base::remove(entry->getGui());
    auto* guiHandle = entry->getHandle();
    if (guiHandle) {
        removeEntry(handles, guiHandle);
        guictr_base::remove(guiHandle);
    }
    entry->parentLayoutContainer = nullptr;

    int32_t posOffset = it - entries.begin();
    entries.erase(it);

    std::shared_ptr<guictr_layout_entry> entry1 = createGuiCtrLayoutEntry(newContainer);
    auto insertPos                              = entries.begin() + posOffset;

    entries.insert(insertPos, entry1);
    auto guiCtr       = entry1->getGui();
    entry1->hasHandle = dynamic_cast<guictr_layout*>(guiCtr) == nullptr || this->ctrLayout == container_layout::TABBED;
    guictr_base::add(guiCtr);
    //guiCtr->snapSides = ivec4(1);
    guiHandle = entry1->getHandle();
    if (guiHandle && entry1->hasHandle) {
        guictr_base::add(guiHandle);
        handles.push_back(guiHandle);
    }
    entry1->parentLayoutContainer = this;
    updateSplitters();
    return entry;
}

guictr_layout_entry::guictr_layout_entry(String _label, std::shared_ptr<guictr_base> _ctr)
    : type(_ctr->getContainerType()),
      frameType(_ctr->getContainerType() == CTR_TYPE_LAYOUT ? layout_ctr_type::GUICTR_LAYOUT : layout_ctr_type::GUICTR_BASE),
      ctr(_ctr),
      label(_label) {
    ctrHandle = new guictr_layout_entry_handle(this, _ctr.get());
}
guictr_layout_entry::~guictr_layout_entry() {
    delete ctrHandle;
}

guibase* guictr_layout_entry::getHandle() {
    return ctrHandle;
}

bool guictr_layout_entry::getContainerRef(std::shared_ptr<guictr_layout_entry>& out, bool remove) {
    return parentLayoutContainer->getContainerRef(this, out, remove);
}

guictr_base* guictr_layout_entry::getGui() {
    return ctr.get();
}

void storeContainerEntrySnapshot(guictr_layout_entry* ctrlayoutEntry, std::shared_ptr<guictrlayout_entry_snapshot_t>& snapshot) {
    dbgassert(ctrlayoutEntry->getType() != container_type::CTR_TYPE_BASE);
    if (ctrlayoutEntry->getFrameType() == layout_ctr_type::GUICTR_LAYOUT) {
        auto sharedSnapshot      = std::make_shared<guictrlayout_snapshot_t>();
        guictr_layout* ctrLayout = dynamic_cast<guictr_layout*>(ctrlayoutEntry->getGui());
        dbgassert(ctrLayout);
        storeContainerSnapshot(ctrLayout, sharedSnapshot.get());
        snapshot = sharedSnapshot;
    } else {
        auto sharedSnapshot = std::make_shared<guictrlayout_entry_snapshot_t>();
        snapshot            = sharedSnapshot;
    }
    snapshot->type  = ctrlayoutEntry->getType();
    snapshot->label = ctrlayoutEntry->getLabel();
}

void loadContainerEntrySnapshot(ContainerFactory& fac,
                                ContainerInstanceContext& ctxt,
                                std::shared_ptr<guictrlayout_entry_snapshot_t>& snapshot,
                                std::shared_ptr<guictr_layout_entry>& out) {
    out  = nullptr;
    if (fac.count(snapshot->type)) {
        ContainerBuilder& builder  = fac[snapshot->type];
        std::shared_ptr<guictr_base> sharedContainer = builder(ctxt);
        if (!sharedContainer) {
            log_printf("Failed building container of type %d\n", snapshot->type);
            return;
        }
        //sharedContainer->label = snapshot->label;
        getContainerLabel(snapshot->type, sharedContainer->label);
        out = createGuiCtrLayoutEntry(sharedContainer);
        if (out->getFrameType() == layout_ctr_type::GUICTR_LAYOUT) {
            auto* ctrLayoutSnapshot = dynamic_cast<guictrlayout_snapshot_t*>(snapshot.get());
            auto* ctrLayout = dynamic_cast<guictr_layout*>(out->getGui());
            dbgassert(ctrLayout);
            dbgassert(ctrLayoutSnapshot);
            loadContainerSnapshot(fac, ctxt, ctrLayout, ctrLayoutSnapshot);
        }
    } else {
        log_printf("Failed loading container of type %d\n", snapshot->type);
    }
}

bool saveDawViewLayoutSnapshot(dawview_layout_t& snapshot, const String& path) {
    using namespace cereal;
    try {
        Stringstream sstream;
        {
            JSONOutputArchive ar(sstream);
            ar(make_nvp("layout", snapshot));
        }
        sstream.flush();
        writeStringStream(App::Platform::toUserdataPath(path), sstream);
        return true;
    } catch (const FileIOException& e) {
        log_printf("savePluginSnapshot File IO exception: %s (%d)\n", e.what(), e.GetErrorCode());
    } catch (const std::exception& e) {
        log_printf("savePluginSnapshot exception: %s\n", e.what());
    }
    return false;
}

std::shared_ptr<dawview_layout_t> loadDawViewLayoutSnapshot(const String& path) {
    using namespace cereal;
    try {
        std::vector<uint8_t> vec;
        ReadFileVector(App::Platform::toUserdataPath(path), vec);
        Stringstream sstream(std::string(vec.cbegin(), vec.cend()));
        std::shared_ptr<dawview_layout_t> snapshot = std::make_shared<dawview_layout_t>();
        dawview_layout_t& ref = *snapshot.get();
        {
            JSONInputArchive ar(sstream);
            ar(make_nvp("layout", ref));
        }
        return snapshot;
    } catch (const FileIOException& e) {
        log_printf("loadDawViewLayoutSnapshot File IO exception: %s (%d)\n", e.what(), e.GetErrorCode());
    } catch (const std::exception& e) {
        log_printf("loadDawViewLayoutSnapshot exception: %s\n", e.what());
    }
    return nullptr;
}

void storeContainerSnapshot(guictr_layout* ctrlayout, guictrlayout_snapshot_t* snapshot) {
    auto& entries               = ctrlayout->getEntries();
    snapshot->splitterPositions = ctrlayout->getSplitterPositions();
    snapshot->activePosition    = ctrlayout->getActivePosition();
    snapshot->ctrLayout         = ctrlayout->getLayout();
    snapshot->entries.reserve(entries.size());
    for (auto& sharedEntry: entries) {
        std::shared_ptr<guictrlayout_entry_snapshot_t> shrdEntrySnapshot;
        storeContainerEntrySnapshot(sharedEntry.get(), shrdEntrySnapshot);
        snapshot->entries.emplace_back(std::move(shrdEntrySnapshot));
    }
}
void loadContainerSnapshot(ContainerFactory& fac,
                            ContainerInstanceContext& ctxt,
                            guictr_layout* ctrlayout,
                            guictrlayout_snapshot_t* snapshot) {
    ctrlayout->setLayout(snapshot->ctrLayout);
    for (auto& shrdEntrySnapshot: snapshot->entries) {
        std::shared_ptr<guictr_layout_entry> sharedEntry;
        loadContainerEntrySnapshot(fac, ctxt, shrdEntrySnapshot, sharedEntry);
        if (sharedEntry) {
            ctrlayout->addEntry(sharedEntry, -2);
        }
    }
    ctrlayout->setSplitterPositions(snapshot->splitterPositions);
    ctrlayout->setActiveEntry(snapshot->activePosition);
    //ctrlayout->postContentChanged();
}
template<class Archive>
void serialize(Archive& archive, guictrlayout_snapshot_t& m) {
    archive(m.label, m.type, m.activePosition, m.ctrLayout, m.entries, m.splitterPositions);
}
template<class Archive>
void serialize(Archive& archive, guictrlayout_entry_snapshot_t& m) {
    archive(m.label, m.type);
}
template<class Archive>
void serialize(Archive& archive, dawview_layout_t& m) {
    archive(m.left, m.right, m.splitterPositions);
}

CEREAL_REGISTER_TYPE(guictrlayout_snapshot_t);
CEREAL_REGISTER_POLYMORPHIC_RELATION(guictrlayout_entry_snapshot_t, guictrlayout_snapshot_t)

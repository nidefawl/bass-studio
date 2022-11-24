#include "container_dnd_layout.h"
#include "appsettings.h"
#include "assert_dbg.h"
#include "basectrl.h"
#include "event.h"
#include "gui/container/container.h"
#include "gui/contextmenu/contextmenu.h"
#include "gui/gui.h"
#include "guiglobals.h"
#include "host/daw/mainctrl.h"
#include "gui/container/container_layout_types.h"
#include "gui/dropdown/dropdown_generic.h"
#include "logging.h"
#include "math/seq_math.h"
#include "platform.h"
#include "fileio.h"
#include "seq_util.h"
#include "str_util.h"
#include "tls.h"

#include <algorithm>
#include <cereal/cereal.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/polymorphic.hpp>
#include <memory>
#include <nanovg.h>

static constexpr int32_t DROP_INDICATOR_WIDTH = 8;
class guictr_layout_entry_handle_button final : public guibutton {
public:
    guictr_layout_entry_handle_button() : guibutton() {
    }
    void render(NVGcontext* vg) override {
        if (!isRenderableSizeAndContext(vg))
            return;
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
                drawFn(vg, renderPos, size, theme->getColor(getLabelColor()), drawParm, isEnabled());
            }
            nvgRestore(vg);
        }
    }
    GuiColor::constant_t getLabelColor() const override {
        return (getStateFlags() & FLG_HVRD) && (getStateFlags() & FLG_ENBL) ? GuiColor::COL_LABEL_ACTIVE : GuiColor::COL_LABEL_INACTIVE;
    }
};

class GuiCtrLayoutEntryHandle final : public guictr_base {
    friend class guictr_layout_entry_handle_context_menu;
    guictr_layout_entry_handle_button btnClose;
    GuiCtrLayoutEntry* const parentCtr;
    guictr_base* const ctr;
    bool hasDragged = false;
    bool hasClicked = false;

public:
    GuiCtrLayoutEntryHandle(GuiCtrLayoutEntry* _parentCtr, guictr_base* _ctr) 
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
        padding = 5;
        margin  = 0;
    }
    ~GuiCtrLayoutEntryHandle() override {
        remove(&btnClose);
    }
    ivec2 paddingTL(int _padding) const override {
        return { _padding - margin * snapSides.x, 0 - margin * snapSides.y };
    }
    ivec2 paddingBR(int _padding) const override {
        return { _padding - margin * snapSides.z, 0 - margin * snapSides.w };
    }
    void buttonClicked(guibase* button) override {
        if (button == &btnClose) {
            parentCtr->removeEntryFromParent();
            parentCtrl->relayout();
        }
    }
    void layout() override {
        auto cs = getSizeContent();
        btnClose.size = ivec2(math::max(4, size.y*2/3));
        btnClose.pos  = ivec2(cs.x - btnClose.size.x, 0 + cs.y/2 - btnClose.size.y/2);
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
    void handleRightClick(MouseEvent& evt) override;
};

bool guictr_layout::setOverlayPos(DropAreaUILayout* area, const dock_pos dockPos, ivec2 overlayPos, ivec2 overlaySize, int32_t childContainerIndex) {
    ivec2 relPos  = overlayPos;
    ivec2 relSize = overlaySize;
    switch (dockPos) {
        case dock_pos::LEFT:
            relPos += ivec2(-DROP_INDICATOR_WIDTH / 2, 0);
            relSize = ivec2(overlaySize.x / 3 + DROP_INDICATOR_WIDTH, overlaySize.y);
            break;
        case dock_pos::RIGHT:
            relPos += ivec2(overlaySize.x * 2 / 3 - DROP_INDICATOR_WIDTH / 2, 0);
            relSize = ivec2(overlaySize.x / 3 + DROP_INDICATOR_WIDTH, overlaySize.y);
            break;
        case dock_pos::TOP:
            relPos += ivec2(0, -DROP_INDICATOR_WIDTH / 2);
            relSize = ivec2(overlaySize.x, overlaySize.y / 3 + DROP_INDICATOR_WIDTH);
            break;
        case dock_pos::BOTTOM:
            relPos += ivec2(0, overlaySize.y * 2 / 3 - DROP_INDICATOR_WIDTH / 2);
            relSize = ivec2(overlaySize.x, overlaySize.y / 3 + DROP_INDICATOR_WIDTH);
            break;
        case dock_pos::CENTER:
            relPos += ivec2(DROP_INDICATOR_WIDTH / 2, DROP_INDICATOR_WIDTH / 2);
            relSize = ivec2(overlaySize.x - DROP_INDICATOR_WIDTH, overlaySize.y - DROP_INDICATOR_WIDTH);
            break;
        case dock_pos::STACK:
            dbgassert(0);
            return false;
        default:
            dbgassert(0);
            return false;
    }
    area->dockPos             = dockPos;
    area->tabPosition         = 0;
    area->childContainerIndex = childContainerIndex;
    area->pos                 = toScreenSpace(relPos - paddingTL(padding));
    area->size                = math::maxvec2(ivec2(relSize), ivec2(10, 10));
    area->label               = "DockPos " + std::to_string(static_cast<int32_t>(area->dockPos)) + " of " + this->getLayoutCtrName();
    return true;
}

bool guictr_layout::setOverlayPosForTab(DropAreaUILayout* area, const dock_pos dockPos, const int32_t dockOffset, const bool rightSideHandle) {
    ivec2 relPos  = ivec2(0);
    ivec2 relSize = size;
    dbgassert(dockPos == dock_pos::STACK);
    auto numEntries = CtrSize(entries);
    if (dockOffset > -1 && dockOffset <= numEntries && numEntries > 0) {
        auto dockIndex = dockOffset >= numEntries ? static_cast<int32_t>(entries.size()) - 1U : dockOffset;
        const SPLayoutEntry& entry = entries[dockIndex];
        const guibase* entryHandle = entry->getHandle();
        if (entryHandle) {
            if (rightSideHandle) {
                relPos = paddingTL(padding) + entryHandle->pos + ivec2(entryHandle->size.x - DROP_INDICATOR_WIDTH / 2, 0);
            } else {
                relPos = paddingTL(padding) + entryHandle->pos + ivec2(-DROP_INDICATOR_WIDTH / 2, 0);
            }
            relSize = ivec2(DROP_INDICATOR_WIDTH, entryHandle->size.y);
        } else {
            // not expected to be called, backup code path
            relPos  = paddingTL(padding) + entry->getGui()->pos - ivec2(FONT_SIZE_CTXT_SMALL + DROP_INDICATOR_WIDTH / 2, 0);
            relSize = ivec2(DROP_INDICATOR_WIDTH, FONT_SIZE_CTXT_SMALL);
        }
        area->dockPos = dockPos;
        area->tabPosition = dockOffset;
        area->priority++;
        area->pos   = toScreenSpace(relPos - paddingTL(padding));
        area->size  = math::maxvec2(ivec2(relSize), ivec2(10, 10));
        area->label = "Tab Pos " + std::to_string(area->tabPosition) + " of " + this->getLayoutCtrName();
        return true;
    }
    return false;
}

DropAreaUILayout* guictr_layout::makeDropArea(int32_t idx) {
    auto& vec = dragdropContainerAreaHelpers;
    while (CtrSize(vec) <= idx) {
        vec.push_back(std::make_shared<DropAreaUILayout>(this));
    }
    auto ptr = vec[idx].get();
    ptr->init();
    int32_t containerDepth = 0;
    auto p = parent;
    while (p) {
        containerDepth++;
        p = p->parent;
    }
    ptr->priority = containerDepth;
    return ptr;
}

void guictr_layout::getOverlays(MouseEvent&, std::vector<std::weak_ptr<DropAreaUILayout>>& vecHandles) {
    int32_t areaOffset = 0;
    if (this->entries.empty() || !parent) {
        setOverlayPos(makeDropArea(areaOffset), dock_pos::CENTER, ivec2(0), size, -1);
        auto& area = dragdropContainerAreaHelpers[areaOffset];
        area->pos = toScreenSpace({});
        area->size = math::maxvec2(toScreenSpace(size) - area->pos, {0, 0});
        int minW = 48;
        auto inset = ivec2(vec2(math::maxvec2f(vec2(0), vec2(area->size - minW) * 0.2f)));
        area->pos += inset;
        area->size -= inset * 2;
        minW = 64;
        // area->priority--;
        for (auto axis : {0,1}) {
            if (area->size[axis] < minW) {
                area->pos[axis] -= (minW - area->size[axis]) / 2;
                area->size[axis] = minW;
            }
            if (area->pos[axis] < 0) { // move right
                area->pos[axis] = 0;
            }
            if (area->pos[axis] + area->size[axis] > parentCtrl->m_size[axis]) { // move left
                area->pos[axis] = parentCtrl->m_size[axis] - area->size[axis];
            }
        }
        area->setAlwaysShow(true);
        vecHandles.push_back(area);
        areaOffset++;
    }
    if (this->entries.empty()) {
        return;
    }
    {
            for (size_t i = 0; i < entries.size(); i++) {
                if (entries[i]->getFrameType() != LayoutCtrType::GUICTR_LAYOUT) {
                    setOverlayPosForTab(makeDropArea(areaOffset), dock_pos::STACK, i, false);
                    vecHandles.push_back(dragdropContainerAreaHelpers[areaOffset]);
                    areaOffset++;
                    // for non tabbed always emit droparea for left and right side of handle
                    if (i + 1 == entries.size() || this->ctrLayout != container_layout::TABBED) {
                        setOverlayPosForTab(makeDropArea(areaOffset), dock_pos::STACK, i, true);
                        vecHandles.push_back(dragdropContainerAreaHelpers[areaOffset]);
                        areaOffset++;
                    }
                }
            }
        // }
        if (!parent && ctrLayout == container_layout::TABBED) {
            // return overlays for all 4 sides of each container entry that is not of type guictr_layout
            for (int j = 0; j < 4; j++) {
                setOverlayPos(makeDropArea(areaOffset), static_cast<dock_pos>(static_cast<int32_t>(dock_pos::LEFT) + j), vec2(0), size, -1);
                vecHandles.push_back(dragdropContainerAreaHelpers[areaOffset]);
                areaOffset++;
            }
        }
        if (ctrLayout == container_layout::SPLIT_H || ctrLayout == container_layout::SPLIT_V || ctrLayout == container_layout::SOLE) {
            // return overlays for all 4 sides of each container entry that is not of type guictr_layout
            for (size_t i = 0; i < entries.size(); i++) {
                for (int j = 0; j < 4; j++) {
                    setOverlayPos(makeDropArea(areaOffset), static_cast<dock_pos>(static_cast<int32_t>(dock_pos::LEFT) + j), entries[i]->pos, entries[i]->size, i);
                    vecHandles.push_back(dragdropContainerAreaHelpers[areaOffset]);
                    areaOffset++;
                }
            }
        }
        if (ctrLayout == container_layout::SPLIT_V) {
            //subdivide by attaching to top or bottom
            setOverlayPos(makeDropArea(areaOffset), dock_pos::TOP, ivec2(0), size, -1);
            vecHandles.push_back(dragdropContainerAreaHelpers[areaOffset++]);
            setOverlayPos(makeDropArea(areaOffset), dock_pos::BOTTOM, ivec2(0), size, -1);
            vecHandles.push_back(dragdropContainerAreaHelpers[areaOffset++]);

            //keep layout, add new child
            setOverlayPos(makeDropArea(areaOffset), dock_pos::LEFT, ivec2(0), size, -1);
            vecHandles.push_back(dragdropContainerAreaHelpers[areaOffset++]);
            setOverlayPos(makeDropArea(areaOffset), dock_pos::RIGHT, ivec2(0), size, -1);
            vecHandles.push_back(dragdropContainerAreaHelpers[areaOffset++]);
        }
        if (ctrLayout == container_layout::SPLIT_H) {
            //subdivide by attaching to left and right
            setOverlayPos(makeDropArea(areaOffset), dock_pos::LEFT, ivec2(0), size, -1);
            vecHandles.push_back(dragdropContainerAreaHelpers[areaOffset++]);
            setOverlayPos(makeDropArea(areaOffset), dock_pos::RIGHT, ivec2(0), size, -1);
            vecHandles.push_back(dragdropContainerAreaHelpers[areaOffset++]);


            //keep layout, add new child
            setOverlayPos(makeDropArea(areaOffset), dock_pos::TOP, ivec2(0), size, -1);
            vecHandles.push_back(dragdropContainerAreaHelpers[areaOffset++]);
            setOverlayPos(makeDropArea(areaOffset), dock_pos::BOTTOM, ivec2(0), size, -1);
            vecHandles.push_back(dragdropContainerAreaHelpers[areaOffset++]);
        }
    }
}


guictr_layout::guictr_layout() : guictr_base() {
    setGuiType(gui_type::CTR_TYPE_LAYOUT);
    //setBackgroundRendered(true);
    //setBackgroundRenderedInset(true);
    this->setCanMouseHit(true);
    margin  = 0;
    padding = 0;
    //padding = 6;
    //margin = padding-4;
}
void guictr_layout::layout() {
    if (!theme)
        return;
    updateHandles();
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
    if (cs.x <= 0 || cs.y <= 0) {
        return;
    }
    vec2 segSizeF    = vec2(cs);
    vec2 axis        = vec2(0);
    auto nEntries = math::max<size_t>(1U, entries.size());

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
    const int32_t paddingHandle = 1;
    const int32_t handleHeight = 20;

    float tabW      = segSizeF.x;
    size_t entryIdx = 0;
    if (this->ctrLayout == container_layout::TABBED) {
        axis.x = 1;
        for (auto& entry: entries) {
            auto* gui       = entry->getGui();
            auto* guiHandle = entry->getHandle();
            bool bIsShown = entry->getHandle()->parent == this;
            const int32_t handleOffset = bIsShown ? 20 : 0;
            const int32_t ctrPadding = bIsShown ? 1 : 0;
            entry->pos = gui->pos = ivec2(ctrPadding) + ivec2(0, handleOffset);
            entry->size = gui->size = math::maxvec2(cs - ivec2(ctrPadding * 2) - ivec2(0, handleOffset), ivec2(4, 4));
            if (guiHandle) {
                guiHandle->pos  = ivec2((float) entryIdx * vec2(tabW, 0) + vec2(paddingHandle, 0));
                guiHandle->size = ivec2(math::maxvec2f(vec2(tabW, handleHeight), vec2(4, 4)) - vec2(paddingHandle * 2, 0));
            }
            entryIdx++;
        }
    } else {
        for (auto& entry: entries) {
            auto* gui                       = entry->getGui();
            auto* guiHandle                 = entry->getHandle();
            vec2 segPos                     = vec2((float) entryIdx * segSizeF * axis);
            if (this->ctrLayout == container_layout::SPLIT_V || this->ctrLayout == container_layout::SPLIT_H) {
                float curScale = 1.0f;
                if (splitters.size() > entryIdx) {
                    curScale = splitters[entryIdx]->getScaleClamped();
                }
                float prevScale = 0.0f;
                if (entryIdx >= 1) {
                    prevScale = splitters[entryIdx - 1]->getScaleClamped();
                }
                segSizeF = (curScale - prevScale) * vec2(cs) * axis + vec2(cs) * vec2(axis.y, axis.x);
                if (entryIdx >= 1) {
                    segPos = splitters[entryIdx - 1]->getScaleClamped() * vec2(cs) * axis;
                }
                if (this->ctrLayout == container_layout::SPLIT_V) {
                    tabW = segSizeF.x;
                }
            }
            bool bIsShown = entry->getHandle()->parent == this;
            const int32_t handleOffset = bIsShown ? 20 : 0;
            const int32_t ctrPadding = bIsShown ? 1 : 0;
            entry->pos = gui->pos = ivec2(segPos + vec2(0, handleOffset) + vec2(ctrPadding));
            entry->size = gui->size = ivec2(math::maxvec2f(segSizeF - vec2(ctrPadding * 2) - vec2(0, handleOffset), vec2(4, 4)));
            if (guiHandle) {
                guiHandle->pos  = ivec2(segPos + vec2(paddingHandle, 0));
                guiHandle->size = ivec2(vec2(tabW, handleHeight) - vec2(paddingHandle * 2, 0));
            }
            if (entryIdx > 0 && splitters.size() > entryIdx - 1) {
                ivec2 splitterPos  = entry->pos;
                ivec2 splitterSize = entry->size;
                if (bIsShown && guiHandle) {
                    splitterPos  = guiHandle->pos;
                    splitterSize = entry->size + guiHandle->size;
                }
                splitters[entryIdx - 1]->pos  = splitterPos - ivec2(vec2(Splitter::SPLITTER_LAYOUT_THICKNESS * 0.5f) * axis);
                auto invAxis                  = ivec2(axis.y, axis.x);
                splitters[entryIdx - 1]->size = (splitterSize) *invAxis + ivec2(vec2(Splitter::SPLITTER_LAYOUT_THICKNESS) * axis);
            }
            entryIdx++;
        }
    }
    for (auto* gui: guis) {
        String name = gui->getClassName();
        dbgassert(gui->size.x > 0);
        dbgassert(gui->size.y > 0);
        gui->determineSize(gui->size);
        if (gui->size.x <= 0) {
            gui->determineSize(gui->size);
        }
        dbgassert(gui->size.x > 0);
        dbgassert(gui->size.y > 0);
        //TODO: do not layout invisible (tabbed) entries
        gui->layout();
    }
    for (auto& entry: entries) {
        auto* gui = entry->getGui();
        gui->size = math::minvec2(gui->size, entry->size);
    }
}

void GuiCtrLayoutEntryHandle::handleDraggedBegin(MouseEvent& evt) {
    hasDragged = false;
    if (!hasClicked) {
        hasClicked = true;
        parent->buttonClicked(this);
    }
}

class guictr_layout_entry_handle_context_menu final : public guictxtmenu {
    GuiCtrLayoutEntryHandle* const ctrHandle;
public:
    explicit guictr_layout_entry_handle_context_menu(GuiCtrLayoutEntryHandle* _parent) : ctrHandle(_parent) {
        this->size.x   = 120;
        maxHeight = 0;
        this->fontSize = FONT_SIZE_CTXT_SMALL;
        this->paddingV = 0;
        std::vector<String> options;
        auto& mapGuiTypeToStr = getContainerRegistry();
        for (auto& [guiType, name]: mapGuiTypeToStr) {
            if (guiType != ctrHandle->parentCtr->getType()
                && guiType != gui_type::CTR_TYPE_LAYOUT) {
                addEntry(new ctxtmenu_entry(name, guiType + 100));
            }
        }
        addEntry(new ctxtmenu_splitter());
        addEntry(new ctxtmenu_entry("Close", 0));
    }
    bool clickedElement(ctxtmenu_entry* e, int _id) override {
        if (_id >= 100) {
            // std::shared_ptr<guictr_layout_entry> pCtr;
            // ctrHandle->parentCtr->getContainerRef(pCtr, true);
            auto ctrl = ctrHandle->parentCtrl;
            gui_type type = static_cast<gui_type>(_id - 100);
            std::shared_ptr<guictr_base> ctr;
            auto context = ContainerInstanceContext{ctrHandle->dawCtrl->getDaw(), ctrHandle->dawCtrl, {}};
            if (makeContainer(context, type, ctr)) {
                ctr->setLabel(e->title);
                SPLayoutEntry ctrEntry = createGuiCtrLayoutEntry(ctr);
                auto layoutCtr = ctrHandle->parentCtr->getParentContainer();
                if (layoutCtr) {
                    layoutCtr->replaceContainerWith(ctrHandle->ctr, ctrEntry);
                    ctrl->relayout();
                }
                dawCtrl->onViewCreated(ctrEntry);
            }
            closeContextMenu();
        } else if (_id == 0) {
            ctrHandle->buttonClicked(&ctrHandle->btnClose);
            closeContextMenu();
        }
        return true;
    }
};

void GuiCtrLayoutEntryHandle::handleRightClick(MouseEvent& evt) {
    if (!hasDragged) {
        parentCtrl->openContextMenu(new guictr_layout_entry_handle_context_menu(this), evt.mousepos);
    }
}
void GuiCtrLayoutEntryHandle::handleDraggedMove(MouseEvent& evt) {
    dbgassert(!hasDragged);
    float fDist = math::distvec2(ivec2(0, 0), *evt.dragDistance);
    if (!hasDragged && (fDist > 2.0f)) {

        parentCtrl->dragContainerBegin(evt, parentCtr);
        hasDragged = true;
    } else if (!hasDragged) {
        //parent->buttonClicked(this);
    }
}
void GuiCtrLayoutEntryHandle::handleDraggedRelease(MouseEvent& evt) {
    if (parent)
        parent->buttonClicked(this);
    hasDragged = false;
}
void GuiCtrLayoutEntryHandle::render(NVGcontext* vg) {
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
    nvgBeginPath(vg);
    nvgRect(vg, 0, 0, size.x, size.y);
    NVGcolor bg = theme->getColor(GuiColor::COL_BASE_BG);
    if (parentCtr->isVisible()) {
        bg = theme->getColor(GuiColor::COL_BASE_BG_FOCUSED);
    }
    nvgFillColor(vg, bg);
    nvgFillCustomPar(vg, -1);
    nvgFill(vg);
    String str = parentCtr->getGui()->label;
    if (str.length()) {
        const int htt = theme->get(GuiConstant::CONST_SMALL_LABEL_HEIGHT);
        vec2 renderPos(htt/2, size.y/2);
        auto fontScale = math::clamp(size.y, 4, 48) * 0.8f;
        renderTextLabel(vg,
                        renderPos,
                        vec2(btnClose.getLeftTop().x - htt/2, size.y),
                        str,
                        theme,
                        fontScale,
                        theme->getColor(getLabelColor()),
                        NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE
                        );
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

void DropAreaUILayout::render(NVGcontext* vg) {
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
                    vec2(1000, 1000),
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
    int splitterLayout = 0;
    size_t numSplitters = 0;
    if (this->ctrLayout == container_layout::SPLIT_H || this->ctrLayout == container_layout::SPLIT_V) {
        numSplitters = math::max<int64_t>(0, CtrSize(entries) - 1);
        splitterLayout = ctrLayout == container_layout::SPLIT_V ? 1 : 0;
    }
    for (size_t i = numSplitters; i < splitters.size(); ++i) {
        guictr_base::remove(splitters.back().get());
        splitters.pop_back();
    }
    float off = 1.0f / (numSplitters + 1);
    for (size_t i = splitters.size(); i < numSplitters; ++i) {
        float splitPos = off + i * off;
        if (splitterPositons.size() > i) {
            splitPos = splitterPositons[i];
        }
        splitters.push_back(std::make_unique<Splitter>(splitterLayout, splitPos));
        splitters[i]->setCallback(this);
        guictr_base::add(splitters.back().get());
    }
    for (size_t i = 0; i < splitters.size() && i < splitterPositons.size(); ++i) {
        splitters[i]->setSplitterType(splitterLayout);
        splitters[i]->setScale(splitterPositons[i]);
    }
    updateSplitterMinMax(splitters);
}

void guictr_layout::updateHandles() {
    int32_t idx = 0;
    for (auto& entry : entries) entry->indexInParent = idx++;
    bool bIsUiLocked = daw_tls::getSettings().dawsettings.uiLayoutLocked && (!parentCtrl || !parentCtrl->isDraggingContainer());
    auto p = parent;
    bool bParentHidesHandles = this->bHideHandlesWhenLocked;
    while(!bParentHidesHandles && p) {
        if (p->getGuiType() == CTR_TYPE_LAYOUT) {
            auto* pLayout = static_cast<guictr_layout*>(p);
            bParentHidesHandles |= pLayout->bHideHandlesWhenLocked;
            p = p->parent;
        }
    }
    for (auto& entry : entries) {
        auto guiHandle = entry->getHandle();
        bool bIsVisible = !!guiHandle->parent;
        bool bHideEntryHandles = bParentHidesHandles;
        if (!bParentHidesHandles && bIsUiLocked && entry->getType() == CTR_TYPE_LAYOUT) {
            bHideEntryHandles = true;
        }
        bool bEntryNeedsHandle = !(bIsUiLocked && bHideEntryHandles);
        if (bIsVisible != bEntryNeedsHandle) {
            if (bEntryNeedsHandle) {
                guictr_base::add(guiHandle);
                handles.push_back(guiHandle);
            } else {
                removeEntry(handles, guiHandle);
                guictr_base::remove(guiHandle);
            }
        }
    }
}
void guictr_layout::updateSplitters() {
    int splitterLayout = 0;
    int numSplitters = 0;
    if (this->ctrLayout == container_layout::SPLIT_H || this->ctrLayout == container_layout::SPLIT_V) {
        numSplitters = math::max<int>(0, CtrSize(entries) - 1);
        splitterLayout = ctrLayout == container_layout::SPLIT_V ? 1 : 0;
    }

    float off = 1.0f / (numSplitters + 1);
    int oldLen = CtrSize(splitters);
    for (int i = oldLen; i < numSplitters; ++i) {
        float splitPos = off + i * off;
        splitters.push_back(std::make_unique<Splitter>(splitterLayout, splitPos));
        splitters[i]->setCallback(this);
        guictr_base::add(splitters.back().get());
    }
    for (int i = numSplitters; i < oldLen; ++i) {
        guictr_base::remove(splitters.back().get());
        splitters.pop_back();
    }
    for (int i = 0; i < numSplitters; ++i) {
        splitters[i]->setSplitterType(splitterLayout);
        float splitPos = off + i * off;
        splitters[i]->setMinMax(splitPos - off * 0.8f, splitPos + off * 0.8f);
        splitters[i]->setScale(splitters[i]->getScaleClamped());
    }
    dbgassert(splitters.empty() || splitters.size() == entries.size() - 1);
}
void guictr_layout::removeAllEntries() {
    for (auto& entry : entries) {
        guictr_base::remove(entry->getGui());
        auto* guiHandle = entry->getHandle();
        if (guiHandle) {
            removeEntry(handles, guiHandle);
            guictr_base::remove(guiHandle);
        }
        entry->setParentContainer(nullptr);
    }
    dbgassert(handles.empty());
    entries.clear();
    handles.clear();
    activePosition = -1;
    updateSplitters();
    dbgassert(splitters.empty() && guis.empty());
}

void guictr_layout::assertEntries() const {
#ifndef NDEBUG
    dbgassert(splitters.empty() || splitters.size() == entries.size() - 1);
    for (auto& entry : entries) {
        dbgassert(entry->getParentContainer() == this && entry->getGui()->parent == this);
    }
    dbgassert(entries.empty() == guis.empty());
#endif
}

void guictr_layout::handleSplitterChanged(Splitter& splitter, float scale, int clampedAt) {
    updateSplitterMinMax(splitters);
    layout();
}
ivec2 guictr_layout::getContainerPos() {
    const bool isShown = isHandleShown();
    const int32_t handleHeight = isShown && handles.size() ? 20 : 0;
    return toScreenSpace(ivec2(0, handleHeight));
}
ivec2 guictr_layout::getContainerSize() {
    ivec2 cs = getSizeContent();
    const bool isShown = isHandleShown();
    const int32_t handleHeight = isShown && handles.size() ? 20 : 0;;
    return cs - ivec2(0, handleHeight);
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
            if (gui->isVisible() && gui->mouseHitTest(localMouse, evt)) {
                return true;
            }
        }
        for (auto& gui: splitters) {
            if (gui->isVisible() && gui->mouseHitTest(localMouse, evt)) {
                return true;
            }
        }
        for (guibase* gui: guis) {
            if (gui->isVisible() && gui->mouseHitTest(localMouse, evt)) {
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

bool guictr_layout::getContainerRef(GuiCtrLayoutEntry* entry, SPLayoutEntry& out, bool remove) {
    auto it = std::find_if(this->entries.begin(), this->entries.end(), [entry](auto& e) {
        return entry == e.get();
    });
    if (it == entries.end()) {
        dbgassert(0);
        return false;
    }
    out = *it;
    if (!remove) {
        return true;
    }
    auto pos = it - entries.begin();
    bool removedActive = getActivePosition() == pos;
    entries.erase(it);
    guictr_base::remove(entry->getGui());
    auto* guiHandle = entry->getHandle();
    if (guiHandle) {
        removeEntry(handles, guiHandle);
        guictr_base::remove(guiHandle);
    }
    entry->setParentContainer(nullptr);
    updateSplitters();
    if (removedActive && this->ctrLayout == container_layout::TABBED) {
        auto sPos = int32_t(pos);
        if (sPos - 1 >= 0 || entries.empty()) {
            sPos--;
        }
        this->activePosition = sPos;
        updateVisible();
    }
    updateHandles();
    out->updateLabel();
    return true;
}

bool guictr_layout::activateEntry(GuiCtrLayoutEntry* entry) {
    auto it = std::find_if(this->entries.begin(), this->entries.end(), [entry](auto& e) {
        if (entry == e.get()) {
            return true;
        }
        auto guiType = e->getGui()->getGuiType();
        if (guiType != gui_type::CTR_TYPE_LAYOUT) {
            return false;
        }
        auto ctr = e->getAsLayoutCtr();
        if (ctr->activateEntry(entry)) {
            return true;
        }
        return false;
    });
    if (it != entries.end()) {
        if (this->ctrLayout == container_layout::TABBED) {
            auto pos = it - entries.begin();
            if (pos != this->activePosition) {
                this->activePosition = static_cast<int32_t>(pos);
            }
        }
        updateVisible();
        return true;
    }
    return false;
}
SPLayoutEntry guictr_layout::findByTagEntry(int32_t tag) {
    for (auto& entry : entries) {
        if (entry->getEntryTag() == tag) {
            return entry;
        }
        auto guiType = entry->getGui()->getGuiType();
        if (guiType != gui_type::CTR_TYPE_LAYOUT) {
            continue;
        }
        auto ctr = entry->getAsLayoutCtr();
        auto found = ctr->findByTagEntry(tag);
        if (found) {
            return found;
        }
    }
    return nullptr;
}
SPLayoutEntry guictr_layout::findByGuiType(gui_type guitype) {
    for (auto& entry : entries) {
        if (entry->getGui()->getGuiType() == guitype) {
            return entry;
        }
        auto guiType = entry->getGui()->getGuiType();
        if (guiType != gui_type::CTR_TYPE_LAYOUT) {
            continue;
        }
        auto ctr = entry->getAsLayoutCtr();
        auto found = ctr->findByGuiType(guitype);
        if (found) {
            return found;
        }
    }
    return nullptr;
}

void guictr_layout::addEntry(const SPLayoutEntry& entry, int32_t posOffset) {
    auto it = std::find(entries.begin(), entries.end(), entry);
    if (it != entries.end()) {
        throw applogicexception(StringFormat("%s - attempt to add element twice", StringAsCStr(getClassName())));
    }
    auto insertPos = entries.begin();
    if (!entries.empty()) {
        insertPos = posOffset == -1 ? entries.begin() : (posOffset == -2 || posOffset >= CtrSize(entries) ? entries.end() : (entries.begin() + posOffset));
    }
    auto updatedCtrLayout = ctrLayout == container_layout::SOLE && !entries.empty() ? container_layout::TABBED : ctrLayout;
    if (ctrLayout != updatedCtrLayout) {
        log_printf("Container layout changed to %d\n", static_cast<int>(updatedCtrLayout));
        setLayout(updatedCtrLayout);
    }
    entries.insert(insertPos, entry);
    auto guiCtr = entry->getGui();
    // a handle is present if the child is not a guictr_layout, or if this container is using tabbed layout

    guictr_base::add(guiCtr);
    entry->setParentContainer(this);
    updateSplitters();
    updateHandles();
    entry->updateLabel();
}

bool guictr_layout::placeContainer(SPLayoutEntry entry, DropAreaUILayout* area) {

    // prevent dropping into self
    guibase* parent = this;
    while (parent) {
        if (parent == entry.get()->getGui()) {
            return false;
        }
        parent = parent->parent;
    }

    dock_pos dockPos      = area->dockPos;
    int32_t dockPosOffset = area->tabPosition;
    auto updatedCtrLayout = DockPosToContainerLayout(dockPos);

    SPLayoutEntry out;
    if (entry->getParentContainer() != nullptr) {
        //undock from current container
        if (!entry->getContainerRef(out, true)) {
            return false;
        }
    }
    auto it = std::find(entries.begin(), entries.end(), entry);
    if (it != entries.end()) {
        throw applogicexception(StringFormat("%s - attempt to add element twice", StringAsCStr(getClassName())));
    }

    if (ctrLayout != updatedCtrLayout) {
        log_printf("Container layout changed to %d\n", static_cast<int>(updatedCtrLayout));
        setLayout(updatedCtrLayout);
    }
    if (dockPos == dock_pos::STACK) {
        if (dockPosOffset <= -1) {
            entries.insert(entries.begin(), entry);
        } else if (dockPosOffset >= CtrSize(entries)) {
            entries.insert(entries.end(), entry);
        } else {
            entries.insert(entries.begin() + dockPosOffset, entry);
        }
    } else if (dockPos == dock_pos::RIGHT || dockPos == dock_pos::BOTTOM) {
        entries.insert(entries.end(), entry);
    } else {
        entries.insert(entries.begin(), entry);
    }

    auto guiCtr    = entry->getGui();
    guictr_base::add(guiCtr);

    entry->setParentContainer(this);
    if (this->ctrLayout == container_layout::TABBED) {
        int32_t newIdx       = indexOfCtr(entries, entry);
        this->activePosition = newIdx;
        updateVisible();
    }
    updateSplitters();
    updateHandles();
    entry->updateLabel();
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

SPLayoutEntry guictr_layout::replaceContainerWith(guictr_base* ctr,
                                                                         SPLayoutEntry& newEntry) {
    std::shared_ptr<guictr_layout> retCtr;
    auto it = std::find_if(entries.begin(), entries.end(), [ctr](SPLayoutEntry& e) {
        return e->getGui() == ctr;
    });
    if (!assert_expr(it != entries.end())) {
        return nullptr;
    }
    SPLayoutEntry entry = *it;// copy for return
    guictr_base::remove(entry->getGui());
    auto* guiHandle = entry->getHandle();
    if (guiHandle) {
        removeEntry(handles, guiHandle);
        guictr_base::remove(guiHandle);
    }
    entry->setParentContainer(nullptr);

    auto posOffset = it - entries.begin();
    entries.erase(it);

    auto insertPos = entries.begin() + posOffset;

    entries.insert(insertPos, newEntry);
    auto guiCtr       = newEntry->getGui();
    guictr_base::add(guiCtr);
    //guiCtr->snapSides = ivec4(1);
    guiHandle = newEntry->getHandle();
    newEntry->setParentContainer(this);
    updateSplitters();
    updateHandles();
    entry->updateLabel();
    return entry;
}

GuiCtrLayoutEntry::GuiCtrLayoutEntry(String _label, const std::shared_ptr<guictr_base>& _ctr)
    : type(_ctr->getGuiType()),
      frameType(_ctr->getGuiType() == CTR_TYPE_LAYOUT ? LayoutCtrType::GUICTR_LAYOUT : LayoutCtrType::GUICTR_BASE),
      ctr(_ctr),
      label(std::move(_label))
{
    ctrHandle = new GuiCtrLayoutEntryHandle(this, _ctr.get());
}
GuiCtrLayoutEntry::~GuiCtrLayoutEntry() {
    delete ctrHandle;
}

guibase* GuiCtrLayoutEntry::getHandle() {
    return ctrHandle;
}

bool GuiCtrLayoutEntry::getContainerRef(SPLayoutEntry& out, bool remove) {
    return getParentContainer()->getContainerRef(this, out, remove);
}

void GuiCtrLayoutEntry::removeEntryFromParent() {
    auto parent = getParentContainer();
    if (parent) {
        SPLayoutEntry out;
        assert_expr(parent->getContainerRef(this, out, true));
        parent->postContentChanged();
    }
}
guictr_base* GuiCtrLayoutEntry::getGui() {
    return ctr.get();
}

void GuiCtrLayoutEntry::assertState() const {
    dbgassert(!!getParentContainer() == !!ctr->parent);
}

void GuiCtrLayoutEntry::updateLabel() {
    if (selfLayoutCtr) {
        String label = "Layout ";
        if (entryTag != -1) {
            label += " " + std::to_string(entryTag);
        }
        switch (selfLayoutCtr->getLayout()) {
            case container_layout::SOLE:
                label += " SOLE";
                break;
            case container_layout::SPLIT_H:
                label += " SPLIT_H";
                break;
            case container_layout::SPLIT_V:
                label += " SPLIT_V";
                break;
            case container_layout::TABBED:
                label += " TABBED";
                break;
            default:
                break;
        }
        selfLayoutCtr->setLabel(label);
    }
}
void GuiCtrLayoutEntry::setEntryTag(int32_t tag) {
    if (tag < 100)
        entryTag = -1;
    else
        entryTag = tag;
    if (selfLayoutCtr) {
        selfLayoutCtr->setTag(entryTag);
    }
    updateLabel();
}


void storeContainerEntrySnapshot(GuiCtrLayoutEntry* ctrlayoutEntry, std::shared_ptr<guictrlayout_entry_snapshot_t>& snapshot) {
    dbgassert(ctrlayoutEntry->getType() != gui_type::CTR_TYPE_UNKNOWN);
    if (ctrlayoutEntry->getFrameType() == LayoutCtrType::GUICTR_LAYOUT) {
        auto sharedSnapshot      = std::make_shared<guictrlayout_snapshot_t>();
        guictr_layout* ctrLayout = dynamic_cast<guictr_layout*>(ctrlayoutEntry->getGui());
        dbgassert(ctrLayout);
        storeContainerSnapshot(ctrLayout, sharedSnapshot.get());
        snapshot = sharedSnapshot;
    } else {
        auto sharedSnapshot = std::make_shared<guictrlayout_entry_snapshot_t>();
        snapshot            = sharedSnapshot;
    }
    snapshot->entryTag = ctrlayoutEntry->getEntryTag();
    snapshot->type  = ctrlayoutEntry->getType();
    dbgassert(snapshot->type == ctrlayoutEntry->getGui()->getGuiType());
    snapshot->label = ctrlayoutEntry->getLabel();
}

void loadContainerEntrySnapshot(ContainerFactory& fac,
                                ContainerInstanceContext& ctxt,
                                std::shared_ptr<guictrlayout_entry_snapshot_t>& snapshot,
                                SPLayoutEntry& out) {
    out  = nullptr;
    const auto typeLoad = snapshot->type;
    ctxt.stats[typeLoad]++;
    if (snapshot->entryTag >= 100) {
        auto it = ctxt.entriesPreconstructed.find(snapshot->entryTag);
        if (it != ctxt.entriesPreconstructed.end()) {
            auto spLayoutEntry = it->second;
            ctxt.entriesPreconstructed.erase(it);
            dbgassert(spLayoutEntry && spLayoutEntry->getEntryTag() == snapshot->entryTag);
            out = spLayoutEntry;
            auto ctrLayout = out->getAsLayoutCtr();
            if (ctrLayout) {
                auto* ctrLayoutSnapshot = dynamic_cast<guictrlayout_snapshot_t*>(snapshot.get());
                if (!assert_expr(ctrLayoutSnapshot)) {
                    return;
                }
                loadContainerSnapshot(fac, ctxt, ctrLayout.get(), ctrLayoutSnapshot);
            }
        }
    } else if (typeLoad == gui_type::CTR_TYPE_LAYOUT) {
        out = createGuiCtrLayoutEntry(std::make_shared<guictr_layout>());
        out->setEntryTag(snapshot->entryTag);
        getContainerLabel(typeLoad, out->getAsLayoutCtr()->label);
        auto* ctrLayoutSnapshot = dynamic_cast<guictrlayout_snapshot_t*>(snapshot.get());
        if (!assert_expr(ctrLayoutSnapshot)) {
            return;
        }
        loadContainerSnapshot(fac, ctxt, out->getAsLayoutCtr().get(), ctrLayoutSnapshot);
    } else {
        std::shared_ptr<guictr_base> sharedContainer;
        if (makeContainer(ctxt, typeLoad, sharedContainer)) {
            out = createGuiCtrLayoutEntry(sharedContainer);
            out->setEntryTag(snapshot->entryTag);
            getContainerLabel(typeLoad, sharedContainer->label);
        }
    }
    if (out) {
        if (!ctxt.entriesConstructed.count(out->getType())) {
            ctxt.entriesConstructed[out->getType()] = { out };
        } else {
            ctxt.entriesConstructed[out->getType()].push_back(out);
        }
    } else {
        log_lf(Log::L_WARN, "Failed loading container of type %d (tag %d)\n", typeLoad, snapshot->entryTag);
    }
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
        SPLayoutEntry sharedEntry;
        loadContainerEntrySnapshot(fac, ctxt, shrdEntrySnapshot, sharedEntry);
        if (sharedEntry) {
            ctrlayout->addEntry(sharedEntry, -2);
        }
    }
    ctrlayout->setSplitterPositions(snapshot->splitterPositions);
    ctrlayout->setActiveEntry(snapshot->activePosition);
    //ctrlayout->postContentChanged();
}

bool guictr_layout::isHandleShown() const {
    if (!daw_tls::getSettings().dawsettings.uiLayoutLocked || (parentCtrl && parentCtrl->isDraggingContainer())) {
        return true;
    }
    auto p = parent;
    bool bHideHandles = this->bHideHandlesWhenLocked;
    while(!bHideHandles && p) {
        if (p->getGuiType() == CTR_TYPE_LAYOUT) {
            auto* pLayout = static_cast<guictr_layout*>(p);
            bHideHandles |= pLayout->bHideHandlesWhenLocked;
            p = p->parent;
        }
    }
    return !bHideHandles;
}

bool guictr_layout::canSimplify() const {
    return getTag() < 0;
}
void guictr_layout::simplify() {
    struct InlineEntry {
        int32_t index = 0;
        SPLayoutEntry entry;
    };
    std::vector<SPLayoutEntry> entriesToRemove;
    std::vector<InlineEntry> entriesInsert;
    int32_t index = 0;
    for (const auto& entry : entries) {
        auto guiCtrLayout = entry->getAsLayoutCtr();
        if (guiCtrLayout) {
            guiCtrLayout->simplify();
            if (canSimplify()) {
                auto& childEntries = guiCtrLayout->getEntries();
                if (childEntries.empty()) {
                    entriesToRemove.push_back(entry);
                } else if (childEntries.size() == 1 && entry->getEntryTag() < 0) {
                    auto& childEntry = childEntries[0];
                    InlineEntry inlineEntry;
                    guiCtrLayout->getContainerRef(childEntry.get(), inlineEntry.entry, true);
                    inlineEntry.index = index;
                    entriesToRemove.push_back(entry);
                    entriesInsert.push_back(inlineEntry);
                }
            }
        }
        index++;
    }
    if (canSimplify()) {
        for (const auto& entry : entriesToRemove) {
            SPLayoutEntry out;
            getContainerRef(entry.get(), out, true);
        }
        for (const auto& entry : entriesInsert) {
            addEntry(entry.entry, entry.index);
        }
        if (entries.size() < 2) {
            setLayout(container_layout::SOLE);
        } else if (entries.size() > 1 && getLayout() == container_layout::SOLE) {
            setLayout(container_layout::TABBED);
        }
    }
}

void guictr_layout::setLayout(container_layout ctrLayoutNew) {
    if (this->ctrLayout == ctrLayoutNew) {
        return;
    }
    this->ctrLayout = ctrLayoutNew;
    if (ctrLayoutNew != container_layout::TABBED) {
        activePosition = -1;
    }
    updateVisible();
    updateSplitters();
    auto parentLayout = gui_cast<guictr_layout, gui_type::CTR_TYPE_LAYOUT>(parent);
    if (parentLayout) {
        auto entry = parentLayout->getEntry(this);
        if (entry)
            entry->updateLabel();
    }
}

void guictr_layout::postContentChanged() {
    if (this->parent) {
        auto ctrLayoutParent = guiParentType<guictr_layout, gui_type::CTR_TYPE_LAYOUT>(parent);
        if (ctrLayoutParent) {
            ctrLayoutParent->postContentChanged();
            return;
        }
    }
    simplify();
    updateVisible();
    if (parentCtrl) {
        layout();
    }
}

void guictr_layout::setActiveEntry(int32_t idx) {
    this->activePosition = math::clamp(idx, 0, CtrSize(entries) - 1);
    updateVisible();
    if (!parent&&parentCtrl) {
        layout();
    }
}
bool guictr_layout::isEntryVisible(GuiCtrLayoutEntry* entry) {
    return entry && entry->getGui()->isVisible() && (this->ctrLayout != container_layout::TABBED || entry->indexInParent == activePosition);
}

void guictr_layout::onChildLayoutChanged(guibase* g) {
    if (!parent&&parentCtrl) {
        layout();
    }
}

void guictr_layout::buttonClicked(guibase* button) {
    if (this->ctrLayout == container_layout::TABBED) {
        int32_t pos = 0;
        for (auto& entry : entries) {
            if (entry->getHandle() == button) {
                setActiveEntry(pos);
                return;
            }
            pos++;
        }
    }
}

ivec2 guictr_layout::paddingTL(int _padding) const {
    if (parentCtrl && parentCtrl->isDraggingContainer()) {
        return ivec2(8);
    }
    //return ivec2(0, _padding);
    //return ivec2(_padding - margin*snapSides.x, _padding - margin*snapSides.y);
    return { _padding - margin * snapSides.x, 0 };
}

ivec2 guictr_layout::paddingBR(int _padding) const {
    if (parentCtrl && parentCtrl->isDraggingContainer()) {
        return ivec2(8);
    }
    //return ivec2(0, _padding);
    //return ivec2(_padding - margin*snapSides.z, _padding - margin*snapSides.w);
    return { _padding - margin * snapSides.z, 0 };
}

String guictr_layout::getLayoutCtrName() {
    if (this->label.empty()) {
        auto ctrName = getClassName();
        return ctrName;//StringFormat("%12zX", reinterpret_cast<uint64_t>(this));
    }
    return this->label;
}
guictr_layout::~guictr_layout() {
    removeGuis();
    for (auto& entry : entries) {
        dbgassert(entry->getParentContainer() == this);
        entry->setParentContainer(nullptr);
    }
    // activePosition = -1;
}


bool GuiCtrLayoutEntry::isVisible() {
    auto layoutEntryThis = this;
    auto ctrLayout = gui_cast<guictr_layout, gui_type::CTR_TYPE_LAYOUT>(ctr->parent);
    while (ctrLayout && layoutEntryThis) {
        if (!ctrLayout->isEntryVisible(layoutEntryThis))
            return false;
        auto ctrLayoutParent = gui_cast<guictr_layout, gui_type::CTR_TYPE_LAYOUT>(ctrLayout->parent);
        if (!ctrLayoutParent) {
            break;
        }
        layoutEntryThis = ctrLayoutParent->getEntry(ctrLayout).get();
        ctrLayout = ctrLayoutParent;
    }
    return layoutEntryThis && layoutEntryThis->ctr->parent && layoutEntryThis->ctr->parent->isVisible();
}
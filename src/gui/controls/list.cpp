#include <cstdint>
#include <nanovg.h>
#include <algorithm>
#include "guiglobals.h"
#include "math/vec.h"
#include "list.h"
#include "gui/gui.h"
#include "mouse.h"
#include "event.h"
#include "guicolors.h"
#include "theme.h"
#include "gui/container/container.h"
#include "renderresources.h"
#include "basectrl.h"
#include "host/daw/mainctrl.h"

void gui_list_entry::handleDraggedMove(MouseEvent& evt) {
    parentCtrl->objectDragMove(this, evt);
}

void gui_list_entry::handleDraggedRelease(MouseEvent& evt) {
    parentCtrl->objectDragRelease(this, evt);
}

void gui_list_entry::render(NVGcontext* vg) {
    if (size.x<1||size.y<1) return;
    BaseCtrl* ctrl  = parentCtrl;
    float spacing   = INSET_TITLE;
    float x         = spacing;
    float rowHeight = size.y;
    if (selected) {
        bool focused = ctrl->isCtrOrChildFocused(this); // this check might get expensive on lists with many entries
        auto color = theme->getColor(focused ? GuiColor::COL_BG_DRKER : GuiColor::COL_BG_DRK);
        nvgBeginPath(vg);
        nvgRect(vg, pos.x, pos.y, size.x, size.y);
        nvgFillColor(vg, color);
        nvgFill(vg);
    }
    nvgTranslate(vg, pos.x + (INSET_TITLE * entryDepth), pos.y);
    if (icon > -1) {
        RenderResources::NvgImageTexture& image = RenderResources::imgIcons[icon];
        drawIcon(vg, size, &image);
        x += rowHeight + spacing;
    }
    renderText(vg, vec2(x, rowHeight*0.5f), size, getText());
    nvgTranslate(vg, -(pos.x + (INSET_TITLE * entryDepth)), -pos.y);
}

void gui_list_entry::handleDraggedBegin(MouseEvent& evt) {
    if (parent) parent->buttonClicked(this);
}
void gui_list::buttonClicked(guibase* button) {
    selectedIdx = indexOfCtr(this->listGuis, button);
    if (selectedIdx > -1) {
        if (parent) parent->buttonClicked(button);
    }
}
bool gui_list::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
    if (this->contains(mpos)) {
        ivec2 localMouse = this->toContainerSpace(mpos);
        if (scrollbar.mouseHitTest(localMouse, evt)) {
            return true;
        }
        ivec2 localMouseOffset = localMouse;
        if (first < last) {
            gui_list_entry* g = listGuis[first];
            localMouseOffset.y += g->top();
        }
        for (int32_t idx = first; idx < last; idx++) {
            if (listGuis[idx]->mouseHitTest(localMouseOffset, evt)) {
                return true;
            }
        }
        evt.requestFocus(this);
        return true;
    }
    return false;
}
void gui_list::render(NVGcontext* vg) {
    if (isBackgroundRendered()) {
        renderBackground(vg);
    }
    if (!setScissorTransform(vg)) {
        return;
    }
    nvgSave(vg);
    if (first < last) {
        gui_list_entry* g = listGuis[first];
        nvgTranslate(vg, 0, -g->top());
    }
    int32_t dragDropHighlightIdx = -1;
    const auto dragDropTarget = dawCtrl->getDragDropTarget();
    const auto dragdropTargetGui = safeRefGet(dragDropTarget.target);
    const auto frameWidth = 3;
    if (dragdropTargetGui && dragdropTargetGui->parent == this) {
        dragDropHighlightIdx = indexOfCtr(listGuis, dragdropTargetGui);
    }
    if (dragDropHighlightIdx >= first && dragDropHighlightIdx < last) {
        auto dst = listGuis[dragDropHighlightIdx];
        nvgBeginPath(vg);
        nvgRect(vg, dst->pos.x + frameWidth, dst->pos.y + frameWidth, dst->size.x - 2 * frameWidth, dst->size.y - 2 * frameWidth);
        nvgFillColor(vg, rgbaToNvg(0x3fdddd33));
        nvgFill(vg);
    }
    for (int32_t idx = first; idx < last; idx++) {
        listGuis[idx]->selected = selectedIdx == idx && dragDropHighlightIdx != idx;
        listGuis[idx]->render(vg);
    }
    if (dragDropHighlightIdx >= first && dragDropHighlightIdx < last) {
        auto dst = listGuis[dragDropHighlightIdx];
        nvgBeginPath(vg);
        nvgRect(vg, dst->pos.x + frameWidth * 0.5f, dst->pos.y + frameWidth * 0.5f, dst->size.x - frameWidth, dst->size.y - frameWidth);
        nvgStrokeColor(vg, rgbaToNvg(0x7fdddd33));
        nvgStrokeWidth(vg, frameWidth);
        nvgStroke(vg);
    }
    if (renderHR && first < last) {
        for (int32_t idx = first; idx < last; idx++) {
            float y = rowHeight * idx;
            nvgBeginPath(vg);
            nvgMoveTo(vg, 0, y + rowHeight);
            nvgLineTo(vg, size.x, y + rowHeight);
            nvgStrokeColor(vg, theme->getColor(GuiColor::COL_GUI_STROKE));
            nvgStrokeWidth(vg, theme->getFloat(GuiConstant::CONST_GUI_FRAME_STROKE_WIDTH));
            nvgStroke(vg);
        }
    }
    nvgRestore(vg);
    scrollbar.render(vg);
}
void gui_list::updateVisible() {
    ivec2 cs            = getSizeContent();
    float offset        = scrollbar.scrollOffset;
    int32_t nEntriesFit = floor(cs.y / (double) rowHeight);
    int32_t nEntries    = math::max(0, (int32_t) listGuis.size() - nEntriesFit);
    first               = math::max(0, (int32_t) floor(offset * nEntries));
    if (listGuis.size() == 0) {
        first = last = 0;
    } else {
        last  = first + (int32_t) nEntriesFit + 1;
        first = math::min((int32_t) (listGuis.size() - 1), first);
        last  = math::min((int32_t) listGuis.size(), last);
    }
}
void gui_list::scrollOffsetChanged(int dir, float offset) {
    updateVisible();
}
bool gui_list::handleMouseScroll(MouseEvent& evt, double xoffset, double yoffset) {
    if (abs(yoffset) > 0.01f) {
        int dir = scrollbar.getAxis();
        ivec2 vcS  = getScrollTotalSize();
        int32_t cS = vcS[dir];
        if (cS > 0) {
            float newOffset = scrollbar.scrollOffset;
            int32_t nEntriesFit = floor(getSizeContent().y / (double) rowHeight);
            int32_t nEntries    = math::max(0, (int32_t) listGuis.size() - nEntriesFit);
            if (nEntries) {
                auto nEntriesToScroll = math::max<float>(1.0f, nEntriesFit *0.25f);
                auto curOffset = scrollbar.scrollOffset;
                float offset = float(yoffset * nEntriesToScroll / nEntries);
                newOffset = curOffset - offset;
            }
            scrollbar.setScrollOffset(newOffset);
        }
    }
    return true;
}

guibase* gui_list::getFocusedContainer() {
    if (this->parent != nullptr && !this->isBackgroundRendered()) {
        if (this->parent->getGuiType() != gui_type::CTR_TYPE_LAYOUT) {
            return this->parent->getFocusedContainer();
        }
    }
    return this;
}

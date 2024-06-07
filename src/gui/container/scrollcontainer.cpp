#include "scrollcontainer.h"
#include <nanovg.h>
#include <vector>
#include "event.h"
#include "gui/gui.h"
#include "gui/container/container.h"
#include "guicolors.h"
#include "gui/controls/scrollbar.h"
#include "basectrl.h"

void guictr_scrollbar::render(NVGcontext* vg) {
    if (isBackgroundRendered()) {
        renderBackground(vg);
    }
    if (!setScissorTransformContainer(vg)) {
        return;
    }
    for (guibase* gui : guis) {
        if (gui == &scrollbar)
            continue;
        if (!renderClipped) {
            if (scrollbar.isVertical() && (gui->bottom() < -2 || gui->top() > size.y + 2))
                continue;
            if (!scrollbar.isVertical() && (gui->right() < - 2 || gui->left() > size.x + 2))
                continue;
        }
        if (gui->size == ivec2{ 0, 0 }) {
            log_lf(Log::L_WARN, "warning, rendering container with size 0 0\n");
        } else {
            nvgSave(vg);
            gui->render(vg);
            nvgRestore(vg);
        }
    }
    if (scrollbar.isVisible()) {
        nvgRestore(vg);
        nvgSave(vg);
        nvgTranslate(vg, pos.x, pos.y);
        scrollbar.render(vg);
    }
}

void guictr_scrollbar::determineSize(glm::ivec2& prefSize) /* const */ {
    ivec2 pos = { 0, 0 };
    ivec2 pad = { 0, 0 };
    ivec2 layoutSize = prefSize;
    switch(getLayoutMode()) {
        case autolayout_mode::LAYOUT_VERTICAL:
            layoutSize.x -= padding * 2;
            break;
        case autolayout_mode::LAYOUT_HORIZONTAL:
            layoutSize.y -= padding * 2;
            break;
        case LAYOUT_GRID:
        case LAYOUT_STACK:
        case LAYOUT_NONE:
            break;
    }

    for (guibase* gui : guis) {
        if (gui == &scrollbar)
            continue;

        gui->pos  = pos;
        gui->size = layoutSize;
        gui->determineSize(gui->size);
        gui->layout();
        switch(getLayoutMode()) {
            case autolayout_mode::LAYOUT_VERTICAL:
                pos.y = gui->bottom() + pad.y;
                break;
            case autolayout_mode::LAYOUT_HORIZONTAL:
                pos.x = gui->right() + pad.x;
                break;
            case LAYOUT_GRID:
            case LAYOUT_STACK:
            case LAYOUT_NONE:
                break;
        }
    }
    ivec2 maxSize = ivec2(0);
    for (guibase* gui : guis) {
        if (gui == &scrollbar)
            continue;

        maxSize.x = math::max(maxSize.x, gui->right());
        maxSize.y = math::max(maxSize.y, gui->bottom());
    }
    prefSize.x               = math::max(maxSize.x, prefSize.x);
    contentHeight            = maxSize.y;
    const gui_scrollbar* bar = &scrollbar;
    if (maxHeight == -1) {
        hasScrollbar = maxSize.y > prefSize.y;
    } else if (maxHeight > 0 && maxSize.y > maxHeight) {
        prefSize.y   = maxHeight;
        // prefSize.y   = maxHeight - 5;
        hasScrollbar = true;
    } else {
        hasScrollbar = false;
        prefSize.y   = maxSize.y;
    }
    scrollbar.setVisible(hasScrollbar);
    scrollbar.parent = this;
    guis.erase(std::remove_if(guis.begin(), guis.end(), [bar](const guibase* x) {
                   return x == bar;
               }),
               guis.end());
    if (hasScrollbar) {
        guis.insert(guis.begin(), &scrollbar);
        scrollbar.parent = this;
    }
}

void guictr_scrollbar::onChildLayoutChanged(guibase* g) {
    layout();
    if (this->parent) {
        this->parent->onChildLayoutChanged(this);
    }
}
void guictr_scrollbar::scrollOffsetChanged(int dir, float offset) {
    this->scrollOffset = 0;
    if (hasScrollbar) {
        this->scrollOffset = -offset * (contentHeight - size.y);
        ivec2 pos = { 0, this->scrollOffset };
        for (guibase* gui : guis) {
            if (gui == &scrollbar)
                continue;

            gui->pos  = pos;
            switch(getLayoutMode()) {
                case autolayout_mode::LAYOUT_VERTICAL:
                    pos.y += gui->size.y;
                    break;
                case autolayout_mode::LAYOUT_HORIZONTAL:
                    pos.x += gui->size.x;
                    break;
                case LAYOUT_GRID:
                case LAYOUT_STACK:
                case LAYOUT_NONE:
                    break;
            }
        }
    }
}
void guictr_scrollbar::layout() {
    ivec2 cs    = getSizeContent();
    int scrollW = gui_scrollbar::defaultW;
    if (hasScrollbar) {
        if (scrollbarOutside) {
            scrollW        = gui_scrollbar::smallW;
            scrollbar.size = ivec2(scrollW - 2, cs.y - 2);
            scrollbar.pos  = ivec2(cs.x, 1);
            size.x += scrollW + 2;
        } else {
            scrollbar.size = ivec2(scrollW - 2, cs.y - 2);
            scrollbar.pos  = ivec2(cs.x - scrollW + 1, 1);
        }
        scrollOffsetChanged(1, scrollbar.scrollOffset);
    } else {
        // for (guibase* gui : guis) {
        //     if (gui == &scrollbar)
        //         continue;
        //     // gui->size.x = cs.x;
        // }
        scrollOffsetChanged(1, 0);
    }
    for (guibase* gui : guis) {
        if (gui == &scrollbar)
            continue;

        // gui->size = ivec2(cs.x, contentHeight);
        if (hasScrollbar) {
            int newRight  = cs.x - scrollW;
            int32_t right = gui->right();
            if (right > newRight) {
                gui->size.x = math::max(10, newRight - gui->pos.x);
                gui->layout();
            }
        }
    }
}

bool guictr_scrollbar::mouseHitTest(ivec2 v, MouseHitEvt& evt) {
    if (this->contains(v)) {
        ivec2 localMouse = this->toContainerSpace(v);
        for (guibase* gui : guis) {
            if (gui == &scrollbar && !scrollbar.isVisible())
                continue;
            if (gui->isVisible() && gui->mouseHitTest(localMouse, evt)) {
                return true;
            }
        }
    }
    return false;
}
ivec2 guictr_scrollbar::toScreenSpace(ivec2 in) const {
    in += getPosContent();
    if (this->parent != NULL) {
        in = this->parent->toScreenSpace(in);
    }
    return in;
}

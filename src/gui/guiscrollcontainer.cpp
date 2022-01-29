#include "guiscrollcontainer.h"
#include <vector>
#include "event.h"
#include "gui.h"
#include "guicontainer.h"
#include "guicolors.h"
#include "scrollbar.h"
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
        if (gui->size == ivec2{ 0, 0 }) {
            log_printf("warning, rendering container with size 0 0\n", 0);
        } else {
            gui->render(vg);
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
    for (guibase* gui : guis) {
        if (gui == &scrollbar)
            continue;

        gui->pos  = { 0, 0 };
        gui->size = prefSize;
        gui->determineSize(gui->size);
        gui->layout();
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
        prefSize.y   = maxHeight - 5;
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
    //glm::ivec2 prefSize = getSizeContent();
    //determineSize(prefSize);
    layout();
    if (this->parent != NULL) {
        this->parent->onChildLayoutChanged(this);
    }
}
void guictr_scrollbar::scrollOffsetChanged(int dir, float offset) {
    this->scrollOffset = 0;
    if (hasScrollbar) {
        this->scrollOffset = -offset * (contentHeight - size.y);
        for (guibase* gui : guis) {
            if (gui == &scrollbar)
                continue;
            gui->pos = { 0, this->scrollOffset };
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
            int entryW     = cs.x - scrollW;
            scrollbar.size = ivec2(scrollW - 2, cs.y - 2);
            scrollbar.pos  = ivec2(cs.x - scrollW + 1, 1);
        }
        scrollOffsetChanged(1, scrollbar.scrollOffset);
    } else {
        for (guibase* gui : guis) {
            if (gui == &scrollbar)
                continue;
            gui->size.x = cs.x;
        }
        scrollOffsetChanged(1, 0);
    }
    for (guibase* gui : guis) {
        if (gui == &scrollbar)
            continue;

        gui->size = ivec2(cs.x, contentHeight);
        if (hasScrollbar) {
            int newRight  = cs.x - scrollW;
            int32_t right = gui->right();
            if (right > newRight) {
                gui->size.x = math::max(10, newRight - gui->pos.x);
            }
        }
        gui->layout();
    }
}

bool guictr_scrollbar::mouseHitTest(ivec2 v, MouseHitEvt& evt) {
    if (this->contains(v)) {
        ivec2 localMouse = this->toContainerSpace(v);
        for (guibase* gui : guis) {
            if (gui == &scrollbar && !scrollbar.isVisible())
                continue;
            if (gui->mouseHitTest(localMouse, evt)) {
                return true;
            }
        }
    }
    return false;
}

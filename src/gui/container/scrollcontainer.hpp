#pragma once
#include <vector>
#include "math/vec.hpp"
#include "event.hpp"
#include "gui/gui.hpp"
#include "gui/container/container.hpp"
#include "guicolors.hpp"
#include "gui/controls/scrollbar.hpp"
#include "basectrl.hpp"

#define INSET_CTXT_MENU_X 1
#define INSET_CTXT_MENU_Y 2
static const ivec2 insetCtxtMenu = ivec2(INSET_CTXT_MENU_X, INSET_CTXT_MENU_Y);

class PopupCtrl;
class guictr_scrollbar : public guictr_base, public gui_scrollcontainer {
    friend class PopupCtrl;
    gui_scrollbar scrollbar;
    int scrollOffset  = 0;
    int contentHeight = 0;
    bool hasScrollbar = false;
    bool renderClipped = false;

public:
    bool scrollbarOutside = false;
    int maxHeight         = 360;
    guictr_scrollbar() : guictr_base(), scrollbar(1, 0.0f, *this) {
        setBackgroundRendered(true);
        setBackgroundRenderedInset(true);
        setSnapSides(ivec4(1));
        scrollbar.setParent(this);
        margin  = 0;
        padding = 0;
    }
    ~guictr_scrollbar() override {
        removeGuis();
    }
    void destroyGuis() override {
        if (hasScrollbar) {
            remove(&scrollbar);
            hasScrollbar = false;
        }
        guictr_base::destroyGuis();
    }
    gui_scrollbar& getScrollbar() {
        return scrollbar;
    }
    void render(NVGcontext* vg) override;
    void determineSize(ivec2& prefSize) override;
    void layout() override;
    void onChildLayoutChanged(guibase* g) override;
    bool mouseHitTest(ivec2 v, MouseHitEvt& evt) override;

    ivec2 getScrollTotalSize() const override {
        ivec2 cs = getSizeContent();
        cs.y     = contentHeight;
        return cs;
    }
    ivec2 getScrollViewSize() const override {
        return getSizeContent();
    }
    ivec2 toScreenSpace(ivec2 in) const override;
    void scrollOffsetChanged(int dir, float offset) override;
    bool handleMouseScroll(MouseEvent& evt, double xoffset, double yoffset) override {
        return scrollbar.handleMouseScroll(evt, xoffset, yoffset);
    }
    void setControl(BaseCtrl* parentCtrl) override {
        guictr_base::setControl(parentCtrl);
        scrollbar.setControl(parentCtrl);
    }
};

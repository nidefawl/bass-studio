#pragma once
#include <vector>
#include "math/vec.h"
#include "event.h"
#include "gui.h"
#include "guicontainer.h"
#include "guicolors.h"
#include "scrollbar.h"
#include "basectrl.h"

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

public:
    bool scrollbarOutside = false;
    int maxHeight         = 360;
    guictr_scrollbar() : guictr_base(), scrollbar(1, 0.0f, *this) {
        setBackgroundRendered(false);
        setBackgroundRenderedInset(false);
        setSnapSides(ivec4(1));
        scrollbar.setParent(this);
        margin  = 0;
        padding = 0;
    }
    ~guictr_scrollbar() {
        removeGuis();
    }
    gui_scrollbar& getScrollbar() {
        return scrollbar;
    }
    virtual void render(NVGcontext* vg);
    void determineSize(ivec2& prefSize) override;
    void layout() override;
    void onChildLayoutChanged(guibase* g) override;
    bool mouseHitTest(ivec2 v, MouseHitEvt& evt) override;

    ivec2 getScrollTotalSize() override {
        ivec2 cs = getSizeContent();
        cs.y     = contentHeight;
        return cs;
    }
    ivec2 getScrollViewSize() override {
        return getSizeContent();
    }
    void scrollOffsetChanged(int dir, float offset);
    virtual bool handleMouseScroll(MouseEvent& evt, double xoffset, double yoffset) {
        return scrollbar.handleMouseScroll(evt, xoffset, yoffset);
    }
    virtual void setControl(BaseCtrl* parentCtrl) override {
        guictr_base::setControl(parentCtrl);
        scrollbar.setControl(parentCtrl);
    }
};

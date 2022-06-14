#pragma once
#include <nanovg.h>
#include "gui/gui.h"
#include "guicolors.h"
#include "math/vec.h"

class gui_scrollcontainer {
public:
    gui_scrollcontainer() = default;
    virtual ~gui_scrollcontainer() = default;
    virtual ivec2 getScrollTotalSize() const = 0;
    virtual ivec2 getScrollViewSize() const = 0;
    virtual void scrollOffsetChanged(int dir, float offset)                         = 0;
    virtual bool handleMouseScroll(MouseEvent& evt, double xoffset, double yoffset) = 0;
};
class gui_scrollbar : public guibase {
    int dir;
    gui_scrollcontainer& ctr;
    float startOffset     = 0;
    float prevScrollRange = -1.0;
    float prevSetOffset   = -1.0;

public:
    float scrollOffset;
    static const int defaultW = 20;
    static const int smallW   = 10;
    gui_scrollbar(int _dir, float _offset, gui_scrollcontainer& _ctr);
    void render(NVGcontext* vg) override;
    void handleDraggedBegin(MouseEvent&  /*evt*/) override {
        startOffset = scrollOffset;
    }
    void setScrollOffset(float f);
    float getScrollRange() {
        ivec2 vcS = ctr.getScrollTotalSize();
        ivec2 vs  = ctr.getScrollViewSize();
        vec2 barOff(0);
        vec2 barS = size;
        if (vcS[dir] > 0) {
            barS[dir]   = math::min((float) size[dir], (vs[dir] / (float) vcS[dir]) * size[dir]);
            barOff[dir] = (size[dir] - barS[dir]) * scrollOffset;
        }
        return size[dir] - barS[dir];
    }
    double toPixels() const {
        ivec2 vcS    = ctr.getScrollTotalSize();
        ivec2 vs     = ctr.getScrollViewSize();
        int32_t dist = vcS[dir] - vs[dir];
        return math::max(0.0, (double) scrollOffset * dist);
    }
    void scrollVisible(int32_t y, int32_t size) {
        ivec2 vcS    = ctr.getScrollTotalSize();
        ivec2 vs     = ctr.getScrollViewSize();
        int32_t dist = vcS[dir] - vs[dir];
        if (dist > 0) {
            double pxOffset = toPixels();
            if (y < pxOffset) {
                double offset = y / (double) dist;
                setScrollOffset((float) offset);
            } else if (y + size > pxOffset + vs[dir]) {
                double offset = (y + size - vs[dir]) / (double) dist;
                setScrollOffset((float) offset);
            }
        }
    }
    void scrollTo(double pixels) {
        ivec2 vcS    = ctr.getScrollTotalSize();
        ivec2 vs     = ctr.getScrollViewSize();
        int32_t dist = vcS[dir] - vs[dir];
        if (dist > 0) {
            double offset = pixels / (double) dist;
            setScrollOffset((float) offset);
        }
    }
    void handleDraggedMove(MouseEvent& evt) override {
        float scrollRange = getScrollRange();
        if (scrollRange > 0) {
            int32_t dragPixels = (evt.mousepos - evt.dragStart)[dir];
            setScrollOffset(startOffset + dragPixels / (float) scrollRange);
        }
    }
    bool handleMouseScroll(MouseEvent& evt, double xoffset, double yoffset) override;
    void handleDraggedRelease(MouseEvent& evt) override {
    }
};

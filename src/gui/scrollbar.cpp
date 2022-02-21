#include <nanovg.h>
#include "math/vec.h"
#include "scrollbar.h"

#include "gui.h"
#include "guicolors.h"
#include "guiconstant.h"
#include "theme.h"
#include "basectrl.h"
#include "splitter.h"

void gui_scrollbar::render(NVGcontext* vg) {
    nvgBeginPath(vg);
    nvgRect(vg, pos.x, pos.y, size.x, size.y);
    NVGcolor bg = theme->getColor(GuiColor::COL_BASE_BG);
    nvgFillColor(vg, bg);
    nvgFill(vg);
    ivec2 vcS = ctr.getScrollTotalSize();
    ivec2 vs  = ctr.getScrollViewSize();
    if (vcS[dir] > 0 && vcS[dir] > vs[dir]) {
        vec2 barOff(0);
        vec2 barS     = size;
        barS[dir]     = math::min((float) size[dir], (vs[dir] / (float) vcS[dir]) * size[dir]);
        barOff[dir]   = (size[dir] - barS[dir]) * scrollOffset;
        int32_t inset = 1;
        nvgBeginPath(vg);
        const int minHandleHeight = 14;
        if (barS[dir] < minHandleHeight) {
            float h = minHandleHeight - barS[dir];
            barOff[dir] -= h / 2.0;
            barS[dir] = minHandleHeight;
        }
        nvgRect(vg, pos.x + barOff.x + inset, pos.y + barOff.y + inset, barS.x - inset * 2, barS.y - inset * 2);

        bool focused = parentCtrl->guiCtrFocused == this->parent || (parentCtrl->guiDragged == NULL && parentCtrl->guiOver == this);
        if (focused) {
//            nvgStrokeWidth(vg, 1.0f);
//            nvgStrokeColor(vg, theme->getColor(GuiColor::COL_BG_DRK_FOCUSED));
//            nvgStroke(vg);
            nvgFillColor(vg, theme->getColor(GuiColor::COL_GUI_HANDLE_FOCUSED));
        } else {
            nvgFillColor(vg, theme->getColor(GuiColor::COL_GUI_HANDLE));
        }
        nvgFill(vg);
    }
}

gui_scrollbar::gui_scrollbar(int _dir, float _offset, gui_scrollcontainer& _ctr) : guibase(), dir(_dir), ctr(_ctr), scrollOffset(_offset) {
    setCanMouseHit(true);
}


void Splitter::render(NVGcontext* vg) {
    if (!isVisible()) {
        log_printf("warning, skip rendering container with state !isVisible()\n", 0);
        return;
    }
    if (isBackgroundRendered()) {
        renderBackground(vg);
    }
    if (!setScissorTransform(vg)) {
        return;
    }
    for (auto c : guis) {
        if (!c->isVisible()) {
//            log_printf("warning, skip rendering child container with state !isVisible()\n", 0);
            continue;
        }
        if (c->size.x <= 0 || c->size.y <= 0) {
            log_printf("warning, skip rendering child container with size <= 0 0\n", 0);
            continue;
        }
    }
    if (parentCtrl && (parentCtrl->getGuiFocused() == this || parentCtrl->guiOver == this)) {
        nvgSave(vg);
        nvgBeginPath(vg);
        if (this->type) {
            nvgRect(vg, 0, 0, size.x, size.y);
        } else {
            nvgRect(vg, 0, 0, size.x, size.y);
        }
        nvgStrokeColor(vg, G_WHITE);
        nvgStrokeWidth(vg, 1.0f);
        nvgStroke(vg);
        nvgRestore(vg);
    }
}

void gui_scrollbar::setScrollOffset(float f) {
    float fRange = getScrollRange();
    if (fRange <= 0) {
        f = 0;
    }
//    if (fRange == 0 && prevScrollRange == 0) {
//        return;
//    }
    float _newOffset = f < 0 ? 0 : f > 1 ? 1
                                         : f;
//    if (_newOffset == 0 && prevSetOffset == 0) {
//        return;
//    }
    bool canSkip = math::abs(fRange - prevScrollRange) < 1 && math::abs(scrollOffset - _newOffset) < 0.001f;
    if (canSkip) {
        static int skipped = 0;
        skipped++;
        if (skipped % 20 == 0) {
            log_printf("setScrollOffset skipped %d\n", skipped);
        }
        return;
    }
    prevScrollRange = fRange;
    prevSetOffset   = scrollOffset;
    scrollOffset    = _newOffset;
    ctr.scrollOffsetChanged(dir, scrollOffset);
}

void Splitter::handleDraggedMove(MouseEvent& evt) {
    ivec2 windowSize;
    ivec2 mpos;
    if (notifyCtrl) {
        windowSize = notifyCtrl->getContainerSize();
        mpos       = evt.mousepos - this->parent->toScreenSpace(ivec2(0));
    } else {
        windowSize = parentCtrl->getScaledSize();
        mpos       = evt.mousepos;
    }
    float sc      = type == 0 ? (mpos.y / (float) (windowSize.y)) : (mpos.x / (float) (windowSize.x));
    int clampedAt = 0;
    if (sc < scaleMin) {
        clampedAt = -1;
    }
    if (sc > scaleMax) {
        clampedAt = 1;
    }
    this->scale = (sc < scaleMin ? scaleMin : sc > scaleMax ? scaleMax
                                                            : sc);
    if (notifyCtrl) {
        notifyCtrl->handleSplitterChanged(*this, this->scale, clampedAt);
    } else {
        parentCtrl->relayout();//TODO: this sucks, triggers a complete relayout
    }
}
void Splitter::addProperties(Table::tbl* table) {
    using Table::table_entry_t;
    using Table::tbl;
    using Table::tbl_row_t;
    using Table::tblfloat;
    using Table::tblint;
    using Table::tblstr;
    using Table::tblString;
    using Table::tbltypesaferef;
    SafeRef<guibase> ref = this->makeSafeRef();
    std::vector<tbl_row_t>& rows = table->rows;
    rows.push_back({{tblstr{"this"}, ref}});
    rows.push_back({{tblstr{"pos"}, tbltypesaferef<glm::ivec2>{ref, this->pos, nullptr}}});
    rows.push_back({{tblstr{"size"}, tbltypesaferef<glm::ivec2>{ref, this->size, nullptr}}});
    rows.push_back({{tblstr{"splittertype"}, tbltypesaferef<int>{ref, this->type, nullptr}}});
    rows.push_back({{tblstr{"scale"}, tbltypesaferef<float>{ref, this->scale, nullptr}}});
    rows.push_back({{tblstr{"scaleMin"}, tbltypesaferef<float>{ref, this->scaleMin, nullptr}}});
    rows.push_back({{tblstr{"scaleMax"}, tbltypesaferef<float>{ref, this->scaleMax, nullptr}}});
}

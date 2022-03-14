#include <nanovg.h>
#include <vector>

#include "tracktimeline.h"
#include "math/seq_math.h"
#include "grid.h"
#include "gui.h"
#include "host/mainctrl.h"

void guitrack_timeline::handleDraggedBegin(MouseEvent& evt) {
    if (evt.guiDragged == this) {
        parentCtrl->captureMouse(this);
        startDrag             = evt.relMousepos;
        dragDirection         = -1;
        float anchor_dragposx = (float) (startDrag.x < 50 ? 0 : evt.relMousepos.x);
        dragPosObjSpace       = grid.toObjSpace(anchor_dragposx);
    }
}
void guitrack_timeline::handleDraggedMove(MouseEvent& evt) {
    if (evt.guiDragged == this) {
        bool lockGesture = true;
        bool isMove      = true;
        if (lockGesture) {
            if (dragDirection < 0) {
                float initialx = (float) math::abs(evt.mousepos.x - evt.dragStart.x);
                float initialy = (float) math::abs(evt.mousepos.y - evt.dragStart.y);
                if (initialx + initialy < 4)
                    return;
                if (initialx > initialy) dragDirection = 1;
                else
                    dragDirection = 0;
            }
            isMove = dragDirection == 1;
        }
        float distx = (float) (evt.dragDistance->x);
        float disty = (float) (evt.dragDistance->y);

        if (math::abs(distx) > 5.0 && (!lockGesture || (lockGesture && isMove && math::abs(distx) > 2.0))) {
            adjustOffset(-evt.dragDistance->x);
            evt.dragDistance->x = 0;
        }

        if ((!lockGesture && math::abs(disty) > 5.0) || (lockGesture && !isMove && math::abs(disty) > 2.0)) {
            evt.dragDistance->y   = 0;
            disty                 = 1.0f + disty * -0.01f;
            float anchor_dragposx = (float) (startDrag.x < 50 ? 0 : evt.relMousepos.x);
            grid.setZoom(grid.zoom * disty);
            double newOffset = grid.calcOffset(anchor_dragposx, dragPosObjSpace);
            grid.setOffset((int) newOffset);
            grid.notifyChange();
        }
    }
}
void guitrack_timeline::adjustZoom(float mousePosXScreenSpaceLocal, float disty) {
    float posPreZoomX = grid.toObjSpace(mousePosXScreenSpaceLocal);
    grid.setZoom(grid.zoom * disty);
    double newOffset = grid.calcOffset(mousePosXScreenSpaceLocal, posPreZoomX);
    grid.setOffset((int) newOffset);
    grid.notifyChange();
}
void guitrack_timeline::adjustOffset(float gridOffset) {
    grid.setOffset(grid.offset + gridOffset);
    grid.notifyChange();
}
void guitrack_timeline::handleDraggedRelease(MouseEvent& evt) {
    DawInstance::get()->updateVisibleTrackContents();
}
void guitrack_timeline::render(NVGcontext* vg) {
    if (!setScissorTransform(vg)) {
        return;
    }
    std::vector<grid_div>& gridList = grid.gridList;
    if (gridList.empty())
        return;

    int printoffset = 1;
    int gap         = 16;

    String textTmp = StringFormat("%d.%d.%d", 333, 4, 4);

    UIFont::font_instance instance = theme->getFont(UIFont::FONT_DECIMAL);
    UIFont::bindFont(vg, instance);
    float textWidth = nvgTextBounds(vg, 0, 0, StringAsCStr(textTmp), nullptr, nullptr);

    float barSize = grid.bar_size;
    int step      = 1;
    while (barSize < textWidth) {
        step *= 2;
        barSize *= 2;
    }
    int substeps = -1;
    if (step == 1 && barSize > textWidth) {
        substeps = 4;
        while (substeps > 1 && barSize > textWidth * 2) {
            substeps /= 2;
            barSize /= 2;
        }
    }
    int subsubsteps = -1;
    if (substeps == 1 && barSize > textWidth) {
        subsubsteps = 4;
        while (subsubsteps > 1 && barSize > textWidth * 2) {
            subsubsteps /= 2;
            barSize /= 2;
        }
    }

    String text;
    int fontSize;

    nvgTextAlign(vg, NVG_ALIGN_BOTTOM | NVG_ALIGN_LEFT);
    float scale = 1.33f;
    for (int a = 0; a < 2; a++) {
        for (grid_div& n : gridList) {
            beatbar16th_t notePos = n.pos;
            if ((n.time & TICK_MASK_16TH) != 0)
                continue;
            if (notePos.bar % step == 0) {
                if ((notePos.beat == 0 || ((substeps > 0 && notePos.beat % substeps == 0))) &&
                    (notePos.th == 0 || (subsubsteps > 0 && notePos.th % subsubsteps == 0))) {
                    if (a == 0) {
                        int h = 10;

                        if (notePos.beat != 0 || notePos.th != 0) {
                            h = 5;
                        }
                        nvgBeginPath(vg);
                        nvgMoveTo(vg, n.screenpos, this->size.y);
                        nvgLineTo(vg, n.screenpos, this->size.y - h * scale);
                        nvgStrokeWidth(vg, 1.f);
                        nvgStrokeColor(vg, THEMECOL_WHITE);
                        nvgStroke(vg);
                    } else {
                        int color = -1;
                        if (notePos.beat != 0 || notePos.th != 0) {
                            fontSize = 14;
                            if (notePos.th != 0) {
                                color = 0xababab;
                                text  = StringFormat(".%d.%d", notePos.beat + printoffset, notePos.th + printoffset);
                            } else {
                                text = StringFormat("%d.%d", notePos.bar + printoffset, notePos.beat + printoffset);
                            }

                        } else {
                            fontSize = 16;
                            text     = StringFormat("%d", notePos.bar + printoffset);
                        }
                        nvgFontSize(vg, fontSize * scale);
                        nvgFillColor(vg, rgbToNvg(color));
                        float posXTimeCode = n.screenpos + gap / 2;
                        /*float posX = */nvgText(vg, posXTimeCode, this->size.y, StringAsCStr(text), nullptr);

                        //if (this->size.y > 28) {
                        //    text = StringFormat("%d", n.time);
                        //    nvgFontSize(vg, fontSize * scale * 0.66f);
                        //    //nvgText(vg, n.screenpos + gap / 2, this->size.y - 21, StringAsCStr(text), NULL);
                        //    text = StringFormat("%f", n.screenpos);
                        //    nvgText(vg, n.screenpos + gap / 2, this->size.y - 21, StringAsCStr(text), NULL);
                        //}
                    }
                }
            }
        }
    }
}

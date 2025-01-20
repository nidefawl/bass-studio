#include <nanovg.h>
#include <vector>

#include "guicolors.hpp"
#include "logging.hpp"
#include "tracktimeline.hpp"
#include "math/seq_math.hpp"
#include "grid.hpp"
#include "gui/gui.hpp"
#include "host/daw/mainctrl.hpp"


bool guitrack_timeline::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
    if (this->contains(mpos) && evt.type <= MouseHitType::MOUSE_RIGHT) {
        evt.requestCursor(CURSOR_ZOOM);
    }
    return guictr_base::mouseHitTest(mpos, evt);
}

void guitrack_timeline::handleDraggedBegin(MouseEvent& evt) {
    if (evt.guiDragged == this) {
        if (evt.type == MouseEventType::M_EVT_DOUBLECLICK) {
            parent->buttonClicked(this);
            return;
        }
        parentCtrl->captureMouse(this);
        startDrag             = evt.relMousepos;
        dragDirection         = -1;
        dragPosSS = static_cast<float>(startDrag.x < 50 ? 0 : evt.relMousepos.x);
        dragPosObjSpace       = grid.toObjSpace(dragPosSS);
    }
}

void guitrack_timeline::handleDraggedMove(MouseEvent& evt) {
    if (evt.guiDragged == this) {
        bool lockGesture = true;
        bool isMove      = true;
        if (lockGesture) {
            if (dragDirection < 0) {
                auto initialx = math::abs(evt.mousepos.x - evt.dragStart.x);
                auto initialy = math::abs(evt.mousepos.y - evt.dragStart.y);
                if (initialx + initialy >= 4) {
                    if (initialx > initialy)
                        dragDirection = 1;
                    else
                        dragDirection = 0;
                }
            }
            isMove = dragDirection == 1;
        }
        bool bChanged = false;
        if (math::abs(evt.dragDistance->x) > 5 && (!lockGesture || (lockGesture && isMove && math::abs(evt.dragDistance->x) > 2))) {
            const auto distx = evt.dragDistance->x;
            evt.dragDistance->x = 0;
            grid.setOffset(grid.offset + -distx);
            bChanged = true;
        }

        if ((!lockGesture && math::abs(evt.dragDistance->y) > 5) || (lockGesture && !isMove && math::abs(evt.dragDistance->y) > 2)) {
            const auto disty = 1.0f + evt.dragDistance->y * -0.01f;
            evt.dragDistance->y   = 0;
            grid.setZoom(grid.zoom * disty);
            double newOffset = grid.calcOffset(dragPosSS, dragPosObjSpace);
            grid.setOffset((int) newOffset);
            bChanged = true;
        }

        if (bChanged) {
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
    dawCtrl->getDaw()->updateVisibleTrackContents();
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

    grid_div& last = gridList.back();
    String textTmp = StringFormat("%d.%d.%d", last.pos.bar, last.pos.beat, last.pos.th);

    theme->bindFont(vg, UIFont::FONT_DECIMAL);
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
                        auto color = GuiColor::COL_LABEL_ACTIVE;
                        if (notePos.beat != 0 || notePos.th != 0) {
                            fontSize = 14;
                            if (notePos.th != 0) {
                                color = GuiColor::COL_LABEL_INACTIVE;
                                text  = StringFormat(".%d.%d", notePos.beat + printoffset, notePos.th + printoffset);
                            } else {
                                text = StringFormat("%d.%d", notePos.bar + printoffset, notePos.beat + printoffset);
                            }

                        } else {
                            fontSize = 16;
                            text     = StringFormat("%d", notePos.bar + printoffset);
                        }
                        nvgFontSize(vg, fontSize * scale);
                        nvgFillColor(vg, theme->getColor(color));
                        float posXTimeCode = float(n.screenpos) + gap / 2.0f;
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

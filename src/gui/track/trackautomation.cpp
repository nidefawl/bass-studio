#include "trackautomation.h"

#include "assert_dbg.h"
#include "grid_constants.h"
#include "guicolors.h"
#include "guiconstant.h"
#include "guiglobals.h"
#include "math/seq_math.h"
#include "gui/gui.h"
#include "cursor.h"
#include "event.h"
#include "color_util.h"
#include "seq_util.h"
#include "host/track/track.h"
#include "host/clip/clip.h"
#include "grid.h"
#include "gui/container/container.h"
#include "str_util.h"
#include "trackctr.h"
#include "basectrl.h"
#include "host/daw/mainctrl.h"
#include "host/automation/automation.h"
#include "host/track/track.h"
#include "host/track/track_impl.h"
#include "renderresources.h"
#include <cstdint>
#include <nanovg.h>

float dataToCtr(float x, float ctrHeight) {
    return (1.0f - x) * ctrHeight;
}
float ctrToData(float screenX, float ctrHeight) {
    return 1.0f - (screenX / ctrHeight);
}
float ctrToDataScale(float screenX, float ctrHeight) {
    return (screenX / ctrHeight);
}

float gui_track_automation::getDstVal() {
    if (at) {
        return at->getParam(paramIdx)->getValue();
    }
    return 0.5f;
}
void gui_track_automation::setDstVal(float f) {
    if (at) {
        at->setParamValue(paramIdx, f, FLG_PAR_UPDATE_USER);
    }
}
using hit_result = gui_track_automation::hit_result;
hit_result gui_track_automation::hitTest(vec2 mpos) {
    if (data.points.empty()) {
        float fDstVal = getDstVal();
        ivec2 cs      = getSizeContent();
        float dstValY = dataToCtr(fDstVal, cs.y);
        float dist    = math::abs(mpos.y - dstValY);
        if (dist < 10) {
            return { drag_empty, -1, -1, dist };
        }
        return { drag_none, -1, -1, 0 };
    }
    std::vector<hit_result> hit;
    for (int32_t i = 0; i < CtrSize(segments); i++) {
        path_segment_t& segment = segments[i];
        auto segBegin           = segment.points.begin();
        auto segEnd             = segment.points.end();
        float minSegLineDist    = 16;
        for (auto it = segBegin; it != segEnd && (it + 1) != segEnd; ++it) {
            int32_t idxSegStart = *it;
            int32_t idxSegEnd   = *(it + 1);
            vec2& ptStart       = cachedShape[idxSegStart];
            vec2& ptEnd         = cachedShape[idxSegEnd];
            float dist          = math::distancePointLine(mpos, ptStart, ptEnd);
            minSegLineDist      = math::min(dist, minSegLineDist);
        }
        if (minSegLineDist < 10) {
            hit_result hitSeg{ dragmode::drag_segment, segment.dataOffset, i, minSegLineDist };
            hit.push_back(std::move(hitSeg));

            int32_t ptIdx      = segment.points.front();
            vec2& ptStart      = cachedShape[ptIdx];
            float distPtEndSeg = math::distvec2(ptStart, mpos);
            if (distPtEndSeg < 10 && segment.dataOffset > -1) {

                hit_result hitNode{ dragmode::drag_node, segment.dataOffset, ptIdx, distPtEndSeg - 4 };
                hit.push_back(std::move(hitNode));
            }
//            if (dataPtIdx + 1 < data.points.size()) {
//                float distB = glm::distance(ptEnd, mpos);
//                hit.push_back({ dragmode::drag_node, dataPtIdx + 1, i + segOffset, distB - 4 });
//            }
        }
    }
    std::sort(hit.begin(), hit.end(), [](hit_result const& a, hit_result const& b) {
        return a.dist < b.dist;
    });
    if (hit.size()) {

        hit_result& h = hit[0];
        if (h.dist < 10) {
            return h;
        }
    }
    return { drag_none, -1, -1, 0 };
}
void gui_track_automation::trackViewDragBegin(guitrack_editor* view, MouseEvent& evt) {
    dragged                = hit_result(dragmode::drag_none, -1, -1, 0.0f);
    canSimplify            = true;
    ivec2 trackEditorLocal = evt.relMousepos;
    ivec2 local            = toContainerSpace(trackEditorLocal);
    tick_t tickAt          = grid.screenToTick(trackEditorLocal.x);
    DAW::Cursor& cursor    = view->cursor;
    dragged                = hitTest(local);
    dataPointsCopy   = data.points;
    trackstate_t trackSnapshot;
    if (dragged.mode != dragmode::drag_none) {
        trackSnapshot.tracks.push_back(new track_snapshot_t(this->at->getTrack(), tracksnapshot_store_opts_t::AutomationOnly()));
        trackSnapshot.cursor = cursor;
    }
    if (dragged.mode != dragmode::drag_node && cursor.containsSubtrack(this->m_trackentry->idx, this->subtrackIdx, tickAt)) {
                
        int32_t steps     = at->getQuantizationSteps(paramIdx);
        float fInitialVal = 0.5f;
        if (at) {
            fInitialVal = math::min(1.0f, math::max(0.0f, at->quantizeVal(paramIdx, getDstVal())));
        }
        addPointAt(data.points, cursor.getTickBegin(), steps, fInitialVal);
        int32_t idx  = addPointAt(data.points, cursor.getTickBegin(), steps, fInitialVal);
        int32_t idx2 = addPointAt(data.points, cursor.getTickEnd(), steps, fInitialVal);
        addPointAt(data.points, cursor.getTickEnd(), steps, fInitialVal);

        dawCtrl->getDaw()->updateVisibleTrackContents();
        dragged           = hitTest(local);
        dragged.mode      = dragmode::drag_selection;
        dragged.dataPt    = idx;
        dragged.numPoints = idx2 - idx + 1;
    }
    if (dragged.mode != dragmode::drag_none && dragged.mode != dragmode::drag_empty) {
        dawCtrl->getDaw()->pushHist(new action_modify_track(StringFormat("Edit Automation (%d)", (int)dragged.mode), std::move(trackSnapshot)));
    }

    dataPointsEdited = data.points;
}
void gui_track_automation::trackViewDragMove(guitrack_editor* view, MouseEvent& evt) {
    //ivec2 trackEditorLocal = evt.relMousepos;
    ivec2 cs          = getSizeContent();
    int32_t disty     = -evt.dragDistance->y;
    int32_t distx     = evt.dragDistance->x;
    if (grid.pixelsToTicks(math::abs(distx)) < grid.getTickLength() / 32) {
        distx = 0;
    } else {
        evt.dragDistance->x = 0;
    }
    float valOffset = ctrToDataScale(disty, cs.y);
    if (dragged.mode == dragmode::drag_empty) {
        evt.dragDistance->y = 0;
        float fOld          = getDstVal();
        float fDstVal       = fOld + valOffset;
        if (at) {
            float fNew = at->quantizeVal(paramIdx, fDstVal);
            if (fNew == fOld) {
                return;
            }
            fDstVal = fNew;
        }
        evt.dragDistance->y = 0;
        fDstVal             = math::min(1.0f, math::max(0.0f, fDstVal));
        setDstVal(fDstVal);
    } else if (dragged.mode) {
        evt.dragDistance->y                            = 0;
        std::vector<automation_point_t>& dataPoints    = dataPointsEdited;
        std::vector<automation_point_t>& pointsClamped = data.points;
        bool firstSegment                              = dragged.dataPt < 0;
        int32_t dataPtIdx1;
        tick_t tickOffset = grid.pixelsToTicks2(distx);
        int32_t numPoints = 1;
        if (firstSegment) {
            dataPtIdx1 = 0;
        } else {
            dataPtIdx1 = dragged.dataPt;
            if (dragged.mode == dragmode::drag_selection) {
                numPoints = dragged.numPoints;
            }
            if (dragged.mode == dragmode::drag_segment) {
                numPoints = 2;
            }
        }


        int32_t dataPtIdx2    = dataPtIdx1 + numPoints - 1;
        bool anyNonSaturatedY = false;
        for (int i = dataPtIdx1; i <= dataPtIdx2; i++) {
            if (i >= 0 && i < CtrSize(dataPoints)) {
                automation_point_t& pt = dataPoints[i];
                anyNonSaturatedY |= (disty < 0 ? pt.val > 0.0f : pt.val < 1.0f);
            }
        }
        automation_point_t zero   = { 0, 0 };
        automation_point_t* minPt = NULL;
        automation_point_t* maxPt = NULL;
        automation_point_t* ptBegin = data.points.empty() || dataPtIdx1 >= CtrSize(data.points) ? nullptr : &data.points[dataPtIdx1];
        automation_point_t* ptEnd = data.points.empty() || dataPtIdx2 >= CtrSize(data.points) ? nullptr : &data.points[dataPtIdx2];
        if (!firstSegment && dataPtIdx1 > 0 && dataPtIdx1 < (int) pointsClamped.size()) {
            minPt = &pointsClamped[dataPtIdx1 - 1];
        } else if (dataPtIdx1 >= 0) {
            minPt = &zero;
        }
        if (dataPtIdx2 >= 0 && dataPtIdx2 + 1 < (int) pointsClamped.size()) {
            maxPt = &pointsClamped[dataPtIdx2 + 1];
        }
        bool bSnapToTickGrid = true;
        auto tickSnapped = math::max(0, grid.screenToTickSnap(evt.relMousepos.x, isAlt(evt.kbmods) ? SNAP_OFF : SNAP_ON));
        if (tickOffset < 0) {
            if (bSnapToTickGrid && ptBegin) {
                tickOffset = tickSnapped - ptBegin->time;
                if (tickOffset > 0) {
                    tickOffset = 0;
                }
            }
            if (dataPtIdx1 >= 0 && dataPtIdx1 < CtrSize(dataPoints)) {
                automation_point_t& ptEd = dataPoints[dataPtIdx1];
                if (minPt) {
                    auto distToPrev = ptEd.time - minPt->time;
                    tickOffset = math::max(-distToPrev, tickOffset);
                }
            }
        }
        else if (tickOffset > 0) {
            if (bSnapToTickGrid && ptEnd) {
                tickOffset = tickSnapped - ptEnd->time;
                if (tickOffset < 0) {
                    tickOffset = 0;
                }
            }
            if (dataPtIdx2 >= 0 && dataPtIdx2 < CtrSize(dataPoints)) {
                automation_point_t& ptEd = dataPoints[dataPtIdx2];
                if (maxPt) {
                    auto distToNext = maxPt->time - ptEd.time;
                    tickOffset = math::min(distToNext, tickOffset);
                }
            }
        }
        for (int i = dataPtIdx1; i <= dataPtIdx2; i++) {
            if (i >= 0 && i < CtrSize(dataPoints)) {
                automation_point_t& pt = dataPoints[i];
                if (tickOffset) {
                    pt.time = pt.time + tickOffset;
                }
                if (anyNonSaturatedY) {
                    pt.val = pt.val + valOffset;
                }
            }
        }
        for (int i = dataPtIdx1; i <= dataPtIdx2; i++) {
            if (i >= 0 && i < CtrSize(dataPoints)) {
                automation_point_t& src = dataPoints[i];
                automation_point_t& dst = pointsClamped[i];
                tick_t newTick          = src.time;
                if (minPt) {
                    newTick = math::max(newTick, minPt->time);
                }
                if (maxPt) {
                    newTick = math::min(newTick, maxPt->time);
                }
                dbgassert(tickOffset || newTick == dst.time);
                dst.time = newTick;
                float f  = math::min(1.0f, math::max(0.0f, src.val));
                if (at) {
                    f = at->quantizeVal(paramIdx, f);
                }
                dst.val = f;
            }
        }
        postEdit();
    }
}
void gui_track_automation::trackViewDragRelease(guitrack_editor* view, MouseEvent& evt) {
    dragged = hit_result(dragmode::drag_none, -1, -1, 0.0f);
    if (canSimplify)
        simplifyData(data.points);
    postEdit();
}
void gui_track_automation::postEdit() {
    auto automatable = this->at;
    if (automatable) {
        auto lock = dawCtrl->lockPlayThread();
        auto atLane = &automatable->getOrCreateAutomation(paramIdx)->src;
        if (atLane) {
            bool activate      = atLane->points.empty() && !data.points.empty();
            atLane->points = data.points;
            if (activate)
                atLane->activate();
        }
    }
    dawCtrl->getDaw()->updateVisibleTrackContents();
}
bool gui_track_automation::trackViewDoubleClick(guitrack_editor* view, MouseEvent& evt) {
    dragged                                     = hit_result(dragmode::drag_none, -1, -1, 0.0f);
    ivec2 trackEditorLocal                      = evt.relMousepos;
    ivec2 local                                 = toContainerSpace(trackEditorLocal);
    hit_result clicked                          = hitTest(local);
    canSimplify                                 = false;

    trackstate_t resizePreModifyState;
    resizePreModifyState.tracks.push_back(new track_snapshot_t(this->at->getTrack(), tracksnapshot_store_opts_t::AutomationOnly()));
    resizePreModifyState.cursor = view->cursor;
    dawCtrl->getDaw()->pushHist(new action_modify_track(StringFormat("Edit Automation (%d)", (int)dragged.mode), std::move(resizePreModifyState)));
    std::vector<automation_point_t>& dataPoints = data.points;
    if (clicked.mode == dragmode::drag_node) {
        int32_t i = clicked.dataPt;
        dbgassert(i >= 0 && i < CtrSize(dataPoints));
        dataPoints.erase(dataPoints.begin() + i);
        postEdit();
        return true;
    } else {
        // this is true until we relayout UI and move mixers left or something
        dbgassert(trackEditorLocal.x == local.x);

        ivec2 cs          = getSizeContent();
        tick_t tick       = grid.screenToTickSnap(trackEditorLocal.x, isAlt(evt.kbmods) ? SNAP_OFF : SNAP_ON);
        float val         = ctrToData(local.y, cs.y);
        if (at) {
            const auto* automation = at && paramIdx > -1 ? at->getRegisteredConstAutomation(paramIdx) : nullptr;
            if (automation) {
                val = automation->getValueAt(tick);
            } else {
                val = at->getParam(paramIdx)->getValue();
                // val = at->quantizeVal(paramIdx, val);
            }
        }
        val         = math::min(1.0f, math::max(0.0f, val));
        int32_t idx = indexOfTick(dataPoints, tick);
        dbgassert(idx >= 0 && idx <= CtrSize(dataPoints));
        automation_point_t pt{ tick, val };
        dataPoints.insert(dataPoints.begin() + idx, pt);
        postEdit();
        return true;
    }
}

void gui_track_automation::handleDraggedBegin(MouseEvent& evt) {
    dawCtrl->setSelectedTrack(m_track);
    evt.relMousepos += getPosContent();
    parent->handleDraggedBegin(evt);
}

void gui_track_automation::handleDraggedMove(MouseEvent& evt) {
    evt.relMousepos += getPosContent();
    parent->handleDraggedMove(evt);
}

void gui_track_automation::handleDraggedRelease(MouseEvent& evt) {
    evt.relMousepos += getPosContent();
    parent->handleDraggedRelease(evt);
}

bool gui_track_automation::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
    if (!this->at || this->paramIdx < 0) {
        return false;
    }
    if (this->contains(mpos)) {
        ivec2 localMouse = this->toContainerSpace(mpos);
        for (guibase* gui : guis) {
            if (gui->isVisible() && gui->mouseHitTest(localMouse, evt)) {
                return true;
            }
        }
        if (evt.type == MouseHitType::MOUSE_RIGHT) {// righclick in selection (create clip etc.)
            tick_t tick       = grid.screenToTickSnap(mpos.x, SNAP_OFF);

            // automation subtracks have priority over parent if they are in the selection range
            if (this->subtrackIdx >= 0 && m_trackentry->parentCtrl->getCursor().containsSubtrack(this->m_trackentry->idx, this->subtrackIdx, tick)) {
                evt.requestFocus(this);
                return true;
            } else if (this->subtrackIdx < 0) {
                //if its the overlay automation we only take focus if its an actual hit
                hit_result hit = hitTest(localMouse);
                if (hit.mode) {
                    evt.requestFocus(this);
                    return true;
                }
            }
            return false;
        }
        if (evt.type <= MouseHitType::MOUSE_LEFT) {
            hit_result hit = hitTest(localMouse);
            if (hit.mode) {
                evt.requestFocus(this);
                return true;
            }
        }
        // tracks need to always cancel further mouse tests for z-order to work in parent container
//        return true;
    }
    return false;
}

void gui_track_automation::updateVisibleTrackContents(scaled_grid& editorGrid) {
    ivec2 cs = getSizeContent();

    std::vector<automation_point_t>& dataPoints = data.points;
    cachedShape.clear();
    segments.clear();
    int32_t segmentDataPtOffset = 0;
    if (!dataPoints.empty()) {
        cachedShape.reserve(dataPoints.size() + 4);
        segments.reserve(dataPoints.size() + 4);
        automation_point_t& firstData = dataPoints[0];
        segmentDataPtOffset = 0;
        float firstX        = (float) grid.tickToScreenD(firstData.time);
        int steps           = at->getQuantizationSteps(paramIdx);
        float firstY        = dataToCtr(firstData.val, cs.y);
        {// > left start of viewable area
            cachedShape.push_back({ -4.0f, firstY });
            path_segment_t seg;
            seg.dataOffset = -1;
            seg.points.push_back(0);
            seg.points.push_back(1);
            segments.push_back(std::move(seg));
            segmentDataPtOffset = -1;
        }
        float lastVal = dataPoints[0].val;
        cachedShape.push_back({ firstX, firstY });
        for (int i = 1; i < CtrSize(dataPoints); i++) {
            path_segment_t seg;
            seg.dataOffset = i + segmentDataPtOffset;
            seg.points.push_back(cachedShape.size() - 1);
            seg.points.push_back(cachedShape.size());
            automation_point_t& nextPt = dataPoints[i];
            float ptX = (float) grid.tickToScreenD(nextPt.time);
            if (steps && nextPt.val != lastVal) {
                vec2 prev = cachedShape.back();
                prev.x    = ptX;
                cachedShape.push_back(std::move(prev));
                seg.points.push_back(cachedShape.size());
            }
            float ptY = dataToCtr(nextPt.val, cs.y);
            cachedShape.push_back({ ptX, ptY });
            segments.push_back(std::move(seg));
            lastVal = nextPt.val;
        }
        vec2& last = cachedShape.back();
        if (last.x < cs.x) {// < right end of viewable area
            path_segment_t seg;
            seg.dataOffset = dataPoints.size() + segmentDataPtOffset;
            seg.points.push_back(cachedShape.size() - 1);
            seg.points.push_back(cachedShape.size());
            cachedShape.push_back({ cs.x + 4.0f, last.y });
            segments.push_back(std::move(seg));
        }
    }
}

void gui_track_automation::render(NVGcontext* vg) {
    if (!this->at || this->paramIdx < 0) {
        return;
    }

    ivec2 sizeInset = getSizeContent();
    if (sizeInset.y <= 0 || sizeInset.x <= 0) {
        return;
    }
    ivec2 posInset = getPosContent();

    const int htt = theme->get(GuiConstant::CONST_SMALL_LABEL_HEIGHT);
    String strTitle = this->at->getAutomatableName();
    strTitle += " ";
    strTitle += this->at->getParamName(paramIdx);
    renderTextLabel(vg,
                    vec2(pos) + vec2(htt/4, size.y),
                    vec2(size.x, math::min(htt, size.y)),
                    strTitle,
                    theme,
                    htt,
                    theme->getColor(GuiColor::COL_LABEL_AUTOMATION_TRACK),
                    NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM
                    );
    nvgTranslate(vg, posInset.x, posInset.y);

    ivec2 imouse = toControlsObjectSpace(dawCtrl->m_mousePos, this);

    const bool bGlbCfg_DrawAutomationLaneMouseCursor = true;
    if (bGlbCfg_DrawAutomationLaneMouseCursor) {
        const int bGlbCfg_MouseCursorType     = 0;
        float fAutomationLaneMouseCursorWidth = bGlbCfg_MouseCursorType == 0 ? 2 : 5;
        nvgBeginPath(vg);
        nvgCircle(vg, imouse.x, imouse.y, fAutomationLaneMouseCursorWidth);
        nvgFillColor(vg, theme->getColor(GuiColor::COL_AUTOMATED));
        nvgFill(vg);
    }

    bool mouseIn              = dawCtrl->getGuiOver() == this && contains(imouse + getPosContent());
    tick_t mouseTick    = !mouseIn ? INVALID_TICK : grid.screenToTickSnap(imouse.x, SNAP_OFF);
    vec2 fmouse               = vec2(imouse);
    bool bIsDragging = dragged.mode;
    auto mouseHit = hitTest(fmouse);
    const hit_result& currentDragged = dragged.mode || !mouseIn ? dragged : mouseHit;
    float valAtMouse = 0.0f;
    auto& cursor = dawCtrl->getCursor();
    if (currentDragged.mode == dragmode::drag_node) {
        int32_t ptIdx = currentDragged.dataPt;
        dbgassert(ptIdx >= 0 && ptIdx < (int) data.points.size());
        automation_point_t& pt = data.points[ptIdx];
        vec2* point = getPathPointSafe(currentDragged.segidx);
        mouseTick   = pt.time;
        if (point)
            fmouse.x = point->x;
        valAtMouse = pt.val;
    } else {
        valAtMouse = data.getValueAt(mouseTick);
        if (mouseHit.mode != dragmode::drag_none && cursor.containsSubtrack(this->m_trackentry->idx, this->subtrackIdx, mouseTick)) {
            mouseHit.mode = dragmode::drag_selection;
        }
    }

    if (!segments.empty()) {
        int len     = segments.size();
        int i       = 0;
        for (; i < len; i++) {
            path_segment_t& segment = segments[i];
            vec2* pt2               = getPathPointSafe(segment.points.back());
            if (pt2 && pt2->x > -4) {
                break;
            }
        }
        int start = i;
        if (i < len) {
            GuiColor::constant_t c1 = isActive() ? this->color : this->colorInactive;
            //                nvgLineJoin(vg, NVGlineCap::NVG_BEVEL);
            nvgBeginPath(vg);
            bool first = true;
            // vec2* ptLast = nullptr;
            for (; i < len; i++) {
                path_segment_t& segment = segments[i];
                for (int& point : segment.points) {
                    vec2* pt2 = getPathPointSafe(point);
                    if (first) {
                        first = false;
                        nvgMoveTo(vg, pt2->x, pt2->y);
                    } else {
                        // nvgBeginPath(vg);
                        // nvgMoveTo(vg, ptLast->x, ptLast->y);
                        // nvgLineTo(vg, pt2->x, pt2->y);
                        // nvgStrokeColor(vg, dbgcolorsArray[i % 8]);
                        // nvgStrokeWidth(vg, lineWidth);
                        // nvgStroke(vg);
                        nvgLineTo(vg, pt2->x, pt2->y);
                        if (pt2->x > sizeInset.x + 4) break;
                    }
                    // ptLast = pt2;
                }
            }
            int end = i;
            nvgStrokeColor(vg, theme->getColor(c1));
            nvgStrokeWidth(vg, lineWidth);
            nvgStroke(vg);
            nvgLineJoin(vg, NVGlineCap::NVG_MITER);

            //Lots of room for optimization here (draw texture for dot, or use custom shader)
            auto ptIdxEnd = math::min(len - 1, end);
            auto nPoints = ptIdxEnd - start;
            if (nPoints > 128)
                nvgShapeAntiAlias(vg, 0);
            nvgBeginPath(vg);
            for (int j = start; j < ptIdxEnd; j++) {
                path_segment_t& segment = segments[j];
                if (currentDragged.mode == dragmode::drag_segment && currentDragged.segidx == j) {
                    continue;
                }
                int32_t idxPtEnd = segment.points.back();
                if (currentDragged.mode == dragmode::drag_node) {
//                    if (currentDragged.segidx >= segment.points.front() && currentDragged.segidx <= segment.points.back()) {
//                        continue;
//                    }
                    if (currentDragged.segidx == idxPtEnd) {
                        continue;
                    }
                }
                vec2* pt = getPathPointSafe(idxPtEnd);
                nvgCircleFastNDivs(vg, pt->x, pt->y, radiusHandle, 6);
            }
//            nvgFillColor(vg, theme->getColor(c1));
//            nvgFill(vg);
            nvgStrokeColor(vg, theme->getColor(color2));
            nvgStrokeWidth(vg, 1.5f);
            nvgStroke(vg);
            if (nPoints > 128)
                nvgShapeAntiAlias(vg, USE_NANOVG_AA);
        }

        if ((currentDragged.mode == dragmode::drag_segment || currentDragged.mode == dragmode::drag_selection)) {

            int32_t firstPtIdx = -1;
            int32_t lastPtIdx  = 0;
            bool bIntersect = false;
            bool bRender = true;
            if (currentDragged.mode == dragmode::drag_segment) {
                auto segment = getSegmentSafe(currentDragged.segidx);
                if (segment) {
                    firstPtIdx = segment->dataOffset;
                    lastPtIdx  = segment->dataOffset+1;
                } else {
                    bRender = false;
                }   
            }
            if (currentDragged.mode == dragmode::drag_selection) {
                firstPtIdx = indexOfTick(data.points, cursor.getTickBegin()) - 1;
                lastPtIdx = indexOfTick(data.points, cursor.getTickEnd());
                bIntersect = true;
                float pxSelectionBegin = grid.tickToScreenD(cursor.getTickBegin());
                float pxSelectionEnd = grid.tickToScreenD(cursor.getTickEnd());
                nvgSave(vg);
                nvgIntersectScissor(vg, pxSelectionBegin, 0, pxSelectionEnd - pxSelectionBegin, sizeInset.y);
                // dbgassert(firstPtIdx >= 0 && firstPtIdx < CtrSize(data.points));
                // dbgassert(lastPtIdx >= 0 && lastPtIdx < CtrSize(data.points));
                automation_point_t apFirst = data.points.front();
                apFirst.time = 0;
                auto dataFirst = firstPtIdx < 0 ? apFirst : data.points[firstPtIdx];
                auto dataLast = lastPtIdx < 0 ? apFirst : data.points[lastPtIdx];
                if (firstPtIdx == 0 && apFirst.time >= cursor.getTickBegin()) {
                    dataFirst.time = cursor.getTickBegin();
                }
                nvgBeginPath(vg);
                nvgCircle(vg, grid.tickToScreenD(dataFirst.time), sizeInset.y * (1.0f - dataFirst.val), 5);
                nvgFillColor(vg, theme->getColor(GuiColor::COL_NOTE_PLAYING));
                nvgFill(vg);
                nvgBeginPath(vg);
                nvgCircle(vg, grid.tickToScreenD(dataLast.time), sizeInset.y * (1.0f - dataLast.val), 5);
                nvgFillColor(vg, theme->getColor(GuiColor::COL_NOTE_PLAYING));
                nvgFill(vg);
            }
            if (bRender) {
                bool first = true;
                auto ptStart = &cachedShape.front();
                auto ptEnd = &cachedShape.back();
                for (auto& s : segments) {
                    if (s.dataOffset >= firstPtIdx && s.dataOffset < lastPtIdx) {
                        for (auto ptIdx : s.points) {
                            auto pt = getPathPointSafe(ptIdx);
                            if (pt) {
                                if (first) {
                                    nvgBeginPath(vg);
                                    nvgMoveTo(vg, pt->x, pt->y);
                                    first = false;
                                    ptStart = pt;
                                } else {
                                    nvgLineTo(vg, pt->x, pt->y);
                                    ptEnd = pt;
                                }
                            }
                        }
                    }
                }
                // for (auto it = segment->points.begin() + 1; it != segment->points.end(); ++it) {
                // for (int32_t idx = firstPtIdx+2; idx <= lastPtIdx; ++idx) {
                //     auto pt2 = getPathPointSafe(idx);
                //     nvgLineTo(vg, pt2->x, pt2->y);
                // }
                if (!first) {
                    nvgStrokeColor(vg, theme->getColor(colorHL));
                    nvgStrokeWidth(vg, 4.0f);
                    nvgStroke(vg);
                }
                nvgBeginPath(vg);
                if (ptStart->x > -4 && ptStart->x < sizeInset.x + 4.0f) {
                    nvgCircle(vg, ptStart->x, ptStart->y, radiusHandleHL);
                }
                if (ptEnd->x > -4 && ptEnd->x < sizeInset.x + 4.0f) {
                    nvgCircle(vg, ptEnd->x, ptEnd->y, radiusHandleHL);
                }
                nvgFillColor(vg, theme->getColor(colorHL));
                nvgFill(vg);
                nvgStrokeColor(vg, theme->getColor(colorHL2));
                nvgStrokeWidth(vg, 1.5f);
                nvgStroke(vg);
                if (bIntersect) {
                    nvgRestore(vg);
                }
            }
        }
        vec2* pt = getPathPointSafe(currentDragged.segidx);
        if (currentDragged.mode == dragmode::drag_node && pt) {
            nvgBeginPath(vg);
            nvgCircle(vg, pt->x, pt->y, radiusHandleHL);
            nvgFillColor(vg, theme->getColor(colorHL));
            nvgFill(vg);
            nvgStrokeColor(vg, theme->getColor(colorHL2));
            nvgStrokeWidth(vg, 1.5f);
            nvgStroke(vg);
        }
    }
    if (!isActive()) {// no data points
        float fDstVal = getDstVal();
        ivec2 cs      = getSizeContent();
        float dstValY = dataToCtr(fDstVal, cs.y);
        nvgBeginPath(vg);
        nvgMoveTo(vg, -4, dstValY);
        nvgLineTo(vg, cs.x + 4, dstValY);
        if (currentDragged.mode == dragmode::drag_empty) {
            nvgStrokeColor(vg, theme->getColor(colorHL));
            nvgStrokeWidth(vg, lineWidth + 0.5f);
        } else {
            nvgStrokeColor(vg, theme->getColor(color));
            nvgStrokeWidth(vg, lineWidth);
        }
        RenderResources::NvgImageTexture& image = RenderResources::imgDashedLine;
        uint32_t texOffsetX = image.width - (grid.getOffset() % image.width);
        NVGpaint paintDown  = nvgImagePattern(vg, texOffsetX, 0, image.width, image.height, (float) (M_PI * 0.5f), image.perContextId[vg], 0.6f);
        nvgStrokePaint(vg, paintDown);
        nvgStroke(vg);
    }
    if (mouseTick != INVALID_TICK) {
        if (currentDragged.mode == dragmode::drag_segment && bIsDragging) {
            mouseTick = data.points[currentDragged.dataPt].time;
            valAtMouse = data.points[currentDragged.dataPt].val;
        }
        if (1) {
            float xTick = grid.tickToScreenD(mouseTick);
            nvgBeginPath(vg);
            nvgCircle(vg, xTick, sizeInset.y * (1.0f - valAtMouse), radiusHandleHL);
            nvgFillColor(vg, theme->getColor(GuiColor::COL_GUI_HANDLE_FOCUSED));
            nvgFill(vg);
            nvgStrokeColor(vg, theme->getColor(GuiColor::COL_GUI_HANDLE));
            nvgStrokeWidth(vg, 1.5f);
            nvgStroke(vg);
        }
        String strRender;
        auto rowHeight = theme->get(GuiConstant::CONST_ROW_HEIGHT);
        auto fontSizeScaled = rowHeight * FONT_AUTOSCALE;
        auto posText = vec2(fmouse.x + 20, INSET_TITLE);
        if (at) {
            auto display = at->convertParamValueToDisplay(paramIdx, valAtMouse);
            strRender = display.value+ " " + display.unit;
            renderTextLabel(vg,
                            posText,
                            vec2(size),
                            strRender,
                            theme,
                            fontSizeScaled,
                            theme->getColor(getLabelColor()),
                            NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
            posText.y += rowHeight;
        } 
        {
            strRender = StringFormat("%.2f %d", valAtMouse, mouseTick);
            renderTextLabel(vg,
                            posText,
                            vec2(size),
                            strRender,
                            theme,
                            fontSizeScaled,
                            theme->getColor(getLabelColor()),
                            NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
        }
    }
}

#include "trackautomation.h"

#include "seq_math.h"
#include "color_util.h"
#include "track.h"
#include "clip.h"
#include "grid.h"
#include "guicontainer.h"
#include "trackctr.h"
#include "automation.h"
#include "track_audiodata.h"

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include "leak_detect.h"
using glm::vec2;
using glm::ivec2;


float dist_to_segment(vec2 a, vec2 b, vec2 pt)
{
	vec2 v = b - a;
	float lenSq = glm::dot(v, v);
	if (lenSq < 1E-4F) {
		return glm::distance(pt, a);
	}
	float t = std::max(0.0f, std::min(1.0f, glm::dot(pt - a, v) / lenSq));
	const vec2 p = a + t * v;
	return glm::distance(pt, p);
}
float dataToCtr(float x, float ctrHeight) {
	return (1.0f-x)*ctrHeight;
}
float ctrToData(float screenX, float ctrHeight) {
	return 1.0f-(screenX/ctrHeight);
}
float ctrToDataScale(float screenX, float ctrHeight) {
	return (screenX/ctrHeight);
}

void copyATMNSegment(vstparam_automation_t& in, automation_clipboard_t& out, int32_t srcPos, int32_t len) {

}
using hit_result = gui_track_automation::hit_result;
hit_result gui_track_automation::hitTest(vec2 mpos) {
	if (data.points.empty()) {
		float fDstVal = data.getDstValue();
		ivec2 cs = getSizeContent();
		float dstValY = dataToCtr(fDstVal, cs.y);
		float dist = abs(mpos.y - dstValY);
		if (dist < 10) {
			return {drag_empty, -1, dist};
		}
		return {drag_none, -1, 0};
	}
	std::vector<hit_result> hit;
	for (int i = 0; i < segments.size(); i++) {
		path_segment_t& segment = segments[i];
		if (mpos.x >= segment.pt1.x - 5 && mpos.x < segment.pt2.x + 5) {
			float dist = dist_to_segment(segment.pt1, segment.pt2, mpos);
			float distA = glm::distance(segment.pt1, mpos);
			float distB = glm::distance(segment.pt2, mpos);
			hit.push_back( { dragmode::drag_segment, i, dist });
			if (i + segmentDataPtOffset >= 0)
				hit.push_back( { dragmode::drag_node, i, distA - 4 });
			if (i + segmentDataPtOffset + 1 < data.points.size())
				hit.push_back( { dragmode::drag_node, i + 1, distB - 4 });
		}
	}
	std::sort(hit.begin(), hit.end(), [](hit_result const & a, hit_result const & b) {
		return a.dist<b.dist;
	});
	hit_result& h = hit[0];
	if (h.dist < 10) {
		return h;
	}
	return {drag_none, -1, 0};
}
	void gui_track_automation::trackViewDragBegin(guitrack_editor* view, MouseEvent& evt) {
		dragged = {dragmode::drag_none, -1, 0};
		canSimplify = true;
		ivec2 trackEditorLocal = evt.relMousepos;
		ivec2 local = toContainerSpace(trackEditorLocal);
		scaled_grid& grid = view->grid;
		tick_t tickAt = grid.screenToTick(trackEditorLocal.x);
		Cursor& cursor = view->cursor;
		dragged = hitTest(local);
		if (dragged.mode != dragmode::drag_node && cursor.contains(this->m_track->idx, tickAt)) {
			addPointAt(data.points, cursor.getTickBegin());
			int32_t idx = addPointAt(data.points, cursor.getTickBegin());
			int32_t idx2 = addPointAt(data.points, cursor.getTickEnd());
			addPointAt(data.points, cursor.getTickEnd());
			updateVisibleTrackContents(view->grid);
			dragged = hitTest(local);
			dragged.mode = dragmode::drag_selection;
			dragged.idx = idx - segmentDataPtOffset;
			dragged.numPoints = idx2 - idx + 1;
		}
		dataPointsCopy = data.points;
		dataPointsEdited = dataPointsCopy;

//		automation_clipboard_t clipboard;
//		copyATMNSegment(data, clipboard, cursor.cursorPos, cursor.selRange);
	}
	void gui_track_automation::trackViewDragMove(guitrack_editor* view, MouseEvent& evt) {
//		ivec2 trackEditorLocal = evt.relMousepos;
		ivec2 cs = getSizeContent();
		scaled_grid& grid = view->grid;
		int32_t disty = -evt.dragDistance->y;
		int32_t distx = evt.dragDistance->x;
		if (grid.pixelsToTicks(abs(distx)) < grid.getTickLength()/32) {
			distx = 0;
		} else {
			evt.dragDistance->x = 0;
		}
		evt.dragDistance->y = 0;
		float valOffset = ctrToDataScale(disty, cs.y);
		vec2* pathPtCur;
		if (dragged.mode == dragmode::drag_empty) {
			evt.dragDistance->y = 0;
			float fDstVal = data.getDstValue();
			fDstVal = min(1.0f, max(0.0f, fDstVal + valOffset));
			data.setDstValue(fDstVal);
		} else if (dragged.mode && (pathPtCur = getPathPointSafe(dragged.idx))) {
			std::vector<automation_point_t>& dataPoints = dataPointsEdited;
			std::vector<automation_point_t>& pointsClamped = data.points;
			bool firstSegment = dragged.idx + segmentDataPtOffset < 0;
			int32_t dataPtIdx1;
			tick_t tickOffset = grid.pixelsToTicks2(distx);
			int32_t numPoints = 1;
			if (firstSegment) {
				dataPtIdx1 = 0;
			} else {
				dataPtIdx1 = dragged.idx + segmentDataPtOffset;
				if (dragged.mode == dragmode::drag_selection) {
					numPoints = dragged.numPoints;
				}
				if (dragged.mode == dragmode::drag_segment) {
					numPoints = 2;
				}
			}


			int32_t dataPtIdx2 = dataPtIdx1+numPoints-1;
			bool anyNonSaturatedY = false;
			for (int i = dataPtIdx1; i <= dataPtIdx2; i++) {
				if (i >= 0 && i < dataPoints.size()) {
					automation_point_t& pt = dataPoints[i];
					anyNonSaturatedY |= (disty < 0 ? pt.val > 0.0f : pt.val < 1.0f);
				}
			}
			automation_point_t zero = {0, 0};
			automation_point_t* minPt = NULL;
			automation_point_t* maxPt = NULL;
			if (!firstSegment && dataPtIdx1 > 0 && dataPtIdx1 < pointsClamped.size()) {
				minPt = &pointsClamped[dataPtIdx1-1];
			} else if (dataPtIdx1 >= 0) {
				minPt = &zero;
			}
			if (dataPtIdx2 >= 0 && dataPtIdx2+1 < pointsClamped.size()) {
				maxPt = &pointsClamped[dataPtIdx2+1];
			}
			if (distx < 0 && minPt) {
				if (dataPtIdx1 >= 0 && dataPtIdx1 < dataPoints.size()) {
					automation_point_t& ptEd = dataPoints[dataPtIdx1];
					tickOffset = max(minPt->time - ptEd.time, tickOffset);
				}
			}
			if (distx > 0 && maxPt) {
				if (dataPtIdx2 >= 0 && dataPtIdx2 < dataPoints.size()) {
					automation_point_t& ptEd = dataPoints[dataPtIdx2];
					tickOffset = min(maxPt->time - ptEd.time, tickOffset);
				}
			}
			for (int i = dataPtIdx1; i <= dataPtIdx2; i++) {
				if (i >= 0 && i < dataPoints.size()) {
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
				if (i >= 0 && i < dataPoints.size()) {
					automation_point_t& src = dataPoints[i];
					automation_point_t& dst = pointsClamped[i];
					tick_t newTick = src.time;
					if (minPt) {
						newTick = max(newTick, minPt->time);
					}
					if (maxPt) {
						newTick = min(newTick, maxPt->time);
					}
					assert(tickOffset || newTick == dst.time);
					dst.time = newTick;
					dst.val = min(1.0f, max(0.0f, src.val));
				}
			}
			postEdit();
		}

	}
	void gui_track_automation::trackViewDragRelease(guitrack_editor* view, MouseEvent& evt) {
		dragged = {dragmode::drag_none, -1, 0};
		if (canSimplify)
			simplifyData(data.points);
		postEdit();
	}
	void gui_track_automation::postEdit() {
		updateVisibleTrackContents(grid);
		automatable_t* automatable = this->at;
		automation_t* automation = NULL;
		if (automatable) {
			automation = automatable->getAutomation(param);
		}
		if (automation) {
			automation->points = data.points;
		}
	}
	bool gui_track_automation::trackViewDoubleClick(guitrack_editor* view, MouseEvent& evt) {
		dragged = {dragmode::drag_none, -1, 0};
		ivec2 trackEditorLocal = evt.relMousepos;
		ivec2 local = toContainerSpace(trackEditorLocal);
		hit_result clicked = hitTest(local);
		canSimplify = false;
		std::vector<automation_point_t>& dataPoints = data.points;
		if (clicked.mode == dragmode::drag_node) {
			int32_t i = clicked.idx + segmentDataPtOffset;
			assert(i >= 0 && i < dataPoints.size());
			dataPoints.erase(dataPoints.begin()+i);
			postEdit();
			return true;
		} else {
			 // this is true until we relayout UI and move mixers left or something
			assert(trackEditorLocal.x == local.x);

			ivec2 cs = getSizeContent();
			scaled_grid& grid = view->grid;
			tick_t tick = grid.screenToTickSnap(trackEditorLocal.x, SNAP_OFF);
			float val = min(1.0f, max(0.0f, ctrToData(local.y, cs.y)));
			int32_t idx = indexOfTick(dataPoints, tick);
			assert(idx >= 0 && idx <= dataPoints.size());
			automation_point_t pt{tick, val};
			dataPoints.insert(dataPoints.begin()+idx, pt);
			postEdit();
			return true;
		}
	}

	bool gui_track_automation::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
		if (this->contains(mpos)) {
			ivec2 localMouse = this->toContainerSpace(mpos);
			for (guibase* gui : guis) {
				if (gui->mouseHitTest(localMouse, evt)) {
					return true;
				}
			}
			if (evt.type == MouseHitType::MOUSE_RIGHT) { // righclick in selection (create clip etc.)
				MainCtrl* ctrl = MainCtrl::get();
				scaled_grid& grid = ctrl->getGrid();
				tick_t tick = grid.screenToTickSnap(mpos.x, SNAP_OFF);
				if (ctrl->cursor.contains(this->m_track->idx, tick)) {
					evt.requestFocus(this);
					return true;
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
//			return true;
		}
		return false;
	}

	void gui_track_automation::updateVisibleTrackContents(scaled_grid& grid) {
//		if (data.points.empty()) {
//			data.points.push_back({TICKS_BAR, 0.3f});
//			data.points.push_back({TICKS_BAR*4, 0.7f});
//			data.points.push_back({TICKS_BAR*9, 0.1f});
//			data.points.push_back({TICKS_BAR*16, 0.5f});
//			data.points.push_back({TICKS_BAR*22, 0.2f});
//		}
		ivec2 cs = getSizeContent();
		std::vector<automation_point_t>& dataPoints = data.points;
		cachedShape.clear();
		segments.clear();
		if (!dataPoints.empty()) {
			automation_point_t& firstData = dataPoints[0];
			segmentDataPtOffset = 0;
			float firstX = (float)grid.tickToScreenD(firstData.time);
			float firstY = dataToCtr(firstData.val, cs.y);
			if (firstX > -4.0f) { // > left start of viewable area
				cachedShape.push_back({-4.0f, firstY});
				segmentDataPtOffset = -1;
			}
			cachedShape.push_back({firstX, firstY});
			for (int i = 1; i < dataPoints.size(); i++) {
				automation_point_t& nextPt = dataPoints[i];
				float ptX = (float)grid.tickToScreenD(nextPt.time);
				float ptY = dataToCtr(nextPt.val, cs.y);
				cachedShape.push_back({ptX, ptY});
			}
			vec2& last = cachedShape.back();
			if (last.x < cs.x) { // < right end of viewable area
				cachedShape.push_back({cs.x+4.0f, last.y});
			}


			for (int i = 0; i < cachedShape.size()-1; i++) {
				segments.push_back({cachedShape[i], cachedShape[i+1], i});
			}
		}
	}

	void gui_track_automation::render(NVGcontext* vg) {

//		if (MainCtrl::get()->getSelectedTrack() == m_track) {
//			nvgBeginPath(vg);
//			nvgRect(vg, pos.x, pos.y, size.x, size.y);
//			nvgFillColor(vg, g_guiColors[COL_BG_SELECTEDTRACK]);
//			nvgFill(vg);
//		}
		ivec2 sizeInset = getSizeContent();
		if (sizeInset.y <= 0 || sizeInset.x <= 0) {
			return;
		}
		ivec2 posInset = getPosContent();
		nvgIntersectScissor(vg, pos.x, pos.y, size.x, size.y);
		nvgTranslate(vg, posInset.x, posInset.y);
	//	nvgBeginPath(vg);
	//	nvgMoveTo(vg, -4, size.y/2);
	//	nvgLineTo(vg, size.x+4, size.y);
	//	nvgStrokeColor(vg, rgbToNvg(0xEFDF62));
	//	nvgStrokeWidth(vg, 3.0f);
	//	nvgStroke(vg);
		ivec2 imouse = toControlsObjectSpace(MainCtrl::get()->m_mousePos, this);
		bool mouseIn = MainCtrl::get()->guiOver == this && contains(imouse+getPosContent());
		tick_t mouseTick = !mouseIn ? INVALID_TICK : MainCtrl::get()->getGrid().screenToTickSnap(imouse.x, SNAP_OFF);
		vec2 fmouse = vec2(imouse);
		hit_result currentDragged = dragged.mode || !mouseIn ? dragged : hitTest(fmouse);
		if (currentDragged.mode == dragmode::drag_node) {
			int32_t ptIdx = currentDragged.idx+segmentDataPtOffset;
			assert(ptIdx >= 0 && ptIdx < data.points.size());
			automation_point_t& pt = data.points[ptIdx];
			vec2* point = getPathPointSafe(currentDragged.idx);
			mouseTick = pt.time;
			fmouse.x = point->x;
		}

		float val = 0;
		if (!cachedShape.empty()) {


			nvgBeginPath(vg);
			vec2& first = cachedShape[0];
			nvgMoveTo(vg, first.x, first.y);
			int len = cachedShape.size();
			for (int i = 1; i < len; i++) {
				vec2& pt = cachedShape[i];
				nvgLineTo(vg, pt.x, pt.y);
			}
			nvgStrokeColor(vg, color);
			nvgStrokeWidth(vg, lineWidth);
			nvgStroke(vg);

			nvgBeginPath(vg);
			for (int i = 1+segmentDataPtOffset; i < len; i++) {
				vec2& pt = cachedShape[i];
				if (currentDragged.mode == dragmode::drag_segment) {
					if (currentDragged.idx == i || currentDragged.idx+1 == i) {
						continue;
					}
				}
				if (currentDragged.mode == dragmode::drag_node) {
					if (currentDragged.idx == i) {
						continue;
					}
				}
				nvgCircle(vg, pt.x, pt.y, radiusHandle);
			}
			nvgFillColor(vg, color);
			nvgFill(vg);
			nvgStrokeColor(vg, color2);
			nvgStrokeWidth(vg, 1.5f);
			nvgStroke(vg);

			path_segment_t* segment = getSegmentSafe(currentDragged.idx);
			if (currentDragged.mode == dragmode::drag_segment && segment) {
				val = data.getValueAt(0);
				nvgBeginPath(vg);
				nvgMoveTo(vg, segment->pt1.x, segment->pt1.y);
				nvgLineTo(vg, segment->pt2.x, segment->pt2.y);
				nvgStrokeColor(vg, colorHL);
				nvgStrokeWidth(vg, lineWidth+0.5f);
				nvgStroke(vg);

				nvgBeginPath(vg);
				nvgCircle(vg, segment->pt1.x, segment->pt1.y, radiusHandleHL);
				nvgCircle(vg, segment->pt2.x, segment->pt2.y, radiusHandleHL);
				nvgFillColor(vg, colorHL);
				nvgFill(vg);
				nvgStrokeColor(vg, colorHL2);
				nvgStrokeWidth(vg, 1.5f);
				nvgStroke(vg);
			}
			vec2* pt = getPathPointSafe(currentDragged.idx);
			if (currentDragged.mode == dragmode::drag_node && pt) {
				nvgBeginPath(vg);
				nvgCircle(vg, pt->x, pt->y, radiusHandleHL);
				nvgFillColor(vg, colorHL);
				nvgFill(vg);
				nvgStrokeColor(vg, colorHL2);
				nvgStrokeWidth(vg, 1.5f);
				nvgStroke(vg);
			}
		} else { // no data points
			float fDstVal = data.getDstValue();
			ivec2 cs = getSizeContent();
			float dstValY = dataToCtr(fDstVal, cs.y);
			nvgBeginPath(vg);
			nvgMoveTo(vg, -4, dstValY);
			nvgLineTo(vg, cs.x+4, dstValY);
			if (currentDragged.mode == dragmode::drag_empty) {
				nvgStrokeColor(vg, colorHL);
				nvgStrokeWidth(vg, lineWidth+0.5f);
			} else {
				nvgStrokeColor(vg, color);
				nvgStrokeWidth(vg, lineWidth);
			}
			nvgStroke(vg);
		}
		if (mouseTick != INVALID_TICK) {
			float val = data.getValueAt(mouseTick);
			setFont(vg, 18, G_WHITE, NVG_ALIGN_TOP|NVG_ALIGN_LEFT);
			nvgText(vg, fmouse.x, INSET_TITLE, StringAsCStr(StringFormat("%.2f %d", val, mouseTick)), NULL);
		}
	}

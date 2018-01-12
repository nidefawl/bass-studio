#include "trackautomation.h"

#include "gui.h"
#include "cursor.h"
#include "event.h"
#include "seq_math.h"
#include "color_util.h"
#include "track.h"
#include "clip.h"
#include "grid.h"
#include "guicontainer.h"
#include "trackctr.h"
#include "mainctrl.h"
#include "automation.h"
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include "track.h"
#include "track_impl.h"
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

float gui_track_automation::getDstVal() {
	if (at) {
		return at->getParamValue(param);
	}
	return 0.5f;
}
void gui_track_automation::setDstVal(float f) {
	if (at) {
		at->setParamValue(param, f);
	}
}
using hit_result = gui_track_automation::hit_result;
hit_result gui_track_automation::hitTest(vec2 mpos) {
	if (data.points.empty()) {
		float fDstVal = getDstVal();
		ivec2 cs = getSizeContent();
		float dstValY = dataToCtr(fDstVal, cs.y);
		float dist = abs(mpos.y - dstValY);
		if (dist < 10) {
			return {drag_empty, -1, -1, dist};
		}
		return {drag_none, -1, -1, 0};
	}
	std::vector<hit_result> hit;
	for (int i = 0; i < segments.size(); i++) {
		path_segment_t& segment = segments[i];
		auto segBegin = segment.points.begin();
		auto segEnd = segment.points.end();
		float minSegLineDist = 16;
		for (auto it = segBegin; it != segEnd && (it+1) != segEnd; ++it) {
			int32_t idxSegStart = *it;
			int32_t idxSegEnd = *(it+1);
			vec2& ptStart = cachedShape[idxSegStart];
			vec2& ptEnd = cachedShape[idxSegEnd];
			float dist = dist_to_segment(ptStart, ptEnd, mpos);
			minSegLineDist = min(dist, minSegLineDist);
		}
		if (minSegLineDist < 10) {
			hit_result hitSeg{ dragmode::drag_segment, segment.dataOffset, i, minSegLineDist };
			hit.push_back(std::move(hitSeg));

			int32_t ptIdx = segment.points.front();
			vec2& ptStart = cachedShape[ptIdx];
			float distPtEndSeg = glm::distance(ptStart, mpos);
			if (distPtEndSeg < 10) {

				hit_result hitSeg { dragmode::drag_node, segment.dataOffset, ptIdx, distPtEndSeg - 4 };
				hit.push_back(std::move(hitSeg));
			}
//			if (dataPtIdx + 1 < data.points.size()) {
//				float distB = glm::distance(ptEnd, mpos);
//				hit.push_back( { dragmode::drag_node, dataPtIdx + 1, i+segOffset, distB - 4 });
//			}
		}
	}
	std::sort(hit.begin(), hit.end(), [](hit_result const & a, hit_result const & b) {
		return a.dist<b.dist;
	});
	if (hit.size()) {

		hit_result& h = hit[0];
		if (h.dist < 10) {
			return h;
		}
	}
	return {drag_none, -1, -1, 0};
}
	void gui_track_automation::trackViewDragBegin(guitrack_editor* view, MouseEvent& evt) {
		dragged = hit_result(dragmode::drag_none, -1, -1, 0.0f);
		canSimplify = true;
		ivec2 trackEditorLocal = evt.relMousepos;
		ivec2 local = toContainerSpace(trackEditorLocal);
		scaled_grid& grid = view->grid;
		tick_t tickAt = grid.screenToTick(trackEditorLocal.x);
		Cursor& cursor = view->cursor;
		dragged = hitTest(local);
		if (dragged.mode != dragmode::drag_node && cursor.containsSubtrack(this->m_track->idx, this->idx, tickAt)) {
			int32_t steps = at->getQuantizationSteps(param);
			addPointAt(data.points, cursor.getTickBegin(), steps);
			int32_t idx = addPointAt(data.points, cursor.getTickBegin(), steps);
			int32_t idx2 = addPointAt(data.points, cursor.getTickEnd(), steps);
			addPointAt(data.points, cursor.getTickEnd(), steps);
//			updateVisibleTrackContents(view->grid);
			MainCtrl::getGuiTrackCtr()->updateVisibleTrackContents();
			dragged = hitTest(local);
			dragged.mode = dragmode::drag_selection;
			dragged.dataPt = idx;
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
		float valOffset = ctrToDataScale(disty, cs.y);
		if (dragged.mode == dragmode::drag_empty) {
			evt.dragDistance->y = 0;
			float fOld = getDstVal();
			float fDstVal = fOld + valOffset;
			if (at) {
				float fNew = at->quantizeVal(param, fDstVal);
				if (fNew == fOld) {
					return;
				}
				fDstVal = fNew;
			}
			evt.dragDistance->y = 0;
			fDstVal = min(1.0f, max(0.0f, fDstVal));
			setDstVal(fDstVal);
		} else if (dragged.mode) {
			evt.dragDistance->y = 0;
			std::vector<automation_point_t>& dataPoints = dataPointsEdited;
			std::vector<automation_point_t>& pointsClamped = data.points;
			bool firstSegment = dragged.dataPt < 0;
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
					float f = min(1.0f, max(0.0f, src.val));
					if (at) {
						f = at->quantizeVal(param, f);
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
		automatable_t* automatable = this->at;
		automation_t* automation = NULL;
		if (automatable) {
			automation = automatable->getAutomation(param);
		}
		if (automation) {
			bool activate = automation->points.empty() && !data.points.empty();
			automation->points = data.points;
			if (activate)
				automation->active = true;
		}
//		updateVisibleTrackContents(grid);
		MainCtrl::getGuiTrackCtr()->updateVisibleTrackContents();
	}
	bool gui_track_automation::trackViewDoubleClick(guitrack_editor* view, MouseEvent& evt) {
		dragged = hit_result(dragmode::drag_none, -1, -1, 0.0f);
		ivec2 trackEditorLocal = evt.relMousepos;
		ivec2 local = toContainerSpace(trackEditorLocal);
		hit_result clicked = hitTest(local);
		canSimplify = false;
		std::vector<automation_point_t>& dataPoints = data.points;
		if (clicked.mode == dragmode::drag_node) {
			int32_t i = clicked.dataPt;
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
			float val = ctrToData(local.y, cs.y);
			if (at) {
				val = at->quantizeVal(param, val);
			}
			val = min(1.0f, max(0.0f, val));
			int32_t idx = indexOfTick(dataPoints, tick);
			assert(idx >= 0 && idx <= dataPoints.size());
			automation_point_t pt{tick, val};
			dataPoints.insert(dataPoints.begin()+idx, pt);
			postEdit();
			return true;
		}
	}

	bool gui_track_automation::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
		if (!this->at || this->param < 0) {
			return false;
		}
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
				if (ctrl->cursor.containsSubtrack(this->m_track->idx, this->idx, tick)) {
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
		int32_t segmentDataPtOffset = 0;
		if (!dataPoints.empty()) {
			cachedShape.reserve(dataPoints.size()+4);
			segments.reserve(dataPoints.size()+4);
			automation_point_t& firstData = dataPoints[0];
			segmentDataPtOffset = 0;
			float firstX = (float)grid.tickToScreenD(firstData.time);
			float val = firstData.val;
			int steps = at->getQuantizationSteps(param);
			float firstY = dataToCtr(val, cs.y);
//			if (firstX > -4.0f)
			{ // > left start of viewable area
				cachedShape.push_back({-4.0f, firstY});
				path_segment_t seg;
				seg.dataOffset = -1;
				seg.points.push_back(0);
				seg.points.push_back(1);
				segments.push_back(std::move(seg));
				segmentDataPtOffset = -1;
			}
			float lastVal = dataPoints[0].val;
			cachedShape.push_back({firstX, firstY});
			for (int i = 1; i < dataPoints.size(); i++) {
				path_segment_t seg;
				seg.dataOffset = i + segmentDataPtOffset;
				seg.points.push_back(cachedShape.size()-1);
				seg.points.push_back(cachedShape.size());
				automation_point_t& nextPt = dataPoints[i];
				float ptX = (float)grid.tickToScreenD(nextPt.time);
				float val = nextPt.val;
				if (steps && val != lastVal) {
					vec2 prev = cachedShape.back();
					prev.x = ptX;
					cachedShape.push_back(std::move(prev));
					seg.points.push_back(cachedShape.size());
				}
				float ptY = dataToCtr(val, cs.y);
				cachedShape.push_back({ptX, ptY});
				segments.push_back(std::move(seg));
				lastVal = val;
			}
			vec2& last = cachedShape.back();
			if (last.x < cs.x) { // < right end of viewable area
				path_segment_t seg;
				seg.dataOffset = dataPoints.size() + segmentDataPtOffset;
				seg.points.push_back(cachedShape.size()-1);
				seg.points.push_back(cachedShape.size());
				cachedShape.push_back({cs.x+4.0f, last.y});
				segments.push_back(std::move(seg));
			}

//			segments.reserve(cachedShape.size()-1);
//			for (int i = 0; i < cachedShape.size()-1; i++) {
//				path_segment_t seg{cachedShape[i], cachedShape[i+1], i};
////				segments.push_back(std::move(seg));
//			}
		}
	}

	void gui_track_automation::render(NVGcontext* vg) {
		if (!this->at || this->param < 0) {
			return;
		}
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
			int32_t ptIdx = currentDragged.dataPt;
			assert(ptIdx >= 0 && ptIdx < data.points.size());
			automation_point_t& pt = data.points[ptIdx];
			vec2* point = getPathPointSafe(currentDragged.segidx);
			mouseTick = pt.time;
			fmouse.x = point->x;
		}

		float val = 0;
		if (!segments.empty()) {


			int len = segments.size();
			int i = 0;
			int skipped = 0;
			for (; i < len; i++) {
				path_segment_t& segment = segments[i];
				vec2* pt2 = getPathPointSafe(segment.points.back());
				if (pt2->x > -4) {
					break;
				}
			}
			int start = i;
			skipped+=i;
			if (i < len) {
				NVGcolor c1 = isActive() ? this->color : this->colorInactive;
//				nvgLineJoin(vg, NVGlineCap::NVG_BEVEL);
				nvgBeginPath(vg);
				bool first = true;
				for (; i < len; i++) {
					path_segment_t& segment = segments[i];
					for (auto it = segment.points.begin(); it != segment.points.end(); ++it) {
						vec2* pt2 = getPathPointSafe(*it);
						if (first) {
							nvgMoveTo(vg, pt2->x, pt2->y);
							first = false;
						} else {
							nvgLineTo(vg, pt2->x, pt2->y);
							if (pt2->x > sizeInset.x+4) break;
						}
					}

				}
				int end = i;
				nvgStrokeColor(vg, c1);
				nvgStrokeWidth(vg, lineWidth);
				nvgStroke(vg);
//				nvgLineJoin(vg, NVGlineCap::NVG_MITER);

				//Lots of room for optimization here (draw texture for dot, or use custom shader)
				nvgShapeAntiAlias(vg, 0);
				nvgBeginPath(vg);
				for (int i = start; i < min(len-1, end); i++) {
					path_segment_t& segment = segments[i];
					if (currentDragged.mode == dragmode::drag_segment && currentDragged.segidx == i) {
						continue;
					}
					int32_t idxPtEnd = segment.points.back();
					if (currentDragged.mode == dragmode::drag_node) {
//						if (currentDragged.segidx >= segment.points.front() && currentDragged.segidx <= segment.points.back()) {
//							continue;
//						}
						if (currentDragged.segidx == idxPtEnd) {
							continue;
						}
					}
					vec2* pt = getPathPointSafe(idxPtEnd);
					nvgCircleFastNDivs(vg, pt->x, pt->y, radiusHandle, 6);

				}
				nvgFillColor(vg, c1);
				nvgFill(vg);
				nvgStrokeColor(vg, color2);
				nvgStrokeWidth(vg, 1.5f);
				nvgStroke(vg);
				nvgShapeAntiAlias(vg, 1);
			}

			path_segment_t* segment = getSegmentSafe(currentDragged.segidx);
			if (currentDragged.mode == dragmode::drag_segment && segment) {
				val = data.getValueAt(0);
				nvgBeginPath(vg);
				vec2* ptStart = getPathPointSafe(segment->points.front());
				nvgMoveTo(vg, ptStart->x, ptStart->y);
				for (auto it = segment->points.begin()+1; it != segment->points.end(); ++it) {
					vec2* pt2 = getPathPointSafe(*it);
					nvgLineTo(vg, pt2->x, pt2->y);

				}
				vec2* ptEnd = getPathPointSafe(segment->points.back());
				nvgStrokeColor(vg, colorHL);
				nvgStrokeWidth(vg, lineWidth+0.5f);
				nvgStroke(vg);

				nvgBeginPath(vg);
				if (ptStart->x > -4 && ptStart->x < sizeInset.x+4.0f) {
					nvgCircle(vg, ptStart->x, ptStart->y, radiusHandleHL);
				}
				if (ptEnd->x > -4 && ptEnd->x < sizeInset.x+4.0f) {
					nvgCircle(vg, ptEnd->x, ptEnd->y, radiusHandleHL);
				}
				nvgFillColor(vg, colorHL);
				nvgFill(vg);
				nvgStrokeColor(vg, colorHL2);
				nvgStrokeWidth(vg, 1.5f);
				nvgStroke(vg);
			}
			vec2* pt = getPathPointSafe(currentDragged.segidx);
			if (currentDragged.mode == dragmode::drag_node && pt) {
				nvgBeginPath(vg);
				nvgCircle(vg, pt->x, pt->y, radiusHandleHL);
				nvgFillColor(vg, colorHL);
				nvgFill(vg);
				nvgStrokeColor(vg, colorHL2);
				nvgStrokeWidth(vg, 1.5f);
				nvgStroke(vg);
			}
		}
		if (!isActive()) { // no data points
			float fDstVal = getDstVal();
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
			RenderResources::NvgImageTexture& image = RenderResources::imgDashedLine;
			uint32_t texOffsetX = image.width - (grid.getOffset()%image.width);
			NVGpaint paintDown = nvgImagePattern(vg, texOffsetX, 0, image.width, image.height, M_PI*0.5f, image.id, 0.6f);
			nvgStrokePaint(vg, paintDown);
			nvgStroke(vg);
		}
		if (mouseTick != INVALID_TICK) {
			float val = data.getValueAt(mouseTick);
			setFont(vg, 18, G_WHITE, NVG_ALIGN_TOP|NVG_ALIGN_LEFT);
			nvgText(vg, fmouse.x, INSET_TITLE, StringAsCStr(StringFormat("%.2f %d", val, mouseTick)), NULL);
		}
	}

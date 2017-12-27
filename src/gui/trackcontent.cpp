
#include "track.h"
#include "trackcontent.h"
#include "trackctr.h"
#include "guicontextmenu.h"
#include "event.h"
#include "../host/vst_plugin.h"
#include "leak_detect.h"
#include <glm/geometric.hpp>

float noteToScreen(float note, float scale, float offset, float sizeY) {
	float offsetKey = note * scale;
	float rel = offsetKey - offset;
	return (sizeY) - rel;
}
void gui_clip::handleRightClick(MouseEvent& evt) {
	MainCtrl::get()->openContextMenu(new guictxtmenu_clip(this->m_clip), evt.mousepos);
}
/*static*/ void gui_clip::renderClip(NVGcontext* vg, const track_t* tr, const clip_t* cl, ivec2 pos, ivec2 size) {
	if (cl->len <= 0) {
		return;
	}
	NVGcolor color = rgbToNvg(cl->rgb);
	nvgBeginPath(vg);
	nvgRect(vg, pos.x, pos.y, size.x, HEIGHT_CLIP_TITLE);
	nvgFillColor(vg, color);
	nvgFill(vg);
	nvgStrokeColor(vg, G_BLACK);
	nvgStrokeWidth(vg, 1.f);
	nvgStroke(vg);
	if (cl->name.length()) {
		setFont(vg, (int) (HEIGHT_CLIP_TITLE * 0.95), getContrastFontColor(cl->rgb), G_TITLE_ALIGN);
		renderText(vg, pos.x + INSET_TITLE, pos.y + HEIGHT_CLIP_TITLE / 2, size.x-INSET_TITLE*3, StringAsCStr(cl->name));
	}
	ivec2 posContents = ivec2(pos.x, pos.y+HEIGHT_CLIP_TITLE+INSET_CLIP_CONTENT);
	ivec2 sizeContents = ivec2(size.x, size.y-HEIGHT_CLIP_TITLE-INSET_CLIP_CONTENT*2);

	tick_t clipLen = cl->len;
	float numBars = clipLen / (float) TICKS_BAR;
	float barSize = sizeContents.x / (float) numBars;
	if (sizeContents.x > 0 && sizeContents.y > 0) {
		nvgSave(vg);
		nvgTranslate(vg, posContents.x, posContents.y);
		nvgBeginPath(vg);

		clip_notes_t& notesView = cl->getNoteViewRender();
		clip_notes_t& notesPlay = cl->getNoteViewPlayback();
	//	clip_notes_t notesPlay;
	//	cl->getNotesView(0, cl->len, notesPlay, true);
		for (int i = 0; i < (tr?(tr->idx%2)+1:1); i++) {
			int32_t rgbNote = i == 0 ? 0xff9933 : 0x33ff33;
			int32_t rgbNoteOverlap = i == 0 ? 0x0000ff : 0xff00ff;
			clip_notes_t& notes = i == 0 ? notesView : notesPlay;
			if (!notes.empty()) {
				note_t minN = notesView.minNote;
				note_t maxN = notesView.maxNote;
				int32_t numNotes = max((int32_t)8, maxN.pitch - minN.pitch);
				float scale = sizeContents.y / (float) numNotes;
				std::vector<const note_t*> notesClipped;
				for (const note_t& note : notes.m_list) {
					tick_t noteTime = note.time;
					if (noteTime >= clipLen) {
						notesClipped.push_back(&note);
						continue;
					}
					if (noteTime < 0) {
						notesClipped.push_back(&note);
						continue;
					}
					float objPosNote = noteTime /(float) TICKS_BAR;
		//			assert(objPosNote >= 0 && objPosNote < numBars);
					float objLenNote = note.len /(float) TICKS_BAR;
		//			assert(objPosNote+objLenNote >= 0);
					float ny = noteToScreen(note.pitch-minN.pitch, scale, 0, sizeContents.y);
					float nx = max(0.0f, objPosNote * barSize);
					float nw = min(objLenNote * barSize, sizeContents.x-nx);
					float nh = scale;
					float insetx = calcInset(1, nw);
					float insety = calcInset(1, nh);
					nvgRect(vg, nx+insetx, ny+insety, nw-insetx*2, nh-insety*2);
				}
				nvgFillColor(vg, rgbToNvg(rgbNote));
				nvgFill(vg);
				if (!notesClipped.empty()) {
					nvgBeginPath(vg);
					for (const note_t* noteClipped : notesClipped) {
						const note_t& note = *noteClipped;
						tick_t noteTime = note.time;
			//			assert(objPosNote >= 0 && objPosNote < numBars);
			//			assert(objPosNote+objLenNote >= 0);

						float objPosNote = noteTime /(float) TICKS_BAR;
						float objLenNote = note.len /(float) TICKS_BAR;
						float ny = noteToScreen(note.pitch-minN.pitch, scale, 0, sizeContents.y);
						float nx = objPosNote * barSize;
						float nw = objLenNote * barSize;
						float nh = scale;
						float insetx = calcInset(1, nw);
						float insety = calcInset(1, nh);
						nvgRect(vg, nx+insetx, ny+insety, nw-insetx*2, nh-insety*2);
					}
					nvgFillColor(vg, rgbToNvg(rgbNoteOverlap));
					nvgFill(vg);
				}
			}
		}
		nvgRestore(vg);
	}
	if (cl->loopEnabled) {
		tick_t posLoopIndicator = cl->getLoopBegin();
		nvgBeginPath(vg);
		while (posLoopIndicator < clipLen) {
			if (posLoopIndicator >= 0) {
				float objPos = posLoopIndicator /(float) TICKS_BAR;
				float nx = barSize*objPos;
				nvgMoveTo(vg, pos.x+nx, pos.y);
				nvgLineTo(vg, pos.x+nx, pos.y+HEIGHT_CLIP_TITLE/4);
				nvgMoveTo(vg, pos.x+nx, pos.y+HEIGHT_CLIP_TITLE*3/4);
				nvgLineTo(vg, pos.x+nx, pos.y+HEIGHT_CLIP_TITLE);
			}
			posLoopIndicator += cl->loopLen;
		}
		nvgStrokeColor(vg, GUI_COLOR(G_S2));
		nvgStrokeWidth(vg, 1.f);
		nvgStroke(vg);
	}
}

void gui_track::handleRightClick(MouseEvent& evt) {
	MainCtrl::get()->openContextMenu(new guictxtmenu_trackcontent(this->m_track->idx), evt.mousepos);
}

void gui_clip::trackViewDragBegin(guitrack_editor* view, MouseEvent& evt) {
	view->dragSelectionBegin(this, evt);
}
void gui_clip::trackViewDragMove(guitrack_editor* view, MouseEvent& evt) {
	view->dragSelectionMove(this, evt);
}
void gui_clip::trackViewDragRelease(guitrack_editor* view, MouseEvent& evt) {
	view->dragSelectionRelease(this, evt);
	//!CLIP COULD BE DELETED AT THIS POINT
}

class gui_track_audiochain : public gui_track {
public:
	gui_track_audiochain(track_t* _track) : gui_track(_track) {

	}
};
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
struct automation_clipboard_t {
	tick_t start;
	tick_t len;
	std::vector<automation_point_t> dataPoints;
};
void copyATMNSegment(trackdata_automation_t& in, automation_clipboard_t& out, int32_t srcPos, int32_t len) {

}
class gui_track_automation : public gui_track {
	enum dragmode {
		drag_none = 0,
		drag_segment,
		drag_node,
		drag_selection,
		drag_empty,
	};
	struct path_segment_t {
		vec2 pt1;
		vec2 pt2;
		int32_t idx;
	};
	struct path_segment_dataidx_t {
		int32_t idx1;
		int32_t idx2;
	};
	struct hit_result {
		dragmode mode;
		int idx;
		float dist;
		int32_t numPoints = 0;
	};
	const NVGcolor color = rgbToNvg(0x62EFDF);
	const NVGcolor color2 = mulSatBright(color, 0.6f, 0.8f);
	const NVGcolor colorHL = rgbToNvg(0xEF62DF);
	const NVGcolor colorHL2 = mulSatBright(colorHL, 0.6f, 0.8f);
	const float radiusHandle = 2.5f;
	const float radiusHandleHL = 3.5f;
	const float lineWidth = 2.5f;

	trackdata_automation_t& data;
	std::vector<vec2> cachedShape;
	std::vector<path_segment_t> segments;
//	std::vector<path_segment_dataidx_t> segmentDataIdx;
	std::vector<automation_point_t> dataPointsCopy;
	std::vector<automation_point_t> dataPointsEdited;
	int32_t segmentDataPtOffset = 0;
	hit_result dragged = {drag_none, -1, 0};

	hit_result hitTest(vec2 mpos) {
		if (data.points.empty()) {
			float fDstVal = data.getDstValue();
			ivec2 cs = getSizeContent();
			float dstValY = dataToCtr(fDstVal, cs.y);
			float dist = abs(mpos.y-dstValY);
			if (dist < 10) {
				return {drag_empty, -1, dist};
			}
			return {drag_none, -1, 0};
		}
		std::vector<hit_result> hit;
		for (int i = 0; i < segments.size(); i++) {
			path_segment_t& segment = segments[i];
			if (mpos.x>=segment.pt1.x-5&&mpos.x<segment.pt2.x+5) {
				float dist = dist_to_segment(segment.pt1, segment.pt2, mpos);
				float distA = glm::distance(segment.pt1, mpos);
				float distB = glm::distance(segment.pt2, mpos);
				hit.push_back({dragmode::drag_segment, i, dist});
				hit.push_back({dragmode::drag_node, i, distA-4});
				hit.push_back({dragmode::drag_node, i+1, distB-4});
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
//	path_segment_t* hitTestSegment(ivec2 mouse) {
//		vec2 mpos = vec2(mouse);
//		for (int i = 0; i < segments.size(); i++) {
//			path_segment_t& segment = segments[i];
//			if (mpos.x>=segment.pt1.x&&mpos.x<segment.pt2.x) {
//				float dist = dist_to_segment(segment.pt1, segment.pt2, mpos);
//				return {&segment, dist};
//			}
//		}
//		return NULL;
//	}
public:
	gui_track_automation(track_t* _track) : gui_track(_track), data(_track->getAutomation()) {
		padding = 8;
	}
	ivec2 paddingTL(int _padding) override {
		return ivec2(0, _padding);
	}
	ivec2 paddingBR(int _padding) override {
		return ivec2(0, _padding);
	}
	path_segment_t* getSegmentSafe(int32_t idx) {
		if (idx >= 0 && idx < segments.size()) {
			return &segments[idx];
		}
		return NULL;
	}
	vec2* getPathPointSafe(int32_t idx) {
		if (idx >= 0 && idx < cachedShape.size()) {
			return &cachedShape[idx];
		}
		return NULL;
	}
	void trackViewDragBegin(guitrack_editor* view, MouseEvent& evt) override {
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
	void trackViewDragMove(guitrack_editor* view, MouseEvent& evt) override {
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
			updateVisibleTrackContents(view->grid);
		}

	}
	void trackViewDragRelease(guitrack_editor* view, MouseEvent& evt) override {
		dragged = {dragmode::drag_none, -1, 0};
		simplifyData(data.points);
		updateVisibleTrackContents(view->grid);
	}
	bool trackViewDoubleClick(guitrack_editor* view, MouseEvent& evt) override {
		ivec2 trackEditorLocal = evt.relMousepos;
		ivec2 local = toContainerSpace(trackEditorLocal);
		hit_result clicked = hitTest(local);
		std::vector<automation_point_t>& dataPoints = data.points;
		if (clicked.mode == dragmode::drag_node) {
			int32_t i = clicked.idx + segmentDataPtOffset;
			assert(i >= 0 && i < dataPoints.size());
			dataPoints.erase(dataPoints.begin()+i);
			updateVisibleTrackContents(view->grid);
			return true;
		} else {
			 // this is true until we relayout UI and move mixers left or something
			assert(trackEditorLocal.x == local.x);

			ivec2 cs = getSizeContent();
			scaled_grid& grid = view->grid;
			tick_t tick = grid.screenToTickSnap(trackEditorLocal.x, SNAP_OFF);
			float val = ctrToData(local.y, cs.y);
			int32_t idx = indexOfTick(dataPoints, tick);
			assert(idx >= 0 && idx <= dataPoints.size());
			automation_point_t pt{tick, val};
			dataPoints.insert(dataPoints.begin()+idx, pt);
			updateVisibleTrackContents(view->grid);
			return true;
		}
	}

	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
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
			return true;
		}
		return false;
	}

	void updateVisibleTrackContents(scaled_grid& grid) override {
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
	void layout() override {

	}

	bool handleKeyInput(KeyEvent& kevt) override {
		return parent->handleKeyInput(kevt);
	}
	void render(NVGcontext* vg) override {

		if (MainCtrl::get()->getSelectedTrack() == m_track) {
			nvgBeginPath(vg);
			nvgRect(vg, pos.x, pos.y, size.x, size.y);
			nvgFillColor(vg, g_guiColors[COL_BG_SELECTEDTRACK]);
			nvgFill(vg);
		}
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
				val = data.src.getValueAt(0);
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
			float val = data.src.getValueAt(mouseTick);
			setFont(vg, 18, G_WHITE, NVG_ALIGN_TOP|NVG_ALIGN_LEFT);
			nvgText(vg, fmouse.x, INSET_TITLE, StringAsCStr(StringFormat("%.2f %d", val, mouseTick)), NULL);
		}
	}
};

class gui_track_midi : public gui_track {
public:
	trackdata_midi_t& midi;
	gui_track_midi(track_t* _track)
		: gui_track(_track),
		midi(m_track->getMidi()) {
	}
	void render(NVGcontext* vg) {
		if (MainCtrl::get()->getSelectedTrack() == m_track) {
			nvgBeginPath(vg);
			nvgRect(vg, pos.x, pos.y, size.x, size.y);
			nvgFillColor(vg, g_guiColors[COL_BG_SELECTEDTRACK]);
			nvgFill(vg);
		}
		if (!setScissorTransform(vg)) {
			return;
		}
//		nvgTranslate(vg, pos.x, pos.y);
		for (clip_t* clip : midi.clips) {
			if(!clip->gClip) {
				continue;
			}
			clip->gClip->render(vg);
		}
	}

	void updateVisibleTrackContents(scaled_grid& grid) {
		for (clip_t* clip : midi.clips) {
//			gui_clip* gClip = clip->gClip;
			if(!clip->gClip) {
				clip->gClip = new gui_clip(clip, m_track);
				add(clip->gClip);
			}
			clip->gClip->updatePosition(grid, size);
		}
	}
};

class gui_trackmixer: public gui_track_controls {
public:
	gui_trackmeter meter;
	gui_trackgain gain;
	gui_trackmixer(track_t* _track) :
		gui_track_controls(_track), meter(_track), gain(_track) {
		padding = 0;
		add(&gain);
		add(&meter);
	}
	~gui_trackmixer() {
		remove(&meter);
		remove(&gain);
	}
	void layout() {
		float mW = 20;
		meter.size = ivec2(mW, size.y);
		meter.pos = ivec2(size.x - mW, 0);
		gain.size = ivec2(120, mW);
		gain.pos = ivec2(INSET_TITLE, HEIGHT_TRACK_TITLE + INSET_TITLE);

	}

	void render(NVGcontext* vg) {
		if (!setScissorTransform(vg)) {
			return;
		}
		NVGcolor color = rgbToNvg(m_track->rgb);
		ivec2 titleSize(size.x-meter.size.x, size.y);
		nvgBeginPath(vg);
		nvgRect(vg, 0, 0, size.x, size.y);
		nvgFillColor(vg, g_guiColors[COL_GRID_BRT]);
		nvgFill(vg);
		if (MainCtrl::get()->getSelectedTrack() == m_track) {
			nvgFillColor(vg, g_guiColors[COL_BG_SELECTEDTRACK]);
			nvgFill(vg);
			color = G_BLACK;
		}
		int titleHeight = min(HEIGHT_TRACK_TITLE, size.y);
		nvgBeginPath(vg);
		nvgRect(vg, 0, 0, titleSize.x, titleHeight);
		nvgFillColor(vg, color);
		nvgFill(vg);
		setFont(vg, (int) (titleHeight * 0.8), getContrastFontColorNvg(color), G_TITLE_ALIGN);
		renderText(vg, 0 + INSET_TITLE, 0 + titleHeight / 2, titleSize.x, StringAsCStr(m_track->name));


		//debug stuff
//		size_t nClips = m_track->clips.size();
//		auto it = m_track->clips.begin();
//		size_t nGuiClips = 0;
//		size_t nGuiClips2 = m_track->content->guis.size();
//		while (it != m_track->clips.end()) {
//			clip_t* c = *it;
//			if (c->gClip != NULL)
//				nGuiClips++;
//			it++;
//		}
//		String s = StringFormat("%u (%u/%u) Clips", nClips, nGuiClips, nGuiClips2);
//		setFont(vg, (int) (titleHeight * 0.6), getContrastFontColor(m_track->rgb), G_TITLE_ALIGN);
//		nvgText(vg, 0 + INSET_TITLE, pos.y + titleHeight+titleHeight, StringAsCStr(s), NULL);

		meter.render(vg);
		gain.render(vg);
	}
};


class gui_trackcontrols_automation : public gui_track_controls {
public:
	gui_trackcontrols_automation(track_t* _track) :
		gui_track_controls(_track) {
		padding = 0;
	}
	~gui_trackcontrols_automation() {
	}
	void layout() {
	}

	void render(NVGcontext* vg) {
		if (!setScissorTransform(vg)) {
			return;
		}
		NVGcolor color = rgbToNvg(m_track->rgb);
		ivec2 titleSize(size.x, size.y);
		nvgBeginPath(vg);
		nvgRect(vg, 0, 0, size.x, size.y);
		nvgFillColor(vg, g_guiColors[COL_GRID_BRT]);
		nvgFill(vg);
		MainCtrl* ctrl = MainCtrl::get();
		if (ctrl->getSelectedTrack() == m_track) {
			nvgFillColor(vg, g_guiColors[COL_BG_SELECTEDTRACK]);
			nvgFill(vg);
			color = G_BLACK;
		}
		const int titleHeight = min(HEIGHT_TRACK_TITLE, size.y);
		nvgBeginPath(vg);
		nvgRect(vg, 0, 0, titleSize.x, titleHeight);
		nvgFillColor(vg, color);
		nvgFill(vg);
		setFont(vg, (int) (titleHeight * 0.8), getContrastFontColorNvg(color), G_TITLE_ALIGN);
		renderText(vg, 0 + INSET_TITLE, 0 + titleHeight / 2, titleSize.x, StringAsCStr(m_track->name));

		int32_t y = titleHeight + titleHeight/2;
		String curvalue = "";
		String target = "<NULL>";
		trackdata_automation_t& automation = this->m_track->getAutomation();
		automated_param_t& param = automation.target;
		if (param.paramIdx >= 0 && param.plugin) {
			target = StringFormat("%s %d", StringAsCStr(param.plugin->sName), param.paramIdx);
		}
		curvalue = StringFormat("%f", automation.src.getValueAt(ctrl->cursor.cursorPos));
		//debug
		setFont(vg, (int) (titleHeight * 0.6), G_WHITE, G_TITLE_ALIGN);
		renderText(vg, 0 + INSET_TITLE, y, titleSize.x, StringAsCStr(target));
		y+=titleHeight;
		renderText(vg, 0 + INSET_TITLE, y, titleSize.x, StringAsCStr(curvalue));

	}
};

//t->mixer = new gui_trackmixer(t);
gui_track_controls* createTrackGuiMixer(track_t* t) {
	switch (t->type) {
	case TRACK_TYPE_RETURN:
	case TRACK_TYPE_MASTER:
	case TRACK_TYPE_MIDI:
		return new gui_trackmixer(t);
	case TRACK_TYPE_AUTOMATION:
		return new gui_trackcontrols_automation(t);
	}
	assert(0&&"unhandled track type");
	return NULL;
}
gui_track* createTrackGui(track_t* t) {
	switch (t->type) {
	case TRACK_TYPE_RETURN:
	case TRACK_TYPE_MASTER:
		return new gui_track_audiochain(t);
	case TRACK_TYPE_MIDI:
		return new gui_track_midi(t);
	case TRACK_TYPE_AUTOMATION:
		return new gui_track_automation(t);
	}
	assert(0&&"unhandled track type");
	return NULL;
}
void gui_track_controls::handleRightClick(MouseEvent& evt) {
	MainCtrl::get()->openContextMenu(new guictxtmenu_track(this->m_track->idx), evt.mousepos);
}

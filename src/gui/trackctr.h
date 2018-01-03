#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <vector>
#include "config.h"
#include "exceptions.h"
#include "seq_util.h"
#include "color_util.h"
#include "seq_math.h"
#include "track.h"
#include "clip.h"
#include "grid.h"
#include "guicontainer.h"
#include "tracktimeline.h"
#include "mouse.h"

#include "platform.h"
#include "dsp_util.h"
#include "leak_detect.h"


int32_t getPosYFirstReturnTrack(project_t& project);
track_t *getTrackFromMouse(project_t& project, ivec2 mouse, bool isDragSnap);
gui_track* createTrackGui(track_t* t, scaled_grid&); // trackcontent.cpp
gui_track_controls* createTrackGuiMixer(track_t* t); // trackcontrols.cpp



class guitrack_editor : public guictr_base {
public:
	Cursor& cursor;
	project_t& project;
	scaled_grid& grid;
	dragdrop_midifile& dragdrop;
	track_t *trSelected = NULL;
	clip_dragaction action;
	std::shared_ptr<clip_clipboard> clipboard;
	tracklayout_t dragStartLayout;
	int32_t dragStartTick = 0;
	int32_t dragStartTrackIdx = 0;

	trackstate_t resizePreModifyState;
	bool selectionMoved = false;

	guitrack_editor(Cursor& _cursor, project_t& _project, scaled_grid& _grid, dragdrop_midifile& _dragdropclip)
		: guictr_base(), 
		cursor(_cursor),
		project(_project),
		grid(_grid),
		dragdrop(_dragdropclip)
	{
		padding = 0;
		sortChildren = true;
	}
	bool mouseHitTest(ivec2 v, MouseHitEvt& evt) override {
		if (this->contains(v)) {
			if (evt.type == MOUSE_DRAGDROP_CLIP) {
				evt.requestFocus(this);
				return true;
			}
			ivec2 localMouse = this->toContainerSpace(v);
			for (guibase* gui : guis) {
				if (gui->mouseHitTest(localMouse, evt)) {
					if (!evt.getGuiHit()) // respect z-order, not an actual hit
						break;
					return true;
				}
			}
			evt.requestFocus(this);
			return true;
		}
		return false;
	}
	bool handleKeyInput(KeyEvent& kevt);

	void trackViewDragBegin(guitrack_editor* view, MouseEvent& evt);
	void trackViewDragMove(guitrack_editor* view, MouseEvent& evt);
	void trackViewDragRelease(guitrack_editor* view, MouseEvent& evt);
	void dragSelectionBegin(gui_clip* gClip, MouseEvent& evt);
	void dragSelectionMove(gui_clip* gui, MouseEvent& evt);
	void dragSelectionRelease(gui_clip* gui, MouseEvent& evt);
	void dragClipboardMove(ivec2 local);

	bool clipDropBegin(dragdrop_midifile& clip, ivec2 mousepos);
	bool clipDropMove(dragdrop_midifile& clip, ivec2 mousepos);
	bool clipDropFinal(dragdrop_midifile& clip, ivec2 mousepos);


	void handleRightClick(MouseEvent& evt);

	void renderClip(NVGcontext* vg, track_t* tr, const clip_t* cl, tick_t offset);
	void renderAction(NVGcontext* vg, clip_dragaction& action);
	void render(NVGcontext* vg);


	void handleDraggedBegin(MouseEvent& evt) {
		if (evt.type == MouseEventType::M_EVT_DOUBLECLICK) {
			if (evt.guiDragged->trackViewDoubleClick(this, evt)) {
				return;
			}
		}
		evt.guiDragged->trackViewDragBegin(this, evt);
	}
	void handleDraggedMove(MouseEvent& evt) {
		evt.guiDragged->trackViewDragMove(this, evt);
	}
	void handleDraggedRelease(MouseEvent& evt) {
		evt.guiDragged->trackViewDragRelease(this, evt);
	}


	void setSelectionRange(clip_t* clicked, track_t *trackClicked) {
		cursor.selRange = clicked->len;
		cursor.selTrackRange = 0;
		cursor.cursorPos = clicked->time;
		cursor.cursorTrack = trackClicked->idx;
	}

	void addTrack(track_t* t);
	void removeTrack(track_t* t);
	void updateVisibleTrackContents();
	void layout() {
		for (guibase* gui : guis) {
			gui->layout();
		}
	}
};


class guitrack_mixers : public guictr_base {
	project_t& project;
public:
	guitrack_mixers(project_t& _project)
		: guictr_base(),
		  project(_project)
	{
		padding = 0;
		sortChildren = true;
	}
	bool mouseHitTest(ivec2 v, MouseHitEvt& evt) override {
		if (this->contains(v)) {
			ivec2 localMouse = this->toContainerSpace(v);
			for (guibase* gui : guis) {
				if (gui->mouseHitTest(localMouse, evt)) {
					if (!evt.getGuiHit()) // respect z-order, not an actual hit
						break;
					return true;
				}
			}
			evt.requestFocus(this);
			return true;
		}
		return false;
	}
	void handleDraggedBegin(MouseEvent& evt) {
	}
	void handleDraggedMove(MouseEvent& evt) {
	}
	void handleDraggedRelease(MouseEvent& evt) {
	}
	void handleRightClick(MouseEvent& evt);
	void render(NVGcontext* vg);
	void addTrack(track_t* t);
	void removeTrack(track_t* t);
	void layout() {
		for (guibase* gui : guis) {
			gui->layout();
		}
	}


};

class te_constants {
protected:
	const uint32_t heightSeperator = 10;
	const uint32_t heightLoopIndicators = 24;
	const uint32_t heightTimelineControls = heightLoopIndicators + heightSeperator;
};
class guictr_tracks_loophandles : public guibase, te_constants {
	project_t& project;
	scaled_grid& grid;
	enum dragmode {
		drag_handle_none,
		drag_handle_loopleft,
		drag_handle_loopright,
		drag_handle_loopbar
	};
	dragmode dragHandle = drag_handle_none;
public:
	ivec2 clipViewSize;
	guictr_tracks_loophandles(project_t& _project, scaled_grid& _grid) :
			guibase(), project(_project), grid(_grid) {

	}
	int32_t dragOffset = 0;
	void handleDraggedBegin(MouseEvent& evt) {
		dragHandle = drag_handle_none;
		ivec2 local = evt.relMousepos;
		dragHandle = getDragZone(local);
		dragOffset = local.x-(int32_t)grid.tickToScreenD(project.loopStart);
	}
	void handleDraggedMove(MouseEvent& evt) {
		if (dragHandle == drag_handle_none) {
			return;
		}
		int32_t mousePosX = evt.relMousepos.x;
		if (dragHandle == drag_handle_loopbar) {
			mousePosX -= dragOffset;
		}
		tick_t tickAt = grid.screenToTickSnap(mousePosX, SNAP_ON);
		tick_t curLoopEnd = project.loopStart + project.loopLen;

		if (dragHandle == drag_handle_loopright) {
			tick_t tickDelta = (tickAt - curLoopEnd);
			tick_t newLen = project.loopLen + tickDelta;
			if (newLen > 0) {
				project.loopLen = newLen;
			}
		}
		if (dragHandle == drag_handle_loopleft) {
			tick_t curLoopStart = project.loopStart;
			tick_t tickDelta = (tickAt - curLoopStart);
			tick_t newStart = project.loopStart + tickDelta;
			if (newStart < curLoopEnd) {
				project.loopStart = newStart;
				project.loopLen = curLoopEnd - newStart;
			}
		}
		if (dragHandle == drag_handle_loopbar) {
			tick_t curLoopStart = project.loopStart;
			tick_t tickDelta = (tickAt - curLoopStart);
			project.loopStart += tickDelta;
		}
//		MainCtrl::get()->updateVisibleTrackContents();
	}
	void handleDraggedRelease(MouseEvent& evt) {
		dragHandle = drag_handle_none;
	}
	float dist(float x, float y, ivec2 mpos) {
		x = x - mpos.x;
		y = y - mpos.y;
		return x*x+y*y;
	}
	dragmode getDragZone(ivec2 local) {
		struct dist_draghandle {
			float dist = 0;
			dragmode mode = drag_handle_none;
		};;
		float dragTop = heightLoopIndicators/2.0f;
		float distBar = std::numeric_limits<float>::max();
		float barSX = clipLoopStartScrX();
		float barEX = clipLoopEndScrX();
		if (local.x >= barSX && local.x < barEX
				&& local.y >= 0 && local.y < heightLoopIndicators) {
			distBar = DRAG_RANGE*DRAG_RANGE*0.8f;
		}
		std::vector<dist_draghandle> hndls {
			{dist(barSX, dragTop, local), dragmode::drag_handle_loopleft},
			{dist(barEX, dragTop, local), dragmode::drag_handle_loopright},
			{distBar, dragmode::drag_handle_loopbar}
		};
		std::sort(hndls.begin(), hndls.end(), [](dist_draghandle const & a, dist_draghandle const & b) {
			return a.dist  < b.dist;
		});
		if (hndls[0].dist < DRAG_RANGE*DRAG_RANGE) {
			return hndls[0].mode;
		}

		return drag_handle_none;
	}
	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
		if (this->contains(mpos)) {
			ivec2 local = this->toContainerSpace(mpos);
			if (evt.type <= MouseHitType::MOUSE_RIGHT) {
				dragmode mode = getDragZone(local);
				if (mode == dragmode::drag_handle_loopleft) {
					evt.requestCursor(CURSOR_CLIP_SIZE_LEFT);
					evt.requestFocus(this);
					return true;
				}
				if (mode == dragmode::drag_handle_loopright) {
					evt.requestCursor(CURSOR_CLIP_SIZE_RIGHT);
					evt.requestFocus(this);
					return true;
				}
				if (mode == dragmode::drag_handle_loopbar) {
					evt.requestCursor(CURSOR_RESIZE_H);
					evt.requestFocus(this);
					return true;
				}
			}
		}
		return false;
	}
	float clipLoopStartScrX() {
		return (float)grid.tickToScreenD(project.loopStart);
	}
	float clipLoopEndScrX() {
		return (float)grid.tickToScreenD(project.loopStart + project.loopLen);
	}
	void render(NVGcontext* vg) {
		ivec2 cs = clipViewSize;
		if (cs.x <= 0 || cs.y <= 0)
			return;
		nvgIntersectScissor(vg, pos.x, pos.y, cs.x, cs.y);
		nvgTranslate(vg, pos.x, pos.y);
		nvgBeginPath(vg);
		nvgRect(vg, -2, 0, cs.x+2, size.y);
		nvgFillColor(vg, g_guiColors[COL_GRID_DRK]);
		nvgFill(vg);

		for (grid_div g : grid.gridList) {
			nvgBeginPath(vg);
			nvgMoveTo(vg, g.screenpos, 0);
			nvgLineTo(vg, g.screenpos, heightLoopIndicators);
			nvgStrokeColor(vg, g_guiColors[COL_LINE_BAR + g.color]);
			nvgStrokeWidth(vg, g.thickness);
			nvgStroke(vg);
		}
		nvgBeginPath(vg);
		nvgRect(vg, -2, heightLoopIndicators, cs.x+2, heightSeperator);
		nvgFillColor(vg, g_guiColors[COL_BG_DRKER2]);
		nvgFill(vg);


		const NVGcolor colLI = GUI_COLOR(120);
		const NVGcolor colLIStroke = GUI_COLOR(G_S1);
		const float strokeWidthLI = 1.0f;
		const float wLoopInidicator = heightLoopIndicators;


		int yOffset = 0;
		float tickBeginX = clipLoopStartScrX();
		float tickEndX = clipLoopEndScrX();
		if (!(tickBeginX - wLoopInidicator > cs.x || tickEndX + wLoopInidicator < 0)) {
			float barBeginX = max(-wLoopInidicator, tickBeginX);
			float barEndX = min(cs.x + wLoopInidicator, tickEndX);
			nvgBeginPath(vg);
			nvgRect(vg, barBeginX, yOffset, barEndX-barBeginX, heightLoopIndicators);

			nvgFillColor(vg, colLI);
			nvgFill(vg);
			nvgStrokeColor(vg, colLIStroke);
			nvgStrokeWidth(vg, strokeWidthLI);
			nvgStroke(vg);

			if (tickBeginX > -wLoopInidicator && tickBeginX < cs.x + wLoopInidicator) {
				nvgBeginPath(vg);
				nvgMoveTo(vg, tickBeginX, yOffset);
				nvgLineTo(vg, tickBeginX, yOffset+cs.y);
				nvgStrokeColor(vg, colLI);
				nvgStrokeWidth(vg, strokeWidthLI);
				nvgStroke(vg);
				drawTri(vg, tickBeginX, yOffset, wLoopInidicator, 0, colLI, colLIStroke, strokeWidthLI);
			}


			if (tickEndX > -wLoopInidicator && tickEndX < cs.x + wLoopInidicator) {
				nvgBeginPath(vg);
				nvgMoveTo(vg, tickEndX, yOffset);
				nvgLineTo(vg, tickEndX, cs.y-yOffset+1);
				nvgStrokeColor(vg, colLI);
				nvgStrokeWidth(vg, strokeWidthLI);
				nvgStroke(vg);
				drawTri(vg, tickEndX, yOffset, wLoopInidicator, 1, colLI, colLIStroke, strokeWidthLI);


			}
		}
		float xJmpFrom = grid.tickToScreenD(MainCtrl::get()->tickJmpFrom);
		float xJmpTo = grid.tickToScreenD(MainCtrl::get()->tickJmpTo);
		nvgBeginPath(vg);
		nvgMoveTo(vg, xJmpFrom, yOffset);
		nvgLineTo(vg, xJmpFrom, cs.y-yOffset+1);
		nvgStrokeColor(vg, G_YELLOW_DRK);
		nvgStrokeWidth(vg, strokeWidthLI);
		nvgStroke(vg);
		nvgBeginPath(vg);
		nvgMoveTo(vg, xJmpTo, yOffset);
		nvgLineTo(vg, xJmpTo, cs.y-yOffset+1);
		nvgStrokeColor(vg, G_GREEN_DRK);
		nvgStrokeWidth(vg, strokeWidthLI);
		nvgStroke(vg);

		yOffset += heightLoopIndicators;


	}
};
class guictr_tracks : public guictr_base, grid_changed_cb, te_constants {
public:
	scaled_grid& grid;
	project_t& project;
	guitrack_mixers trackControls;
	guitrack_editor trackView;
	guitrack_timeline trackTimeline;
	guictr_tracks_loophandles loophandles;
	guictr_tracks(Cursor& _cursor, project_t& _project, scaled_grid& _grid, dragdrop_midifile& _dragdropclip)
		: guictr_base(),
		grid(_grid),
		project(_project),
		trackControls(_project),
		trackView(_cursor, _project, _grid, _dragdropclip),
		trackTimeline(_grid),
		loophandles(_project, _grid)
	{
		_grid.addCallback(this);
		add(&trackTimeline);
		add(&loophandles);
		add(&trackControls);
		add(&trackView);
	}
	~guictr_tracks() {
		remove(&trackView);
		remove(&trackControls);
		remove(&loophandles);
		remove(&trackTimeline);
	}

	void addSingleTrack(track_t* t) {
		trackControls.addTrack(t);
		trackView.addTrack(t);
		layout();
	}
	void removeSingleTrack(track_t* t) {
		trackControls.removeTrack(t);
		trackView.removeTrack(t);
		layout();
	}
	void addTrack(track_t* t) {
		trackControls.addTrack(t);
		trackView.addTrack(t);
	}
	void removeTrack(track_t* t) {
		trackControls.removeTrack(t);
		trackView.removeTrack(t);
	}
	void drawSeperator(NVGcontext* vg, track_t* g, ivec2& cs);
	void setTrackPosition(track_t* t, int32_t trackheight, int32_t y);
	void render(NVGcontext* vg) {
		guictr_base::renderBackground(vg);
		ivec2 cs = getSizeContent();
		ivec2 cp = getPosContent();
		if (cs.y <= 0 || cs.x <= 0) {
			return;
		}
		nvgIntersectScissor(vg, cp.x, cp.y, cs.x, cs.y);
		nvgTranslate(vg, cp.x, cp.y);
		nvgSave(vg);
			trackView.render(vg);
		nvgRestore(vg);
		nvgSave(vg);
			trackControls.render(vg);
		nvgRestore(vg);
		nvgSave(vg);
			trackTimeline.render(vg);
		nvgRestore(vg);

		nvgSave(vg);
			nvgTranslate(vg, 0, trackView.top());
			int ySplit = getPosYFirstReturnTrack(project);
			if (ySplit > 0) {
				nvgSave(vg);
				nvgIntersectScissor(vg, 0, 0, cs.x, ySplit);
				for (track_t* g : project.trackCtr) {
					drawSeperator(vg, g, cs);
				}
				nvgRestore(vg);
			}
			if (project.tracksBottom.size()) {
				if (ySplit > 0) {
					nvgIntersectScissor(vg, 0, ySplit, cs.x, trackView.size.y-ySplit);
				} else {
					nvgIntersectScissor(vg, 0, 0, cs.x, trackView.size.y);
				}
				for (track_t* g : project.tracksBottom) {
					drawSeperator(vg, g, cs);
				}
			}
		nvgRestore(vg);

		nvgBeginPath(vg);
		nvgMoveTo(vg, trackControls.left(), trackControls.top());
		nvgLineTo(vg, trackControls.left(), trackControls.bottom());
		nvgStrokeColor(vg, g_guiColors[COL_LINE_SEPERATOR]);
		nvgStrokeWidth(vg, 3);
		nvgStroke(vg);




		nvgSave(vg);
		loophandles.render(vg);
		nvgRestore(vg);

		if (trackView.size.x > 0) {
			nvgIntersectScissor(vg, 0, 0, trackView.size.x, cs.y);
			tick_t pos = project.playbackPos;
	//		if (project.loopEnabled) {
	//			if (pos > project.loopStart) {
	//				pos = project.loopStart + (pos - project.loopStart) % project.loopLen;
	//			}
	//		}
			float playBackX = (float) grid.tickToScreenD(pos);
			if (playBackX > -4.0f && playBackX < cs.x + 4.0f) {
				nvgBeginPath(vg);
				nvgMoveTo(vg, playBackX, 0);
				nvgLineTo(vg, playBackX, cs.y);
				nvgStrokeColor(vg, GUI_COLOR(120));
				nvgStrokeWidth(vg, 3);
				nvgStroke(vg);
				nvgBeginPath(vg);
				nvgMoveTo(vg, playBackX, 0);
				nvgLineTo(vg, playBackX, cs.y);
				nvgStrokeColor(vg, GUI_COLOR(250));
				nvgStrokeWidth(vg, 1);
				nvgStroke(vg);
			}
	//		nvgIntersectScissor(vg, 0, 0, trackView.size.x, trackView.size.y);
	//		nvgTranslate(vg, 0, trackTimeline.bottom());

	//		double playBackX = grid.tickToScreenD(MainCtrl::get()->playbackPos);
	//		if (playBackX > -4 && playBackX < cs.x+4) {
	//			nvgBeginPath(vg);
	//			nvgMoveTo(vg, playBackX, 0);
	//			nvgLineTo(vg, playBackX, cs.y);
	//			nvgStrokeColor(vg, GUI_COLOR(120));
	//			nvgStrokeWidth(vg, 3);
	//			nvgStroke(vg);
	//			nvgBeginPath(vg);
	//			nvgMoveTo(vg, playBackX, 0);
	//			nvgLineTo(vg, playBackX, cs.y);
	//			nvgStrokeColor(vg, GUI_COLOR(250));
	//			nvgStrokeWidth(vg, 1);
	//			nvgStroke(vg);
	//		}
		}
	}
	void layout() {
		const int mixerwidth = 380;
		ivec2 cs = getSizeContent();
		trackTimeline.pos = ivec2(0, 0);
		trackTimeline.size = ivec2(cs.x - mixerwidth, 32);
		loophandles.pos = ivec2(trackTimeline.left(), trackTimeline.bottom());
		loophandles.size = ivec2(trackTimeline.size.x, heightTimelineControls);

		trackView.pos = ivec2(0, loophandles.bottom());
		trackControls.pos = ivec2(cs.x - mixerwidth, loophandles.bottom());
		trackView.size = ivec2(cs.x - mixerwidth - trackView.pos.x, cs.y - loophandles.bottom());
		trackControls.size = ivec2(mixerwidth, trackView.size.y);

		loophandles.clipViewSize = ivec2(trackView.size.x, trackView.size.y+loophandles.size.y);


		ivec2 csTrackView = trackView.getSizeContent();
		int y = TRACK_HEIGHT_SPACING;
		for (track_t* t : project.trackCtr) {
			int trackheight = t->height * TRACK_HEIGHT_STEP;
			assert(t->content != NULL);
			setTrackPosition(t, trackheight, y);
			y += trackheight + TRACK_HEIGHT_SPACING;
		}
		y = csTrackView.y-TRACK_HEIGHT_SPACING;
//		y = 0;
		auto itMastersTracks = project.tracksBottom.rbegin();
		auto itMastersEnd = project.tracksBottom.rend();
		while (itMastersTracks != itMastersEnd) {
			track_t* t = *itMastersTracks;
			int trackheight = t->height * TRACK_HEIGHT_STEP;
			y -= trackheight;
//			y -= 100;
			assert(t->content != NULL);
			setTrackPosition(t, trackheight, y);
//			y += trackheight + TRACK_HEIGHT_SPACING;
			y -= TRACK_HEIGHT_SPACING;
			itMastersTracks++;
		}

		for (guibase* gui : guis) {
			gui->layout();
		}
		MainCtrl::get()->updateGrid();
	}
	void updateVisibleTrackContents() {
		trackView.updateVisibleTrackContents();
	}
	bool mouseHitTest(ivec2 v, MouseHitEvt& evt) override {
		if (this->contains(v)) {
			ivec2 localMouse = this->toContainerSpace(v);
			for (guibase* gui : guis) {
				if (gui->mouseHitTest(localMouse, evt)) {
					return true;
				}
			}
		}
		return false;
	}

	void onChildLayoutChanged(guibase* g) {
		layout();
	}
	void gridChanged(scaled_grid& _grid) override {
		MainCtrl::get()->updateGrid();
	}
	bool handleKeyInput(KeyEvent& kevt) {
		return trackView.handleKeyInput(kevt);
	}
};


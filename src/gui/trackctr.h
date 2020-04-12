#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <vector>
#include <memory>
#include "config.h"
#include "math/vec.h"
#include "math/seq_math.h"
#include "exceptions.h"
#include "seq_util.h"
#include "color_util.h"
#include "track.h"
#include "clip.h"
#include "clipboard.h"
#include "grid.h"
#include "gui.h"
#include "guicontainer.h"
#include "scrollbar.h"
#include "tracktimeline.h"
#include "track_snapshot.h"
#include "mouse.h"
#include "keyboard.h"
#include "cursor.h"
#include "platform.h"
#include "dsp_util.h"
#include "../host/mainctrl.h"
#include "trackctr_types.h"

void updateStoreLoadSubtracks(guictr_tracks* guiTracks, track_gui_entry_t* entry) ;
class track_gui_manager_i {
public:
	track_gui_manager_i() = default;
	virtual ~track_gui_manager_i() { };
	virtual bool getTrackEntryCopy(const track_t* t, track_gui_entry_t& out) = 0;
	virtual bool getPointerEntry(const track_t* t, track_gui_entry_t** out) = 0;

	virtual bool isVisible(const track_gui_entry_t* entry) = 0;
	virtual bool validTrackIdx(int32_t idx) const = 0;
	virtual const track_gui_entry_t* at(const size_t i) const = 0;
	virtual track_gui_entry_t* atNC(const size_t i) = 0;
	virtual int32_t clampTrackIdx(int32_t idx) const  = 0;
	virtual const track_gui_vector_td& getTracksVisibleFlat() = 0;
	virtual void reset() = 0;
	virtual int32_t getTrackProjectIndex(int32_t guiIdx) const  = 0;
};

int32_t getPosYFirstReturnTrack(const track_gui_vector_td& tracksVisibleFlat);
track_gui_entry_t *getTrackFromMouse(track_gui_manager_i& iGuiMgr, ivec2 mouse, bool isDragSnap);

gui_track_subtrack* getSubTrackFromMouse(track_gui_manager_i& iGuiMgr, ivec2 mouse, bool isDragSnap);
gui_track* createTrackGui(track_gui_entry_t* _entry, scaled_grid&); // trackcontent.cpp
gui_track_controls* createTrackGuiMixer(track_gui_entry_t* _entry); // trackcontrols.cpp
void drawSeperator(NVGcontext* vg, const guitheme_t* theme, int32_t seperatorY, ivec2& cs);



class guitrack_editor : public guictr_base {
	DawCtrl* const dawCtrl;

public:
	track_gui_manager_i& iGuiMgr;
	DAW::Cursor& cursor;
	project_t& project;
	project_globals_t& projectGlobals;
	scaled_grid& grid;
	dragdrop_midifile& dragdrop;
	track_gui_entry_t *trSelected = NULL;
	gui_track_subtrack* subTrSelected = NULL;
	clip_dragaction action;						 // move up in hierachy
	std::shared_ptr<clip_clipboard> clipboard; // move up in hierachy
	tracklayout_t dragStartLayout;
	int32_t dragStartTick = 0;
	int32_t dragStartTrackIdx = 0;

	trackstate_t resizePreModifyState;
	bool selectionMoved = false;
	guitrack_editor(DawCtrl* const _dawCtrl, track_gui_manager_i& _iGuiMgr, DAW::Cursor& _cursor, project_t& _project, project_globals_t& _projectGlobals, scaled_grid& _grid, dragdrop_midifile& _dragdropclip)
		: guictr_base(), 
	    dawCtrl(_dawCtrl),
		iGuiMgr(_iGuiMgr),
		cursor(_cursor),
		project(_project),
		projectGlobals(_projectGlobals),
		grid(_grid),
		dragdrop(_dragdropclip)
	{
		padding = 0;
		sortChildren = true;
	}
	~guitrack_editor() {
		clipboard.reset();
	}
	bool mouseHitTest(ivec2 v, MouseHitEvt& evt) override;
	bool handleKeyInput(KeyEvent& kevt);

	void trackViewDragBegin(guitrack_editor* view, MouseEvent& evt);
	void trackViewDragMove(guitrack_editor* view, MouseEvent& evt);
	void trackViewDragRelease(guitrack_editor* view, MouseEvent& evt);
	void dragSelectionBegin(gui_clip* gClip, MouseEvent& evt);
	void dragSelectionMove(gui_clip* gui, MouseEvent& evt);
	void dragSelectionRelease(gui_clip* gui, MouseEvent& evt);
	void dragClipboardMove(ivec2 local, int kbmods);

	bool clipDropBegin(dragdrop_midifile& clip, ivec2 mousepos, int kbmods);
	bool clipDropMove(dragdrop_midifile& clip, ivec2 mousepos, int kbmods);
	bool clipDropFinal(dragdrop_midifile& clip, ivec2 mousepos, int kbmods);


	void handleRightClick(MouseEvent& evt);

	void renderClip(NVGcontext* vg, const track_gui_entry_t* const entry, clip_t* cl, tick_t offset);
	void renderAction(NVGcontext* vg, clip_dragaction& action);
	void render(NVGcontext* vg);
	void prerender(NVGcontext* vg) override;

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


	void setSelectionRange(clip_t* clicked, track_gui_entry_t *trackClicked) {
		cursor.selRange = clicked->getLen();
		cursor.selTrackRange = 0;
		cursor.cursorPos = clicked->time;
		cursor.setTrack(trackClicked->idx);
		cursor.cursorSubTrack = -1;
		cursor.selSubTrackRange = 0;
	}
	void addSubtrack(track_gui_entry_t* entry, gui_track_subtrack* al, bool insertFront);

	void removeSubtrack(track_gui_entry_t* entry, gui_track_automationlane* al);
	void removeAllAutomationLanes(track_gui_entry_t* entry, automatable_t* at, int32_t paramIdx);
	void removeAllAutomationLanes(track_gui_entry_t* entry, automatable_t* at);
	void removeAllSubtracks(track_gui_entry_t* entry);
	virtual void trackEntryDragMove(gui_track* g, ivec2 mousepos);
	virtual void trackEntryDragRelease(gui_track* g, ivec2 mousepos);
	void addTrackEntry(track_gui_entry_t& e);
	void removeTrackEntry(track_gui_entry_t& e);
	void layout();
};


class guitrack_mixers : public guictr_base {
	track_gui_manager_i& iGuiMgr;
	project_t& project;
public:
	guitrack_mixers(track_gui_manager_i& _iGuiMgr, project_t& _project)
		: guictr_base(),
		  iGuiMgr(_iGuiMgr),
		  project(_project)
	{
		padding = 0;
		sortChildren = true;
	}
	bool mouseHitTest(ivec2 v, MouseHitEvt& evt) override {
		if (this->contains(v)) {
			if (evt.type == MouseHitType::MOUSE_DRAGDROP_OBJECT) {
				evt.requestFocus(this);
				return true;
			}
			ivec2 localMouse = this->toContainerSpace(v);
			for (guibase* gui : guis) {
				if (gui->isVisible() && gui->mouseHitTest(localMouse, evt)) {
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
	virtual void trackEntryDragMove(gui_track* g, ivec2 mousepos);
	virtual void trackEntryDragRelease(gui_track* g, ivec2 mousepos);
//	void addAutomationLane(track_t* t, gui_track_automationlane* al);
//	void removeAutomationLane(gui_track_automationlane* al);
//	void removeAllAutomationLanes(track_t* t, automatable_t* at, int32_t paramIdx);
//	void removeAllAutomationLanes(track_t* t, automatable_t* at);
//	void removeAllAutomationLanes(track_t* t);
	void addTrackEntry(track_gui_entry_t& e);
	void removeTrackEntry(track_gui_entry_t& e);
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
	project_globals_t& projectGlobals;
	scaled_grid& grid;
	enum dragmode {
		drag_handle_none,
		drag_handle_loopleft,
		drag_handle_loopright,
		drag_handle_loopbar
	};
	dragmode dragHandle = drag_handle_none;
public:
	ivec2 clipViewSize{ 0, 0 };
	guictr_tracks_loophandles(project_t& _project, project_globals_t& _projectGlobals, scaled_grid& _grid) :
			guibase(), project(_project), projectGlobals(_projectGlobals), grid(_grid) {

	}
	int32_t dragOffset = 0;
	void handleDraggedBegin(MouseEvent& evt) {
		dragHandle = drag_handle_none;
		ivec2 local = evt.relMousepos;
		dragHandle = getDragZone(local);
		dragOffset = local.x-(int32_t)grid.tickToScreenD(projectGlobals.loopStart);
	}
	void handleDraggedMove(MouseEvent& evt) {
		if (dragHandle == drag_handle_none) {
			return;
		}
		int32_t mousePosX = evt.relMousepos.x;
		if (dragHandle == drag_handle_loopbar) {
			mousePosX -= dragOffset;
		}
		tick_t tickAt = grid.screenToTickSnap(mousePosX, isAlt(evt.kbmods) ? SNAP_OFF : SNAP_ON);
		tick_t curLoopEnd = projectGlobals.loopStart + projectGlobals.loopLen;

		if (dragHandle == drag_handle_loopright) {
			tick_t tickDelta = (tickAt - curLoopEnd);
			tick_t newLen = projectGlobals.loopLen + tickDelta;
			if (newLen > 0) {
				projectGlobals.loopLen = newLen;
			}
		}
		if (dragHandle == drag_handle_loopleft) {
			tick_t curLoopStart = projectGlobals.loopStart;
			tick_t tickDelta = (tickAt - curLoopStart);
			tick_t newStart = projectGlobals.loopStart + tickDelta;
			if (newStart < curLoopEnd) {
				projectGlobals.loopStart = newStart;
				projectGlobals.loopLen = curLoopEnd - newStart;
			}
		}
		if (dragHandle == drag_handle_loopbar) {
			tick_t curLoopStart = projectGlobals.loopStart;
			tick_t tickDelta = (tickAt - curLoopStart);
			projectGlobals.loopStart += tickDelta;
		}
//		DawInstance::get()->updateVisibleTrackContents();
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
				&& local.y >= 0 && local.y < (int)heightLoopIndicators) {
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
		return (float)grid.tickToScreenD(projectGlobals.loopStart);
	}
	float clipLoopEndScrX() {
		return (float)grid.tickToScreenD(projectGlobals.loopStart + projectGlobals.loopLen);
	}
	void render(NVGcontext* vg) {
		ivec2 cs = clipViewSize;
		if (cs.x <= 0 || cs.y <= 0)
			return;
		nvgIntersectScissor(vg, pos.x, pos.y, cs.x, cs.y);
		nvgTranslate(vg, pos.x, pos.y);
		nvgBeginPath(vg);
		nvgRect(vg, -2, 0, cs.x+2, size.y);
		nvgFillColor(vg, theme->getColor(GuiColor::COL_GRID_DRK));
		nvgFill(vg);

		for (grid_div g : grid.gridList) {
			nvgBeginPath(vg);
			nvgMoveTo(vg, g.screenpos, 0);
			nvgLineTo(vg, g.screenpos, heightLoopIndicators);
			NVGcolor col;
			switch (g.color) {
			case 0:
				col = theme->getColor(GuiColor::COL_LINE_BAR);
				break;
			case 1:
				col = theme->getColor(GuiColor::COL_LINE_QRT);
				break;
			case 2:
			default:
				col = theme->getColor(GuiColor::COL_LINE_XTH);
				break;
			}
			nvgStrokeColor(vg, col);
			nvgStrokeWidth(vg, g.thickness);
			nvgStroke(vg);
		}
		nvgBeginPath(vg);
		nvgRect(vg, -2, heightLoopIndicators, cs.x+2, heightSeperator);
		nvgFillColor(vg, theme->getColor(GuiColor::COL_BG_DRKER2));
		nvgFill(vg);


		const NVGcolor colLI = GUI_COLOR(120);
		const NVGcolor colLIStroke = theme->getFrameColorOutline();
		const float strokeWidthLI = 1.0f;
		const float wLoopInidicator = heightLoopIndicators;


		int yOffset = 0;
		float tickBeginX = clipLoopStartScrX();
		float tickEndX = clipLoopEndScrX();
		if (!(tickBeginX - wLoopInidicator > cs.x || tickEndX + wLoopInidicator < 0)) {
			float barBeginX = math::max(-wLoopInidicator, tickBeginX);
			float barEndX = math::min(cs.x + wLoopInidicator, tickEndX);
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
		float xJmpFrom = grid.tickToScreenD(DawInstance::get()->tickJmpFrom);
		float xJmpTo = grid.tickToScreenD(DawInstance::get()->tickJmpTo);
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
class track_gui_manager_t : public track_gui_manager_i {
	friend class guictr_tracks;
	track_gui_vector_td entries;
	track_gui_vector_td trackEntriesTop;
	track_gui_vector_td trackEntriesBottom;
	track_gui_vector_td tracksVisibleFlat;
public:
	bool getTrackEntryCopy(const track_t* t, track_gui_entry_t& out) override;
	bool getPointerEntry(const track_t* t, track_gui_entry_t** out) override;
	bool getTrackEntry(const track_t* t, track_gui_entry_t** out) ;

	void removeTrack(track_gui_entry_t& entry) {
		auto it = std::remove_if(begin(entries), end(entries), [&entry](track_gui_entry_t* e) {
			return e->track == entry.track;
		});
		if (it == entries.end()) {
			dbgassert(0);
			return;
		};
		removeEntry(trackEntriesTop, &entry);
		removeEntry(trackEntriesBottom, &entry);
		removeEntry(tracksVisibleFlat, &entry);
		delete &entry;
		entries.erase(it, entries.end());
	}
	void addTrack(track_gui_entry_t* entry) {
		auto it = std::find_if(begin(entries), end(entries), [this, entry](track_gui_entry_t* e) {
			if (e->track == entry->track) {
				return true;
			}
			return false;
		});
		dbgassert(it == entries.end() && "Attempt to add track_gui_entry_t twice");
		entries.push_back(entry);

	}
	/**
	 * checks if entry is visible.
	 * A track_gui_entry_t is visible if
	 * all of its parents have the field
	 * this->layout.hideTrack == true
	 */
	bool isVisible(const track_gui_entry_t* entry) override {
		bool bHidden = false;
		track_t* p = entry->track->parent;
		while (!bHidden && p) {
			track_gui_entry_t* parentEntry;
			if (!getPointerEntry(p, &parentEntry)) {
				return false;
			}
			bHidden |= parentEntry->layout.hideTrack;
			p = p->parent;
		}
		return !bHidden;
	}
	bool validTrackIdx(int32_t idx) const override {
		return idx >= 0 && idx < (int32_t) tracksVisibleFlat.size();
	}
	const track_gui_entry_t* at(const size_t i) const override {
		if (!validTrackIdx(i)) {
			return nullptr;
		}
		return tracksVisibleFlat.at(i);
	}
	track_gui_entry_t* atNC(const size_t i) override {
		if (!validTrackIdx(i)) {
			return nullptr;
		}
		return tracksVisibleFlat.at(i);
	}
	int32_t clampTrackIdx(int32_t idx) const override {
		return math::max(0, math::min((int32_t) tracksVisibleFlat.size() - 1, idx));
	}
	void updateVisibleTracks(trackallcontainer_t& trackList) {
		/** turn tree structure into linear pointer array with trackTop at the beginning and the deepest child at the end **/
		track_gui_vector_td vecNewTracksFlat;
		std::deque<track_t*> stack;
		stack.insert(stack.begin(), trackList.cbeginTree(), trackList.cendTree());
		trackEntriesTop.clear();
		trackEntriesBottom.clear();
		while (!stack.empty()) {
			track_t* current = stack.front();
			stack.pop_front();
			track_gui_entry_t* entry;

			if ((getPointerEntry(current, &entry))) {
				if (!entry->layout.hideTrack && current->children.size()) {
					stack.insert(stack.begin(), current->children.cbegin(), current->children.cend());
				}
				dbgassert(isVisible(entry));
				dbgassert(entry->track == current);
				if (TRACKTYPE_TO_CTR(entry->track->type)  == TRACK_CTR_MIDIAUDIO) {
					trackEntriesTop.push_back(entry);
				} else {
					trackEntriesBottom.push_back(entry);
				}
				if (entry->idx != vecNewTracksFlat.size()) {
					log_printf("entry idx changed from %d to %d (track_t idx: %d)\n", entry->idx, vecNewTracksFlat.size(), current->projectIdx);
				}
				entry->idx = vecNewTracksFlat.size();
				vecNewTracksFlat.push_back(entry);
			}
		}

		tracksVisibleFlat = vecNewTracksFlat;
	}
	const track_gui_vector_td& getTracksVisibleFlat() override {
		return tracksVisibleFlat;
	}
	void reset() override {
		dbgassert(entries.empty());
		tracksVisibleFlat.clear();
		trackEntriesTop.clear();
		trackEntriesBottom.clear();
	}
	int32_t getTrackProjectIndex(int32_t guiIdx) const override {
		if (assert_expr(validTrackIdx(guiIdx))) {
			return at(guiIdx)->track->projectIdx;
		}
		return -1;
	}
};
class guictr_tracks : public guictr_base, grid_changed_cb, te_constants, public gui_scrollcontainer {
	friend class guitrack_editor;
	int32_t globalIndex = 0;
	DawCtrl* const dawCtrl;
public:
	scaled_grid& grid;
	project_t& project;
	project_globals_t& projectGlobals;
	track_gui_manager_t guiMgr;
	guitrack_mixers trackControls;
	guitrack_editor trackView;
	guitrack_timeline trackTimeline;
	guictr_tracks_loophandles loophandles;
protected:
	gui_scrollbar scrollbar;
	int32_t contentHeight = 0;
	int32_t contentViewSize = 0;
public:
public:
	guictr_tracks(DawCtrl* _dawCtrl, DAW::Cursor& _cursor, project_t& _project, project_globals_t& _projectGlobals, scaled_grid& _grid, dragdrop_midifile& _dragdropclip)
		: guictr_base(),
		dawCtrl(_dawCtrl),
		grid(_grid),
		project(_project),
		projectGlobals(_projectGlobals),
		guiMgr(),
		trackControls(guiMgr, _project),
		trackView(_dawCtrl, guiMgr, _cursor, _project, _projectGlobals, _grid, _dragdropclip),
		trackTimeline(_grid),
		loophandles(_project, _projectGlobals, _grid),
		scrollbar(1, 0.0f, *this)
	{
		setBackgroundRendered(true);
		_grid.addCallback(this);
		add(&scrollbar);
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
		remove(&scrollbar);
	}

	int32_t setTrackPosition(track_gui_entry_t* e, int32_t y, bool isBottom);
	void render(NVGcontext* vg);
	void scrollTo(guibase* g);
	void layout();
private:
	void updateVisibleTracks();
public:
	void updateVisibleTrackContents();

	void onChildLayoutChanged(guibase* g) {
		layout();
	}
	void gridChanged(scaled_grid& _grid) override {
		dawCtrl->updateGrid();
	}
	bool handleKeyInput(KeyEvent& kevt) {
		return trackView.handleKeyInput(kevt);
	}
	ivec2 getScrollTotalSize() override {
		ivec2 cs = getSizeContent();
		cs.y = contentHeight;
		return cs;
	}
	ivec2 getScrollViewSize() override {
		ivec2 cs = getSizeContent();
		cs.y = contentViewSize;
		return cs;
	}
	void scrollOffsetChanged(int dir, float offset);
	virtual bool handleMouseScroll(MouseEvent& evt, double xoffset, double yoffset) {
		return scrollbar.handleMouseScroll(evt, xoffset, yoffset);
	}
	void setScrollOffset(float offset) {
		this->scrollbar.setScrollOffset(offset);
	}
	float getScrollOffset() {
		return this->scrollbar.scrollOffset;
	}
	void removeTrack(track_t* track, int flags);
	void addTrack(track_t* track, int flags);
	void removeAllTracks();
	void showAutomationLane(track_gui_entry_t* entry, automatable_t* at, int32_t paramIdx);
	void addSubTrack(track_gui_entry_t* entry, gui_track_subtrack* subtrack, bool insertFront);

	gui_track_automationlane* addAutomationLane(track_gui_entry_t* entry, automatable_t* at, int32_t paramIdx, bool insertFront);
	void removeAutomationLane(gui_track_automationlane* al);
	void removeAllAutomationLanes(track_gui_entry_t* entry, automatable_t* at, int32_t paramIdx);
	void removeAllAutomationLanes(track_gui_entry_t* entry, automatable_t* at);
	void removeAllSubtracks(track_gui_entry_t* entry);
	void loadTrackLayouts(trackcontainer_snapshot_t& in);
	bool getTrackEntry(track_t* t, track_gui_entry_t** out) {
		return guiMgr.getTrackEntry(t, out);
	}
	bool getPointerEntry(track_t* t, track_gui_entry_t** out) {
		return guiMgr.getPointerEntry(t, out);
	}
	bool isTrackEntryVisible(const track_gui_entry_t* entry) {
		return guiMgr.isVisible(entry);
	}
	void resetView();
};


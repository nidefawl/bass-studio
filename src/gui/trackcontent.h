#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <vector>
#include "seq_math.h"
#include "seq_util.h"
#include "color_util.h"
#include "track.h"
#include "clip.h"
#include "grid.h"
#include "guicontainer.h"
#include "leak_detect.h"


class gui_clip : public guibase {
public:
	clip_t* const m_clip;
	track_t* const m_track;
	bool culled = true;
	gui_clip(clip_t* _clip, track_t* _track)
		: guibase(),
		  m_clip(_clip),
		  m_track(_track) {
	}
	bool isClipTitleBar(ivec2 mpos) {
		return mpos.x >= pos.x &&
			mpos.y >= pos.y &&
			mpos.x < pos.x + size.x &&
			mpos.y < pos.y + HEIGHT_CLIP_TITLE;
	}
	bool isLeftDragZone(ivec2 mpos) {
		return mpos.x >= pos.x &&
			mpos.y >= pos.y &&
			mpos.x < pos.x+DRAG_RANGE &&
			mpos.y < pos.y + HEIGHT_CLIP_TITLE;
	}
	bool isRightDragZone(ivec2 mpos) {
		return mpos.x >= pos.x + size.x-DRAG_RANGE &&
			mpos.y >= pos.y &&
			mpos.x < pos.x + size.x &&
			mpos.y < pos.y + HEIGHT_CLIP_TITLE;
	}
	void render(NVGcontext* vg) {
		if (!culled) {
			renderClip(vg, m_track, m_clip, pos, size);
		}
	}
	static void renderClip(NVGcontext* vg, const track_t* tr, const clip_t* cl, ivec2 pos, ivec2 size);
	static bool getClipPosition(scaled_grid& grid, const ivec2& trackSize, const clip_t* cl, ivec2& pos, ivec2& size, tick_t offset) {
		tick_t tickBegin = cl->time + offset;
		tick_t tickEnd = cl->time + offset + cl->len;
		double tickBeginX = grid.tickToScreenD(tickBegin);
		double tickEndX = grid.tickToScreenD(tickEnd);
		if (tickEndX < -4 || tickBeginX > trackSize.x + 4) {
			return false;
		}
		double width = tickEndX - tickBeginX;
		assert(FitsTypeRange<int32_t>(tickBeginX));
		assert(FitsTypeRange<int32_t>(tickEndX));
		int32_t tickBeginPx = (int32_t) round(tickBeginX);
		int32_t widthPx = (int32_t) round(width);
		pos = ivec2(tickBeginPx, INSET_TRACK_CONTENT);
		size = ivec2(widthPx, size.y-INSET_TRACK_CONTENT*2);
		return true;
	}
	void updatePosition(scaled_grid& grid, ivec2& trackSize) {
		size = this->parent->size;
		culled = !getClipPosition(grid, trackSize, m_clip, pos, size, 0);
	}
	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
		if (culled) {
			return false;
		}
		if (isLeftDragZone(mpos)) {
			if (evt.type <= MouseHitType::MOUSE_RIGHT)
				evt.requestCursor(CURSOR_CLIP_SIZE_LEFT);
			evt.requestFocus(this);
			return true;
		}
		if (isRightDragZone(mpos)) {
			if (evt.type <= MouseHitType::MOUSE_RIGHT)
				evt.requestCursor(CURSOR_CLIP_SIZE_RIGHT);
			evt.requestFocus(this);
			return true;
		}
		if (isClipTitleBar(mpos)) {
			evt.requestFocus(this);
			return true;
		}
		return false;
	}
	bool handleKeyInput(KeyEvent& kevt) {
		return parent->handleKeyInput(kevt);
	}
	void handleDraggedBegin(MouseEvent& evt) {
		evt.relMousepos += pos;
		parent->handleDraggedBegin(evt);
	}

	void handleDraggedMove(MouseEvent& evt) {
		evt.relMousepos += pos;
		parent->handleDraggedMove(evt);
	}

	void handleDraggedRelease(MouseEvent& evt) {
		evt.relMousepos += pos;
		parent->handleDraggedRelease(evt);
	}
	void handleRightClick(MouseEvent& evt);

	void trackViewDragBegin(guitrack_editor* view, MouseEvent& evt);
	void trackViewDragMove(guitrack_editor* view, MouseEvent& evt);
	void trackViewDragRelease(guitrack_editor* view, MouseEvent& evt);
	virtual void onRemove() {
		assert(m_clip->gClip == this);
		m_clip->gClip = NULL;
	}
	bool isDragMoveable() {
		return true;
	}
};
class gui_track : public guictr_base {
public:
	track_t* const m_track;
	gui_track(track_t* _track) : guictr_base(), m_track(_track) {
		padding = 0;
	}
	virtual ~gui_track() {

	}
	bool isStaticContainer() {
		return false;
	}
	bool handleKeyInput(KeyEvent& kevt) override {
		return parent->handleKeyInput(kevt);
	}

	virtual void handleDraggedBegin(MouseEvent& evt) override {
		MainCtrl::get()->setSelectedTrack(m_track);
		evt.relMousepos += getPosContent();
		parent->handleDraggedBegin(evt);
	}

	void handleDraggedMove(MouseEvent& evt) override {
		evt.relMousepos += getPosContent();
		parent->handleDraggedMove(evt);
	}

	void handleDraggedRelease(MouseEvent& evt) override {
		evt.relMousepos += getPosContent();
		parent->handleDraggedRelease(evt);
	}

	void handleRightClick(MouseEvent& evt) override;

	virtual void render(NVGcontext* vg) override {
		if (MainCtrl::get()->getSelectedTrack() == m_track) {
			nvgBeginPath(vg);
			nvgRect(vg, pos.x, pos.y, size.x, size.y);
			nvgFillColor(vg, g_guiColors[COL_BG_SELECTEDTRACK]);
			nvgFill(vg);
		}
	}
	virtual bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
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
			}
			// tracks need to always cancel further mouse tests for z-order to work in parent container
			return true;
		}
		return false;
	}
	virtual void updateVisibleTrackContents(scaled_grid& grid) {

	}
};
class gui_track_controls: public guictr_base {
public:
	track_t* const m_track;
private:
	guictr_base* title;
	guictr_base* mixer;
	int dragMode = -1;
	const int resizeHitY = 8;
	const int DRAG_RESIZE = 1;
public:
	gui_track_controls(track_t* _track);
	~gui_track_controls();
	bool isStaticContainer() {
		return false;
	}
	void render(NVGcontext* vg) override;
	void handleDraggedBegin(MouseEvent& evt) {
		MainCtrl::get()->setSelectedTrack(m_track);
		if (isResize(evt.relMousepos+this->pos)) {
			dragMode = DRAG_RESIZE;
		}
	}

	void handleDraggedMove(MouseEvent& evt) {
		if (dragMode == DRAG_RESIZE) {
			int32_t mouseDragDist = evt.relMousepos.y;
			bool resizeTop = m_track->type < TRACK_TYPE_MIDI;
			if (resizeTop) {
				mouseDragDist = -evt.relMousepos.y+size.y;
			}
			m_track->height = min(12, max(1, (mouseDragDist) / TRACK_HEIGHT_STEP));
			this->parent->onChildLayoutChanged(this);
		}
	}

	void handleDraggedRelease(MouseEvent& evt) {
		dragMode = -1;
	}
	void handleRightClick(MouseEvent& evt);
	bool isResize(ivec2 mpos) {
		int32_t resizeTopOrBottom = m_track->type < TRACK_TYPE_MIDI ? top() : bottom();
		return mpos.y >= resizeTopOrBottom - resizeHitY
				&& mpos.y < resizeTopOrBottom + resizeHitY;
	}

	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
		if (contains(mpos)) {
			ivec2 local = this->toContainerSpace(mpos);
			for (guibase* gui : guis) {
				if (gui->mouseHitTest(local, evt)) {
					return true;
				}
			}
			evt.requestFocus(this);
			return true; // always need to return true if contained, parent has z-order
		}
		if (isResize(mpos)) {
			evt.requestFocus(this);
			if (evt.type <= MouseHitType::MOUSE_RIGHT)
				evt.requestCursor(CURSOR_RESIZE_V);
			return true;
		}
		return false;
	}
	void layout() override;
};


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

#define DRAG_RANGE 10
class gui_clip : public guibase {
public:
	clip_t* const m_clip;
	gui_clip(clip_t* _clip)
		: guibase(),
		  m_clip(_clip) {
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
		renderClip(vg, m_clip, pos, size);
	}
	static void renderClip(NVGcontext* vg, const clip_t* cl, ivec2 pos, ivec2 size);
	static void getClipPosition(scaled_grid& grid, const clip_t* cl, ivec2& pos, ivec2& size, tick_t offset) {
		tick_t tickBegin = cl->time + offset;
		tick_t tickEnd = cl->time + offset + cl->len;
		double tickBeginX = grid.tickToScreenD(tickBegin);
		double tickEndX = grid.tickToScreenD(tickEnd);
		double width = tickEndX - tickBeginX;
		int32_t tickBeginPx = (int32_t) round(tickBeginX);
		int32_t widthPx = (int32_t) round(width);
		pos = ivec2(tickBeginPx, INSET_TRACK_CONTENT);
		size = ivec2(widthPx, size.y-INSET_TRACK_CONTENT*2);
	}
	void updatePosition(scaled_grid& grid) {
		size = this->parent->size;
		getClipPosition(grid, m_clip, pos, size, 0);
	}
	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
		assert(m_clip->tr);
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
		assert(m_clip->tr);
		evt.relMousepos += pos;
		parent->handleDraggedBegin(evt);
	}

	void handleDraggedMove(MouseEvent& evt) {
		assert(m_clip->tr);
		evt.relMousepos += pos;
		parent->handleDraggedMove(evt);
	}

	void handleDraggedRelease(MouseEvent& evt) {
		assert(m_clip->tr);
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
class gui_trackcontent : public guictr_base {
public:
	track_t* const m_track;
	gui_trackcontent(track_t* _track)
		: guictr_base(),
		m_track(_track) {
		padding = 0;
	}
	bool isStaticContainer() {
		return false;
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
		for (clip_t* clip : m_track->clips) {
			if(!clip->gClip) {
				continue;
			}
			clip->gClip->render(vg);
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
			}
		}
		return false;
	}
	bool handleKeyInput(KeyEvent& kevt) {
		return parent->handleKeyInput(kevt);
	}
	void handleRightClick(MouseEvent& evt);

	void updateVisibleTrackContents(scaled_grid& grid) {
		for (clip_t* clip : m_track->clips) {
//			gui_clip* gClip = clip->gClip;
			if(!clip->gClip) {
				clip->gClip = new gui_clip(clip);
				add(clip->gClip);
			}
			clip->gClip->updatePosition(grid);
		}
	}
	void handleDraggedBegin(MouseEvent& evt) {
		MainCtrl::get()->setSelectedTrack(m_track);
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
};


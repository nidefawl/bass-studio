#pragma once
#include <stdbool.h>
#include <glm/vec2.hpp>
#include <stdint.h>
#include <vector>
#include "seq_math.h"
#include "seq_util.h"
#include "color_util.h"
#include "track.h"
#include "clip.h"
#include "grid.h"
#include "guicontainer.h"
#include "trackautomation.h"
#include "leak_detect.h"

using glm::ivec2;

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



class gui_track_midi : public guictr_base {
protected:
	track_t* const m_track;
public:
	trackdata_midi_t& midi;
	gui_track_midi(track_t* _track)
		: guictr_base(), m_track(_track),
		midi(m_track->getMidi()) {
		padding = 0;
	}
	void render(NVGcontext* vg) {
//		if (MainCtrl::get()->getSelectedTrack() == m_track) {
//			nvgBeginPath(vg);
//			nvgRect(vg, pos.x, pos.y, size.x, size.y);
//			nvgFillColor(vg, g_guiColors[COL_BG_SELECTEDTRACK]);
//			nvgFill(vg);
//		}
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

class gui_track_automationlane : public guictr_base {
public:
	track_t* const m_track;
protected:
	gui_track_automation automation;
public:
	automatable_t* at;
	int32_t param;
	int32_t height = 4;
	int32_t idx = -1;
	gui_track_automationlane(track_t* _track, scaled_grid& _grid, automatable_t* _at, int32_t _param);
	virtual ~gui_track_automationlane() {

	}
	automation_t* getAutomation() {
		if (at) {
			return at->getAutomation(param);
		}
		return NULL;
	}
	void handleRightClick(MouseEvent& evt) override;
	virtual void updateVisibleTrackContents(scaled_grid& grid);
	bool isStaticContainer() {
		return false;
	}
	bool handleKeyInput(KeyEvent& kevt) override {
		return parent->handleKeyInput(kevt);
	}
	void handleDraggedBegin(MouseEvent& evt) override {
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


	virtual void render(NVGcontext* vg) override {
		if (MainCtrl::get()->getSelectedTrack() == m_track) {
			nvgBeginPath(vg);
			nvgRect(vg, pos.x, pos.y, size.x, size.y);
			nvgFillColor(vg, g_guiColors[COL_BG_SELECTEDTRACK]);
			nvgFill(vg);
		}
		nvgSave(vg);
		automation.render(vg);
		nvgRestore(vg);
	}
	virtual bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
		if (automation.mouseHitTest(mpos, evt)) {
			return true;
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
			}
			// tracks need to always cancel further mouse tests for z-order to work in parent container
			return true;
		}
		return false;
	}
	void positionChanged() {
		automation.parent = this->parent;
		automation.pos = this->pos;
		automation.size = this->size;
	}
	void layout() override {
		positionChanged();
		automation.layout();
	}
	void destroyGuis() override {
		automation.destroyGuis();
		guictr_base::destroyGuis();
	}
};
class gui_track : public guictr_base {
protected:
	track_t* const m_track;
	trackdata_midi_t& midi;
	gui_track_automation automation;
	int subtrackIdx = -1;
public:
	gui_track(track_t* _track, scaled_grid& _grid);
	virtual ~gui_track() {

	}
	void handleRightClick(MouseEvent& evt) override;
	virtual void updateVisibleTrackContents(scaled_grid& grid);
	bool isStaticContainer() {
		return false;
	}
	bool handleKeyInput(KeyEvent& kevt) override {
		return parent->handleKeyInput(kevt);
	}
	void handleDraggedBegin(MouseEvent& evt) override {
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


	virtual void render(NVGcontext* vg) override {
		if (MainCtrl::get()->getSelectedTrack() == m_track) {
			nvgBeginPath(vg);
			nvgRect(vg, pos.x, pos.y, size.x, size.y);
			nvgFillColor(vg, g_guiColors[COL_BG_SELECTEDTRACK]);
			nvgFill(vg);
		}
		nvgSave(vg);
		if (setScissorTransform(vg)) {
			for (clip_t* clip : midi.clips) {
				if(!clip->gClip) {
					continue;
				}
				clip->gClip->render(vg);
			}
		}
		nvgRestore(vg);
		nvgSave(vg);
		automation.render(vg);
		nvgRestore(vg);
	}
	virtual bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
		if (automation.mouseHitTest(mpos, evt)) {
			return true;
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
	void layout() override {
		positionChanged();
		automation.layout();
	}
	void positionChanged() {
		automation.parent = this->parent;
		automation.pos = this->pos;
		automation.size = this->size;
	}
	void destroyGuis() override {
		automation.destroyGuis();
		guictr_base::destroyGuis();
	}
};

//class gui_track_audiochain : public gui_track {
//public:
//	gui_track_audiochain(track_t* _track) : gui_track(_track) {
//
//	}
//};



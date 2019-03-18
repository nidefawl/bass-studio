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
#include "automation.h"
#include "automatable.h"
#include "trackautomation.h"
#include "audiowaveform.h"
#include "leak_detect.h"
#include "cliprenderer.h"
#include "logging.h"

#include <glm/glm.hpp>
#include <glm/vec2.hpp>
using glm::vec2;
using glm::ivec2;

struct gui_waveform_texture_ref;
class guictxtmenu_base;
class gui_clip : public guibase {
public:
	track_t* const m_track;
	clip_t* const m_clip;
	bool culled = true;
	gui_clip(track_t* _track, clip_t* _clip)
		: guibase(),
		  m_track(_track),
		  m_clip(_clip) {
		my_printf("gui_clip\n", 0);
	}
	virtual ~gui_clip() {
		my_printf("~gui_clip\n", 0);
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

	void trackViewDragBegin(guitrack_editor* view, MouseEvent& evt);
	void trackViewDragMove(guitrack_editor* view, MouseEvent& evt);
	void trackViewDragRelease(guitrack_editor* view, MouseEvent& evt);
	bool isDragMoveable() {
		return true;
	}
	virtual int getClipType() = 0;
	virtual void updatePosition(project_t& project, scaled_grid& grid, ivec2& trackSize) = 0;
};
class gui_midi_clip : public gui_clip {
public:
	gui_midi_clip(track_t* _track, clip_t* _clip)
		: gui_clip(_track, _clip)  {
	}
	int getClipType() {
		return CLIP_MIDI;
	}
	void updatePosition(project_t& project, scaled_grid& grid, ivec2& trackSize) {
		size = this->parent->size;
		culled = !getClipPosition(grid, trackSize, m_clip, pos, size, 0);
	}
	void render(NVGcontext* vg) {
		if (!culled) {
			renderMidiClip(vg, theme, m_track, m_clip, pos, size);
		}
	}
	void onRemove() {
		assert(m_clip->gClip == this);
		m_clip->gClip = NULL;
	}
	void handleRightClick(MouseEvent& evt);
};
class gui_audio_clip : public gui_clip {
	audioclip_texture_t updatedWaveform;
public:
	gui_audio_clip(track_t* _track, clip_t* _clip)
		: gui_clip(_track, _clip)  {
	}
	int getClipType() {
		return CLIP_AUDIO;
	}
	void updatePosition(project_t& project, scaled_grid& grid, ivec2& trackSize);
	void render(NVGcontext* vg) override {
		if (!culled) {
			ivec2 clipSize = ivec2(size.x, size.y-(HEIGHT_CLIP_TITLE+INSET_CLIP_CONTENT*2));
			ivec2 posClipped = ivec2(pos.x, pos.y+(HEIGHT_CLIP_TITLE+INSET_CLIP_CONTENT*2));
			ivec2 sizeClipped = clipSize;
			this->parent->scissorClip(posClipped, sizeClipped);
			sizeClipped.y = clipSize.y;
			renderAudioClip(vg, theme, m_track, m_clip, &m_clip->audio.waveformRef, pos, size, sizeClipped);
		}
	}
	void releaseRendered();
	void prerender(NVGcontext* vg) override;
	void onIdle() override;

	void onTick(AppCtrl* appctrl) override;
	guictxtmenu_base* getTooltip(AppCtrl* appctrl) override;
	void onRemove() {
//		my_printf("release %012x from onRemove()\n", &m_clip->audio.waveformRef);
		releaseRendered();
		assert(m_clip->gClip == this);
		m_clip->gClip = NULL;
	}
	void handleRightClick(MouseEvent& evt);
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
			nvgFillColor(vg, theme->getColor(GuiColor::COL_BG_SELECTEDTRACK));
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
		automation.setParent(this->parent);
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
	virtual void updateVisibleTrackContents(project_t& project, scaled_grid& grid);
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
			nvgFillColor(vg, theme->getColor(GuiColor::COL_BG_SELECTEDTRACK));
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
		automation.setParent(this->parent);
		automation.pos = this->pos;
		automation.size = this->size;
	}
	void destroyGuis() override {
		automation.destroyGuis();
		guictr_base::destroyGuis();
	}
	track_t* getTrack() {
		return this->m_track;
	}
};

//class gui_track_audiochain : public gui_track {
//public:
//	gui_track_audiochain(track_t* _track) : gui_track(_track) {
//
//	}
//};



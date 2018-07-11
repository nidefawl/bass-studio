
#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <glm/geometric.hpp>


#include "track.h"
#include "trackautomation.h"
#include "track_impl.h"
#include "../host/vst_plugin.h"

#include "event.h"
#include "button.h"
#include "dropdown.h"
#include "contextmenus.h"
#include "trackctr.h"
#include "trackcontent.h"
#include "audiocache.h"
#include "drawwaveform.h"
#include "samplerate.h"
#include "../host/vst_host.h"


#include "leak_detect.h"

using glm::vec2;
using glm::ivec2;

void gui_midi_clip::handleRightClick(MouseEvent& evt) {
	MainCtrl::get()->openContextMenu(new guictxtmenu_clip(this->m_clip), evt.mousepos);
}
void gui_audio_clip::handleRightClick(MouseEvent& evt) {
	MainCtrl::get()->openContextMenu(new guictxtmenu_clip(this->m_clip), evt.mousepos);
}
void gui_audio_clip::releaseRendered() {
	waveformrender::getInstance()->release(m_clip->audio.waveformRef.fbId);
	m_clip->audio.waveformRef.fbId = -1;
	m_clip->audio.waveformRef.rendered = false;
}

void gui_audio_clip::updatePosition(project_t& project, scaled_grid& grid, ivec2& trackSize) {
	size = this->parent->size;
	culled = !getClipPosition(grid, trackSize, m_clip, pos, size, 0);
	cachedaudio_t* audio = audiocache::getInstance()->get(m_clip->audio.id);
	if (culled || !audio) {
		releaseRendered();
	}
//test clipping
//	ivec2 prevSize = size;
//	ivec4 clippedP = ivec4(pos, size);
//	this->parent->scissorClip(clippedP);
//	pos.x = clippedP.x;
//	pos.y = clippedP.y;
//	size.x = clippedP.z;
//	size.y = clippedP.w;
//	if (size != prevSize) {
//		my_printf("%d %d -> %d %d\n", prevSize.x, prevSize.y, size.x, size.y);
//	}
	if (!culled) {
		assert(size.x > 0);
		if (audio) {
			ivec2 clipSize = ivec2(size.x, size.y-(HEIGHT_CLIP_TITLE+INSET_CLIP_CONTENT*2));
			ivec2 posClipped = pos;
			ivec2 sizeClipped = clipSize;
			this->parent->scissorClip(posClipped, sizeClipped);
			auto waveform = makeWaveformFromClip(project, grid, trackSize, m_clip, pos, clipSize, posClipped, sizeClipped);
			if (waveform != m_clip->audio.waveformRef.waveform) {
				releaseRendered();
				m_clip->audio.waveformRef.waveform = waveform;
			}
		}
	}
}
void gui_audio_clip::prerender(NVGcontext* vg) {
	if (!culled && !m_clip->audio.waveformRef.rendered) {
		cachedaudio_t* audio = audiocache::getInstance()->get(m_clip->audio.id);
		if (audio) {
			int ret = waveformrender::getInstance()->render(vg, audio, &m_clip->audio.waveformRef.waveform, 1);
			m_clip->audio.waveformRef.fbId = ret;
			m_clip->audio.waveformRef.rendered = true;
		}
	}
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

void gui_track::updateVisibleTrackContents(project_t& project, scaled_grid& grid) {
	automation.setData();
	automation.updateVisibleTrackContents(grid);
	for (clip_t* clip : midi.clips) {
		if (!clip->gClip) {
			if (clip->clipType == CLIP_MIDI) {
				clip->gClip = new gui_midi_clip(m_track, clip);
			} else {
				clip->gClip = new gui_audio_clip(m_track, clip);
			}
			add(clip->gClip);
		}
		clip->gClip->updatePosition(project, grid, size);
	}
}

void gui_track_automationlane::updateVisibleTrackContents(scaled_grid& grid) {
	automation.setData();
	automation.updateVisibleTrackContents(grid);
}

gui_track_automationlane::gui_track_automationlane(track_t* _track, scaled_grid& _grid, automatable_t* _at, int32_t _param)
  : guictr_base(), m_track(_track), automation(_track, _grid, at, param, idx), at(_at), param(_param)
{
	padding = 0;
}
gui_track::gui_track(track_t* _track, scaled_grid& _grid)
  : guictr_base(), m_track(_track), midi(_track->getMidi()), automation(_track, _grid, m_track->audio->selectedAutomationCtr, m_track->audio->selectedAutomationParam, subtrackIdx)
{
	padding = 0;
}
gui_track* createTrackGui(track_t* t, scaled_grid& grid) {
	return new gui_track(t, grid);
}



class guictxtmenu_trackcontent : public guictxtmenu_base {
public:
	int32_t trackid;
	guictxtmenu_trackcontent(int32_t _trackid) {
		this->trackid = _trackid;
		this->size.x = 320;
		MainCtrl* ctrl = MainCtrl::get();
		track_t* tr = ctrl->getTrackId(this->trackid);
		auto newClip = new ctxtmenu_entry("Create clip", 20);
		add(newClip);
		add(new ctxtmenu_splitter());
		scaled_grid& grid = MainCtrl::get()->getGrid();
		auto adaptive = new ctxtmenu_time_select(grid, "Adaptive Grid", 0);
		adaptive->initAdaptive();
		add(adaptive);
		auto fixed = new ctxtmenu_time_select(grid, "Fixed Grid", 0);
		fixed->initFixed();
		add(fixed);
	}
	void clicked(int _id) {
		MainCtrl* ctrl = MainCtrl::get();
		scaled_grid& grid = ctrl->getGrid();
		if (_id == 20) {
			Cursor cursor = MainCtrl::get()->cursor.getLeftAligned();
			if (cursor.selRange) {
				track_t* tr = ctrl->getTrackId(this->trackid);
				if (tr && tr->type == TRACK_TYPE_MIDI) {
					clip_t* cl = new clip_t(CLIP_MIDI, StringFormat("%s Clip", StringAsCStr(tr->name)));
					cl->time = cursor.cursorPos;
					cl->setLen(cursor.selRange);
					cl->loopStart = 0;
					cl->loopLen = cl->getLen();
					tr->getMidi().addClipSort(cl);
				}
				if (tr && tr->type == TRACK_TYPE_AUDIO) {
					clip_t* cl = new clip_t(CLIP_AUDIO, StringFormat("%s", StringAsCStr(tr->name)));
					cl->time = cursor.cursorPos;
					cl->setLen(cursor.selRange);
					cl->loopStart = 0;
					cl->loopLen = cl->getLen();
					tr->getMidi().addClipSort(cl);
				}
			}
		}
		else if (_id == 110+9) { // OFF
			grid.grid_dens.enabled = false;
		} else if (_id >= 110) {
			grid.grid_dens.enabled = true;
			grid.grid_dens.fixedBars = _id - 110;
			grid.grid_dens.isfixed = true;
		} else {
			grid.grid_dens.enabled = true;
			grid.grid_dens.dynamicDensity = _id - 100;
			grid.grid_dens.isfixed = false;
		}
//		ctrl->updateVisibleTrackContents();
		MainCtrl::get()->updateGrid();

		MainCtrl::get()->closeContextMenu();
	}
};
void gui_track_automationlane::handleRightClick(MouseEvent& evt) {
	MainCtrl::get()->openContextMenu(new guictxtmenu_trackcontent(this->m_track->idx), evt.mousepos);
}
void gui_track::handleRightClick(MouseEvent& evt) {
	MainCtrl::get()->openContextMenu(new guictxtmenu_trackcontent(this->m_track->idx), evt.mousepos);
}
void guitrack_editor::handleRightClick(MouseEvent& evt) {
	MainCtrl::get()->openContextMenu(new guictxtmenu_trackcontent(-1), evt.mousepos);
}

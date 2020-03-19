#pragma once
#include <stdint.h>
#include "guicontainer.h"
#include "track.h"
#include "trackctr_types.h"
#include "trackctr.h"
#include "../host/mainctrl.h"

class gui_track_subtrack_mixer;
class gui_trackcontrols_title;

class gui_track_controls: public guictr_base {
public:
	track_t* const m_track;
	track_gui_entry_t* const m_trackentry;
private:
	gui_trackcontrols_title* title;
	guictr_base* mixer;
	guictr_base* io;
	std::vector<gui_track_subtrack_mixer*> automationLaneControls;
	int dragMode = -1;
	const int resizeHitY = 8;
	const int DRAG_RESIZE = 1;
public:
	gui_track_controls(track_gui_entry_t& _entry);
	~gui_track_controls();
	bool isStaticContainer() {
		return false;
	}
	void addSubtrackMixer(track_gui_entry_t& entry, gui_track_subtrack* al);
	void removeSubtrackMixer(gui_track_subtrack* al);
	void removeAllAutomationLanes(automatable_t* at, int32_t paramIdx);
	void removeAllAutomationLanes(automatable_t* at);
	void removeAllSubtracks();
	void render(NVGcontext* vg) override;
	void renderGroupHandle(NVGcontext* vg);
	void handleDraggedBegin(MouseEvent& evt) {
		DawInstance::get()->setSelectedTrack(m_track);
		if (isResize(evt.relMousepos+this->pos)) {
			dragMode = DRAG_RESIZE;
		}
	}

	void handleDraggedMove(MouseEvent& evt);

	void handleDraggedRelease(MouseEvent& evt) {
		dragMode = -1;
	}
	void handleRightClick(MouseEvent& evt);
	bool isResize(ivec2 mpos) {
		int32_t resizeTopOrBottom = m_track->type < TRACK_TYPE_MIDI ? top() : bottom();
		return mpos.y >= resizeTopOrBottom - resizeHitY
				&& mpos.y < resizeTopOrBottom + resizeHitY;
	}

	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
	void layout() override;
};


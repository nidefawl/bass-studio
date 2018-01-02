#pragma once
#include <stdint.h>
#include "guicontainer.h"
#include "track.h"
#include "leak_detect.h"

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


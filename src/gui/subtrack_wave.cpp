#include "trackcontent.h"
#include "subtrack.h"
#include "basectrl.h"
#include "host/mainctrl.h"
#include "subtrack.h"

class gui_subtrack_waveview : public gui_track_subtrack {
public:
	gui_subtrack_waveview(track_t* mtrack, MainCtrl* ctrl) : gui_track_subtrack(mtrack, ctrl->getGrid(), nullptr, 0) {

	}
	void render(NVGcontext* vg) {
		if (MainCtrl::get()->getSelectedTrack() == m_track) {
		}
		nvgBeginPath(vg);
		nvgRect(vg, pos.x, pos.y, size.x, size.y);
		nvgFillColor(vg, rgbToNvg(0xff00ff));
		nvgFill(vg);
		nvgSave(vg);
		automation.render(vg);
		nvgRestore(vg);
	}
};

gui_track_subtrack* makeGuiSubtrack(MainCtrl* ctrl, track_t* track, int type) {

	return new gui_subtrack_waveview(track, ctrl);
}

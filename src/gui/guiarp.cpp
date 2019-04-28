#include "guiarp.h"
#include "automatable.h"
#include "guicontextmenu.h"
#include "../host/mainctrl.h"
#include "../threads/playbackthread.h"

class guictxtmenu_param: public guictxtmenu {
	midiarp* const effect;
	track_t* const m_track;
	automatable_param_t* const entry;
public:
	guictxtmenu_param(track_t* track, midiarp* _effect, automatable_param_t* _entry) :
			effect(_effect), m_track(track), entry(_entry) {
		this->size.x = 240;
		addContextEntriesAutomation(this, track, effect, entry->idx);
	}
	void clicked(int _id) {
		handleAutomatbleContextMenu(m_track, effect, entry->idx, _id);
		parentCtrl->closePopup();
	}
};
void gui_arp::buttonClicked(guibase* _button) {
	if (_button == &buttonBypass) {
		midiarp* arp = getArp();
		if (arp) {
	    	ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
	    	float f = arp->getParamValue(PARAM_ENABLE);
	    	float f2 = f > 0.5 ? 0 : 1;
	    	arp->setParamValue(PARAM_ENABLE, f2, 2);
	    	arp->postSetParameter(PARAM_ENABLE, f, f2, 2);
		}
	}
}
void gui_arp::rightClicked(MouseEvent& evt, guibase* button) {
	int32_t clickedParamIdx = -1;
	if (button == &this->buttonBypass) {
		clickedParamIdx = PARAM_ENABLE;
	}
	if (button == &this->clock) {
		clickedParamIdx = ARP_PARAM_CLOCK;
	}
	if (button == &this->gate) {
		clickedParamIdx = ARP_PARAM_GATE;
	}
	if (button == &this->pattern) {
		clickedParamIdx = ARP_PARAM_PATTERN;
	}
	if (clickedParamIdx != -1) {
		guictxtmenu_param* ctxt = new guictxtmenu_param(this->clipview.track(), this->getArp(), getArp()->getParam(clickedParamIdx));
		MainCtrl::get()->openContextMenu(ctxt, evt.mousepos);
	}

}

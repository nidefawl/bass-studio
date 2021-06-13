#include "guiarp.h"
#include "automatable.h"
#include "guicontextmenu.h"
#include "guicontextmenu_daw.h"
#include "../host/mainctrl.h"
#include "../threads/playbackthread.h"

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
	if (clickedParamIdx != -1) {
		auto* ctxt = new guictxtmenu_at_param(this->getArp(), clickedParamIdx);
		parentCtrl->openContextMenu(ctxt, evt.mousepos);
	}

}

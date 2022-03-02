#include "guiarp.h"
#include "automatable.h"
#include "guicontextmenu.h"
#include "guicontextmenu_daw.h"

void gui_arp::buttonClicked(guibase* _button) {
    if (_button == &buttonBypass) {
        midiarp* arp = getArp();
        if (arp) {
            ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
            toggleDeviceEnableState(arp, FLG_PAR_UPDATE_USER);
        }
    }
}
void gui_arp::rightClicked(MouseEvent& evt, guibase* button) {
    int32_t clickedParamIdx = -1;
    if (button == &this->buttonBypass) {
        clickedParamIdx = PARAM_ENABLE;
    }
    if (clickedParamIdx != -1) {
        auto* ctxt = new guictxtmenu_at_param(this->dawCtrl, this->getArp(), clickedParamIdx);
        parentCtrl->openContextMenu(ctxt, evt.mousepos);
    }
}

#include "arp.h"
#include "gui/automation/automatable.h"
#include "gui/contextmenu/contextmenu.h"
#include "gui/contextmenu/contextmenu_daw.h"
#include "seq_util.h"

void gui_arp::buttonClicked(guibase* _button) {
    if (_button == &buttonBypass) {
        auto* arp = getArp();
        if (arp) {
            ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
            toggleDeviceEnableState(arp, FLG_PAR_UPDATE_USER);
        }
    }
    if (stl_contains(knobs, _button)) {
        auto button = dynamic_cast<guiknob_arp*>(_button);
        auto* arp = getArp();
        if (button && arp) {
            auto paramIdx = button->getParamIdx();
            auto paramValue = arp->getParamValueDisplay(paramIdx);
            editfield.mCallbackEnd = [this, button, arpBegin = arp, paramValue, paramIdx](const std::string& str) {
                auto* arp = getArp();
                if (arp && arpBegin == arp) {
                    auto paramConverted = arp->convertParamValueDisplay(paramIdx, param_unit_t{str, paramValue.unit});
                    if (paramConverted.success) {
                        arp->setParamValue(paramIdx, paramConverted.floatVal, FLG_PAR_UPDATE_USER);
                        if (button->fnValueEditChanged)
                            button->fnValueEditChanged(button->getValue(), paramConverted.floatVal);
                    }
                    editfield.setVisible(false);
                }
                return true;
            };
            editfield.pos = button->getRightTop() + ivec2{0, button->size.y / 2};
            editfield.size = {getSizeContent().x - editfield.pos.x / 2, button->size.y / 2};
            editfield.setVisible(true);
            editfield.layout();
            editfield.setValue(paramValue.value);
            editfield.setSelectionRange(-1, -1);
            editfield.setFontSize(editfield.size.y * theme->getFloat(GuiConstant::CONST_FONT_SCALE));
            parentCtrl->focusGui(&editfield);
            return;
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

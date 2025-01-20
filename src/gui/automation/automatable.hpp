#pragma once
#include "math/vec.hpp"
#include "host/automation/automation.hpp"

class BaseCtrl;
class guictxtmenu;

namespace DAW {
    void AddContextEntriesAutomation(guictxtmenu* ctxt, automatable_t* atl, int paramIdx);
    void AddContextEntriesModulation(guictxtmenu* ctxt, automatable_t* atl, int paramIdx);
    void OpenModulationEditor(BaseCtrl* parentCtrl, ivec2 mousePos, automatable_t* atl, int32_t paramIdx);
    bool HandleAutomatableContextMenu(BaseCtrl* parentCtrl, automatable_t* at, int paramIdx, int _id);
}

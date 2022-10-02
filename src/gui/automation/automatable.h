#pragma once
#include "math/vec.h"
#include "automation.h"
class DawCtrl;
class guictxtmenu;
namespace DAW {
    void AddContextEntriesAutomation(guictxtmenu* ctxt, automatable_t* atl, int paramIdx);
    void AddContextEntriesModulation(guictxtmenu* ctxt, automatable_t* atl, int paramIdx);
    void OpenModulationEditor(DawCtrl* dawCtrl, ivec2 mousePos, automatable_t* atl, int32_t paramIdx);
    bool HandleAutomatableContextMenu(DawCtrl* _dawCtrl, automatable_t* at, int paramIdx, int _id);
}
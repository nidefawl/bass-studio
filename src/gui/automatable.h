#pragma once
#include "../host/mainctrl.h"
#include "automation.h"
#include "guicontextmenu.h"

void addContextEntriesAutomation(guictxtmenu* ctxt, automatable_t* atl, int paramIdx);
bool handleAutomatableContextMenu(DawCtrl* _dawCtrl, automatable_t* at, int paramIdx, int _id);

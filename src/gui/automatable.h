#pragma once
#include "../host/mainctrl.h"
#include "automation.h"
#include "guicontextmenu.h"

void addContextEntriesAutomation(guictxtmenu* ctxt, automatable_t* atl, int paramIdx);
bool handleAutomatbleContextMenu(automatable_t* at, int paramIdx, int _id);


#pragma once
#include "../host/mainctrl.h"
#include "automation.h"
#include "guicontextmenu.h"

void addContextEntries(guictxtmenu_base* ctxt, track_t* tr, automatable_t* atl, int paramIdx);
bool handleAutomatbleContextMenu(track_t* tr, automatable_t* at, int paramIdx, int _id);

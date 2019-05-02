#include "automation.h"
#include "automatable.h"
#include "../host/plugin/base_plugin.h"
#include "guicontextmenu_daw.h"

guictxtmenu_at_param::guictxtmenu_at_param(automatable_t* _atl, int32_t _paramIdx)
	: atl(_atl), paramIdx(_paramIdx)
{
	this->size.x = 240;
	addContextEntriesAutomation(this, _atl, paramIdx);
}
void guictxtmenu_at_param::clicked(int _id) {
	handleAutomatbleContextMenu(atl, paramIdx, _id);
	closeContextMenu();
}


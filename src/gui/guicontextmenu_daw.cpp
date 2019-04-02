#include "automation.h"
#include "automatable.h"
#include "../host/plugin/base_plugin.h"
#include "guicontextmenu_daw.h"

guictxtmenu_vstparam::guictxtmenu_vstparam(effectbase* _effect, automatable_param_t* _entry) : effect(_effect), entry(_entry)
{
	this->size.x = 240;
	addContextEntriesAutomation(this, effect->getTrack(), effect, entry->idx);
}
void guictxtmenu_vstparam::clicked(int _id) {
	handleAutomatbleContextMenu(effect->getTrack(), effect, entry->idx, _id);
	closeContextMenu();
}

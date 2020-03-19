#include "automatable.h"
#include "../host/mainctrl.h"
#include "automation.h"
#include "track.h"
#include "trackctr.h"
#include "trackcontent.h"
#include "guicontextmenu_base.h"
#include "guicontextmenu.h"

#define ID_DELETE 1
#define ID_REENABLE 2
#define ID_SHOW 3
#define ID_SHOW_NEW 4
void addContextEntriesAutomation(guictxtmenu* ctxt, automatable_t* atl, int paramIdx) {

	auto* track = atl->getTrack();
	if (track) {
		MainCtrl::get()->showAutomation(track, atl, paramIdx);
	}
	const automation_t* at = atl->getRegisteredAutomation(paramIdx);
	if (at && at->isAutomated()) {
		if (!at->active) {
			ctxt->addEntry(new ctxtmenu_entry("Reenable Automation", ID_REENABLE));
		}
		ctxt->addEntry(new ctxtmenu_entry("Delete Automation", ID_DELETE));
	}
	ctxt->addEntry(new ctxtmenu_entry("Show Automation", ID_SHOW));
	ctxt->addEntry(new ctxtmenu_entry("Show in new Automation Lane", ID_SHOW_NEW));
}
bool handleAutomatbleContextMenu(automatable_t* atl, int paramIdx, int _id) {
	auto* track = atl->getTrack();
	dbgassert(track);
	automation_t* param = atl->getRegisteredAutomation(paramIdx);
	auto* guiTrackCtr = MainCtrl::getGuiTrackCtr();
	switch (_id) {
		case ID_SHOW_NEW: {
			track_gui_entry_t entry;
			dbgassert(guiTrackCtr->getTrackEntry(track, entry));
			gui_track_automationlane* lane = MainCtrl::getGuiTrackCtr()->addAutomationLane(entry, atl, paramIdx, true);
			guiTrackCtr->layout();
			guiTrackCtr->updateVisibleTrackContents();
			guiTrackCtr->scrollTo(lane);
			return true;
		}
		case ID_SHOW: {
			MainCtrl::get()->showAutomation(track, atl, paramIdx);
			track_gui_entry_t entry;
			if (guiTrackCtr->getTrackEntry(track, entry)) {
				guiTrackCtr->scrollTo(entry.content);
			}
			return true;
		}
		case ID_DELETE: {
			if (param) {
				param->points.clear();
				MainCtrl::getGuiTrackCtr()->updateVisibleTrackContents();
			}
			return true;
		}
		case ID_REENABLE: {
			if (param && !param->isActive()) {
				MainCtrl::get()->showAutomation(track, atl, paramIdx);
				param->active=true;
			}
			return true;
		}
	}
	return false;
}

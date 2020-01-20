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
	switch (_id) {
		case ID_SHOW_NEW: {
			gui_track_automationlane* lane = MainCtrl::getGuiTrackCtr()->addAutomationLane(track, atl, paramIdx, true);
			MainCtrl::getGuiTrackCtr()->layout();
			MainCtrl::getGuiTrackCtr()->updateVisibleTrackContents();
			MainCtrl::getGuiTrackCtr()->scrollTo(lane);
			return true;
		}
		case ID_SHOW: {
			MainCtrl::get()->showAutomation(track, atl, paramIdx);
			MainCtrl::getGuiTrackCtr()->scrollTo(track->content);
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

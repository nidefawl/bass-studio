#include "automatable.h"
#include "../host/mainctrl.h"
#include "automation.h"
#include "track.h"
#include "trackctr.h"
#include "trackcontent.h"
#include "guicontextmenu.h"
#include "leak_detect.h"

#define ID_DELETE 1
#define ID_REENABLE 2
#define ID_SHOW 3
#define ID_SHOW_NEW 4
void addContextEntriesAutomation(guictxtmenu* ctxt, track_t* tr, automatable_t* atl, int paramIdx) {

	MainCtrl::get()->showAutomation(tr, atl, paramIdx);
	automation_t* at = atl->getAutomation(paramIdx);
	if (at && at->isAutomated()) {
		if (!at->active) {
			ctxt->addEntry(new ctxtmenu_entry("Reenable Automation", ID_REENABLE));
		}
		ctxt->addEntry(new ctxtmenu_entry("Delete Automation", ID_DELETE));
	}
	ctxt->addEntry(new ctxtmenu_entry("Show Automation", ID_SHOW));
	ctxt->addEntry(new ctxtmenu_entry("Show in new Automation Lane", ID_SHOW_NEW));
}
bool handleAutomatbleContextMenu(track_t* tr, automatable_t* at, int paramIdx, int _id) {
	automation_t* param = at->getAutomation(paramIdx);
	switch (_id) {
		case ID_SHOW_NEW: {
			gui_track_automationlane* lane = MainCtrl::getGuiTrackCtr()->addAutomationLane(tr, at, paramIdx, true);
			MainCtrl::getGuiTrackCtr()->layout();
			MainCtrl::getGuiTrackCtr()->updateVisibleTrackContents();
			MainCtrl::getGuiTrackCtr()->scrollTo(lane);
			return true;
		}
		case ID_SHOW: {
			MainCtrl::get()->showAutomation(tr, at, paramIdx);
			MainCtrl::getGuiTrackCtr()->scrollTo(tr->content);
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
				MainCtrl::get()->showAutomation(tr, at, paramIdx);
				param->active=true;
			}
			return true;
		}
	}
	return false;
}

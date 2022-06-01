#include "automatable.h"
#include "host/mainctrl.h"
#include "automation.h"
#include "track.h"
#include "gui/track/trackctr.h"
#include "gui/track/trackcontent.h"
#include "gui/contextmenu/contextmenu_base.h"
#include "gui/contextmenu/contextmenu.h"

static constexpr int32_t ID_DELETE = 1;
static constexpr int32_t ID_REENABLE = 2;
static constexpr int32_t ID_SHOW = 3;
static constexpr int32_t ID_SHOW_NEW = 4;
static constexpr int32_t ID_RESET_TO_DEFAULT = 5;
void addContextEntriesAutomation(guictxtmenu* ctxt, automatable_t* atl, int paramIdx) {
    const automation_t* at = atl->getRegisteredAutomation(paramIdx);
    if (at && at->isAutomated()) {
        if (!at->active) {
            ctxt->addEntry(new ctxtmenu_entry("Reenable Automation", ID_REENABLE));
        }
        ctxt->addEntry(new ctxtmenu_entry("Delete Automation", ID_DELETE));
    }
    ctxt->addEntry(new ctxtmenu_entry("Show Automation", ID_SHOW));
    ctxt->addEntry(new ctxtmenu_entry("Show in new Automation Lane", ID_SHOW_NEW));
    ctxt->addEntry(new ctxtmenu_entry("Reset to default", ID_RESET_TO_DEFAULT));
}
bool handleAutomatableContextMenu(DawCtrl* dawCtrl, automatable_t* atl, int paramIdx, int _id) {
    auto* track = atl->getTrack();
    dbgassert(track);
    auto* guiTrackCtr   = dawCtrl->getTrackContainer();
    track_gui_entry_t* entry{};
    if(!guiTrackCtr->getPointerEntry(track, &entry))
        return false;
    switch (_id) {
        case ID_SHOW_NEW: {
            auto const lane = guiTrackCtr->addAutomationLane(entry, atl, paramIdx, true);
            dawCtrl->updateVisibleTrackContents();
            guiTrackCtr->scrollTo(lane);
            return true;
        }
        case ID_SHOW: {
            guiTrackCtr->showAutomationLane(entry, atl, paramIdx);
            dawCtrl->updateVisibleTrackContents();
            guiTrackCtr->scrollTo(entry->content);
            return true;
        }
        case ID_DELETE: {
            automation_t* param = atl->getRegisteredAutomation(paramIdx);
            if (param) {
                param->points.clear();
                dawCtrl->updateVisibleTrackContents();
            }
            return true;
        }
        case ID_REENABLE: {
            automation_t* param = atl->getRegisteredAutomation(paramIdx);
            if (param && !param->isActive()) {
                guiTrackCtr->showAutomationLane(entry, atl, paramIdx);
                dawCtrl->updateVisibleTrackContents();
                param->active = true;
            }
            return true;
        }
        case ID_RESET_TO_DEFAULT: {
            atl->resetParamValue(paramIdx, FLG_PAR_UPDATE_USER);
            return true;
        }
        default:
            break;
    }
    return false;
}

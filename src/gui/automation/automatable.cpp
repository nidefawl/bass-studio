#include "automatable.h"
#include "host/mainctrl.h"
#include "automation.h"
#include "track.h"
#include "gui/track/trackctr.h"
#include "gui/track/trackcontent.h"
#include "gui/contextmenu/contextmenu_base.h"
#include "gui/contextmenu/contextmenu.h"
#include "gui/contextmenu/contextmenu_daw.h"
#include "track_impl.h"

static constexpr int32_t ID_DELETE = 1;
static constexpr int32_t ID_REENABLE = 2;
static constexpr int32_t ID_SHOW = 3;
static constexpr int32_t ID_SHOW_NEW = 4;
static constexpr int32_t ID_RESET_TO_DEFAULT = 5;
static constexpr int32_t ID_REMOVE_MODULATION = 6;
static constexpr int32_t ID_MENU_MODULATION = 7;
void addContextEntriesAutomation(guictxtmenu* ctxt, automatable_t* atl, int paramIdx) {
    const auto* at = atl->getRegisteredAutomation(paramIdx);
    if (at && at->isAutomated()) {
        if (!at->isActive()) {
            ctxt->addEntry(new ctxtmenu_entry("Reenable Automation", ID_REENABLE));
        }
        ctxt->addEntry(new ctxtmenu_entry("Delete Automation", ID_DELETE));
    }
    ctxt->addEntry(new ctxtmenu_entry("Show Automation", ID_SHOW));
    ctxt->addEntry(new ctxtmenu_entry("Show in new Automation Lane", ID_SHOW_NEW));
    ctxt->addEntry(new ctxtmenu_entry("Reset to default", ID_RESET_TO_DEFAULT));
    
}
void addContextEntriesModulation(guictxtmenu* ctxt, automatable_t* atl, int paramIdx) {
    if (DAW::IsParamModulated(atl, paramIdx)) {
        ctxt->addEntry(new ctxtmenu_entry("Remove Modulation", ID_REMOVE_MODULATION));
    }
    // ctxt->addEntry(new ctxtmenu_entry("Modulation", ID_MENU_MODULATION));
}
class guictxtmenu_select_modulation : public guictxtmenu {
    automatable_t* const atl;
    int32_t const paramIdx;

public:
    guictxtmenu_select_modulation(DawCtrl* _dawCtrl, automatable_t* _atl, int32_t _paramIdx)
        : atl(_atl), paramIdx(_paramIdx)
    {
        this->dawCtrl = _dawCtrl;
        this->size.x = 240;
        addEntry(new ctxtmenu_entry("JA_UHM", -1));
    }
    void clicked(int _id) override {
        handleAutomatableContextMenu(dawCtrl, atl, paramIdx, _id);
        closeContextMenu();
    }
};

guictxtmenu* guictxtmenu_at_param::createPopupForEntry(ctxtmenu_entry* e, int lvl) {
    guictxtmenu* popup = nullptr;
    if (e->id == ID_MENU_MODULATION) {
        popup = new guictxtmenu_select_modulation(dawCtrl, atl, paramIdx);
    }
    return popup;
}

bool handleAutomatableContextMenu(DawCtrl* dawCtrl, automatable_t* atl, int paramIdx, int _id) {
    switch (_id) {
        case ID_REMOVE_MODULATION: {
            DAW::DisonnectModulationInputChannel(atl, paramIdx);
            return true;
        }
    }
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
            auto* param = atl->getRegisteredAutomation(paramIdx);
            if (param) {
                param->src.points.clear();
                dawCtrl->updateVisibleTrackContents();
            }
            return true;
        }
        case ID_REENABLE: {
            auto* param = atl->getRegisteredAutomation(paramIdx);
            if (param && !param->isActive()) {
                guiTrackCtr->showAutomationLane(entry, atl, paramIdx);
                dawCtrl->updateVisibleTrackContents();
                param->src.active = true;
            }
            return true;
        }
        case ID_RESET_TO_DEFAULT: {
            atl->resetParamValue(paramIdx, FLG_PAR_UPDATE_USER | FLG_PAR_UPDATE_FINISH);
            return true;
        }
        default:
            break;
    }
    return false;
}

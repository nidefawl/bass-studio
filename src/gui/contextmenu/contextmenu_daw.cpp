#include "automation.h"
#include "contextmenu_daw.h"
#include "gui/automation/automatable.h"
#include "gui/contextmenu/contextmenu_base.h"
#include "gui/contextmenu/contextmenu_daw.h"
#include "gui/contextmenu/contextmenu.h"
#include "gui/track/trackcontent.h"
#include "gui/track/trackctr.h"
#include "host/host_pluginmanager.h"
#include "host/mainctrl.h"
#include "host/plugin/base_plugin.h"
#include "logging.h"
#include "math/vec.h"
#include "snapshot/track-snapshot.h"
#include "str_util.h"
#include "str_util.h"
#include "track_impl.h"
#include "track.h"
#include <cstdint>


guictxtmenu_at_param::guictxtmenu_at_param(DawCtrl* _dawCtrl, automatable_t* _atl, int32_t _paramIdx)
    : atl(_atl), paramIdx(_paramIdx) {
    this->dawCtrl = _dawCtrl;
    this->size.x = 240;
    DAW::AddContextEntriesAutomation(this, _atl, paramIdx);
    DAW::AddContextEntriesModulation(this, _atl, paramIdx);
}
void guictxtmenu_at_param::clicked(int _id) {
    DAW::HandleAutomatableContextMenu(dawCtrl, atl, paramIdx, _id);
}

void guictxtmenu_notrack::clicked(int _id) {
    auto daw = DawInstance::get();
    auto window = parentCtrl->window;
    // promptUserFilePath initiates a native dialog that would close this context menu
    // so we do it ourself controlled here
    closeContextMenu();// deletes this
    // now we make sure not to access heap (this) after this point
    if (_id >= idxImport) {
        String path;
        if (promptUserFilePath(window, 0, vFILE_TYPES_TRACKSNAPSHOT, path)) {
            std::shared_ptr<trackcontainer_snapshot_t> ctr = loadTrackContainer(path);
            dbgassert(ctr);
            if (ctr) {
                auto* pluginMgr = daw->getPluginManager();
                ThreadLock lock = daw->getPlayThread()->lockThread();
                for (track_snapshot_t& ts : ctr->tracks) {
                    ts.trackLoaded = new track_t(ts);
                    daw->addTrackImpl(-1, ts.trackLoaded, 0);
                }

                //load plugins
                for (track_snapshot_t& ts : ctr->tracks) {
                    log_printf("track '%s' loading %zu plugins\n", StringAsCStr(ts.trackLoaded->name), ts.data.pluginSnapshots.size());
                    DAW::assignFreeStageIdsTrackSnapshot(pluginMgr, ts);
                    ts.trackLoaded->loadSnapshot(ts);
                    std::vector<effectbase*> effects = ts.trackLoaded->audio->deferredEffects;
                    for (auto effect: effects) {
                        pluginMgr->activateDeferred(effect, DAW::Host::PluginManager::FLAG_HOST_UNLOAD_PLUGIN_NO_NOTIFY);
                    }
                }
                for (track_snapshot_t& ts: ctr->tracks) {
                    ts.trackLoaded->getStage()->pluginsChanged();
                }
                daw->onPluginsChanged();
                daw->updateVisibleTrackContents();
            }
        }
    } else {
        daw->insertNewTrack(-1, _id);
    }
}
namespace DAW {
static constexpr int32_t ID_DELETE = 1;
static constexpr int32_t ID_REENABLE = 2;
static constexpr int32_t ID_SHOW = 3;
static constexpr int32_t ID_SHOW_NEW = 4;
static constexpr int32_t ID_RESET_TO_DEFAULT = 5;
static constexpr int32_t ID_REMOVE_PARAM_MODULATION = 6;
static constexpr int32_t ID_EDIT_PARAM_MODULATION = 7;
void AddContextEntriesAutomation(guictxtmenu* ctxt, automatable_t* atl, int paramIdx) {
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
void AddContextEntriesModulation(guictxtmenu* ctxt, automatable_t* atl, int paramIdx) {
    if (DAW::IsParamModulated(atl, paramIdx)) {
        ctxt->addEntry(new ctxtmenu_entry("Remove Modulation", ID_REMOVE_PARAM_MODULATION));
    }
    ctxt->addEntry(new ctxtmenu_entry("Edit Modulation", ID_EDIT_PARAM_MODULATION));
}
}
class guictxtmenu_select_modulation : public guictxtmenu {
    automatable_t* const atl;
    int32_t const paramIdx;

    class ctxtmenu_modulation_entry : public ctxtmenu_entry {
        const DAW::modulation_channel_ref ref;
    public:
        ctxtmenu_modulation_entry(String _title, int _id, DAW::modulation_channel_ref ref)
            : ctxtmenu_entry(std::move(_title), _id), ref(ref)
        {
        }
        DAW::modulation_channel_ref getRef() const {
            return ref;
        }
    };
public:
    guictxtmenu_select_modulation(DawCtrl* _dawCtrl, automatable_t* _atl, int32_t _paramIdx)
        : atl(_atl), paramIdx(_paramIdx)
    {
        (void) paramIdx;
        this->dawCtrl = _dawCtrl;
        this->size.x = 240;
        int32_t inputIdx = 0;
        if (atl->isParamModulated(_paramIdx)) {
            auto& mods = _atl->getModulations(_paramIdx);
            for (auto& mod : mods) {
                auto modChannel = DAW::ResolveModulationChannel(_dawCtrl->getDaw()->getPluginManager(), *mod);
                auto name = StringFormat("%d", inputIdx);
                if (modChannel) {
                    name = modChannel->getName();
                }
                addEntry(new ctxtmenu_modulation_entry(name, inputIdx, *mod));
                inputIdx++;
            }
        }
    }
    void clicked(int _id) override {
        if (_id >= 0) {
            auto ref = static_cast<ctxtmenu_modulation_entry*>(entries[_id])->getRef();
            DAW::DisonnectModulationInputChannel(atl, ref);
        }
        closeContextMenu();
    }
};

guictxtmenu* guictxtmenu_at_param::createPopupForEntry(ctxtmenu_entry* e, int lvl) {
    guictxtmenu* popup = nullptr;
    if (e->id == DAW::ID_REMOVE_PARAM_MODULATION) {
        popup = new guictxtmenu_select_modulation(dawCtrl, atl, paramIdx);
    }
    return popup;
}
namespace DAW {
    bool HandleAutomatableContextMenu(DawCtrl* dawCtrl, automatable_t* atl, int paramIdx, int _id) {
        switch (_id) {
            case ID_EDIT_PARAM_MODULATION: {
                dawCtrl->closeContextMenu();
                const auto& inputs = atl->getModulations();
                if (!inputs.empty())
                    DAW::OpenModulationEditor(dawCtrl, dawCtrl->lastMouseEvent.mousepos, atl, paramIdx, inputs.front());
                return true;
        
            }
            case ID_REMOVE_PARAM_MODULATION: {
                DAW::DisonnectModulationForParam(atl, paramIdx);
                dawCtrl->closeContextMenu();
                return true;
            }
        }

        auto* track = atl->getTrack();
        dbgassert(track);
        auto* guiTrackCtr   = dawCtrl->getTrackContainer();
        track_gui_entry_t* entry{};
        if(!guiTrackCtr->getPointerEntry(track, &entry))
            return false;
        dawCtrl->closeContextMenu();
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

}
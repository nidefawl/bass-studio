#include "automation.h"
#include "contextmenu_daw.h"
#include "gui/automation/automatable.h"
#include "gui/contextmenu/contextmenu_base.h"
#include "gui/contextmenu/contextmenu_daw.h"
#include "gui/contextmenu/contextmenu_grid.h"
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
bool guictxtmenu_at_param::clickedElement(ctxtmenu_entry* e, int _id) {
    DAW::HandleAutomatableContextMenu(dawCtrl, atl, paramIdx, _id);
    return true;
}

bool guictxtmenu_notrack::clickedElement(ctxtmenu_entry* e, int _id) {
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
    return true;
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
            const auto pModulations = _atl->getModulations(_paramIdx);
            for (auto& mod : *pModulations) {
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
    bool clickedElement(ctxtmenu_entry* e, int _id) override {
        if (_id >= 0) {
            closeContextMenu();
            auto lock = dawCtrl->lockPlayThread();
            auto ref = static_cast<ctxtmenu_modulation_entry*>(entries[_id])->getRef();
            DAW::DisonnectModulationInputChannel(atl, ref);
            return true;
        }
        closeContextMenu();
        return true;
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
                DAW::OpenModulationEditor(dawCtrl, dawCtrl->lastMouseEvent.mousepos, atl, paramIdx);
                return true;
        
            }
            case ID_REMOVE_PARAM_MODULATION: {
                {
                    dawCtrl->closeContextMenu();
                    auto lock = dawCtrl->lockPlayThread();
                    DAW::DisonnectModulationForParam(atl, paramIdx);
                }
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
                    param->src.activate();
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

}// namespace DAW


guictxtmenu_track_editor::guictxtmenu_track_editor(DawCtrl* const _dawCtrl, track_gui_entry_t* const _trackentry, gui_clip* optionalContextClip)
    : guictxtmenu(), m_trackentry(_trackentry) {
    this->size.x = 220;
    this->dawCtrl = _dawCtrl;
    this->maxHeight = 0;
    auto& cursor = _dawCtrl->getCursor();
    bool bHasContentSelected = optionalContextClip != nullptr;
    if (!bHasContentSelected) {
        bHasContentSelected = !DAW::isSelectionEmpty(m_trackentry->parent->guiMgr, cursor, true);
    }
    if (bHasContentSelected) {
        addEntry(new ctxtmenu_entry(_dawCtrl, GlobalCommandType::CMD_CONSOLIDATE));
        addEntry(new ctxtmenu_entry(_dawCtrl, GlobalCommandType::CMD_MUTE));
    } else {
        addEntry(new ctxtmenu_entry(_dawCtrl, GlobalCommandType::CMD_CREATE_EMPTY_CLIP));
    }
    addEntry(new ctxtmenu_splitter());
    if (cursor.getRange()) {
        addEntry(new ctxtmenu_entry(_dawCtrl, GlobalCommandType::CMD_CUT));
        addEntry(new ctxtmenu_entry(_dawCtrl, GlobalCommandType::CMD_COPY));
    }
    addEntry(new ctxtmenu_entry(_dawCtrl, GlobalCommandType::CMD_PASTE));
    addEntry(new ctxtmenu_entry(_dawCtrl, GlobalCommandType::CMD_PASTE_NO_AUTOMATION));
    if (optionalContextClip || cursor.getRange()) {
        addEntry(new ctxtmenu_entry(_dawCtrl, GlobalCommandType::CMD_DELETE));
        addEntry(new ctxtmenu_splitter());
        sel = new ctxtmenu_color_select("Pick Color", 100);
        addEntry(sel);
    }
    addEntry(new ctxtmenu_splitter());
    scaled_grid& grid = _dawCtrl->getGrid();
    auto adaptive     = new ctxtmenu_time_select(grid, "Adaptive Grid", 0);
    adaptive->initAdaptive();
    addEntry(adaptive);
    auto fixed = new ctxtmenu_time_select(grid, "Fixed Grid", 0);
    fixed->initFixed();
    addEntry(fixed);
}

bool guictxtmenu_track_editor::clickedElement(ctxtmenu_entry* e, int _id) {
    if (e->commandtype != GlobalCommandType::CMD_NONE) {
        dbgassert(m_trackentry->parent);
        KeyEvent evt{};
        m_trackentry->parent->trackView.handleEditorCommand(evt, e->commandtype);
        closeContextMenu();
        return true;
    }
    return false;
}

guictxtmenu_clip::guictxtmenu_clip(DawCtrl* const _dawCtrl, gui_clip* const _gclip) : guictxtmenu_track_editor(_dawCtrl, _gclip->m_trackentry, _gclip), m_gclip(_gclip) {
}

bool guictxtmenu_clip::clickedElement(ctxtmenu_entry* e, int _id) {
    if (guictxtmenu_track_editor::clickedElement(e, _id)) {
        return true;
    }
    if (_id >= sel->id) {
        _id -= sel->id;
        if (_id < COLOR_PALETTE_LEN) {
            if (m_gclip->m_clip) {
                m_gclip->m_clip->rgb = colorPalette[_id];
            }
        }
    }
    closeContextMenu();
    return true;
}
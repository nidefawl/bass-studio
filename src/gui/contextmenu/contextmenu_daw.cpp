#include "automation.h"
#include "commands.h"
#include "contextmenu_daw.h"
#include "event.h"
#include "gui/automation/automatable.h"
#include "gui/contextmenu/contextmenu_base.h"
#include "gui/contextmenu/contextmenu_daw.h"
#include "gui/contextmenu/contextmenu_grid.h"
#include "gui/contextmenu/contextmenu.h"
#include "gui/properties/properties_table.h"
#include "gui/track/trackcontent.h"
#include "gui/track/trackctr.h"
#include "gui/plugin/pluginctr.h"
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
    : guictxtmenu(), m_trackentry(_trackentry), m_gclip(optionalContextClip) {
    this->size.x = 260;
    this->dawCtrl = _dawCtrl;
    this->maxHeight = 0;
    auto& cursor = _dawCtrl->getCursor();
    auto clipboardType = _dawCtrl->getDaw()->getClipboardType();
    bool bHasContentSelected = optionalContextClip != nullptr;
    if (!bHasContentSelected && m_trackentry && m_trackentry->parent) {
        bHasContentSelected = !DAW::isSelectionEmpty(m_trackentry->parent->guiMgr, cursor, true);
    }
    if (bHasContentSelected) {
        addEntry(new ctxtmenu_entry(_dawCtrl, GlobalCommandType::CMD_CONSOLIDATE));
        addEntry(new ctxtmenu_entry(_dawCtrl, GlobalCommandType::CMD_MUTE));
        addEntry(new ctxtmenu_entry(_dawCtrl, GlobalCommandType::CMD_BEGIN_RENAME));
    } else {
        addEntry(new ctxtmenu_entry(_dawCtrl, GlobalCommandType::CMD_CREATE_EMPTY_CLIP));
        entries.back()->setGrayedOut(cursor.getRange() < 2);
    }
    addEntry(new ctxtmenu_splitter());
    addEntry(new ctxtmenu_entry(_dawCtrl, GlobalCommandType::CMD_CUT));
    entries.back()->setGrayedOut(!bHasContentSelected);
    addEntry(new ctxtmenu_entry(_dawCtrl, GlobalCommandType::CMD_COPY));
    entries.back()->setGrayedOut(!bHasContentSelected);
    addEntry(new ctxtmenu_entry(_dawCtrl, GlobalCommandType::CMD_PASTE));
    entries.back()->setGrayedOut(clipboardType != ClipBoardType::CLIPBOARD_CLIPS);
    addEntry(new ctxtmenu_entry(_dawCtrl, GlobalCommandType::CMD_PASTE_NO_AUTOMATION));
    entries.back()->setGrayedOut(clipboardType != ClipBoardType::CLIPBOARD_CLIPS);
    addEntry(new ctxtmenu_entry(_dawCtrl, GlobalCommandType::CMD_DELETE));
    entries.back()->setGrayedOut(!bHasContentSelected);
    if (bHasContentSelected) {
        addEntry(new ctxtmenu_splitter());
        sel = new ctxtmenu_color_select("Pick Color", 100);
        addEntry(sel);
    }
    addEntry(new ctxtmenu_splitter());
    scaled_grid& grid = _dawCtrl->getGrid();
    timeSel1     = new ctxtmenu_time_select(grid, "Adaptive Grid", 0);
    timeSel1->initAdaptive();
    addEntry(timeSel1);
    timeSel2 = new ctxtmenu_time_select(grid, "Fixed Grid", 0);
    timeSel2->initFixed();
    addEntry(timeSel2);
}


bool guictxtmenu::clickedElement(ctxtmenu_entry* e, int _id) {
    if (dawCtrl && e->commandtype != GlobalCommandType::CMD_NONE) {
        auto temp = DAW::UI::CommandContext{e->commandtype};
        if (dawCtrl->handleGlobalCommand(temp)) {
            closeContextMenu();
            return true;
        }
    }
    return false;
}

bool guictxtmenu_track_editor::clickedElement(ctxtmenu_entry* e, int _id) {
    scaled_grid& grid = dawCtrl->getGrid();
    if (e == this->timeSel1 || e == this->timeSel2) {
        if (_id == 110 + 9) {// OFF
            grid.grid_dens.enabled = false;
        } else if (_id >= 110) {
            grid.grid_dens.enabled   = true;
            grid.grid_dens.fixedBars = _id - 110;
            grid.grid_dens.isfixed   = true;
        } else {
            grid.grid_dens.enabled        = true;
            grid.grid_dens.dynamicDensity = _id - 100;
            grid.grid_dens.isfixed        = false;
        }
        grid.notifyChange();
    } else {
        if (dawCtrl) {
            auto cmd = DAW::UI::CommandContext{e->commandtype};
             if (e == this->sel) {
                _id -= sel->id;
                if (_id < COLOR_PALETTE_LEN) {
                    cmd = DAW::UI::CommandContext{GlobalCommandType::CMD_SET_COLOR};
                    cmd.argInt = colorPalette[_id];
                    m_trackentry->parent->handleEditorCommand(cmd);
                }
            }
            if (cmd.type != GlobalCommandType::CMD_NONE) {
                if (m_trackentry) {
                    m_trackentry->parent->handleEditorCommand(cmd);
                    closeContextMenu();
                    return true;
                }
                if (dawCtrl->handleGlobalCommand(cmd)) {
                    closeContextMenu();
                    return true;
                }
            }
        }
    }
    dawCtrl->getDaw()->updateVisibleTrackContents();
    closeContextMenu();
    return true;
}

guictxtmenu_clip::guictxtmenu_clip(DawCtrl* const _dawCtrl, gui_clip* const _gclip)
: guictxtmenu_track_editor(_dawCtrl, _gclip->m_trackentry, _gclip)
{
}

bool guictxtmenu_clip::clickedElement(ctxtmenu_entry* e, int _id) {
    if (guictxtmenu_track_editor::clickedElement(e, _id)) {
        return true;
    }
    return false;
}

guictxtmenu_plugin::guictxtmenu_plugin(DawCtrl* _dawCtrl, guictr_plugins* _ctrOptional, effectbase* _effectOptional)
    : effectOptional(_effectOptional), pluginCtrOptional(_ctrOptional) {
    this->dawCtrl = _dawCtrl;
    this->size.x  = 260;
    auto clipboardType = dawCtrl->getDaw()->getClipboardType();
    bool bHasSel = dawCtrl->getPluginSel().hasSelection();
    if (_effectOptional) {
        addEntry(new ctxtmenu_entry("Show all automation", CMD_SHOW_AUTOMATION));
        addEntry(new ctxtmenu_entry("Show parameter list", CMD_SHOW_PARAM_LIST));
        addEntry(new ctxtmenu_splitter());
    }
    addEntry(new ctxtmenu_entry(_dawCtrl, GlobalCommandType::CMD_CUT));
    entries.back()->setGrayedOut(!bHasSel);
    addEntry(new ctxtmenu_entry(_dawCtrl, GlobalCommandType::CMD_COPY));
    entries.back()->setGrayedOut(!bHasSel);
    addEntry(new ctxtmenu_entry(_dawCtrl, GlobalCommandType::CMD_PASTE));
    entries.back()->setGrayedOut(clipboardType != ClipBoardType::CLIPBOARD_PLUGINS);
    addEntry(new ctxtmenu_entry(_dawCtrl, GlobalCommandType::CMD_DELETE));
    entries.back()->setGrayedOut(!bHasSel);
    addEntry(new ctxtmenu_entry(_dawCtrl, GlobalCommandType::CMD_DUPLICATE));
    entries.back()->setGrayedOut(!bHasSel);
    addEntry(new ctxtmenu_splitter());
    addEntry(new ctxtmenu_entry("Load plugin", CMD_LOAD_PLUGIN));
}

bool guictxtmenu_plugin::clickedElement(ctxtmenu_entry* e, int _id) {
    if (pluginCtrOptional && e->commandtype != GlobalCommandType::CMD_NONE) {
        DAW::UI::CommandContext ctxt = {e->commandtype};
        closeContextMenu();
        pluginCtrOptional->handleCommand(ctxt);
        return true;
    }
    // return guictxtmenu::clickedElement(e, _id);
    ThreadLock lock = dawCtrl->lockPlayThread();
    if (_id == CMD_SHOW_PARAM_LIST && effectOptional) {
        auto* gui = effectOptional->getGui();
        if (gui) {
            guictr_properties_table* dbgPropertiesCtrPopup = guictr_properties_table::MakeUniquePropertiesCtr();
            guictxtmenu_base* ctxtMenu                     = new guictxtmenu_base();
            ctxtMenu->setBackgroundRendered(true);
            ctxtMenu->size = { 640, 480 };
            ctxtMenu->add(static_cast<guibase*>(dbgPropertiesCtrPopup));
            ivec2 wndPos{ 0 };
            dbgPropertiesCtrPopup->setDebugPropertyHandle(gui);
            dawCtrl->openContextMenu(ctxtMenu, gui->toScreenSpace({ gui->size.x, 0 }));
            return true;
        }
    }
    if (_id == CMD_SHOW_AUTOMATION && effectOptional) {
        auto tr                          = effectOptional->getTrack();
        auto trCtr                       = dawCtrl->getTrackContainer();
        gui_track_automationlane* gtr_at = nullptr;
        if (tr) {
            track_gui_entry_t* entry = nullptr;
            if (!trCtr->getTrackEntry(tr, &entry)) {
                dbgassert(0);
            } else {
                entry->layout.hideTrack     = false;
                entry->layout.hideSubtracks = false;
                updateStoreLoadSubtracks(trCtr, entry);

                std::vector<int32_t> automated;
                effectOptional->getAutomated(automated);
                for (int32_t param : automated) {
                    auto lane = trCtr->addAutomationLane(entry, effectOptional, param, true);
                    if (!gtr_at) {
                        gtr_at = lane;
                    }
                }
            }
        }
        if (trCtr && gtr_at) {
            dawCtrl->updateVisibleTrackContents();
            trCtr->scrollTo(gtr_at);
        }
    }
    auto stage = pluginCtrOptional ? pluginCtrOptional->stage : nullptr;
    if (_id == CMD_LOAD_PLUGIN && stage) {
        auto window = parentCtrl->window;
        String path;
        if (promptUserFilePath(window, 0, vFILE_TYPE_PLUGINSNAPSHOT, path)) {
            ThreadLock lock                                   = dawCtrl->lockPlayThread();
            std::shared_ptr<plugin_snapshot_t> pluginSnapshot = loadPluginSnapshot(path);
            dbgassert(pluginSnapshot);
            if (pluginSnapshot) {
                auto* pluginMgr = dawCtrl->getDaw()->getPluginManager();
                DAW::assignFreeStageIds(pluginMgr, *pluginSnapshot);
                auto effect = pluginMgr->loadPluginDeferred(*pluginSnapshot);
                if (effect) {
                    effect->projectGlobalId = 0;// generate new id
                    if (!pluginMgr->addDeferredEffect(effect)) {
                        log_printf("Failed loading effect\n");
                        delete effect;
                        return true;
                    }
                    effect->getSnapshot().projectGlobalId = effect->projectGlobalId;
                    effect->load(pluginMgr);
                    pluginMgr->insertNewPlugin(stage, effect, -2);// insert at end
                    // host->activateDeferred(effect, 0);
                }
            }
        }
    }
    closeContextMenu();
    return true;
}

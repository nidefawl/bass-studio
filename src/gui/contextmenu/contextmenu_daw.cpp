#include "str_util.h"
#include "automation.h"
#include "gui/automation/automatable.h"
#include "host/plugin/base_plugin.h"
#include "contextmenu_daw.h"
#include "track_snapshot.h"
#include "logging.h"
#include "host/pluginmanager.h"

guictxtmenu_at_param::guictxtmenu_at_param(DawCtrl* _dawCtrl, automatable_t* _atl, int32_t _paramIdx)
    : atl(_atl), paramIdx(_paramIdx) {
    this->dawCtrl = _dawCtrl;
    this->size.x = 240;
    addContextEntriesAutomation(this, _atl, paramIdx);
}
void guictxtmenu_at_param::clicked(int _id) {
    handleAutomatableContextMenu(dawCtrl, atl, paramIdx, _id);
    closeContextMenu();
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
                        pluginMgr->activateDeferred(effect, DAW::pluginmanager::FLAG_HOST_UNLOAD_PLUGIN_NO_NOTIFY);
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

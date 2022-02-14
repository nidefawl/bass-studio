#include "tls.h"
#include "mainctrl.h"

#include "plugin/base_plugin.h"
#include "plugin/vst_plugin.h"
#include "vst_host.h"
#include "track_impl.h"
#include "logging.h"
#include "menu.h"
#include "gui/trackcontrols.h"
#include "gui/trackcontent.h"
#include "commands.h"
#include "gui/subtrack.h"
#include "appconfig.h"

void setLoopPosition(DawCtrl* dawCtrl, float fStart, float fLength) {
    DawInstance* dawInstance = dawCtrl->getDaw();

    // ctrl may still alter project settings during copy here if not locked
    auto& projectGlobals             = dawInstance->getGlobals();

    projectGlobals.cursor.cursorPos = projectGlobals.loopStart;
    if (fStart >= 0.0f) {
        projectGlobals.cursor.cursorPos = math::roundfS32(fStart * TICKS_BAR);
        projectGlobals.loopStart        = math::roundfS32(fStart * TICKS_BAR);
    }

    if (fStart >= 0.0f && fLength >= 0.0f) {
        projectGlobals.loopEnabled = true;
        projectGlobals.loopLen     = math::roundfS32(fLength * TICKS_BAR);
    }
};
void setSelection(DawCtrl* dawCtrl, int32_t trackBegin, int32_t trackEnd, float fStart, float fLength) {
    DAW::Cursor& cursor = dawCtrl->getCursor();
    cursor.setBegin(math::roundfS32(fStart * TICKS_BAR));
    cursor.setEnd(math::roundfS32((fStart+fLength) * TICKS_BAR));
    cursor.setTrackBegin(trackBegin);
    cursor.setTrackEnd(trackEnd);
}
void loadPluginAndInsertOnTrack(DawCtrl* dawCtrl, String modulePath, int32_t trackIdx) {
    DawInstance* dawInstance = dawCtrl->getDaw();
    auto* project = dawInstance->getProject();
    auto* host = dawInstance->getHost();
    auto trackList = project->getTracksFlatVec();

    if (trackList.size() < trackIdx) {
        dbgassert(0);
        return;
    }

    vstpluginloadres loadRes = vsthost::getInstance()->loadPlugin(modulePath, 0);

    if (loadRes.result != 0) {
        dbgassert(0);
        return;
    }

    audio_stage_t* trImpl1 = trackList[trackIdx]->getStage();
    host->insertNewPlugin(trImpl1, loadRes.plugin, 0);
    loadRes.plugin->resume();
    vsthost::getInstance()->postPluginLoaded(trImpl1, loadRes.plugin);


#if 0
    // example cloning plugin
    auto defEffect = loadRes.plugin->toDeferred();
    if (!defEffect) {
        dbgassert(0);
        return;
    }
    if (!host->addDeferredEffect(defEffect)) {
        log_printf("Failed loading effect\n", 0);
        delete defEffect;
        dbgassert(0);
        return;
    }
    audio_stage_t* trImpl2 = trackList[1]->getStage();

    defEffect->getSnapshot().projectGlobalId = -1;
    defEffect->load(host);
    host->insertNewPlugin(trImpl2, defEffect, 0);
    dbgassert(defEffect->trackImpl == trImpl2);
    dbgassert(trImpl2->effects.size());
#endif
}

void dawinstance_startup_commands(const std::vector<String>& args, daw_tls::tlsinstance& tls) {
    //  if (1==1)
    //    return;
    auto* const dawMainCtrl = tls.mainCtrl;
    if (!dawMainCtrl) {
        return;
    }
    auto dawInstance = dawMainCtrl->getDaw();
    //  String dawPath = "C:/dev/daw/run/";
    String dawPath  = "./projects/";
    String projName = "startup.project";
    //String projName = "test-wave-2.project";
    //projName = "kshmr-samples-test.project";
    //  projName = "test-empty-midi-loop.project";
    //projName = "arp-test.project";
    //projName = "test-send-automation.project";
    //projName = "kshmr-samples-test.project";
    //projName = "kshmr-samples-test.project";
    int flags = 0x1;// defer load
    //  flags = 0; // no defer load
    dawInstance->cbProjectLoadCompleteCallback = [dawMainCtrl](DawInstance*, std::shared_ptr<project_file> file, int errorState) {


        /**
         * Code for setting cursor and loop position
         */
        const bool dbPlaceLoopPosition = false;
        if (dbPlaceLoopPosition) {
            setLoopPosition(dawMainCtrl, 177.0f, 64.0f);
            setSelection(dawMainCtrl, 0, 1, 177.0f, 64.0f);
        }

        /**
         * Code for inserting a plugin on track at index 0, then placing a deferred copy instance of the same plugin on track at index 1
         */

        const bool dbgLoadPlugins = false;
        if (dbgLoadPlugins) {
            loadPluginAndInsertOnTrack(dawMainCtrl, "C:/PluginManager/configs/default/hosts/Ableton/categories/melda/MPowerSynth.dll", 0);
        }
#if 0
        const bool loadPlugins = 0;
        if (loadPlugins) {
            ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
            auto* host      = vsthost::getInstance();
            std::vector<effectbase*> pluginsDeferred;
            host->getDeferredEffects(pluginsDeferred);
            log_printf("loading %d plugins\n", pluginsDeferred.size());
            for (auto effect : pluginsDeferred) {
                log_printf("activate %s\n", StringAsCStr(effect->sName));
                host->activateDeferred(effect, vsthost::FLAG_HOST_UNLOAD_PLUGIN_NO_NOTIFY);
            }
            auto& trackList = dawInstance->getProject()->trackList;
            for (track_t* tr : trackList) {
                tr->getStage()->pluginsChanged();
            }
            host->onTrackLayoutChange();
            dawInstance->onPluginsChanged();

            //        dawMainCtrl->menuCommand(CMD_NOARG(CMD_PREFERENCES));
        }
#endif

#if 0
        // open subtrack waveview
        dbgassert(dawInstance->getTracks().size() > 1);
        auto guiTrackCtr = dawMainCtrl->getGuiTrackCtr();
        auto track = dawInstance->getTracks()[1];
        track_gui_entry_t* entry;
        dbgassert(guiTrackCtr->getTrackEntry(track, &entry));

        track->audio->flags |= audiostageflags_t::CONVERT_OUTPUT | audiostageflags_t::WRITE_OUTPUT;

        auto gui = makeGuiSubtrack(entry, dawMainCtrl, gui_track_subtrack::SUBTRACK_TYPE_WAVE);
        MainCtrl::getGuiTrackCtr()->addSubTrack(entry, gui, true);
        entry->parent->layout();
        entry->parent->updateVisibleTrackContents();
        //dawInstance->startPlaying();

        daw_tls::getTls().config->enableClipRendererDebugLayer=true;
#endif
    };
    //    dawMainCtrl->setVisible(false);
    //    dawMainCtrl->menuCommand(CMD_NUMBER_ARG(CMD_SHOW_DEBUG_WINDOW, 0));
    //    dawMainCtrl->menuCommand(CMD_NUMBER_ARG(CMD_SHOW_DEBUG_WINDOW, 1));
    if (dawMainCtrl->getLoadProjectFilePath().empty())
        dawInstance->loadFile(dawPath + projName, flags);
}

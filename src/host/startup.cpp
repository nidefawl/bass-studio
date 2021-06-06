#include "tls.h"
#include "mainctrl.h"

#include "plugin/base_plugin.h"
#include "plugin/vst_plugin.h"
#include "vst_host.h"
#include "track_impl.h"
#include "logging.h"
#include "menu.h"
#include "commands.h"
#include "gui/trackcontrols.h"
#include "gui/trackcontent.h"
#include "gui/subtrack.h"


void dawinstance_startup_commands(daw_tls::tlsinstance& tls) {
//	if (1==1)
//		return;
	auto* const dawMainCtrl = tls.mainCtrl;
    if (!dawMainCtrl) {
        return;
    }
	auto dawInstance = dawMainCtrl->getDaw();
	vsthost* host = vsthost::getInstance();
	String dawPath = "C:/Users/Michael/daw/run/";
	String projName = "spire-test.project";
	int flags = 0x1; // defer load
	dawInstance->cbProjectLoadCompleteCallback = [tls, dawMainCtrl, dawInstance, host](DawInstance*, std::shared_ptr<project_file> file, int errorState) {
        DAW::Cursor& cursor = dawMainCtrl->getCursor();



        /**
         * Code for setting cursor and loop position
         */
        const bool dbPlaceLoopPosition = false;
        if (dbPlaceLoopPosition) {
            float fStart = 0.0f;
            float fLength = 2.0f;
            // ctrl may still alter project settings during copy here if not locked
            auto& projectGlobals = dawInstance->getGlobals();
            project_controller_t* const ctrl = dawInstance;
            my_printf("Tempo100: %d\n", projectGlobals.tempo100);
            my_printf("project.cursor.cursorPos: %d\n", projectGlobals.cursor.cursorPos);
            projectGlobals.cursor.cursorPos = projectGlobals.loopStart;
            if (fStart >= 0.0f) {
                projectGlobals.cursor.cursorPos = math::round(fStart * TICKS_BAR);
                projectGlobals.loopStart = math::round(fStart * TICKS_BAR);
            }

            if (fStart >= 0.0f && fLength >= 0.0f) {
                projectGlobals.loopEnabled = true;
                projectGlobals.loopLen = math::round(fLength * TICKS_BAR);
            }
            cursor.setBegin(projectGlobals.cursor.cursorPos);
            cursor.setEnd(projectGlobals.cursor.cursorPos + projectGlobals.loopLen);
            cursor.setTrackBegin(0);
            cursor.setTrackEnd(1);
        }

        /**
         * Code for inserting a plugin on track at index 0, then placing a deferred copy instance of the same plugin on track at index 1
         */

        const bool dbgLoadPlugins = false;
        if (dbgLoadPlugins) {
            auto trackList = tls.project->getTracks().getMidiAudioTracksFlatVec();
            dbgassert(trackList.size() > 1);
            vstpluginloadres loadRes =
                vsthost::getInstance()->loadPlugin("C:/PluginManager/configs/default/hosts/Ableton/categories/melda/MPowerSynth.dll", 0);

            if (loadRes.plugin && loadRes.result == 0) {
                audio_stage_t* trImpl1 = trackList[0]->getStage();
                host->insertNewPlugin(trImpl1, loadRes.plugin, 0);
                loadRes.plugin->resume();
                vsthost::getInstance()->postPluginLoaded(trImpl1, loadRes.plugin);
                auto defEffect = loadRes.plugin->toDeferred();
                if (defEffect) {
                    if (!host->addDeferredEffect(defEffect)) {
                        log_printf("Failed loading effect\n", 0);
                        delete defEffect;
                    }
                    else {
                        audio_stage_t* trImpl2 = trackList[1]->getStage();
                        defEffect->getSnapshot().projectGlobalId = -1;
                        defEffect->load(host);
                        host->insertNewPlugin(trImpl2, defEffect, 0);
                        dbgassert(defEffect->trackImpl == trImpl2);
                        dbgassert(trImpl2->effects.size());
                    }
                }
                else {
                    // TODO: handle
                }
            }
        }
        const bool loadPlugins = 1;
        if (loadPlugins) {
            ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
            auto* host = vsthost::getInstance();
            std::vector<effectbase*> pluginsDeferred;
            host->getDeferredEffects(pluginsDeferred);
            my_printf("loading %d plugins\n", pluginsDeferred.size());
            for (auto plugin : pluginsDeferred) {
                my_printf("activate %s\n", StringAsCStr(plugin->sName));
                effectbase* effectLoaded = nullptr;
                host->activateDeferred(plugin, &effectLoaded);
                DawInstance::get()->onPluginsChanged();
                //            			if (effectLoaded) {
                //            				effectLoaded->show();
                //            			}
            }
            //        dawMainCtrl->menuCommand(CMD_NOARG(CMD_PREFERENCES));
        }
        dawMainCtrl->menuCommand(CMD_NOARG(CMD_SHOW_DEBUG_WINDOW));
        auto trackList = tls.project->getTracks().getMidiAudioTracksFlatVec();
        dbgassert(trackList.size() > 1 && trackList[1]->audio->guiInstances.size() > 0);

        track_gui_entry_t* trackGui = trackList[1]->audio->guiInstances[0];
		auto gui = makeGuiSubtrack(trackGui, MainCtrl::get(), gui_track_subtrack::SUBTRACK_TYPE_WAVE);
		MainCtrl::getGuiTrackCtr()->addSubTrack(trackGui, gui, true);
		trackGui->parent->layout();
		trackGui->parent->updateVisibleTrackContents();
		dawInstance->startPlaying();
	};
	dawInstance->loadFile(dawPath + projName, flags);
}

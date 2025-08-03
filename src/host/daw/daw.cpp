#include <algorithm>
#include <archive_entry.h>
#include <archive.h>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <functional>
#include <GLFW/glfw3.h>
#include <memory>
#include <nanovg.h>
#include <optional>
#include <utility>
#include <variant>
#include <vector>
#include "appconfig.hpp"
#include "appsettings.hpp"
#include "assert_dbg.h"
#include "basectrl.hpp"
#include "color_util.hpp"
#include "commands.hpp"
#include "config.hpp"
#include "cursor.hpp"
#include "daw_async_project_load.hpp"
#include "daw.hpp"
#include "edithistory.hpp"
#include "error.hpp"
#include "event.hpp"
#include "exceptions.hpp"
#include "file/projectfile-v2.hpp"
#include "fileio.hpp"
#include "fileloader.hpp"
#include "glheaders.h"
#include "grid.hpp"
#include "gui/clipeditor/clipeditor.hpp"
#include "gui/container/container_builder.hpp"
#include "gui/container/container_dnd_layout.hpp"
#include "gui/container/container_layout_types.hpp"
#include "gui/container/container.hpp"
#include "gui/dialog/about.hpp"
#include "gui/dialog/dialog_io.hpp"
#include "gui/dialog/dialogs.hpp"
#include "gui/linetess/pymachine.hpp"
#include "guicolors.hpp"
#include "host/audiocache/audiocache.hpp"
#include "host/audiohost/audio_host.hpp"
#include "host/clip/clip.hpp"
#include "host/daw/daw_async_task.hpp"
#include "host/daw/mainctrl.hpp"
#include "host/graph/effect_graph.hpp"
#include "host/graph/track_graph.hpp"
#include "host/host_pluginmanager.hpp"
#include "host/host.hpp"
#include "host/midihost/midi_host.hpp"
#include "host/plugin/base/base-plugin.hpp"
#include "host/plugin/vst/vstplugin.hpp"
#include "host/project/project.hpp"
#include "host/track/track_impl.hpp"
#include "host/track/track.hpp"
#include "keyboard.hpp"
#include "logging.hpp"
#include "math/seq_math.hpp"
#include "menu.hpp"
#include "msgbox.hpp"
#include "note.hpp"
#include "platform.hpp"
#include "saferef.hpp"
#include "seq_time.hpp"
#include "seq_util.hpp"
#include "str_util.hpp"
#include "thread.hpp"
#include "threads/playbackthread.hpp"
#include "threads/workerthread.hpp"
#include "tls.hpp"
#include "types.hpp"
#include "util/profiling.hpp"
#include "wave/waveform_render_impl.hpp"
#include "window_impl.hpp"
#include "window.hpp"
#include "sse.hpp"
#include "platform.hpp"
#include "gui/clipeditor/clipeditor_python_processor.hpp"


std::shared_ptr<window_abstract_t> getWindowDebugWaveformCache();
std::shared_ptr<window_abstract_t> getWindowPerf();
std::shared_ptr<window_abstract_t> getWindowDebugNanoVG();

int32_t getNumClipAllocations();
void printLeakedAudioBuffers();
void printClipAllocations();

extern "C" {
void resetShaderTimeOffset(void);
}

namespace DAW {
void GetProjectReferencedSampleIds(const project_t& project, std::vector<int32_t>& uniqueSampleIds) {
    for (track_t* t : project.trackList) {
        auto& clipContainer = t->getClips();
        for (auto& clip : clipContainer.getClips()) {
            if (clip->audio.id >= 0 && !std::binary_search(uniqueSampleIds.cbegin(), uniqueSampleIds.cend(), clip->audio.id)) {
                insertSorted(uniqueSampleIds, clip->audio.id);
            }
            if (clip->audio.idDerived >= 0 && !std::binary_search(uniqueSampleIds.cbegin(), uniqueSampleIds.cend(), clip->audio.idDerived)) {
                insertSorted(uniqueSampleIds, clip->audio.idDerived);
            }
        }
    }
}

guictxtmenu_base* makeGuiAutosave(int64_t delay);

String getProjectAutosaveFilename(String projectPath) {
    String bakPathName;
    if (projectPath.empty()) {
        App::Platform::createUniqueFilename(bakPathName, App::Platform::toUserdataPath("unsaved.project"));
    } else {
        String path, name, ext, nameExt;//path, name, ext, nameExt
        SplitPath(projectPath, &path, &name, &ext, &nameExt);
        bakPathName = path;
        bakPathName += FILE_PATHSEP_STR;
        bakPathName += name;
        bakPathName += "-autosave.";
        bakPathName += ext;
    }
    return bakPathName;
}
}// namespace DAW

static SupportedFileType FILE_TYPE_PROJECT{ "Bass Studio Project", PROJECT_FILE_EXT };
static SupportedFileType FILE_TYPE_PROJECT_LEGACY{ "Legacy Project", PROJECT_LEGACY_FILE_EXT };
static SupportedFileType FILE_TYPE_PROJECT_BUNDLE{ "Project Bundle", PROJECT_BUNDLE_FILE_EXT };
SupportedFileTypes FILE_TYPES_PROJECT_SAVE = SupportedFileTypes{PROJECT_FILE_TYPE_DESC " (json)", std::vector<SupportedFileType>{ FILE_TYPE_PROJECT } };
SupportedFileTypes FILE_TYPES_BUNDLE = SupportedFileTypes{PROJECT_FILE_TYPE_DESC " (bundle)", { FILE_TYPE_PROJECT_BUNDLE } };
SupportedFileTypes FILE_TYPES_PROJECT_LOAD = SupportedFileTypes{PROJECT_FILE_TYPE_DESC "s", { FILE_TYPE_PROJECT, FILE_TYPE_PROJECT_LEGACY, FILE_TYPE_PROJECT_BUNDLE } };

MainCtrl* DawInstance::getMainControl() {
    return this->tls.mainCtrl;
}
void DawInstance::startPlaying(tick_t pos) {
    if (pos >= 0) {
        projectGlobals.cursor.setEmptySelection();
        projectGlobals.cursor.cursorPos = pos;
    }
    playThread.addRequest(PlaybackThread::REQ_PLAYBACK_STATE, (int) playback_state::status_playback, true);
}

void DawInstance::startExport() {
    setAudioThreadState(playback_state::status_no_process);
    if (tls.audioHost) {
        tls.audioHost->stopAudio();
    }
    if (tls.midiHost) {
        tls.midiHost->stopMidi();
    }
    tls.host->setOutput(nullptr);
    tls.host->setProcessingQuality(DAW::Host::ProcessingQuality::Q_RENDER);
    playThread.addRequestWithCallback(PlaybackThread::REQ_PLAYBACK_STATE, (int) playback_state::status_render, []() {
        auto& tls = daw_tls::getTls();
        tls.dawInstance->bExportFinished = true;
    }, true);
}

void DawInstance::stopPlaying() {
    setAudioThreadState(playback_state::status_stop);
}

void DawInstance::setAudioThreadState(playback_state state) {
    playThread.addRequest(PlaybackThread::REQ_PLAYBACK_STATE, (int) state, true);
}

bool DawInstance::toggleLoop() {
    projectGlobals.loopEnabled = !projectGlobals.loopEnabled;
    return projectGlobals.loopEnabled;
}

bool DawInstance::isPlaying() {
    return playThread.getState() == playback_state::status_playback;
}

void DawInstance::triggerAutoSave() {
    tmLastSave          = getTimeMillis();
    projectPathAutosave = DAW::getProjectAutosaveFilename(projectPath);

    std::shared_ptr<project_file> f = createProjectFile();
    DAW::ProjectFileV2::saveProjectToJsonFile(f, projectPathAutosave);
}

String DawInstance::getAutoSaveFilename() {
    return DAW::getProjectAutosaveFilename(projectPath);
}
bool DawInstance::configureSampleRate() {
    bool bSuccess = true;
    const bool wasPlaying = isPlaying();
    if (wasPlaying) {
        stopPlaying();
    }
    setAudioThreadState(playback_state::status_stop);
    setAudioThreadState(playback_state::status_no_process);
    auto& settings = daw_tls::getSettings();
    {

        ThreadLock lock  = getPlayThread()->lockThread();
        auto host  = getHost();
        auto ahost = getAudioHost();
        host->setOutput(nullptr);
        ahost->stopAudio();
        if (settings.dawsettings.audioEnabled) {
            auto oldSampleRate = host->m_sampleFormatInternal.sampleRate;
            host->setSampleFormat(sampleformat_t{static_cast<samplerate_t>(settings.iosettings.internalSamplerate),
                                                 settings.iosettings.internalBlocksize, sampleformat_bits_t::FLOAT_32});
            auto newSampleRate = host->m_sampleFormatInternal.sampleRate;
            for (track_t* t : project.trackList) {
                t->updateAudioClipLengths(projectGlobals.tempo100, oldSampleRate, newSampleRate);
            }
            if (ahost->startAudio(settings.iosettings)) {
                host->setOutput(ahost->getStreamSharedPtr(0));
            } else {
                bSuccess = false;
                //settings.dawsettings.audioEnabled = false;
            }
        }
    }
    if (settings.dawsettings.audioEnabled) {
        if (wasPlaying) {
            startPlaying();
        } else {
            setAudioThreadState(playback_state::status_stop);
        }
    }
    return bSuccess;
}
void DawInstance::processTasksMainThread() {
    using state = DAW::async_task_t::state;
    auto runAsyncTask = asyncTask;
    if (runAsyncTask) {
        runAsyncTask->run();
        if (runAsyncTask->getAndResetReqFrame()) {
            getMainControl()->requestRedraw();
        }
        switch (runAsyncTask->getState()) {
            case state::idle:
                dbgassert(0);
                break;
            case state::running:
                break;
            case state::error:
                log_lf(Log::L_ERROR, "async task %s error: %s\n", runAsyncTask->getTaskName().c_str(), runAsyncTask->getError().c_str());
            case state::finished:
            case state::cancelled:
                delete runAsyncTask;
                setAsyncTask(nullptr);
                break;
        }
    }
}

void DawInstance::onTick() {
    const bool bWroteMidiData = tls.host->writeRecordedData(this);
    if (bWroteMidiData) {
        updateVisibleTrackContents();
    }

    auto settings = tls.settings;
    auto host = tls.host;
    auto audioHost = tls.audioHost;
    auto midiHost = tls.midiHost;
    if (bExportFinished) {
        bExportFinished = false;
        {
            auto lock = getPlayThread()->lockThread();
            tls.host->setProcessingQuality(DAW::Host::ProcessingQuality::Q_PLAYBACK);
            if (settings->dawsettings.audioEnabled) {
                if (audioHost->startAudio(settings->iosettings)) {
                    auto stream = audioHost->getStreamSharedPtr(0);
                    host->setOutput(stream);
                }
            }
            if (midiHost) {
                midiHost->startMidi();
            }
        }
        setAudioThreadState(playback_state::status_stop);
    }
    
    if (host->isStreaming() && audioHost->isResetRequested()) {
        auto lock = getPlayThread()->lockThread();
        host->setOutput(nullptr);
        audioHost->stopAudio();
        if (audioHost->startAudio(settings->iosettings)) {
            auto stream = audioHost->getStreamSharedPtr(0);
            host->setOutput(stream);
        }
    }

    updateLoadAudioTasks();
    updateAudioProcessingTask();
    fileSearchUpdate();

    host->onTick();

    bool noPopups = true;
    for (auto* ctrl : dawCtrls) {
        noPopups &= ctrl->getGuiDraggedRef().isEmpty() && ctrl->getGuiCapturedRef().isEmpty() && !ctrl->ctxtmenu;
    }
    if (noPopups && tls.mainCtrl && !tls.mainCtrl->loadProject.empty()) {
        String file;
        std::swap(tls.mainCtrl->loadProject, file);
        loadFile(file, DAW::PluginLoadFlags::FLAG_INVOKE_USER_CB_DEFERLOAD);
    }
    if (noPopups && projectToLoad) {
        setAsyncTask(new DAW::load_project_task(this, std::move(projectToLoad)));
        projectToLoad = nullptr;
    }
    
    if (tls.mainCtrl) {
        auto& settings = daw_tls::getSettings();
        if (settings.autosave.tmSaveDelayMinutes > 0) {
            if (0 == autosaveState.tmLastTrigger) {
                autosaveState.tmLastTrigger = getTimeMillis();
            }
            int64_t tmNow = getTimeMillis();
            const auto ms60k = 60 * 1000;
            if ((tmNow - tmLastSave) / ms60k > settings.autosave.tmSaveDelayMinutes) {
                bool canOpenAutosave = noPopups;
                bool hasAnyInputFocus = false;
                for (auto* ctrl : dawCtrls) {
                    canOpenAutosave &= !ctrl->window->isMouseCaptured();
                    canOpenAutosave &= !ctrl->hasDialogWindows();
                    canOpenAutosave &= !ctrl->hasContextMenu();
                    hasAnyInputFocus |= ctrl->hasInputFocus();
                    /*canOpenAutosave &= last click was n seconds ago*/
                    canOpenAutosave &= playThread.getState() != playback_state::status_render;
                }
                canOpenAutosave &= hasAnyInputFocus;
                if (canOpenAutosave &&  (tmNow - autosaveState.tmLastTrigger) / ms60k > math::max<int64_t>(settings.autosave.tmReminderDelayMinutes, 1)) {
                    autosaveState.tmLastTrigger = tmNow;
                    auto tooltip                = DAW::makeGuiAutosave(1500);
                    auto ctrlSize               = tls.mainCtrl->m_size;
                    tooltip->size               = ivec2(420, 90);
                    tooltip->maxHeight          = tooltip->size.y;
                    tls.mainCtrl->openContextMenu(tooltip, ivec2(ctrlSize.x / 2, ctrlSize.y - 100) - tooltip->size / 2);
                }
            }
        }
    }
}

void DawInstance::pushHist(action_base* action) {
    hist.push(this, action);
}

std::shared_ptr<project_file> DawInstance::createProjectFile() {
    ThreadLock lock                    = playThread.lockThread();
    std::shared_ptr<project_file> file = std::make_shared<project_file>();
    file->path                         = projectPath;
    project.copyTo(file->project);
    file->layouts.resize(dawCtrls.size());
    auto itOut = file->layouts.begin();
    for (auto& ctrl : dawCtrls) {
        ctrl->storeLayout(*itOut++);
    }
    file->project.globals        = projectGlobals;
    file->project.exportSettings = getExportSettings();
    file->project.quantizeSettings = getQuantizeSettings();
    if (tls.host) {
        file->project.samplerate = tls.host->m_sampleFormatInternal.sampleRate;
    }
    std::vector<int32_t> uniqueSampleIds;
    DAW::GetProjectReferencedSampleIds(project, uniqueSampleIds);
    tls.audioCache->saveSamples(uniqueSampleIds);
    tls.audioCache->store(uniqueSampleIds, file->sampleFileIndex);

    //TODO: layout settings should be handled on editor container level
    auto mainCtrl = getMainControl();
    if (mainCtrl) {
        auto ctrTracks = mainCtrl->getTrackContainer();
        if (ctrTracks) {
            file->layout.layoutGrid = ctrTracks->getGrid();
            file->layout.scrollOffsetX = ctrTracks->getScrollOffset();
        }
    }
    return file;
}

bool DawInstance::setProjectToLoad(const std::shared_ptr<project_file>& file, int flags) {
    projectToLoad = std::make_shared<project_to_load_t>(project_to_load_t{ file, flags });
    return true;
}
void DawInstance::unloadProject() {
    AppWndProc_enableBlockReentrant();
    dbgassert(!playThread.isRunning() || playThread.isLockedOrNotProcessing());
    for (DawCtrl* ctrl : dawCtrls) {
        ctrl->resetClipViews();
        ctrl->closeContextMenu();
        ctrl->closeAllAppMenus();
        ctrl->resetMouseContext();
        ctrl->setSelectedTrack(nullptr);
    }
    projectPath = "";
    resetClipViews();
    setEmptyClipboard();

    projectGlobals.cursor.setEmptySelection();

    hist.clear(this);

    std::vector<track_t*> _tracks     = project.trackList.getAllTracksFlatVec();// iterate a copy
    std::vector<track_t*> _rootTracks = project.trackList.getAllTracksTreeVec();
    log_lf(Log::L_DEBUG, "Unloading project with %zu tracks\n", _tracks.size());
    for (auto it = _tracks.rbegin(); it != _tracks.rend(); it++) {
        track_t* track = *it;
        removeTrackImpl(track, FLG_TRK_CHANGE_LOAD);
    }
    project.trackList.clear();
    for (auto it = _tracks.rbegin(); it != _tracks.rend(); it++) {
        track_t* track = *it;
        releaseTrackResources(track, this);
        delete track;
    }

    tls.host->unload();
    tls.audioCache->unloadAll();
    projectGlobals.grooveData.clear();

    /** reset maximum stage id and determine new maximum stage id */
    tls.host->updateMaximumStageId();
    for (DawCtrl* pDawCtrl : dawCtrls) {
        if (pDawCtrl->isOk()) {
            pDawCtrl->onPostUnloadProject();
        }
    }

    {
        std::vector<effectbase*> pluginsDeferred;
        tls.host->getDeferredEffects(pluginsDeferred);
        dbgassert(pluginsDeferred.empty());
    }
    AppWndProc_disableBlockReentrant();
}

void DawInstance::loadFileCStr(const char* str) {
    loadFile(str, 0);
}

void DawInstance::saveFile(const String& path) {
    if (!path.empty()) {
        std::optional<String> error = std::nullopt;
        if (projectFileType == PROJECT_FILETYPE_JSON || projectFileType == PROJECT_FILETYPE_JSON_LEGACY) {
            std::shared_ptr<project_file> f = createProjectFile();
            error = DAW::ProjectFileV2::saveProjectToJsonFile(f, path);
            projectFileType = PROJECT_FILETYPE_JSON;
        } else {
            error = saveProjectBundle(path);
        }
        if (!error && tls.mainCtrl) {
            tls.mainCtrl->setStatusText(StringFormat("Saved project to %s", StringAsCStr(path)));
        } else if (error) {
            if (tls.mainCtrl) {
                String fullError = "Failed saving '";
                fullError += path;
                fullError += "':\n\n";
                fullError += *error;
                tls.mainCtrl->openDialog(new guidialog_message_box("Failed saving project", fullError));
                tls.mainCtrl->setStatusText("Failed saving " + FileNameFromPath(path) + ": " + *error);
            }
            log_lf(Log::L_ERROR, "Failed saving %s: %s\n", StringAsCStr(path), StringAsCStr(*error));
        }
        projectPath = path;
        if (tls.mainCtrl) {
            String projectFileName;
            SplitPath(path, &lastProjectDirectory, &projectFileName, nullptr, nullptr);
            tls.mainCtrl->setWindowName(StringFormat("%s - %s", BuildInfo::PRODUCT_NAME_DISPLAY, StringAsCStr(projectFileName)));
        }
        tls.settings->recentfiles.add(path);
    }
}

void DawInstance::loadFile(String path, int flags) {
    std::variant<std::shared_ptr<project_file>, String> fileOrErr = "Failed loading project";
    String loadFileExt, loadFileDirectory;
    SplitPath(path, &loadFileDirectory, nullptr, &loadFileExt);
    std::vector<uint8_t> projJsonData;
    try {
        if (loadFileExt == PROJECT_BUNDLE_FILE_EXT && !path.empty()) {
            struct archive* a = archive_read_new();
            archive_read_support_filter_all(a);
            archive_read_support_format_all(a);
            archive_read_open_filename(a, path.c_str(), 10240);
            struct archive_entry* entry = nullptr;
            while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
                const char* entryPath = archive_entry_pathname(entry);
                String entryPathExt;;
                SplitPath(entryPath, nullptr, nullptr, &entryPathExt);
                if (entryPathExt == PROJECT_FILE_EXT) {
                    projJsonData.resize(archive_entry_size(entry));
                    archive_read_data(a, projJsonData.data(), projJsonData.size());
                    break;
                }
            }
            archive_read_free(a);
        } else {
            ReadFileVector(path, projJsonData);
        }
        timer.reset(); 
        fileOrErr = DAW::ProjectFileV2::loadProject(projJsonData);
    } catch (const std::exception& e) {
        fileOrErr = e.what();
    }
    if (std::holds_alternative<String>(fileOrErr)) {
        String errMsg = std::get<String>(fileOrErr);
        if (tls.mainCtrl) {
            String fullError = "Failed loading '";
            fullError += path;
            fullError += "':\n\n";
            fullError += errMsg;
            tls.mainCtrl->openDialog(new guidialog_message_box("Failed loading project", fullError));
            tls.mainCtrl->setStatusText("Failed loading " + FileNameFromPath(path) + ": " + errMsg);
            log_lf(Log::L_ERROR, "Failed loading %s: %s\n", StringAsCStr(path), StringAsCStr(errMsg));
        }
    } else {
        auto f = std::get<std::shared_ptr<project_file>>(fileOrErr);
        f->path = path;
        const bool wasUserCallback = (flags & DAW::PluginLoadFlags::FLAG_INVOKE_USER_CB_DEFERLOAD) != 0;
        auto cb                    = [this, path, projFile = f, wasUserCallback](int n) {
            int loadFlags = 0;
            if (wasUserCallback) {
                loadFlags = n == 0 ? DAW::PluginLoadFlags::FLAG_DEFER_LOAD : 0;
            } else {
                loadFlags = n;
            }
            setProjectToLoad(projFile, loadFlags);
            closeContextMenus();
            closeDialogs();
        };
        if (!tls.mainCtrl || (flags & DAW::PluginLoadFlags::FLAG_INVOKE_USER_CB_DEFERLOAD) == 0) {
            cb(flags & DAW::PluginLoadFlags::FLAG_DEFER_LOAD);
        } else {
            auto dlg = new guidialog_cb_yes_no("Load plugins", "Load all plugin instances?");
            dlg->cb = cb;
            tls.mainCtrl->openDialog(dlg);
        }
    }
}

void DawInstance::setEmptyProject() {
    ThreadLock lock = playThread.lockThread();
    unloadProject();
    projectFileType = PROJECT_FILETYPE_JSON;
    int totalAllocs = getNumClipAllocations();
    if (totalAllocs != 0) {
        log_lf(Log::L_WARN, "getNumClipAllocations == %d!\n", totalAllocs);
        // dbgassert(getNumClipAllocations() == 0);
    }
    insertNewTrack(-1, TRACK_TYPE_MIDI, FLG_TRK_CHANGE_LOAD);
    insertNewTrack(-1, TRACK_TYPE_MASTER, 0);
    resetShaderTimeOffset();
    tls.mainCtrl->setWindowName(StringFormat("%s - %s", BuildInfo::PRODUCT_NAME_DISPLAY, "New Project"));
}

void DawInstance::onDawCompanionWindowClose(DawWindowCompanion& entry) {
    auto it = std::find_if(dawCtrls.begin(), dawCtrls.end(), [pDawCtrlClosing = entry.ctrl.get()](auto* pDawCtrl) {
        return pDawCtrl == pDawCtrlClosing;
    });
    if (it != dawCtrls.end()) {
        dawCtrls.erase(it);
    }
    entry.wnd->setInvalid();
}

void DawInstance::onAudioStageChanged(audio_stage_t* stage) {
    for (DawCtrl* pDawCtrl : dawCtrls) {
        dbgassert(pDawCtrl->isOk());
        pDawCtrl->getPluginSel().clear();
    }
}

void DawInstance::setSoloState(audio_stage_ref_t ref, bool enableSolo) {
    track_t* track = getTracks().resolveTrack(ref);
    dbgassert(track);
    dbgassert(track->audio);
    if (enableSolo) {
        track->audio->flags |= audiostageflags_t::SOLO;
    } else {
        track->audio->flags &= ~audiostageflags_t::SOLO;
    }
    DAW::updateSoloFlag(tls.host, &project, getTracks().getAllTracksFlatVecRef());
}
void DawInstance::unsoloAll() {
    DAW::unsoloAll(tls.host, &project, getTracks().getAllTracksFlatVecRef());
}
void DawInstance::setTrackArmed(audio_stage_ref_t ref, bool enabledArmed) {
    track_t* track = getTracks().resolveTrack(ref);
    dbgassert(track);
    dbgassert(track->audio);
    if (enabledArmed) {
        track->audio->flags |= audiostageflags_t::RECORD_ARMED;
    } else {
        track->audio->flags &= ~audiostageflags_t::RECORD_ARMED;
    }
}

bool DawInstance::onChildOverlayWindowClose(window_main* window) {
    auto it = std::find_if(companionWindows.begin(), companionWindows.end(), [this, window](auto& wndEntry) {
        if (wndEntry.wnd == window) {
            this->onDawCompanionWindowClose(wndEntry);
            return true;
        }
        return false;
    });
    if (it != companionWindows.end()) {
        companionWindows.erase(it);
        return true;
    }
    return false;
}

std::optional<String> DawInstance::saveProjectBundle(const String& path) {
    String ext;
    String projectFileName;
    String parentDir;
    String bundlePath = path;
    SplitPath(bundlePath, &parentDir, &projectFileName, &ext);
    if (ext != PROJECT_BUNDLE_FILE_EXT) {
        bundlePath = parentDir + FILE_PATHSEP_STR + projectFileName + "." PROJECT_BUNDLE_FILE_EXT;
    }
    String projFileName = projectFileName + "." PROJECT_FILE_EXT;
    
    std::function<void(const String&, int32_t, int32_t)> onProgress = [path](const String& curFile, int32_t i, int32_t total) {
        log_lf(Log::L_ERROR, "[%d/%d] Saving %s to %s\n", i, total, StringAsCStr(curFile), StringAsCStr(path));
    };

    std::vector<int32_t> uniqueSampleIds;
    DAW::GetProjectReferencedSampleIds(project, uniqueSampleIds);
    // create a new archive
    struct archive* ar = archive_write_new();
    if (!ar || ARCHIVE_OK != archive_write_set_format_zip(ar)) {
        return "Failed to create archive";
    }
    if (ARCHIVE_OK != archive_write_zip_set_compression_deflate(ar)) {
        return "Failed to compress archive";
    }
    if (ARCHIVE_OK != archive_write_open_filename(ar, bundlePath.c_str())) {
        return "Failed to open archive for writing";
    }
    std::optional<String> audioCacheError = std::nullopt;
    std::function<void(const String& msg, const String& file)> onError = [&audioCacheError](const String& msg, const String& file) {
        audioCacheError = "Failed writing " + file + ": " + msg;
    };
    if (ARCHIVE_OK != getAudioCache()->writeToArchive(uniqueSampleIds, ar, onProgress, onError)) {
        if (audioCacheError) {
            return audioCacheError;
        }
        return "Failed to write audio cache to archive";
    }
    std::shared_ptr<project_file> f = createProjectFile();
    std::vector<uint8_t> buffer;
    DAW::ProjectFileV2::saveProject(f, buffer);
    // // add a file to the archive
    struct archive_entry* entry = archive_entry_new();
    if (!entry) {
        return "Failed to create archive entry";
    }
    auto bufSize = int64_t(buffer.size());
    archive_entry_set_pathname(entry, projFileName.c_str());
    archive_entry_set_mtime(entry, time(nullptr), 0);
    archive_entry_set_size(entry, bufSize);
    archive_entry_set_filetype(entry, AE_IFREG);
    archive_entry_set_perm(entry, 0644);
    if (ARCHIVE_OK != archive_write_header(ar, entry)) {
        return "Failed to write archive header";
    }
    auto sizeWritten = archive_write_data(ar, buffer.data(), buffer.size());
    if (sizeWritten != bufSize) {
        return "Failed to write archive data";
    }
    archive_entry_free(entry);
    // finish writing the archive
    if (ARCHIVE_OK != archive_write_close(ar)) {
        return "Failed to close archive";
    }
    if (ARCHIVE_OK != archive_write_free(ar)) {
        return "Failed to free archive";
    }
    return std::nullopt;
}

bool DawInstance::menuCommand(const menucmd_t& command) {
    try {
        auto mainCtrl = tls.mainCtrl;
        switch (command.command) {
            case CMD_IMPORT_TRACK: {
                String path;
                auto importDir = getProjectDirectory();
                if (promptUserFilePath(mainCtrl->window, 0, FILE_TYPES_TRACKSNAPSHOT, path, importDir)) {
                    auto res = DAW::ProjectFileV2::loadTrackContainer(path);
                    std::shared_ptr<trackcontainer_snapshot_t> ctr;
                    if (std::holds_alternative<String>(res)) {
                        log_lf(Log::L_ERROR, "Failed to load track snapshot from %s\n", StringAsCStr(path));
                    } else {
                        ctr = std::get<std::shared_ptr<trackcontainer_snapshot_t>>(res);
                    }
                    if (ctr) {
                        auto* pluginMgr = getPluginManager();
                        ThreadLock lock = getPlayThread()->lockThread();
                        for (track_snapshot_t& ts : ctr->tracks) {
                            DAW::assignFreeStageIdsTrackSnapshot(pluginMgr, ts);
                            ts.trackLoaded = new track_t(ts);
                            addTrackImpl(-1, ts.trackLoaded, FLG_TRK_CHANGE_USER, loadTrackIdSnapshot(ts.stageIds));
                        }

                        //load plugins
                        for (track_snapshot_t& ts : ctr->tracks) {
                            log_printf("track '%s' loading %zu plugins\n", StringAsCStr(ts.trackLoaded->name), ts.data.pluginSnapshots.size());
                            ts.trackLoaded->loadSnapshot(tls.host, ts);
                            std::vector<effectbase*> effects = ts.trackLoaded->audio->deferredEffects;
                            for (auto effect: effects) {
                                pluginMgr->activateDeferred(effect, 0);
                            }
                        }
                        onPluginsChanged();
                        updateVisibleTrackContents();
                    }
                }
                return true;
            }
            case CMD_REACTIVATE_AUTOMATION: {
                ThreadLock lock = playThread.lockThread();
                std::vector<automatable_t*> targets;
                for (auto& track : project.trackList.getAllTracksFlatVecRef()) {
                    targets.clear();
                    track->audio->getAutomatableTrackTargets(targets);
                    for (auto& target : targets) {
                        target->visitAutomatedParams([](auto& param) {
                            param.src.activate();
                        });
                    }
                }
                return true;
            }
            case CMD_SHOW_CONSOLE: {
                showProgramConsole();
                return true;
            }
            case CMD_OPEN_SECOND_WINDOW: {
                if (companionWindows.empty()) {
                    size_t highestIndex = 0;
                    for (auto& companionCtrl : this->dawCtrls) {
                        if (companionCtrl->getDawWindowIndex() > highestIndex) {
                            highestIndex = companionCtrl->getDawWindowIndex();
                        }
                    }
                    auto companionCtrlStdPtr = std::make_shared<CompanionCtrl>(mainCtrl, *this, highestIndex + 1);
                    ivec2 windowSize;
                    mainCtrl->mainWindow->getSize(&windowSize);
                    windowSize = math::maxvec2(windowSize, ivec2(320, 240));
                    auto compWindowNew = mainCtrl->mainWindow->createOverlay(companionCtrlStdPtr, windowSize, WINDOW_STORE_WINDOW_POS_SIZE | WINDOW_IS_MAINWINDOW_SLAVE | WINDOW_IS_RESIZABLE);
                    auto idxOfWindow = companionCtrlStdPtr->getDawWindowIndex();
                    companionWindows.push_back(DawWindowCompanion{ compWindowNew, companionCtrlStdPtr });
                    compWindowNew->initControl();
                    if (companionCtrlStdPtr->isOk()) {
                        companionWindows[0].wnd->show();
                        if (this->layoutsFromProjectFile.size() > idxOfWindow) {
                            companionCtrlStdPtr->loadLayout(this->layoutsFromProjectFile[idxOfWindow]);
                        }
                        companionCtrlStdPtr->fixCursor();
                        companionCtrlStdPtr->updateVisibleTrackContents();
                    }
                    if (companionCtrlStdPtr->isOk()) {
                        this->dawCtrls.push_back(companionCtrlStdPtr.get());
                    }
                } else if (companionWindows.size() && companionWindows[0].ctrl && companionWindows[0].ctrl->isOk()) {
                    companionWindows[0].wnd->show();
                }
                return true;
            }
            case CMD_UNDO:
                if (hist.canUndo()) {
                    ThreadLock lock = playThread.lockThread();
                    hist.undoStep(this);
                    updateVisibleTrackContents();
                }
                return true;
            case CMD_REDO:
                if (hist.canRedo()) {
                    ThreadLock lock = playThread.lockThread();
                    hist.redoStep(this);
                    updateVisibleTrackContents();
                }
                return true;
            case CMD_FILE_NEW: {
                stopPlaying();
                setAudioThreadState(playback_state::status_no_process);
                setEmptyProject();
                layoutTrackEditors();
                updateVisibleTrackContents();
                setAudioThreadState(playback_state::status_stop);
                return true;
            }
            case CMD_FILE_OPEN: {
                if (command.arg1.empty()) {
                    String path;
                    if (promptUserFilePath(mainCtrl->window, 0, FILE_TYPES_PROJECT_LOAD, path, lastProjectDirectory)) {
                        loadFile(path, DAW::PluginLoadFlags::FLAG_INVOKE_USER_CB_DEFERLOAD);
                    }
                } else {
                    loadFile(command.arg1, DAW::PluginLoadFlags::FLAG_INVOKE_USER_CB_DEFERLOAD);
                }
                return true;
            }
            case CMD_BUNDLE_PROJECT_ZIP: {
                String bundlePath;
                if (!promptUserFilePath(tls.mainCtrl->window, 1, FILE_TYPES_BUNDLE, bundlePath, lastProjectDirectory)) {
                    return true;
                }
                String ext;
                SplitPath(bundlePath, nullptr, nullptr, &ext);
                if (ext.empty()) {
                    bundlePath += "." PROJECT_BUNDLE_FILE_EXT;
                }
                projectFileType = PROJECT_FILETYPE_BUNDLE;
                saveFile(bundlePath);
                return true;
            }
            case CMD_BUNDLE_PROJECT_DIRECTORY: {
                String bundlePath;
                if (browseForFolder("Select project folder", projectPath, bundlePath)) {
                    return true;
                }
                String dirName;
                SplitPath(bundlePath, nullptr, &dirName, nullptr, nullptr);
                
                std::vector<int32_t> uniqueSampleIds;
                DAW::GetProjectReferencedSampleIds(project, uniqueSampleIds);
                getAudioCache()->rellocateSamples(uniqueSampleIds, bundlePath);
                projectPath = bundlePath + FILE_PATHSEP_STR + dirName + "." PROJECT_FILE_EXT;
                projectFileType = PROJECT_FILETYPE_JSON;
                saveFile(projectPath);
                return true;
            }
            case CMD_SET_STARTUP_PROJECT:
            case CMD_FILE_SAVEAS:
            case CMD_FILE_SAVE: {
                if (command.command == CMD_SET_STARTUP_PROJECT && !projectPath.empty()) {
                    tls.settings->dawsettings.startupProjectPath = projectPath;
                    saveSettings(*tls.settings);
                    return true;
                }
                String path = projectPath;
                if (command.command == CMD_FILE_SAVEAS || path.empty() || projectFileType == PROJECT_FILETYPE_JSON_LEGACY) {
                    if (projectFileType == PROJECT_FILETYPE_JSON_LEGACY) {
                        // update extension
                        String ext;
                        SplitPath(path, nullptr, nullptr, &ext);
                        if (ext == PROJECT_LEGACY_FILE_EXT) {
                            path = path.substr(0, path.size() - strlen(PROJECT_LEGACY_FILE_EXT)) + PROJECT_FILE_EXT;
                        }
                    }
                    if (!promptUserFilePath(mainCtrl->window, 1, FILE_TYPES_PROJECT_SAVE, path, lastProjectDirectory)) {
                        return true;
                    }
                    String ext;
                    SplitPath(path, nullptr, nullptr, &ext);
                    if (ext.empty()) {
                        path += "." PROJECT_FILE_EXT;
                    }
                }
                saveFile(path);
                if (command.command == CMD_SET_STARTUP_PROJECT && !projectPath.empty()) {
                    tls.settings->dawsettings.startupProjectPath = projectPath;
                    saveSettings(*tls.settings);
                }
                return true;
            }
            case CMD_INSERT_AUDIO_TRACK:
            case CMD_INSERT_MIDI_TRACK:
            case CMD_INSERT_RETURN_TRACK:
            case CMD_INSERT_MASTER_TRACK: {
                
                int32_t trackType = (command.command - CMD_INSERT_MASTER_TRACK) % NUM_TRACK_TYPES;
                insertNewTrack(-1, trackType);
                return true;
            }
            case CMD_ABOUT:
                mainCtrl->openDialog(new guidialog_about());
                return true;
            case CMD_SHOW_DEBUG_WINDOW:
                if (command.argInt == 3) {

                    auto guidialog  = new guidialog_about();
                    auto popupCtrl = std::make_shared<PopupCtrl>(mainCtrl);
                    popupCtrl->setDawCtrl(mainCtrl);
                    popupCtrl->m_scale = mainCtrl->m_scale;
                    popupCtrl->m_size = math::maxvec2(ivec2(20, 20), guidialog->size);
                    *popupCtrl->getTheme() = *mainCtrl->getTheme();
                    const ivec2 windowSize = ivec2(vec2(popupCtrl->m_size) * popupCtrl->m_scale);
                    popupCtrl->setWindowName(guidialog->getLabel());
                    auto dialogWindow = mainCtrl->mainWindow->createOverlay(popupCtrl, windowSize, 0);
                    
                    dialogWindow->setSizeLimits(windowSize, windowSize);

                    companionWindows.push_back(DawWindowCompanion{ dialogWindow, popupCtrl });

                    if (popupCtrl->isOk()) {
                        ivec2 wndPos(0);
                        determineWindowPos(guidialog, mainCtrl->mainWindow, mainCtrl->m_scale, 0, ivec2(0), wndPos);
                        popupCtrl->open(guidialog, wndPos, true, true);
                    }
                    return true;
                }
#if CREATE_DEBUG_COMPANION_WINDOW
                if (command.argInt == 0) {
                    window_dialog* dialog = mainCtrl->mainWindow->createDialog("waveform atlas cache", 1280, 720, getWindowDebugWaveformCache());
                    dialog->show();
                    return true;
                }
                if (command.argInt == 1) {
                    window_dialog* dialog = mainCtrl->mainWindow->createDialog("nanovg debug", 1280, 720, getWindowDebugNanoVG());
                    dialog->show();
                    return true;
                }
                if (command.argInt == 2) {
                    window_dialog* dialog = mainCtrl->mainWindow->createDialog("performance graphs", 1280, 720, getWindowPerf());
                    dialog->show();
                    return true;
                }
#endif
                return true;
            case CMD_PREFERENCES:{
                auto dialog = new DAW::DialogSettings::guidialog_settings(this);
                if (mainCtrl->openDialog(dialog)) {
                    dialog->setActiveEntry(command.argInt);
                }
                return true;
            }
            case CMD_EXPORT_AUDIO: {
                mainCtrl->openDialog(DAW::UI::makeGuiExportDialog(create_ctr_t{this}));
                return true;
            }
            case CMD_EXIT:
                mainCtrl->mainWindow->requestClose();
                return true;
        }
    } catch (std::exception& e) {
        handleStdException(e);
    }
    return false;
}

void DawInstance::unloadUnreferencedSamples() {
    std::vector<int32_t> uniqueSampleIds;
    DAW::GetProjectReferencedSampleIds(project, uniqueSampleIds);
    log_lf(Log::L_DEBUG, "Found %zu sample ids\n", uniqueSampleIds.size());
    tls.audioCache->unloadUnreferenced(uniqueSampleIds);
}

void DawInstance::startDaw() {
    dbgassert(initState == 1);
    initState++;
    plugindb.openDatabase();
    grooves.loadGrooves();
}

void DawInstance::initDaw() {
    dbgassert(initState == 0);
    initState++;
    //TODO allow passing in optional tls or refactor loadSettings in seperate function
    bool isAlreadyInitialized = daw_tls::isTlsInitialized();
    auto& initTls = isAlreadyInitialized ? daw_tls::getTls() : daw_tls::initNewTls();
    auto& settings = *initTls.settings;

    if (!isAlreadyInitialized) {
        try {
            loadSettings(settings);
        } catch (std::exception& e) {
            log_lf(Log::L_ERROR, "Failed loading settings %s: %s\n", StringAsCStr(App::Platform::toUserdataPath(SETTINGS_NAME)), e.what());
            ngui::showNotification(ngui::Style::Warning, "Couldn't read config file", "Some settings may have been reset");
        }
    }
    initTls.dawInstance = this;
    initTls.host = new DAW::Host::Host();
    initTls.pluginManager = initTls.host;
    if (!DAW::Host::PluginManager::assignMasterCallback(initTls.pluginManager)) {
        delete initTls.host;
        dbgassert(0);
        throw applogicexception("no empty vst callback slot");
    }

    initTls.project        = this;
    initTls.audioHost      = new audiohost();
    initTls.midiHost       = new midihost();
    initTls.pluginDatabase = &plugindb;
    initTls.audioCache     = new audiocache(settings.iosettings.samplerate);
    initTls.host->setTls(initTls);
#ifndef NDEBUG
    // initTls.host->addAuxOutput(&this->auxSourceNoise);
#endif
    this->tls = initTls;

    setSSEFlushDenormals();
    initTls.host->setSampleFormat(sampleformat_t{
        static_cast<samplerate_t>(settings.iosettings.internalSamplerate),
        settings.iosettings.internalBlocksize,
        sampleformat_bits_t::FLOAT_32
    });
}

void DawInstance::updateClipViews(clip_t* notifyClip) {
    for (auto* ctrl : dawCtrls) {
        ctrl->updateClipViews(notifyClip);
    }
}


void DawInstance::updateClipViewsAndCursor(clip_t* notifyClip, clip_cursor_t cursor) {
    for (auto* ctrl : dawCtrls) {
        ctrl->updateClipViewsAndCursor(notifyClip, cursor);
    }
}

void DawInstance::resetMouseContext() {
    for (auto* ctrl : this->dawCtrls) {
        ctrl->resetMouseContext();
    }
}

void DawInstance::closeContextMenus() {
    for (auto* ctrl : this->dawCtrls) {
        ctrl->closeContextMenu();
    }
}

void DawInstance::closeDialogs() {
    for (auto* ctrl : this->dawCtrls) {
        ctrl->closeDialogs();
    }
}

void DawInstance::resetAutomationContext() {
    for (auto* ctrl : this->dawCtrls) {
        ctrl->resetAutomationContext();
    }
}

void DawInstance::resetClipViews() {
    for (auto* dawctrl : dawCtrls) {
        dawctrl->resetClipViews();
    }
}

void DawInstance::updateVisibleTrackContents() {
    for (DawCtrl* pDawCtrl : dawCtrls) {
        dbgassert(pDawCtrl->isOk());
        pDawCtrl->updateVisibleTrackContents();
    }
}

void DawInstance::layoutTrackEditors() {
    for (DawCtrl* pDawCtrl : dawCtrls) {
        dbgassert(pDawCtrl->isOk());
        pDawCtrl->layoutView();
    }
}

void DawInstance::setSingleClip(clip_t* clip) {
    for (auto* ctrl : this->dawCtrls) {
        ctrl->setSingleClip(clip);
    }
}

void DawInstance::setEditorSelection(clip_t* clip, const editor_view_selection_t& clipboardView) {
    for (auto* ctrl : this->dawCtrls) {
        ctrl->setEditorSelection(clip, clipboardView);
    }
}

void DawInstance::setAsyncTask(DAW::async_task_t* task) {
    asyncTask = task;
    for (auto& ctrl : dawCtrls) {
        ctrl->setAsyncTask(task);
    }
}

void DawInstance::refreshAllUserlibraryBrowsers() {
    for (auto* dawctrl : dawCtrls) {
        dawctrl->refreshAllUserlibraryBrowsers();
    }
}

void DawInstance::setMainControl(MainCtrl* _mainCtrl) {
    dbgassert(!tls.mainCtrl);
    tls.mainCtrl = _mainCtrl;
    daw_tls::getTls().mainCtrl = tls.mainCtrl;
    tls.host->setTls(tls);
    this->dawCtrls.push_back(tls.mainCtrl);
}

void DawInstance::setEmptyClipboard() {
    clipboardType    = CLIPBOARD_NONE;
    clipboardPlugins = std::make_shared<plugin_clipboard_t>();
    clipboardClips   = std::make_shared<clip_clipboard>();
    clipboardNotes   = std::make_shared<notes_clipboard>();
}

/**
 * setLoadedProject - releases current project and resources and loads in new project from passed project_file
 *
 * - puts audio thread into state playback_state::status_no_process
 * - establishes lock against AudioThread
 * - unloads project (freeing resources)
 * - loads samplefile index
 * - populates tracklist
 * - creates audio instances for all tracks
 * - adds tracks to MainCtrls guictr_tracks, creating gui instances
 * - pre loads plugins
 * - optionally fully loads plugin instances
 * - loads track layouts
 * - loads cursor state
 * - sets project_file::path as current project path
 * - puts audio thread into state playback_state::status_stop
 *
 * @param file - shared_ptr to project_file instance containg project data to load from
 * @param flags - 0 or DAW::PluginLoadFlags::FLAG_DEFER_LOAD (don't load vst plugins, use placeholders)
 * @return
 */
bool DawInstance::setLoadedProject(const std::shared_ptr<project_file>& file, int flags) {
    ThreadLock lock = playThread.lockThread();
    loadProject0(file);
    bool b = loadProject1(file, flags);
    loadProjectFinish();
    return b;
}

void DawInstance::loadProject0(const std::shared_ptr<project_file>& spFile) {
    auto file = spFile.get();
    setAudioThreadState(playback_state::status_no_process);
    log_printf("Loading project %s: %zu tracks\n", StringAsCStr(file->path), project.trackList.size());
    unloadProject();
    /** make sure call to unloadProject unloaded all vst2 instances */
    dbgassert(tls.host->getNumAudioStages() == 0);
    dbgassert(tls.host->getVst2Instances().empty());
    //TODO: assert that audiocache is empty
    dbgassert(tls.audioCache->isEmpty());
#ifndef NDEBUG
    tls.host->validateIds();
#endif

    String loadFileExt, loadFileDirectory;
    SplitPath(file->path, &loadFileDirectory, nullptr, &loadFileExt);
    if (loadFileExt == PROJECT_LEGACY_FILE_EXT) {
        this->projectFileType = PROJECT_FILETYPE_JSON_LEGACY;
    } else if (loadFileExt == PROJECT_BUNDLE_FILE_EXT) {
        this->projectFileType = PROJECT_FILETYPE_BUNDLE;
    } else {
        this->projectFileType = PROJECT_FILETYPE_JSON;
    }

    /** set as current project */
    this->projectPath = file->path;
    lastProjectDirectory = loadFileDirectory;
    this->layoutsFromProjectFile = file->layouts;

    /** populates trackList */
    project.copyFrom(file->project);

    projectGlobals      = file->project.globals;
    getExportSettings() = file->project.exportSettings;
    getQuantizeSettings() = file->project.quantizeSettings;

    /** load track snapshots */
    project.trackList.loadProjectSnapshot(tls.host, file->project);

    // restore soloed tracks and record armed tracks
    for (auto track : project.trackList.getAllTracksFlatVecRef()) {
        bool isSolo = STL_CONTAINS(file->project.solodTracks, track->audio->stageId.stageId);
        bool isArmed = STL_CONTAINS(file->project.recordArmedTracks, track->audio->stageId.stageId);
        if (isSolo) {
            track->audio->flags |= audiostageflags_t::SOLO;
        }
        if (isArmed) {
            track->audio->flags |= audiostageflags_t::RECORD_ARMED;
        }
    }

    /** fix audio clip lengths */
    for (track_t* t : project.trackList) {
        t->updateAudioClipLengths(projectGlobals.tempo100, file->project.samplerate, tls.host->m_sampleFormatInternal.sampleRate);
    }

    /** create all gui instances */
    for (DawCtrl* pDawCtrl : dawCtrls) {
        for (track_t* tr : project.trackList) {
            pDawCtrl->addTrackToView(tr, FLG_TRK_CHANGE_LOAD);
        }
    }
    
#ifndef NDEBUG
    tls.host->validateIds();
#endif
    onPluginsChanged();

    /** reset maximum stage id and determine new maximum stage id */
    tls.host->updateMaximumStageId();

    /** remove routings to missing track */
    DAW::validateTrackRoutings(tls.host, project.getTracksFlatVec());
    /** create all gui instances */
    for (track_t* tr : project.trackList) {
        DAW::validateEffectRoutings(tls.host, tr->audio);
    }

    /** inform host about track layout changes so it resets and updates internal structures */
    tls.host->onTrackLayoutChange();
}

bool DawInstance::loadProject1(const std::shared_ptr<project_file>& file, int flags) {
    /**
     * The following loop calls activateDeferred on all tracks, effectively doing the following sequence for each track:
     *  - load shared libraries
     *  - create audioeffect instance
     *  - load binary plugin snapshots
     *  - load plugin, mixer, arp parameter values
     *  - load plugin, mixer, arp automation
     *
     * plugin loading can take a long time and will block the main thread.
     * Ideally this would happen on another thread, but that might not work for all vst plugins.
     */
    std::vector<effectbase*> pluginsDeferred;
    tls.host->getDeferredEffects(pluginsDeferred);

    if ((flags & DAW::PluginLoadFlags::FLAG_DEFER_LOAD) == 0) {
         auto len = pluginsDeferred.size();
        for (size_t i = 0; i < len; i++) {
            dbgassert(pluginsDeferred[i]->getModuleType() == MODULE_TYPE_DEFERRED);
            auto plugin = dynamic_cast<effect_deferred*>(pluginsDeferred[i]);
            effectbase* pluginLoaded = nullptr;
            tls.host->activateDeferred(plugin, 0, &pluginLoaded);
            (void) pluginLoaded;
        }
    }

    onPluginsChanged();

    tls.audioCache->load(file->sampleFileIndex, projectFileType, file->path, lastProjectDirectory);
    for (track_t* tr : project.trackList) {
        tr->getStage()->pluginsChanged();
    }
    tls.host->onTrackLayoutChange();
    /** load layout data */
    for (auto& dawCtrl : dawCtrls) {
        auto index = dawCtrl->getDawWindowIndex();
        if (index < this->layoutsFromProjectFile.size()) {
            dawCtrl->loadLayout(this->layoutsFromProjectFile[index]);
        }
    }
    updateVisibleTrackContents();
    return true;
}

void DawInstance::loadProjectFinish() {
    /** reset maximum stage id and determine new maximum stage id */
    tls.host->updateMaximumStageId();

    onPluginsChanged();
    for (DawCtrl* pDawCtrl : dawCtrls) {
        pDawCtrl->fixCursor();
    }
    tls.settings->recentfiles.add(projectPath);
    this->tmLastSave  = getTimeMillis();
    String s = StringFormat("Loaded project %s", StringAsCStr(this->projectPath));
    log_lf(Log::L_INFO, "%s\n", StringAsCStr(s));
    if (tls.mainCtrl) {
        tls.mainCtrl->setStatusText(s);
        String projectFileName;
        SplitPath(this->projectPath, nullptr, &projectFileName, nullptr);
        tls.mainCtrl->setWindowName(StringFormat("%s - %s", BuildInfo::PRODUCT_NAME_DISPLAY, StringAsCStr(projectFileName)));
    }

    setAudioThreadState(playback_state::status_stop);
    if (cbProjectLoadCompleteCallback) {
        auto projectLoadErrored = false;
        cbProjectLoadCompleteCallback(this, projectLoadErrored ? 1 : 0);
        cbProjectLoadCompleteCallback = nullptr;
    }
    updateVisibleTrackContents();
}


track_t* DawInstance::createNewTrack(int trackType) {
    dbgassert(trackType >= 0 && trackType < NUM_TRACK_TYPES);
    int32_t tryTypeOffset = project.trackTypeCtrs[trackType]->size();

    String name       = StringFormat("%s %d", TrackTypeToName(trackType), tryTypeOffset + 1);
    track_t* newTrack = new track_t(trackType, name, true);
    newTrack->rgb     = colorPalette[rand.rng_rand(COLOR_PALETTE_LEN)];
    return newTrack;
}

track_t* DawInstance::insertNewTrack(int trackInsertPos, int trackType, int flags) {
    track_t* newTrack = createNewTrack(trackType);
    ThreadLock lock   = playThread.lockThread();
    addTrackImpl(trackInsertPos, newTrack, flags);
    return newTrack;
}

class action_modify_track_add final : public action_base {
public:
    int32_t trackIdx  = -1;
    int32_t localIdx  = -1;
    int32_t parentIdx = -1;
    int32_t childIdxTree = -1;
    track_t* trackPtr;
    action_modify_track_add() = delete;

    action_modify_track_add(String description, track_t* _trackPtr) : action_base() {
        desc     = std::move(description);
        trackPtr = nullptr;
        trackIdx = _trackPtr->projectIdx;
        localIdx = _trackPtr->localIdxFlat;
        if (_trackPtr->parent) {
            parentIdx = _trackPtr->parent->projectIdx;
        }
        childIdxTree = _trackPtr->childIdxTree;
    }

    ~action_modify_track_add() override = default;

    void releaseResources(DawInstance* daw) override {
        if (trackPtr) {
            releaseTrackResources(trackPtr, daw);
            delete trackPtr;
            trackPtr = nullptr;
        }
    }

    void undo(DawInstance* daw) override {
        daw->resetMouseContext();
        daw->resetClipViews();
        trackPtr = daw->getTrackId(trackIdx);
        dbgassert(trackPtr);
        dbgassert(localIdx == trackPtr->localIdxFlat);
        if (trackPtr->parent) {
            parentIdx = trackPtr->parent->projectIdx;
        } else {
            parentIdx = -1;
        }
        childIdxTree = trackPtr->childIdxTree;
        localIdx = trackPtr->localIdxFlat;
        daw->removeTrackImpl(trackPtr, FLG_TRK_CHANGE_HISTORY_UNDO);
    }

    void redo(DawInstance* daw) override {
        dbgassert(trackPtr);
        // daw->resetMouseContext();
        daw->resetClipViews();
        auto parent = parentIdx >= 0 ? daw->getTrackId(parentIdx) : nullptr;
        trackPtr->childIdxTree = childIdxTree;
        if (parent) {
            parent->addChild(trackPtr);
        }
        daw->addTrackImpl(localIdx, trackPtr, FLG_TRK_CHANGE_HISTORY_UNDO);
        dbgassert(localIdx == trackPtr->localIdxFlat);
        localIdx = trackPtr->localIdxFlat;
        trackPtr = nullptr;
    }
};

class action_modify_track_remove final : public action_base {
public:
    int32_t trackIdx = -1;
    int32_t localIdx = -1;
    int32_t parentIdx = -1;
    int32_t childIdxTree = -1;
    track_t* trackPtr;
    std::vector<DAW::removed_track_routings> removedRoutings;

    action_modify_track_remove() = delete;

    action_modify_track_remove(String description, track_t* _trackPtr, int32_t _parentIdx, int32_t _childIdxTree, const std::vector<DAW::removed_track_routings>& _removedRoutings) : action_base() {
        desc     = std::move(description);
        trackPtr = _trackPtr;
        trackIdx = _trackPtr->projectIdx;
        localIdx = _trackPtr->localIdxFlat;
        parentIdx = _parentIdx;
        childIdxTree = _childIdxTree;
        removedRoutings = _removedRoutings;
    }

    ~action_modify_track_remove() override = default;

    void releaseResources(DawInstance* daw) override {
        if (trackPtr) {
            releaseTrackResources(trackPtr, daw);
            delete trackPtr;
            trackPtr = nullptr;
        }
    }

    void undo(DawInstance* daw) override {
        String name = trackPtr->name;
        daw->resetMouseContext();
        daw->resetClipViews();
        auto parent = parentIdx >= 0 ? daw->getTrackId(parentIdx) : nullptr;
        trackPtr->childIdxTree = childIdxTree;
        if (parent) {
            parent->addChild(trackPtr);
        }
        daw->addTrackImpl(localIdx, trackPtr, FLG_TRK_CHANGE_HISTORY_UNDO);
        dbgassert(localIdx == trackPtr->localIdxFlat);
        localIdx = trackPtr->localIdxFlat;
        trackPtr = nullptr;
        auto host = daw->getHost();
        for (auto& routing : removedRoutings) {
            if (routing.inputChannel) {
                auto stage = host->getAudioStage(routing.stageRef);
                if (stage && stage->getTrack()) {
                    stage->getTrack()->getStage()->inputChannel = *routing.inputChannel;
                }
            }
            if (routing.outputChannel) {
                auto stage = host->getAudioStage(routing.stageRef);
                if (stage && stage->getTrack()) {
                    stage->getTrack()->getStage()->outputChannel = *routing.outputChannel;
                }
            }
            for (auto& midiInput : routing.midiInputChannels) {
                auto stage = host->getAudioStage(routing.stageRef);
                if (stage && stage->getTrack()) {
                    stage->getTrack()->getStage()->midiInputChannels.push_back(midiInput);
                }
            }
        }
    }

    void redo(DawInstance* daw) override {
        daw->resetMouseContext();
        daw->resetClipViews();
        trackPtr = daw->getTrackId(trackIdx);
        if (trackPtr->parent) {
            parentIdx = trackPtr->parent->projectIdx;
        } else {
            parentIdx = -1;
        }
        dbgassert(trackPtr);
        daw->removeTrackImpl(trackPtr, FLG_TRK_CHANGE_HISTORY_UNDO);
        dbgassert(localIdx == trackPtr->localIdxFlat);
    }
};

void DawInstance::addTrackImpl(int32_t trackInsertPos, track_t* newTrack, int flags, std::optional<audio_stage_id_t> stageId) {
    project.trackList.addTrack(trackInsertPos, newTrack);
    if ((flags & FLG_TRK_CHANGE_HISTORY_UNDO) != 0) {
        dbgassert(newTrack->audio);
    } else {
        dbgassert(!newTrack->audio);
        tls.host->createAudio(newTrack, stageId);
    }
    for (DawCtrl* pDawCtrl : dawCtrls) {
        if (pDawCtrl->isOk()) {
            pDawCtrl->addTrackToView(newTrack, flags);
        }
    }
    if (flags & FLG_TRK_CHANGE_USER) {
        pushHist(new action_modify_track_add(StringFormat("Add %s Track", TrackTypeToName(newTrack->type)), newTrack));
    }

    tls.host->onTrackLayoutChange();
}

void DawInstance::removeTrackId(uint32_t trackId) {
    if (project.trackList.validTrackIdx(trackId)) {
        auto tr = project.trackList[trackId];
        // delete children recursively
        for (auto& child : tr->children) {
            removeTrackId(child->projectIdx);
        }
        removeTrackImpl(tr, FLG_TRK_CHANGE_USER);
    }
}

void DawInstance::removeTrackImpl(track_t* track, int flags) {
    resetClipViews();
    auto parentId = track->parent ? track->parent->projectIdx : -1;
    auto childTreeIdx = track->childIdxTree;
    for (DawCtrl* pDawCtrl : dawCtrls) {
        if (pDawCtrl->isOk()) {
            pDawCtrl->closeAllContextMenus();
        }
    }
    project.trackList.removeTrack(track);
    for (DawCtrl* pDawCtrl : dawCtrls) {
        if (pDawCtrl->isOk()) {
            pDawCtrl->removeTrackFromView(track, flags);
        }
    }
    std::vector<DAW::removed_track_routings> removedRoutings;
    auto v = DAW::removeTrackRoutings(project.getTracksFlatVec(), track->audio->stageId.stageId);
    removedRoutings.insert(removedRoutings.end(), v.begin(), v.end());
    v = DAW::removeTrackRoutings(project.getTracksFlatVec(), track->audio->stageId.inputStageId);
    removedRoutings.insert(removedRoutings.end(), v.begin(), v.end());
    v = DAW::removeTrackRoutings(project.getTracksFlatVec(), track->audio->stageId.outputStageId);
    removedRoutings.insert(removedRoutings.end(), v.begin(), v.end());
    v = DAW::removeTrackRoutings(project.getTracksFlatVec(), track->audio->stageId.outputPostStageId);
    removedRoutings.insert(removedRoutings.end(), v.begin(), v.end());
    if (flags & FLG_TRK_CHANGE_USER) {
        pushHist(new action_modify_track_remove(StringFormat("Remove %s Track", TrackTypeToName(track->type)), track, parentId, childTreeIdx, removedRoutings));
    }
    tls.host->onTrackLayoutChange();
}

track_t* DawInstance::getTrackId(uint32_t trackId) {
    return project.trackList[trackId];// operator[] returns nullptr on oob
}

void DawInstance::preClipDelete(clip_t* clip) {
    // resetClipViews();
    //resetMouseContext();
}

void DawInstance::preTrackDelete(track_t* track) {
    resetMouseContext();
    resetClipViews();
}

void DawInstance::setTempo(int32_t _tempo100) {
    playThread.call([this, _tempo100]() {
        projectGlobals.tempo100 = CLAMP_I(_tempo100, 100, 99900);
        auto sr = daw_tls::getTls().host->m_sampleFormatInternal.sampleRate;
        for (track_t* t : project.trackList) {
            t->updateAudioClipLengths(projectGlobals.tempo100, sr, sr);
        }
    }, true);
}

void DawInstance::initProcessingResources() {
    dbgassert(initState == 2);
    initState++;
    DAW::PythonNoteProcessor::Init();
    tls.host->initThreads();
}

void DawInstance::initRealtimeResources() {
    dbgassert(initState == 3);
    initState++;
    if (tls.runtime->enableAudioIO) {
        tls.audioHost->initPa();
    }
    tls.midiHost->initPm();
    tls.host->setLowLatencyMode(tls.settings->dawsettings.lowLatencyMode);
    if (tls.settings->dawsettings.audioEnabled) {
        if (tls.audioHost->startAudio(tls.settings->iosettings)) {
            auto stream = tls.audioHost->getStreamSharedPtr(0);
            tls.host->setOutput(stream);
        } else {
            //notify user
            log_lf(Log::L_ERROR, "audioHost->startAudio() failed\n");
        }
    }
    tls.midiHost->startMidi();

    this->playThread.setTls(tls);
    this->playThread.startThread(this);
    dbgassert(this->playThread.getState() == playback_state::status_no_process);


    this->workerThread.setTls(tls);
    this->workerThread.startThread("File Loader Thread", seqthreads::ThreadType::WorkerThread);

    setAudioThreadState(playback_state::status_stop);
}
std::pair<String, String> DawInstance::createUniqueNonExistingFilename(const String& filePath) {
    String absFilePath = filePath;
    App::Platform::sanitizePathToFile(absFilePath);
    String name;
    String ext;
    String path;
    int32_t idx = 0;
    String uniqueFilePath = absFilePath;
    SplitPath(absFilePath, &path, &name, &ext);
    App::Platform::sanitizePathToDirectory(path);
    String uniqueFileName = name;
    while ((FileExists(uniqueFilePath) || tls.audioCache->getByFilename(uniqueFilePath) != nullptr) && ++idx < 10000) {
        uniqueFileName = name;
        uniqueFileName += "-";
        uniqueFileName += std::to_string(idx);
        uniqueFileName += ".";
        uniqueFileName += ext;
        uniqueFilePath = path;
        uniqueFilePath += uniqueFileName;
    }
    return {uniqueFilePath, uniqueFileName};
}
std::pair<String, String> DawInstance::createUniqueNonExistingProjectFilename(const String& baseDir, const String& trackName, const String& sampleName, const String& fileExt) {
    String uniqueFileName;
    if (!trackName.empty()) {
        uniqueFileName += trackName;
        uniqueFileName += " - ";
    }
    uniqueFileName += sampleName;

    String pathInput = baseDir;
    pathInput += FILE_PATHSEP_CHAR;
    String projName = getProjectName();
    if (projName.empty()) {
        projName = "Untitled";
    }
    pathInput += projName;
    pathInput += FILE_PATHSEP_CHAR;
    pathInput += uniqueFileName;
    pathInput += ".";
    pathInput += fileExt;

    String sampleFilePath = App::Platform::toUserdataPath(pathInput);
    return createUniqueNonExistingFilename(sampleFilePath);
}

void DawInstance::onPrePreDestroy() {
    dbgassert(initState > 2);
    const bool isRealtimeInstance = initState > 3;
    initState = -1;

    if (isRealtimeInstance) {
        setAudioThreadState(playback_state::status_no_process);
        tls.midiHost->stopMidi();
        tls.audioHost->stopAudio();
    }
    projectToLoad = nullptr;
    clipboardPlugins = nullptr;
    dragdropclip.reset();
    plugindb.closeDatabase();

    if (isRealtimeInstance) {
        this->workerThread.stopThread();
        this->workerThread.joinThread();
        this->playThread.stopThread();
        this->playThread.joinThread();
        tls.audioHost->deinitPa();
        tls.midiHost->deinitPm();
    }
}

void DawInstance::onPreDestroy() {
    tls.host->unload();
    tls.audioCache->unloadAll();
    tls.host->destroy();

    if (DAW::IsPythonInitialized()) {
        DAW::DeinitPythonInterpreter();
    }
}

void DawInstance::destroy() {
#ifndef NDEBUG
        for (auto& companion : companionWindows) {
            dbgassert(!companion.ctrl->isOk());
        }
#endif // NDEBUG
    companionWindows.clear();
    try {
        if (tls.settings->saveOnExit) {
            saveSettings(*tls.settings);
        }
    } catch (std::exception& e) {
        log_lf(Log::L_ERROR, "Failed saving settings %s: %s\n", StringAsCStr(App::Platform::toUserdataPath(SETTINGS_NAME)), e.what());
        ngui::showNotification(ngui::Style::Warning, "Couldn't write config file", "Some settings may have been reset");
    }
    delete tls.commandManager;
    delete tls.settings;
    delete tls.audioCache;
    delete tls.midiHost;
    delete tls.audioHost;
    delete tls.host;
    tls.runtime->safeRefs.onPreDestroy();
    delete tls.runtime;
    tls.dawInstance    = nullptr;
    tls.host           = nullptr;
    tls.pluginManager  = nullptr;
    tls.runtime        = nullptr;
    tls.settings       = nullptr;
    tls.midiHost       = nullptr;
    tls.audioHost      = nullptr;
    tls.mainCtrl       = nullptr;
    tls.project        = nullptr;
    tls.pluginDatabase = nullptr;
    tls.audioCache     = nullptr;
    tls.commandManager = nullptr;
    tls.tlsInitialized = false;
    daw_tls::setTls(tls);
    printClipAllocations();
    printLeakedAudioBuffers();
    int totalAllocs = getNumClipAllocations();
    if (totalAllocs != 0) {
        log_printf("getNumClipAllocations == %d!\n", totalAllocs);
        dbgassert(getNumClipAllocations() == 0);
    }
}

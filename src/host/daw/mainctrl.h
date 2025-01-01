#pragma once
#include <list>
#include <optional>
#include <utility>
#include <vector>
#include <set>
#include "assert_dbg.h"
#include "commands.h"
#include "gui/gui.h"
#include "gui/container/container.h"
#include "gui/dialog/dialog.h"
#include "gui/views/asynctask.h"
#include "host/daw/daw_async_task.h"
#include "host/project/project.h"
#include "saferef.h"
#include "types.h"
#include <memory>

#include "config.h"
#include "math/vec.h"
#include "math/seq_math.h"
#include "str_util.h"
#include "seq_util.h"
#include "seq_time.h"
#include "basectrl.h"
#include "window.h"
#include "menu.h"
#include "mouse.h"
#include "keyboard.h"
#include "event.h"
#include "grid.h"
#include "cursor.h"
#include "host/track/track.h"
#include "host/clip/clip.h"
#include "clipboard.h"
#include "note.h"
#include "logging.h"
#include "host/automation/automation.h"
#include "gui/automation/modulation.h"
#include "threads/workerthread.h"
#include "threads/playbackthread.h"
#include "edithistory.h"
#include "file/projectfile.h"
#include "hires_timer.h"
#include "host/plugindatabase/plugindatabase.h"
#include "rand.h"
#include "host/project/projectcontroller.h"
#include "dragdrop.h"
#include "gui/container/container_dnd_layout.h"
#include "gui/controls/draggedfiles.hpp"
#include "buildinfo.h"
#include "host/aux-source.hpp"

struct automatable_t;
struct KeyEvent;
struct MouseEvent;
struct NVGcontext;

class guictr_plugins;
class guictr_pluginview;
class guitrack_editor;
class guiplugin;
class guictr_test;
class guictr_daw_controls;
class guictr_tracks;
class guictr_nodes_splitview;
class gui_statusbar;
class gui_notify;
class gui_dragged_files;
class guictr_clipeditor;
class guictr_clipeditorview;
class guictxtmenu_base;
class appwindow_main;
class DawViewContainers;
class DawViewContainersMain;
class track_gui_manager_i;
class track_gui_manager_t;

enum clip_dragtype_t {
    DRAG_NONE,
    DRAG_CLIPS_MOVE,
    DRAG_CLIPS_COPY,
    DRAG_CLIPS_RESIZE_LEFT,
    DRAG_CLIPS_RESIZE_RIGHT,
    DROP_FILE_EXTERNAL
};

struct clip_dragaction {
    clip_dragtype_t dragtype = DRAG_NONE;
    std::shared_ptr<clip_clipboard> clipboard;
    DAW::Cursor cursorBegin;
};

struct dragdrop_file {
    enum Type {
        TYPE_NONE,
        TYPE_AUDIOFILE,
        TYPE_CLIP,
        TYPE_TRACK_CONTAINER,
        TYPE_PLUGIN_PRESET,
        TYPE_DIRECTORY
    } type = TYPE_NONE;
    enum State {
        STATE_NONE,
        STATE_FAILED_LOADING,
        STATE_LOADED
    } state = STATE_NONE;
    String path;
    std::shared_ptr<plugin_snapshot_t> pluginSnapshot;
    std::shared_ptr<trackcontainer_snapshot_t> trackcontainer;
    std::shared_ptr<clip_clipboard> clipboard;
    bool isValidTarget = false;
    SafeRef<guibase> target;
    void reset();
};

class plugin_selection {
public:
    int32_t firstSelection    = -1;
    int32_t lastSelection     = -1;
    guictr_plugins* pluginCtr = nullptr;
    bool hasSelection() const;
    int32_t getSelectionCount() const {
        return hasSelection() ? lastSelection - firstSelection + 1 : 0;
    }
    void clear() {
        firstSelection = -1;
        lastSelection  = -1;
        // pluginCtr      = nullptr;
    }
};

namespace DAW {
    struct load_project_task;
    class LoadAudioTask;
    class ProcessClipAudioThreadTask;
    class SearchFileTask;
    std::shared_ptr<clip_clipboard> copySelection(const track_gui_manager_i& trackList, const Cursor& _cursor, bool copyAutomation);
    std::shared_ptr<clip_clipboard> consolidateClipboard(std::shared_ptr<clip_clipboard>& clipboardIn, const Cursor& _cursor);
    void pasteFullClipboard(DawInstance* daw, track_gui_manager_i& trackList, clip_clipboard* clipboard, int32_t track, tick_t tick, bool pasteAutomation);
    void pasteClipboard(DawInstance* daw, track_gui_manager_i& trackList, clip_clipboard* clipboard, Cursor& cursor, bool pasteAutomation);
    void cutSelection(DawInstance* daw, track_gui_manager_i& trackList, const Cursor& cursor, bool cutAutomation);
    bool isSelectionEmpty(const track_gui_manager_i& trackList, const DAW::Cursor& _cursor, bool bIgnoreAutomation);
    void GetProjectReferencedSampleIds(const project_t& project, std::vector<int32_t>& uniqueSampleIds);
    String MakeUniqueTrackName(project_t* project, const String& strNewName);
    void OpenFloatingTextInput(AppCtrl* ctrl, ivec2 popupPos, ivec2 popupSize, const String& initialStr, const std::function<bool(const String& str)>& callback);
    void OpenRenameTrackPopup(DawCtrl* ctrl, track_gui_entry_t* trackentry);
    bool OpenRenameAbsoluteFilePopup(AppCtrl* ctrl, ivec2 popupPos, ivec2 popupSize, const String& pathAbs, std::function<bool(const String& str)> callback);
    void GetClipboardView(const track_gui_manager_i& trackList, const DAW::Cursor& cursor, editor_view_selection_t& view, gui_clip* contextClip);
    void deleteTime(DawInstance* daw, track_gui_manager_i& trackList, const DAW::Cursor& _cursor);
    void insertTime(DawInstance* daw, track_gui_manager_i& trackList, const DAW::Cursor& _cursor, int32_t len);


    class SearchFileTask final : public WorkerThread::ThreadTask {
        std::atomic<bool> m_cancelled{};

        std::vector<String> directories;
        std::vector<String> fileExtensions;
        std::vector<String> searchTerms;

        std::vector<FileFound> filesFound;
        size_t maxFiles = 1000;

    public:
        ~SearchFileTask() {
        }
        void setMaxFiles(size_t _maxFiles) {
            maxFiles = _maxFiles;
        }
        void setSearchOptions(const std::vector<String>& _directories, const std::vector<String>& _fileExtensions, const std::vector<String>& _searchTerms) {
            directories    = _directories;
            fileExtensions = _fileExtensions;
            searchTerms    = _searchTerms;
        }
        void run() override {
            std::vector<String> dirsVisited;// to avoid infinite recursion
            for (auto& dir : directories) {
                dirsVisited.push_back(dir);
                updateListRecursive(dir, dirsVisited, 0);
            }
        }

        void setCancelled() {
            m_cancelled = true;
            setError();
        }

        bool isCancelled() const {
            return m_cancelled;
        }

        void updateListRecursive(const String& path, std::vector<String>& dirsVisited, int32_t depth) {
            if (isCancelled()) {
                return;
            }
            std::vector<FileFound> files;
            listDirectoryFiles(path, fileExtensions, files);
            for (auto& f : files) {
                auto fClone = f;
                // fClone.depth = depth;
                if (fClone.bIsDir) {
                    if (std::find(dirsVisited.cbegin(), dirsVisited.cend(), fClone.path) == dirsVisited.cend()) {
                        dirsVisited.push_back(fClone.path);
                        updateListRecursive(fClone.path, dirsVisited, depth + 1);
                    }
                } else {
                    bool bMatch = true;
                    for (auto& term : searchTerms) {
                        if (fClone.name.find(term) == String::npos) {
                            bMatch = false;
                            break;
                        }
                    }
                    if (bMatch) {
                        this->filesFound.push_back(fClone);
                        if (this->filesFound.size() > maxFiles) {
                            break;
                        }
                    }
                }
                if (isCancelled()) {
                    break;
                }
            }
        }

        std::vector<FileFound>& getFilesFound() {
            return filesFound;
        }
    };

}// namespace DAW

struct clip_cursor_t {
    tick_t start = 0;
    tick_t end   = 0;
};

inline bool operator==(const clip_cursor_t& lhs, const clip_cursor_t& rhs) {
    return lhs.start == rhs.start && lhs.end == rhs.end;
}
inline bool operator!=(const clip_cursor_t& lhs, const clip_cursor_t& rhs) { return !operator==(lhs, rhs); }

class clip_ref_t {
    project_t* m_project = nullptr; // Pointer must be null or valid!
    track_t* m_track     = nullptr; // Can be dangling pointer!
    clip_t* m_clip       = nullptr; // Can be dangling pointer!
public:
    clip_ref_t() = default;
    bool isTrackValid(const track_t* track) const;
    bool isValid() const;
    bool isValidUpdate();

    clip_t* clip() const;
    track_t* track() const;
    void set(clip_t* clip);
    void resetClipOnly() {
        m_clip = nullptr;
    }
};

struct clip_notes_dragged_t {
    clip_notes_t dragStartNotes;
    std::vector<note_t> draggedSelectionBegin;
    std::vector<note_t> draggedSelection;
    void clear() {
        dragStartNotes.clear();
        draggedSelectionBegin.clear();
        draggedSelection.clear();
    }
};

class clip_view_t {
    clip_ref_t m_clipRef;
public:
    clip_cursor_t m_cursor;
    std::map<clip_t*, clip_notes_dragged_t> m_notesDragged;
    std::vector<int32_t> notePitches;
    editor_view_selection_t m_selectionView;
    noteview_render_t notesViewTemp;
    bool bIsAbsoluteMode = false;
    clip_view_t() = default;
    // delete copy constructor
    clip_view_t(const clip_view_t&) = delete;
    clip_view_t& operator=(const clip_view_t&) = delete;

    clip_ref_t& clipRef() {
        return m_clipRef;
    }
    const clip_ref_t& clipRef() const {
        return m_clipRef;
    }

    bool isAbsoluteTimeMode() const {
        return bIsAbsoluteMode;
    }

    clip_t* clip() const {
        return m_clipRef.clip();
    }

    track_t* track() const {
        return m_clipRef.track();
    }

    void getNotePitches(std::vector<int32_t>& out) const {
        out = notePitches;
    }

    bool contains(clip_t* _clip) const;
    void copySelectedNoteList();
    void reset();
    void setEditorSelection(clip_t* _clip, const editor_view_selection_t& clipboardView);
    void setSelected(clip_t* _clip);
    void setSingleClip(clip_t* _clip);

    void updateNotePitches(bool reset);
    float nextFoldNote(float note, int dir);
    float toFoldNote(float note) const;
    float unfoldNote(float note);
    float unfoldNoteClamped(float note);
    
    template<typename T>
    void visitClipViewReverse(T&& visitor) {
        auto contextClip = clip();
        if (contextClip) {
            if (!visitor(contextClip)) {
                return;
            }
        }
        for (auto it = m_selectionView.tracks.rbegin(); it != m_selectionView.tracks.rend(); ++it) {
            auto& [trackEntry, vecClips] = *it;
            if (!clipRef().isTrackValid(trackEntry.track)) {
                continue;
            }
            for (clip_t* cl : vecClips) {
                if (cl == contextClip) {
                    continue;
                }
                if (!trackEntry.track->getClips().hasClip(cl)) {
                    continue;
                }
                if (!visitor(cl)) {
                    return;
                }
            }
        }
    }

    template<typename T>
    void visitClipView(T&& visitor) {
        auto contextClip = clip();
        for (auto& [trackEntry, vecClips] : m_selectionView.tracks) {
            if (!clipRef().isTrackValid(trackEntry.track)) {
                continue;
            }
            for (clip_t* cl : vecClips) {
                if (cl == contextClip) {
                    continue;
                }
                if (!trackEntry.track->getClips().hasClip(cl)) {
                    continue;
                }
                if (!visitor(cl)) {
                    return;
                }
            }
        }
        if (contextClip) {
            if (!visitor(contextClip)) {
                return;
            }
        }
    }

    template<typename T>
    void visitClipViewTracks(T&& visitor) {
        for (auto& [trackEntry, vecClips] : m_selectionView.tracks) {
            if (!clipRef().isTrackValid(trackEntry.track)) {
                continue;
            }
            if (!visitor(trackEntry.track, vecClips)) {
                return;
            }
        }
    }
    template<typename T>
    int32_t selectAll(T&& visitor) {
        int32_t numSelected = 0;
        visitClipView([&](clip_t* cl) {
            notesViewTemp.clear();
            cl->notes.clearSelection();
            cl->getInTimeRange(cl->start(), cl->end(), -1, -1, notesViewTemp.m_list, {
                .bCutNotes = false,
                .bCutMutedNotes = false,
                .bApplyGroove = false,
                .bRelative = false,
            });
            for (auto& note: notesViewTemp.m_list) {
                auto pNote = &cl->notes.m_list[note.id];
                if (visitor(pNote)) {
                    numSelected++;
                    cl->notes.selection.insert(pNote);
                }
            }
            cl->updateNoteViewSelection();
            return true;
        });
        copySelectedNoteList();
        return numSelected;
    }
};

struct Menus {
    ngui::Menu file;
    ngui::Menu recent;
    ngui::Menu edit;
    ngui::Menu tools;
    ngui::Menu views;
};

enum view_mode_t {
    TRACK_TIMELINE,
    MIXER,
    NODE_EDITOR,
};
namespace DAW {
enum EditAreaType : uint32_t {
    EDIT_AREA_CLIP_EDITOR,
    EDIT_AREA_PLUGIN_CONTAINER
};
enum EditAreaLayout : uint32_t {
    EDIT_AREA_SINGLE = 0,
    EDIT_AREA_SPLIT_VERTICAL,
    EDIT_AREA_SPLIT_HORIZONTAL,
};
}
enum ClipBoardType {
    CLIPBOARD_NONE,
    CLIPBOARD_CLIPS,
    CLIPBOARD_NOTES,
    CLIPBOARD_PLUGINS,
    CLIPBOARD_TRACKS,
    CLIPBOARD_AUTOMATION_DATA,
};

class MainCtrl;
class CompanionCtrl;
class DawCtrl;
class guictr_menubar;
struct track_gui_entry_t;
struct guictrlayout_entry_snapshot_t;

class DawViewContainers {
protected:
    std::vector<std::shared_ptr<guictr_layout>> topLevelContainers;
public:
    DawViewContainers() = default;
    virtual ~DawViewContainers() = default;
    void add(std::shared_ptr<guictr_layout> container) {
        topLevelContainers.emplace_back(std::move(container));
    }
    template<typename T>
    bool visitLayoutContainers(T&& visitor) {
        for (auto& container : topLevelContainers) {
            if (!visitor(container)) {
                return false;
            }
        }
        return true;
    }

    virtual void addTo(std::vector<guictr_base*>& v) = 0;
    virtual void layout(int32_t winW, int32_t winH) = 0;
    virtual guictr_menubar* getMenu() = 0;
};

struct autosave_state_t {
    int64_t tmLastTrigger   = 0L;
};

class GrooveLibrary {
    public:
    std::vector<groove_data_t> grooves;
    GrooveLibrary() = default;
    void loadGrooves();
    std::vector<groove_data_t>& getGrooves() {
        return grooves;
    }
    const std::vector<groove_data_t>& getGrooves() const {
        return grooves;
    }
    const groove_data_t* findGroove(const String& name) const {
        for (auto& entry : grooves) {
            if (entry.grooveName == name) {
                return &entry;
            }
        }
        return nullptr;
    }
};
namespace DAW {
class AudioPreviewStream : public AuxOutputSource {
    static constexpr channelnum_t numChannels = 2;
    audiothread_ringbuffer_t ringbuffer;
    moodycamel::ReaderWriterQueue<AudioBuffer*> audioQueue;
    seq_rand rnd;
public:
    AudioPreviewStream() {
        allocRingBuffer(ringbuffer, numChannels);
    }
    ~AudioPreviewStream() override {
        freeRingBuffer(ringbuffer);
    }
    bool feedTo(AudioBlock& block) override { 
        AudioBuffer* buf = nullptr;
        if (audioQueue.try_dequeue(buf)) {
            buf->inUse = false;
            block.addFromOp(buf->output, mix_op::ADD, 0.5);
            return true;
        }
        return false;
    }
    void processBlock(const sampleformat_t& sampleFormat, int32_t sample, double posDouble, playback_state state, const project_globals_t& prjGlobals) override {
        auto& writePos = ringbuffer.writePos;
        AudioBuffer* bufferWrite = ringbuffer.buffers[writePos];
        if (!bufferWrite->inUse) {
            bufferWrite->output->realloc(sampleFormat.blockSize);
            bufferWrite->output->fillNoise(rnd, dsp_util::fromdBFS(-24.0 + 6.0));
            bufferWrite->inUse = true;
            bufferWrite->time.inputTimeSeconds = posDouble;
            audioQueue.enqueue(bufferWrite);
            writePos++;
            writePos &= RING_BUF_MASK;
        }
    }
};
} // namespace DAW
class DawInstance final : public project_controller_t, public delete_cb {
    friend class MainCtrl;
    friend class CompanionCtrl;
    friend class DawCtrl;
    friend class DAW::ProcessClipAudioThreadTask;
    friend class DAW::LoadAudioTask;
    friend struct DAW::load_project_task;
    ProjectFileType projectFileType = ProjectFileType::PROJECT_FILETYPE_JSON;
    project_t project;
    project_globals_t projectGlobals;
    int32_t initState      = 0;
    daw_tls::tlsinstance tls;

    struct DawWindowCompanion {
        window_main* wnd{ nullptr };
        std::shared_ptr<BaseCtrl> ctrl{ nullptr };
    };

    std::vector<DawWindowCompanion> companionWindows;
    std::vector<DawCtrl*> dawCtrls;
    std::vector<dawview_layout_t> layoutsFromProjectFile;
    edithistory hist;
    WorkerThread workerThread;
    PlaybackThread playThread;
    plugindatabase_t plugindb;
    GrooveLibrary grooves;
    String lastProjectDirectory;
    int64_t tmLastSave = 0L;
    String projectPathAutosave;
    std::shared_ptr<project_to_load_t> projectToLoad;
    std::shared_ptr<DAW::ProcessClipAudioThreadTask> processAudioTaskRunning;
    std::vector<std::shared_ptr<DAW::ProcessClipAudioThreadTask>> processAudioTasks;
    std::vector<std::shared_ptr<DAW::LoadAudioTask>> loadAudioTasks;
    std::shared_ptr<DAW::SearchFileTask> searchFileTask;
    std::vector<std::shared_ptr<DAW::SearchFileTask>> previousSearchFileTasks;

    ClipBoardType clipboardType = CLIPBOARD_NONE;
    std::shared_ptr<plugin_clipboard_t> clipboardPlugins;
    std::shared_ptr<clip_clipboard> clipboardClips;
    std::shared_ptr<notes_clipboard> clipboardNotes;
    std::shared_ptr<automation_clipboard_t> clipboardAutomation;

    dragdrop_file dragdropclip;
    dragdrop_target_indicator_t dragdropTarget;
    autosave_state_t autosaveState;
    DAW::async_task_t* asyncTask = nullptr;
    bool bExportFinished = false;
public:
    std::function<void(DawInstance*, int)> cbProjectLoadCompleteCallback;
    tick_t tickJmpFrom = 0;
    tick_t tickJmpTo   = 0;
    plugin_selection pluginSel;

private:
    hires_timer_t timer;
    seq_rand rand;
    DAW::AuxOutputNoiseSource auxSourceNoise;
    DAW::AudioPreviewStream auxAudioPreview;
public:
    DawInstance() : project_controller_t(&project, &projectGlobals) {
        setEmptyClipboard();
    }
    dragdrop_file& getDragDropClip() {
        return dragdropclip;
    }
    edithistory& getHist() {
        return hist;
    }
    int32_t getInitState() const {
        return initState;
    }
    DAW::async_task_t* getAsyncTask() {
        return asyncTask;
    }
    void setAsyncTask(DAW::async_task_t* task);
    plugindatabase_t& getPluginDatabase() {
        return plugindb;
    }
    const GrooveLibrary& getGrooveLibrary() const {
        return grooves;
    }
    PlaybackThread* getPlayThread() {
        return &playThread;
    }
    WorkerThread* getWorkerThread() {
        return &workerThread;
    }
    ThreadLock lockPlayThread() {
        return playThread.lockThread();
    }
    DAW::Host::Host* getHost() {
        return tls.host;
    }
    DAW::Host::PluginManager* getPluginManager() {
        return tls.pluginManager;
    }
    audiocache* getAudioCache() {
        return tls.audioCache;
    }
    midihost* getMidiHost() {
        return tls.midiHost;
    }
    audiohost* getAudioHost() {
        return tls.audioHost;
    }
    DAW::UI::CommandManager* getCommandManager() {
        return tls.commandManager;
    }
    const project_globals_t& getProjectGlobals() const {
        return projectGlobals;
    }
    static DawInstance* get();
    static DawInstance* getOptional();
    std::pair<String, String> createUniqueNonExistingFilename(const String& filePath);
    std::pair<String, String> createUniqueNonExistingProjectFilename(const String& baseDir, const String& sampleName, const String& trackName, const String& fileExt);
    void initProcessingResources();
    void initRealtimeResources();
    void initDaw();
    void startDaw();

    void setTempo(int32_t _tempo100) override;
    /**
     * addTrackImpl - adds track to trackCtr and creates gui
     * int32 trackInserPos - track-type-container local pos
     */
    void addTrackImpl(int32_t trackInsertPos, track_t* t, int flags, std::optional<audio_stage_id_t> stageId = std::nullopt) override;

    void pushHist(action_base* action);
    void removeTrackImpl(track_t* t, int flags);
    track_t* getTrackId(uint32_t trackId);
    void removeTrackId(uint32_t trackId);
    void unloadProject();
    void preClipDelete(clip_t* clip) override;
    void preTrackDelete(track_t* clip) override;
    void setPluginClipboard(std::shared_ptr<plugin_clipboard_t> clipboard) {
        clipboardType = CLIPBOARD_PLUGINS;
        clipboardPlugins = std::move(clipboard);
    }
    std::shared_ptr<plugin_clipboard_t>& getPluginClipboard() {
        return clipboardPlugins;
    }
    void setClipClipboard(std::shared_ptr<clip_clipboard> clipboard) {
        clipboardType = CLIPBOARD_CLIPS;
        clipboardClips = std::move(clipboard);
    }
    std::shared_ptr<clip_clipboard>& getClipsClipboard() {
        return clipboardClips;
    }
    void setNotesClipboard(std::shared_ptr<notes_clipboard> clipboard) {
        clipboardType = CLIPBOARD_NOTES;
        clipboardNotes = std::move(clipboard);
    }
    std::shared_ptr<notes_clipboard>& getNotesClipboard() {
        return clipboardNotes;
    }
    void setAutomationClipboard(std::shared_ptr<automation_clipboard_t> clipboard) {
        clipboardType = CLIPBOARD_AUTOMATION_DATA;
        clipboardAutomation = std::move(clipboard);
    }
    std::shared_ptr<automation_clipboard_t>& getAutomationClipboard() {
        return clipboardAutomation;
    }
    ClipBoardType getClipboardType() const {
        return clipboardType;
    }
    void setEmptyClipboard();
    void resetDragDropClipboards();
    bool preloadDraggedFiles(const std::vector<String>& files);

    void setJumpFromTo(tick_t _tickJmpFrom, tick_t _tickJmpTo) {
        this->tickJmpFrom = _tickJmpFrom;
        this->tickJmpTo   = _tickJmpTo;
    }
    void setEmptyProject();
    void saveFile(const String& path);
    /**
     * Loads project file at location path
     * @param path - path to a valid .project file
     * @param flags - 0 or DAW::PluginLoadFlags::FLAG_DEFER_LOAD or FLAG_INVOKE_USER_CB_DEFERLOAD
     */
    void loadFile(String path, int flags);
    void loadFileCStr(const char* str);

    /**
     * Locks audiothread and creates a copy of the project that can be used for serialization
     * @return shared_ptr to project_file instance
     */
    std::shared_ptr<project_file> createProjectFile();
    
    bool setProjectToLoad(const std::shared_ptr<project_file>& file, int flags);
    /**
     * setLoadedProject - releases current project and resources and loads in new project from passed project_file
     * @param file - shared_ptr to project_file instance containg project data to load from
     * @param flags - 0 or DAW::PluginLoadFlags::FLAG_DEFER_LOAD (don't load vst plugins, use placeholders)
     * @return reserved - always true
     */
    bool setLoadedProject(const std::shared_ptr<project_file>& file, int flags);
    void loadProject0(const std::shared_ptr<project_file>& file);
    bool loadProject1(const std::shared_ptr<project_file>& file, int flags);
    void loadProjectFinish();
    void unloadUnreferencedSamples();
    void startPlaying(tick_t pos = -1);
    void stopPlaying();
    void startExport();
    /**
     * setAudioThreadState - puts audio thread into requested state - synchronized.
     * does not return before audio thread is in requested state.
     * Attention: If the calling thread holds the playback thread lock it will result in a deadlock
     */
    void setAudioThreadState(playback_state state);
    bool isPlaying();
    bool toggleLoop();
    void resetMouseContext();
    void setSingleClip(clip_t* _clip);
    void setEditorSelection(clip_t* _clip, const editor_view_selection_t& clipboardView);
    void resetClipViews();
    void resetAutomationContext();
    void closeContextMenus();
    void closeDialogs();
    void cutIntersecting(track_t* tr, clip_t* mask);
    void cutIntersecting(track_t* tr, tick_t tickBegin, tick_t tickEnd);
    track_t* createNewTrack(int trackType);
    track_t* insertNewTrack(int trackInsertPos, int trackType, int flags = FLG_TRK_CHANGE_USER);
    bool menuCommand(const menucmd_t& command);
    void onPrePreDestroy();
    void onPreDestroy();
    void destroy();
    void updateClipViews(clip_t* notifyClip);
    void updateClipViewsAndCursor(clip_t* notifyClip, clip_cursor_t cursor);
    void onTick();
    void processTasksMainThread();
    void setMainControl(MainCtrl*);
    MainCtrl* getMainControl();
    void updateVisibleTrackContents();
    /* called after 1-n plugins were added, removed, moved or rerouted */
    void onPluginsChanged();
    /* called immediately after a plugin or track configuration changed */
    void onAudioStageChanged(audio_stage_t* stage);
    void layoutTrackEditors();
    bool onChildOverlayWindowClose(window_main*);
    void setSoloState(audio_stage_ref_t ref, bool enableSolo);
    void unsoloAll();
    void setTrackArmed(audio_stage_ref_t ref, bool enabledArmed);
    void triggerAutoSave();
    String getAutoSaveFilename();
    bool configureSampleRate();
    void updateDerivedAudio(clip_t* clip, const clip_audio_settings_t& settings);
    void updateAudioProcessingTask();
    void updateLoadAudioTasks();
    void refreshAllUserlibraryBrowsers();
    void fileSearchCancel();
    void fileSearchUpdate();
    void fileSeachStart(std::shared_ptr<DAW::SearchFileTask> task);
private:
    void onDawCompanionWindowClose(DawWindowCompanion& entry);
    void saveProjectBundle(const String& path);
};

class DawCtrl : public AppCtrl {
    Menus menus;
protected:
    waveformrender* waveformRenderer = nullptr;
    hires_timer_t timer;
    seq_rand rand;
    int32_t numCallsWaitEvents = 0;

    track_gui_entry_t* lastHoveredTrack = nullptr;
    int64_t tmLastRenderUpdatesMs       = 0;

    track_t* selectedTrack = nullptr;
    int32_t layoutIndex = 0;
    size_t dawCtrlWindowIndex = 0; // (dawCtrlWindowIndex > 0) == isCompanion()
    std::array<dawview_layout_t, 10> layouts;
    SafeRef<guibase> guiEditModulation;
    size_t statsTickDelay = 0;
    track_gui_manager_t* trackGuis = nullptr;
public:
    std::vector<guictr_base*> viewGuiContainers;
    gui_asyc_progress guiCtrProgress;
    gui_dragged_files guiDraggedFiles;
    gui_notify* guiNotify = nullptr;
    std::vector<guictr_base*> viewAsyncProgress = {&guiCtrProgress};
    std::vector<guictr_base*> viewRender;
    struct ui_modulation_targets_t {
        SafeRef<guibase> target;
        NVGstate state;
    };
    bool bIsContainerRenderPass = false;
    bool getIsContainerRenderPass() const {
        return bIsContainerRenderPass;
    }
    std::vector<ui_modulation_targets_t> uiModulationTargets;
    std::vector<ui_modulation_targets_t>& getUIModulationTargets() {
        return uiModulationTargets;
    }
    const std::vector<guictr_base*>& getRenderContainers() const override { return viewRender; }
    String lastKeyDebug;
    DawViewContainersMain* view = nullptr;
    DawInstance& daw;
    view_mode_t viewMode = view_mode_t::TRACK_TIMELINE;
    DawCtrl(AppCtrl* parent, DawInstance& _daw, int32_t _dawCtrlWindowIndex);
    ~DawCtrl() override = default;
    void updateViewGuiContainers();

    size_t getDawWindowIndex() const {
        return dawCtrlWindowIndex;
    }

    int32_t& getLayoutIndex() {
        return layoutIndex;
    }

    std::array<dawview_layout_t, 10>& getLayouts() {
        return layouts;
    }

    void setLayoutIndex(int32_t index) {
        layoutIndex = index;
    }

    DawInstance* getDaw() {
        return &daw;
    }
    dragdrop_target_indicator_t& getDragDropTarget() {
        return daw.dragdropTarget;
    }
    plugin_selection& getPluginSel() {
        return daw.pluginSel;
    }
    String& getProjectPath() {
        return daw.projectPath;
    }
    String& getProjectDirectory() {
        return daw.lastProjectDirectory;
    }
    WorkerThread* getWorkerThread() {
        return &daw.workerThread;
    }
    ThreadLock lockPlayThread() {
        return daw.playThread.lockThread();
    }
    waveformrender* getWaveformRenderer() {
        return this->waveformRenderer;
    }
    track_t* getSelectedTrack() {
        return selectedTrack;
    }
    void onViewCreated(SPLayoutEntry& ctrEntry);
    void setSelectedTrackEntry(track_gui_entry_t* trackEntry);
    void setSelectedTrack(track_t* track);
    void revealPlugin(effectbase* effect);
    void focusChanged(guibase* oldFocused, guibase* newFocused) override;
    void resetMouseContext() override;
    bool filesDropMove(ivec2 pos, KeyboardMods kbmods) override;
    bool filesDropBegin(const std::vector<String>& files, ivec2 pos, KeyboardMods kbmods, bool bIsExternal) override;
    void filesDropCancel() override;
    bool filesDropFinal(ivec2 pos, KeyboardMods kbmods) override;
    void mouseMoved(ivec2 mousePos, ivec2 deltaPos, KeyboardMods kbmods) override;
    bool menuCommand(const menucmd_t& command) override;
    void updateMenubar() override;
    void onTick() override;
    void destroy() override;
    void onPreDestroy() override;
    void relayout() override {
        BaseCtrl::relayout();
    }
    void relayout(int32_t w, int32_t h) override;
    bool processGlobalKeyevent(const KeyEvent& event) override;
    bool handleGlobalCommand(DAW::UI::CommandContext& ctxt) override;
    bool mouseDownPre() override;
    void prerender(NVGcontext* nanovgCtxt, int32_t x, int32_t y, int32_t w, int32_t h, float pixelRatio) override;


    void initApp(const std::vector<String>& args) override { };
    bool initAppWindow(window_main* window, NVGcontext* nanovg) override;
    size_t getAppWindowIndex() override { return dawCtrlWindowIndex; }
    bool onWindowCloseRequest() override;
    void startApp() override { };

    virtual DAW::Cursor& getCursor()              = 0;
    void setupView();
    void layoutView(int32_t w, int32_t h);
    void setSingleClip(clip_t* _clip);
    void setEditorSelection(clip_t* _clip, const editor_view_selection_t& clipboardView);
    void updateClipViews(clip_t* notifyClip);
    void updateClipViewsAndCursor(clip_t* notifyClip, clip_cursor_t cursor);
    void resetClipViews();
    void onTrackContentRemoved(track_gui_entry_t& e);
    void onTrackMixerRemoved(track_gui_entry_t& e);
    bool visitGuis(gui_type type, std::function<void(guictr_base*)> func);
    void refreshAllUserlibraryBrowsers();
    void updateVisibleTrackContents();

    virtual void setStatusText(String s) {
    }

    bool isCompanion() const {
        return dawCtrlWindowIndex > 0;
    }

    virtual void resetAutomationContext() {
    }

    void addTrackToView(track_t* track, int flags);
    void removeTrackFromView(track_t* track, int flags);

    void updateZoomLevel(float f) override;
    void onPostUnloadProject();
    void layoutView();
    void fixCursor();
    bool isZooming();
    bool isClipEditorVisible();
    bool isPluginViewVisible();
    void showPluginView();
    void showClipEditor();
    void setAsyncTask(DAW::async_task_t* task);

    view_mode_t getViewMode() const;
    void setViewMode(view_mode_t mode);
    void toggleViewModeEditArea();
    void setEditAreaLayout(DAW::EditAreaLayout layout);
    void setEditAreaType(DAW::EditAreaType editAreaType);
    void storeLayout(dawview_layout_t& layout);
    void loadLayout(const dawview_layout_t& viewLayout);
    void loadTrackLayouts(const std::shared_ptr<project_file>& file);
    std::shared_ptr<guictr_layout> replaceContainerWith(guictr_base* ctr,
                                                        std::shared_ptr<guictr_layout> newContainer) override;
    void dragContainerRelayout(drag_ctr_event evt) override;
    std::shared_ptr<guictr_tracks> getTrackContainer();
    std::shared_ptr<guictr_clipeditor> getClipEditor();

    virtual void onPluginSelected();
    bool isGlobalKeybindCodepoint(uint32_t codepoint) override {
        return codepoint == 32 || codepoint == 45 || codepoint == 43;
    }
    DAW::UI::IDraggedModulationSource* getDraggedModulation() {
        auto guiDragged = getGuiDragged();
        if (guiDragged && (guiDragged->getGuiType() == gui_type::CTR_TYPE_MODULATION_BUTTON
                            || guiDragged->getGuiType() == gui_type::CTR_TYPE_MODULATION_DRAGGED)) {
            return dynamic_cast<DAW::UI::IDraggedModulationSource*>(guiDragged);
        }
        return nullptr;
    }
    DAW::UI::IDraggedModulationSource* getEditModulation() {
        auto guiDragged = safeRefGet(guiEditModulation);
        if (guiDragged) {
            if (guiDragged->getGuiType() == gui_type::CTR_TYPE_MODULATION_BUTTON) {
                return dynamic_cast<DAW::UI::IDraggedModulationSource*>(guiDragged);
            }
            if (guiDragged->parent && guiDragged->parent->getGuiType() == gui_type::CTR_TYPE_MODULATION_BUTTON) {
                return dynamic_cast<DAW::UI::IDraggedModulationSource*>(guiDragged->parent);
            }
        }
        return nullptr;
    }
    void setEditModulation(const SafeRef<guibase>& ref) {
        guiEditModulation = ref;
    }
    DAW::UI::IDraggedModulationSource* getFocusedModulation() {
        auto guiOver = getGuiOver();
        if (guiOver && guiOver->getGuiType() == gui_type::CTR_TYPE_MODULATION_BUTTON) {
            return dynamic_cast<DAW::UI::IDraggedModulationSource*>(guiOver);
        }
        return nullptr;
    }
    std::optional<DAW::modulation_channel_ref> getDraggedModulationRef() {
        auto dragged = getDraggedModulation();
        if (dragged) {
            return dragged->getChannelRef();
        }
        return {};
    }
    
    void renderContainers(NVGcontext* nanovgCtxt, int32_t x, int32_t y, int32_t w, int32_t h, float ratio) override;
};
namespace DAW::UI::Modulation {
    bool IsHiglightedModulation(const guibase* gui, automatable_t* at, int32_t paramIdx);
    bool IsEditModulation(const guibase* gui, automatable_t* at, int32_t paramIdx);
};

class ProjectGraphMonitor {
    gui_notify* popupNotifyError = nullptr;
    std::shared_ptr<DAW::processing_graph_t> processingGraph;
    std::shared_ptr<DAW::processing_graph_t> lastWorkingProcGraph;
    bool bWorkingProcessingGraph = false;
    public:
    void onTick(MainCtrl* ctrl);
    gui_notify* getNotifyError() const {
        return popupNotifyError;
    }
};
class MainCtrl final : public DawCtrl {
    friend class DawInstance;
    friend class DawCtrl;
    friend struct DAW::load_project_task;
    String loadProject;
    int loadFlags = 0;
    std::shared_ptr<Logger> statusbarLogger;
    ProjectGraphMonitor graphMonitor;
public:
    static MainCtrl* get();
    explicit MainCtrl(DawInstance& _daw);
    ~MainCtrl() override = default;

    void initApp(const std::vector<String>& args) override;
    void startApp() override;

    const String& getLoadProjectFilePath() {
        return loadProject;
    }
    void render(NVGcontext* nanovgCtxt, int32_t x, int32_t y, int32_t w, int32_t h, float ratio) override;
    void onTick() override;
    void onFastTick() override;
    bool processGlobalKeyevent(const KeyEvent& event) override;
    void addDebug(String s);
    void setStatusText(String s) override;
    void setStatusText(const String& s, GuiColor::constant_t color);
    void destroy() override;
    void onPreDestroy() override;
    DAW::Cursor& getCursor() override;
    void onChildOverlayWindowClose(window_main*) override;
};

class CompanionCtrl final : public DawCtrl {
    DAW::Cursor cursor;
    DAW::TrackSelection trackSelection;

public:
    explicit CompanionCtrl(AppCtrl* parent, DawInstance& _daw, int32_t windowIndex = 1)
    : DawCtrl(parent, _daw, windowIndex)
    {
        dbgassert(windowIndex > 0);
    }
    ~CompanionCtrl() override = default;
    DAW::Cursor& getCursor() override { return cursor; };
    bool initAppWindow(window_main* window, NVGcontext* nanovg) override {
        return DawCtrl::initAppWindow(window, nanovg);
    }
};

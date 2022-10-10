#pragma once
#include <list>
#include <utility>
#include <vector>
#include <set>
#include "commands.h"
#include "project.h"
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
#include "track.h"
#include "clip.h"
#include "clipboard.h"
#include "note.h"
#include "logging.h"
#include "automation.h"
#include "gui/automation/modulation.h"
#include "../threads/workerthread.h"
#include "../threads/playbackthread.h"
#include "edithistory.h"
#include "projectfile.h"
#include "hires_timer.h"
#include "../host/plugindatabase.h"
#include "rand.h"
#include "projectcontroller.h"
#include "dragdrop.h"
#include "../gui/container/container_dnd_layout.h"
#include "buildinfo.h"

struct automatable_t;
struct KeyEvent;
struct MouseEvent;
struct NVGcontext;

class guibase;
class guictr_base;
class guictr_plugins;
class guictr_pluginview;
class guitrack_editor;
class guiplugin;
class guictr_test;
class guictr_tempocontrols;
class guictr_tracks;
class guictr_nodes_splitview;
class gui_statusbar;
class gui_notify;
class guictr_clipeditor;
class guictr_clipeditorview;
class guictxtmenu_base;
class appwindow_main;
class DawViewContainers;
class DawViewContainersMain;
class DawViewContainersCompanion;
class track_gui_manager_i;

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

struct dragdrop_midifile {
    std::shared_ptr<clip_clipboard> clipboard;
    bool isLoaded      = false;
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
    std::shared_ptr<clip_clipboard> copySelection(const track_gui_manager_i& trackList, const Cursor& _cursor, bool copyAutomation);
    std::shared_ptr<clip_clipboard> consolidateClipboard(std::shared_ptr<clip_clipboard>& clipboardIn, const Cursor& _cursor);
    void pasteFullClipboard(DawInstance* daw, track_gui_manager_i& trackList, clip_clipboard* clipboard, int32_t track, tick_t tick, bool pasteAutomation);
    void pasteClipboard(DawInstance* daw, track_gui_manager_i& trackList, clip_clipboard* clipboard, Cursor& cursor, bool pasteAutomation);
    void cutSelection(DawInstance* daw, track_gui_manager_i& trackList, const Cursor& cursor, bool cutAutomation);
    bool isSelectionEmpty(const track_gui_manager_i& trackList, const DAW::Cursor& _cursor, bool bIgnoreAutomation);
    void GetProjectReferencedSampleIds(const project_t& project, std::vector<int32_t>& uniqueSampleIds);
    String MakeUniqueTrackName(project_t* project, const String& strNewName);
    void OpenFloatingTextInput(DawCtrl* ctrl, ivec2 popupPos, ivec2 popupSize, const String& initialStr, const std::function<bool(const String& str)>& callback);
    void OpenRenameTrackPopup(DawCtrl* ctrl, track_gui_entry_t* trackentry);
}// namespace DAW

struct clip_cursor_t {
    tick_t start = 0;
    tick_t end   = 0;
};

inline bool operator==(const clip_cursor_t& lhs, const clip_cursor_t& rhs) {
    return lhs.start == rhs.start && lhs.end == rhs.end;
}
inline bool operator!=(const clip_cursor_t& lhs, const clip_cursor_t& rhs) { return !operator==(lhs, rhs); }
struct notes_clipboard {
    clip_notes_t notes;
    tick_t cursorRange = 0;
    bool empty() const {
        return notes.empty();
    }
};
class clip_view {
public:
    gui_clip* gui = nullptr;
    clip_cursor_t cursor;
    clip_notes_t dragStartNotes;
    std::vector<note_t> draggedSelectionBegin;
    std::vector<note_t> draggedSelection;
    std::vector<int32_t> notePitches;

    void set(gui_clip* _clip) {
        this->gui = _clip;
        updateNotePitches(true);
    }

    clip_t* clip() const;
    track_t* track() const;

    void copySelectedNoteList() {
        dragStartNotes = clip()->notes;
        clip()->notes.copySelectionTo(draggedSelection);
        clip()->notes.copySelectionTo(draggedSelectionBegin);
    }

    void getNotePitches(std::vector<int32_t>& out) const {
        out = notePitches;
    }

    float toFoldNote(float note) const {
        const auto len = notePitches.size();
        const auto iNote = math::floorfS32(note);
        for (uint32_t i = 0; i < len; i++) {
            if (notePitches[i] >= iNote) {
                return i;
            }
        }
        if (len) {
            if (iNote >= notePitches[len - 1])
                return len + (note - notePitches[len - 1]);
        }
        return note;
    }

    float nextFoldNote(float note, int dir) {
        float f = toFoldNote(note);
        return unfoldNoteClamped(f + dir);
    }

    float unfoldNoteClamped(float note) {
        const auto len = CtrSize(notePitches);
        if (len) {
            const auto idx = math::clamp<int32_t>(math::floorfS32(note), 0, len - 1);
            return notePitches[idx];
        }
        return 0;
    }

    float unfoldNote(float note) {
        const auto len = CtrSize(notePitches);
        if (len) {
            const auto iNote = math::floorfS32(note);
            if (iNote < 0)
                return notePitches[0] + note;

            if (iNote >= len)
                return note - len + 1 + notePitches[len - 1];
            return notePitches[iNote];
        }
        return 0;
    }

    void updateNotePitches(bool reset) {
        if (reset)
            notePitches.clear();
        clip_t* clipPtr = clip();
        if (clipPtr)
            clipPtr->notes.getNotePitches(notePitches);
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
    NODE_EDITOR,
    MIXER
};
enum ClipBoardType {
    CLIPBOARD_NONE,
    CLIPBOARD_CLIPS,
    CLIPBOARD_NOTES,
    CLIPBOARD_PLUGINS,
    CLIPBOARD_TRACKS,
};

class MainCtrl;
class CompanionCtrl;
class DawCtrl;
class guictr_menubar;
struct track_gui_entry_t;
struct guictrlayout_snapshot_t;

class DawViewContainers {
public:
    int indexContent    = 0;
    DawViewContainers() = default;
    virtual ~DawViewContainers() = default;

    virtual void addTo(std::vector<guictr_base*>& v) = 0;
    virtual void layout(int32_t winW, int32_t winH) = 0;
    virtual guictr_menubar* getMenu() = 0;
    virtual void dragContainerRelayout(MainCtrl* ctrl, BaseCtrl::drag_ctr_event evt) {
    }
};

struct autosave_state_t {
    int64_t tmLastTrigger   = 0L;
};

class DawInstance : public project_controller_t, public delete_cb {
    friend class MainCtrl;
    friend class CompanionCtrl;
    friend class DawCtrl;
    ProjectFileType projectFileType = ProjectFileType::PROJECT_FILETYPE_JSON;
    project_t project;
    project_globals_t projectGlobals;
    int initState      = 0;
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
    String lastProjectDirectory;
    int64_t tmLastSave = 0L;
    String projectPathAutosave;
    track_t* selectedTrack = nullptr;
    struct project_to_load_t {
        std::shared_ptr<project_file> projectfile;
        int loadflags;
    };
    std::shared_ptr<project_to_load_t> projectToLoad;

    ClipBoardType clipboardType = CLIPBOARD_NONE;
    std::shared_ptr<plugin_clipboard_t> clipboardPlugins;
    std::shared_ptr<clip_clipboard> clipboardClips;
    std::shared_ptr<notes_clipboard> clipboardNotes;

    dragdrop_midifile dragdropclip;
    dragdrop_target_indicator_t dragdropTarget;
    autosave_state_t autosaveState;

public:
    std::function<void(DawInstance*, std::shared_ptr<project_file>, int)> cbProjectLoadCompleteCallback;
    tick_t tickJmpFrom = 0;
    tick_t tickJmpTo   = 0;
    plugin_selection pluginSel;

private:
    hires_timer_t timer;
    seq_rand rand;
    //    int curTooltip = 0;
public:
    DawInstance() : project_controller_t(&project, &projectGlobals) {
        setEmptyClipboard();
    }
    void setEmptyClipboard();
    edithistory& getHist() {
        return hist;
    }
    plugindatabase_t& getPluginDatabase() {
        return plugindb;
    }
    PlaybackThread* getPlayThread() {
        return &playThread;
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
    std::pair<String, String> createUniqueNonExistingFilename(const String& baseDir, const String& sampleName, const String& trackName, const String& fileExt);
    void initProcessingResources();
    void initRealtimeResources();
    void initDaw();
    void startDaw();

    void setTempo(int32_t _tempo100) override;
    /**
     * addTrackImpl - adds track to trackCtr and creates gui
     * int32 trackInserPos - track-type-container local pos
     */
    void addTrackImpl(int32_t trackInsertPos, track_t* t, int flags) override;

    void pushHist(action_base* action);
    void removeTrackImpl(track_t* t, int flags);
    track_t* getTrackId(uint32_t trackId);
    void removeTrackId(uint32_t trackId);
    void unloadProject();
    void setSelectedTrackEntry(track_gui_entry_t* trackEntry);
    void setSelectedTrack(track_t* track);
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
    ClipBoardType getClipboardType() const {
        return clipboardType;
    }

    void setJumpFromTo(tick_t _tickJmpFrom, tick_t _tickJmpTo) {
        this->tickJmpFrom = _tickJmpFrom;
        this->tickJmpTo   = _tickJmpTo;
    }
    void setEmptyProject();
    void saveFile(const String& path);
    /**
     * Loads project file at location path
     * @param path - path to a valid .project file
     * @param flags - 0 or FLAG_DEFER_LOAD or FLAG_INVOKE_USER_CB_DEFERLOAD
     */
    void loadFile(String path, int flags);
    void loadFileCStr(const char* str);

    /**
     * Locks audiothread and creates a copy of the project that can be used for serialization
     * @return shared_ptr to project_file instance
     */
    std::shared_ptr<project_file> createProjectFile();
    
    /** assuming current thread is main thread when this is called **/
    /**
     * setLoadedProject - releases current project and resources and loads in new project from passed project_file
     * @param file - shared_ptr to project_file instance containg project data to load from
     * @param flags - 0 or FLAG_DEFER_LOAD (don't load vst plugins, use placeholders)
     * @return reserved - always true
     */
    bool setLoadedProject(std::shared_ptr<project_file> file, int flags);
    bool setProjectToLoad(std::shared_ptr<project_file> file, int flags);
    void unloadUnreferencedSamples();
    void startPlaying();
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
    void resetEditClip();
    void setEditClip(gui_clip* gclip);
    void resetAutomationContext();
    void closeContextMenus();
    void closeDialogs();
    void cutIntersecting(track_t* tr, clip_t* mask);
    void cutIntersecting(track_t* tr, tick_t tickBegin, tick_t tickEnd);
    track_t* createNewTrack(int trackType);
    track_t* insertNewTrack(int trackInsertPos, int trackType, int flags = FLG_TRK_CHANGE_USER);

    track_t* getSelectedTrack();
    bool menuCommand(const menucmd_t& command);
    void destroy();
    void updateClipViews(clip_t* notifyClip, clip_cursor_t cursor);
    void onTick();
    void setMainControl(MainCtrl*);
    MainCtrl* getMainControl();
    void getTrackContainers(std::vector<guictr_tracks*>& trackContainers);
    void updateVisibleTrackContents();
    void onPluginsChanged();
    void layoutTrackEditors();
    bool onChildOverlayWindowClose(window_main*);
    void setSoloState(audio_stage_ref_t ref, bool enableSolo);
    void setTrackArmed(audio_stage_ref_t ref, bool enabledArmed);
    void triggerAutoSave();
    String getAutoSaveFilename();
    void configureSampleRate();
private:
    void onDawCompanionWindowClose(DawWindowCompanion& entry);
    void saveProjectBundle(const String& path);
};

class DawCtrl : public AppCtrl {
    Menus menus;
protected:
    waveformrender* waveformRenderer = nullptr;
protected:
    hires_timer_t timer;
    seq_rand rand;
    int32_t numCallsWaitEvents = 0;

    track_gui_entry_t* lastHoveredTrack = nullptr;
    int32_t lastHoveredTrackTicks       = 0;
    int64_t tmLastRenderUpdatesMs       = 0;

public:
    String lastKeyDebug;
    DawViewContainers* viewContainers = nullptr;
    DawInstance& daw;
    scaled_grid grid;
    clip_view clipView;
    view_mode_t viewMode = view_mode_t::TRACK_TIMELINE;

    explicit DawCtrl(AppCtrl* parent, DawInstance& _daw)
    : AppCtrl(parent), daw(_daw)
    {
#if BUILD_DAW_HOST
        this->parentDawCtrl = this;
#endif
        setWindowName(BuildInfo::BUILD_BINARY_NAME);
    }

    ~DawCtrl() override = default;

    scaled_grid& getGrid() {
        return grid;
    }
    clip_view& getClipView() {
        return clipView;
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
    WorkerThread* getWorkerThread() {
        return &daw.workerThread;
    }
    ThreadLock lockPlayThread() {
        return daw.playThread.lockThread();
    }
    waveformrender* getWaveformRenderer() {
        return this->waveformRenderer;
    }


    void resetMouseContext() override;
    bool filesDropMove(ivec2 pos, KeyboardMods kbmods) override;
    bool filesDropBegin(std::vector<String>& files, ivec2 pos, KeyboardMods kbmods) override;
    void filesDropCancel() override;
    bool filesDropFinal(std::vector<String>& files, ivec2 pos, KeyboardMods kbmods) override;
    void mouseMoved(ivec2 mousePos, ivec2 deltaPos, KeyboardMods kbmods) override;
    bool menuCommand(const menucmd_t& command) override;
    void updateMenubar() override;
    void onTick() override;
    void destroy() override;
    void relayout(int32_t w, int32_t h) override;
    bool processGlobalKeyevent(const KeyEvent& event) override;
    bool handleGlobalCommand(DAW::UI::CommandContext& ctxt) override;
    bool mouseDownPre() override;
    void uncaptureMouse();
    void onUncaptureMouse();
    void prerender(NVGcontext* nanovgCtxt, int32_t x, int32_t y, int32_t w, int32_t h, float pixelRatio) override;


    void initApp(const std::vector<String>& args) override { };
    bool initAppWindow(window_main* window, NVGcontext* nanovg) override;
    void startApp() override { };

    virtual void setEditClip(gui_clip* gclip);
    virtual DAW::Cursor& getCursor()              = 0;
    virtual void setupView()                      = 0;
    virtual void layoutView(int32_t w, int32_t h) = 0;

    void updateVisibleTrackContents();

    virtual void onPluginsChanged() {
    }

    virtual void setStatusText(String s) {
    }

    virtual bool isCompanion() const {
        return false;
    }

    virtual void resetAutomationContext() {
    }

    virtual void addTrackToView(track_t* track, int flags)      = 0;
    virtual void removeTrackFromView(track_t* track, int flags) = 0;

    virtual void resetView()  = 0;
    virtual void layoutView() = 0;
    virtual void fixCursor()  = 0;
    virtual bool isZooming()  = 0;
    virtual bool isClipEditorVisible() = 0;
    virtual bool isPluginViewVisible() = 0;
    virtual void showPluginView() = 0;
    virtual void showClipEditor() = 0;

    virtual void setViewMode(view_mode_t mode) = 0;
    view_mode_t getViewMode() const;
    virtual void storeLayout(dawview_layout_t& layout) = 0;
    virtual void loadLayout(const dawview_layout_t& viewLayout) = 0;

    virtual void getTrackContainers(std::vector<guictr_tracks*>& trackContainers) = 0;
    virtual guictr_tracks* getTrackContainer() = 0;
    virtual guictr_clipeditor* getClipEditor() = 0;
    virtual guictr_plugins* getPluginsView() = 0;
    virtual guictr_nodes_splitview* getNodesContainer() = 0;
    virtual void onPluginSelected();
    bool isGlobalKeybindCodepoint(uint32_t codepoint) override {
        return codepoint == 32 || codepoint == 45 || codepoint == 43;
    }
    DAW::UI::IDraggedModulationSource* getDraggedModulation() const {
        if (guiDragged && (guiDragged->getGuiType() == gui_type::CTR_TYPE_MODULATION_BUTTON
                            || guiDragged->getGuiType() == gui_type::CTR_TYPE_MODULATION_DRAGGED)) {
            return dynamic_cast<DAW::UI::IDraggedModulationSource*>(guiDragged);
        }
        return nullptr;
    }
    DAW::UI::IDraggedModulationSource* getFocusedModulation() const {
        if (guiOver && guiOver->getGuiType() == gui_type::CTR_TYPE_MODULATION_BUTTON) {
            return dynamic_cast<DAW::UI::IDraggedModulationSource*>(guiOver);
        }
        return nullptr;
    }
    std::optional<DAW::modulation_channel_ref> getDraggedModulationRef() const {
        auto dragged = getDraggedModulation();
        if (dragged) {
            return dragged->getChannelRef();
        }
        return {};
    }
};
namespace DAW::UI::Modulation {
    bool IsHiglightedModulation(const guibase* gui, automatable_t* at, int32_t paramIdx);
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
class MainCtrl : public DawCtrl {
    friend class DawInstance;
    friend class DawCtrl;
    DawViewContainersMain* view = nullptr;
    std::array<dawview_layout_t, 10> layouts;
    String loadProject;
    int loadFlags = 0;
    std::shared_ptr<Logger> statusbarLogger;
    ProjectGraphMonitor graphMonitor;
public:
    static MainCtrl* get();
    explicit MainCtrl(DawInstance& _daw);
    ~MainCtrl() override = default;

    static PlaybackThread* getPlayThread() {
        MainCtrl* ctrl = MainCtrl::get();
        return ctrl ? ctrl->daw.getPlayThread() : nullptr;
    }
    static guictr_plugins* getPluginCtr();
    static guictr_tracks* getGuiTrackCtr();

    void initApp(const std::vector<String>& args) override;
    void startApp() override;

    const String& getLoadProjectFilePath() {
        return loadProject;
    }
    void render(NVGcontext* nanovgCtxt, int32_t x, int32_t y, int32_t w, int32_t h, float ratio) override;
    void onTick() override;
    void setupView() override;
    bool isClipEditorVisible() override;
    bool isPluginViewVisible() override;
    void showPluginView() override;
    void showClipEditor() override;
    void onPluginsChanged() override;
    bool processGlobalKeyevent(const KeyEvent& event) override;
    bool handleGlobalCommand(DAW::UI::CommandContext& ctxt) override;
    guitrack_editor& getTrackEditor();
    void addDebug(String s);
    void resetMouseContext() override;
    void setEditClip(gui_clip* gclip) override;
    void layoutView(int32_t w, int32_t h) override;
    void setStatusText(String s) override;
    void setStatusText(const String& s, GuiColor::constant_t color);
    void destroy() override;
    DAW::Cursor& getCursor() override {
        return daw.projectGlobals.cursor;
    }
    void onChildOverlayWindowClose(window_main*) override;
    void addTrackToView(track_t* track, int flags) override;
    void removeTrackFromView(track_t* track, int flags) override;
    void resetView() override;
    void layoutView() override;
    void fixCursor() override;
    bool isZooming() override;
    void setViewMode(view_mode_t mode) override;
    void storeLayout(dawview_layout_t& layout) override;
    void loadLayout(const dawview_layout_t& viewLayout) override;
    std::shared_ptr<guictr_layout> replaceContainerWith(guictr_base* ctr,
                                                        std::shared_ptr<guictr_layout> newContainer) override;
    void dragContainerRelayout(drag_ctr_event evt) override;
    void getTrackContainers(std::vector<guictr_tracks*>& trackContainers) override;
    guictr_tracks* getTrackContainer() override;
    guictr_nodes_splitview* getNodesContainer() override;
    guictr_clipeditor* getClipEditor() override;
    guictr_plugins* getPluginsView() override;
};

class CompanionCtrl : public DawCtrl {
    DAW::Cursor cursor;
    DAW::TrackSelection trackSelection;

public:
    DawViewContainersCompanion* view = nullptr;
    explicit CompanionCtrl(AppCtrl* parent, DawInstance& _daw): DawCtrl(parent, _daw) {
    }

    ~CompanionCtrl() override = default;

    void setupView() override;
    void layoutView(int32_t w, int32_t h) override;
    void resetMouseContext() override;
    void destroy() override;
    bool isCompanion() const override {
        return true;
    }
    void onPluginsChanged() override;
    DAW::Cursor& getCursor() override {
        return cursor;
    }
    void addTrackToView(track_t* track, int flags) override;
    void removeTrackFromView(track_t* track, int flags) override;
    void resetView() override;
    void layoutView() override;
    void fixCursor() override;
    bool isZooming() override;
    bool isClipEditorVisible() override;
    bool isPluginViewVisible() override;
    void setViewMode(view_mode_t mode) override;
    void storeLayout(dawview_layout_t& layout) override;
    void loadLayout(const dawview_layout_t& viewLayout) override;
    void setEditClip(gui_clip* gclip) override;
    void getTrackContainers(std::vector<guictr_tracks*>& trackContainers) override;
    guictr_tracks* getTrackContainer() override;
    guictr_nodes_splitview* getNodesContainer() override;
    guictr_clipeditor* getClipEditor() override;
    guictr_plugins* getPluginsView() override;
    void showPluginView() override;
    bool handleGlobalCommand(DAW::UI::CommandContext& ctxt) override;
    void showClipEditor() override;
};

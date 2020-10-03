#pragma once
#include <list>
#include <vector>
#include <set>
#include <stdint.h>
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
#include "../threads/workerthread.h"
#include "../threads/playbackthread.h"
#include "edithistory.h"
#include "projectfile.h"
#include "hires_timer.h"
#include "../host/plugindatabase.h"
#include "rand.h"
#include "projectcontroller.h"
#include "dragdrop.h"
#include "../gui/container/guicontainer_dnd_layout.h"

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
class guictr_nodes;
class gui_statusbar;
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
	bool isLoaded = false;
	bool isValidTarget = false;
	void reset();
};
class plugin_selection {
public:
	int32_t firstSelection = -1;
	int32_t lastSelection = -1;
	guictr_plugins* pluginCtr = nullptr;
	bool hasSelection() {
		return firstSelection >= 0 && lastSelection >= 0 && pluginCtr;
	}
	void clear() {
		firstSelection = -1;
		lastSelection = -1;
		pluginCtr = nullptr;
	}
};
namespace DAW {
std::shared_ptr<clip_clipboard> copySelection(const track_gui_manager_i& trackList, const DAW::Cursor& _cursor);
std::shared_ptr<clip_clipboard> consolidateClipboard(std::shared_ptr<clip_clipboard>& clipboardIn, const DAW::Cursor& _cursor);
void pasteClipboard(track_gui_manager_i& trackList, clip_clipboard* clipboard, int32_t track, tick_t tick);
void pasteClipboard(track_gui_manager_i& trackList, clip_clipboard* clipboard, DAW::Cursor& cursor);
void cutSelection(track_gui_manager_i& trackList, const DAW::Cursor& cursor);
void muteIntersecting(track_gui_manager_i& trackList, const DAW::Cursor& _cursor);
}

KeyEvent keyEvent(int key, int scancode, int keyState, int mods, const char* key_name);


struct clip_cursor_t {
	tick_t start = 0;
	tick_t end = 0;
};
inline bool operator==(const clip_cursor_t& lhs, const clip_cursor_t& rhs){
	return lhs.start == rhs.start && lhs.end == rhs.end;
}
inline bool operator!=(const clip_cursor_t& lhs, const clip_cursor_t& rhs){return !operator==(lhs,rhs);}

class clip_view {
public:
	gui_clip * gui = NULL;
	clip_cursor_t cursor;
	clip_notes_t dragStartNotes;
	std::vector<note_t> draggedSelectionBegin;
	std::vector<note_t> draggedSelection;
	clip_notes_t clipboard;
	tick_t clipboardCursorRange = 0;
	std::vector<int32_t> notePitches;
	void set(gui_clip* _clip) {
		this->gui = _clip;
		updateNotePitches(true);
//		this->selection.clear();
	}
	clip_t* clip() const;
//	{
//		return this->gui->m_clip;
//	}
	track_t* track() const;
//	{
//		return this->gui->m_track;
//	}
	void copySelectedNoteList() {
		dragStartNotes = clip()->notes;
		clip()->notes.copySelectionTo(draggedSelection);
		clip()->notes.copySelectionTo(draggedSelectionBegin);
	}
	void getNotePitches(std::vector<int32_t>& out) {
		out = notePitches;
	}
	float toFoldNote(float note) {
		auto len = notePitches.size();
		for (uint32_t i = 0; i < len; i++) {
			if (notePitches[i] >= (int32_t)note) {
				return i;
			}
		}
		if (len) {
			if (note >= notePitches[len-1])
				return len+(note-notePitches[len-1]);
			if (note < 0)
				return note;
		}
		return note;
	}
	float nextFoldNote(float note, int dir) {
		float f = toFoldNote(note);
		return unfoldNoteClamped(f+dir);
	}
	float unfoldNoteClamped(float note) {
		uint32_t iNote = math::max(0, math::floorF32toS32(note));
		auto len = notePitches.size();
		if (!len) {
			return 0;
		}
		if (iNote < 0)
			return notePitches[0];
		else if (iNote >= len) {
			return notePitches[len-1];
		}
		return notePitches[iNote];
	}
	float unfoldNote(float note) {
		uint32_t iNote = math::max(0, math::floorF32toS32(note));
		auto len = notePitches.size();
		if (!len) {
			return 0;
		}
		if (iNote < 0)
			return notePitches[0]+note;
		else if (iNote >= len) {
			return note-len+1+notePitches[len-1];
		}
		return notePitches[iNote];
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
};
enum view_mode_t {
	TRACK_TIMELINE, NODE_EDITOR, MIXER
};
class MainCtrl;
class CompanionCtrl;
class DawCtrl;
class guictr_menubar;
struct track_gui_entry_t;
struct dawview_layout_t {
	std::shared_ptr<guictrlayout_snapshot_t> left;
	std::shared_ptr<guictrlayout_snapshot_t> right;
	std::vector<float> splitterPositions;
};
class DawViewContainers {
public:
	DawViewContainers() = default;
	virtual ~DawViewContainers() {
	}
	virtual void addTo(std::vector<guictr_base*>& v) {

	}
	virtual void layout(int32_t winW, int32_t winH) {
	}
	guictr_menubar* getMenu() {
		return nullptr;
	}
	virtual void dragContainerRelayout(MainCtrl* ctrl, BaseCtrl::drag_ctr_event evt) {

	}
};
class DawInstance : public project_controller_t, public delete_cb {
	friend class MainCtrl;
	friend class CompanionCtrl;
	friend class DawCtrl;
	project_t project;
	project_globals_t projectGlobals;
	int initState = 0;
	MainCtrl* mainCtrl = nullptr;
//	CompanionCtrl* companionCtrl = nullptr;
//	std::shared_ptr<CompanionCtrl> companionCtrlStdPtr{nullptr};
	struct DawWindowCompanion {
		window_main* wnd{nullptr};
		std::shared_ptr<CompanionCtrl> ctrl{nullptr};
	};
	std::vector<DawWindowCompanion> companionWindows;
	std::vector<DawCtrl*> dawCtrls;
	edithistory hist;
	WorkerThread workerThread;
	PlaybackThread playThread;
	plugindatabase_t plugindb;
	String projectPath;
	track_t* selectedTrack = nullptr;
	String loadProject = "";
	struct project_to_load_t {
		std::shared_ptr<project_file> projectfile;
		int loadflags;
	};
	std::shared_ptr<project_to_load_t> projectToLoad;
	std::shared_ptr<plugin_clipboard_t> pluginClipboard;
	dragdrop_midifile dragdropclip;
	dragdrop_target_indicator_t dragdropTarget;
public:
	tick_t tickJmpFrom = 0;
	tick_t tickJmpTo = 0;
	plugin_selection pluginSel;
	int nextTooltipId = 0;
private:
	hires_timer_t timer;
	seq_rand rand;
//	int curTooltip = 0;
public:
	DawInstance() : project_controller_t(&project, &projectGlobals) {

	}
	edithistory& getHist() {
		return hist;
	}
	plugindatabase_t& getPluginDatabase() {
		return plugindb;
	}
	static DawInstance* get();

	void postInit();
	void initDaw(int argc, char* argv[]);
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
	void preClipDelete(clip_t* clip);
	void preTrackDelete(track_t* clip);
	void setPluginClipboard(std::shared_ptr<plugin_clipboard_t> clipboard) {
		pluginClipboard = clipboard;
	}
	std::shared_ptr<plugin_clipboard_t> getPluginClipboard() {
		return pluginClipboard;
	}

	void setJumpFromTo(tick_t tickJmpFrom, tick_t tickJmpTo) {
		this->tickJmpFrom = tickJmpFrom;
		this->tickJmpTo = tickJmpTo;
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
	void startPlaying();
	void stopPlaying();
	/**
	 * setAudioThreadState - puts audio thread into requested state - synchronized
	 * 						 does not return before audio thread is in requested state
	 */
	void setAudioThreadState(playback_state state);
	bool isPlaying();
	bool toggleLoop();
	void resetMouseContext();
	void resetEditClip();
	void setEditClip(gui_clip* gclip);
	void resetAutomationContext();
	void closeContextMenus();
	void cutIntersecting(track_t* tr, clip_t* mask);
	void cutIntersecting(track_t* tr, tick_t tickBegin, tick_t tickEnd);
	track_t* createNewTrack(int trackType);
	track_t* insertNewTrack(int trackInsertPos, int trackType, int flags = FLG_TRK_CHANGE_USER);

//	void muteIntersecting(const DAW::Cursor& _cursor);
//	void copyClipsInRange(trackcontents_t* in, trackcontents_t* out, int32_t srcPos, int32_t dstPos, int32_t len);

	track_t* getSelectedTrack();
	void menuCommand(const menucmd_t&& command);
	void destroy();
	void updateClipViews(clip_t* notifyClip, clip_cursor_t cursor);
	void onTick();
	void setMainControl(MainCtrl*);
	guictr_tracks* getTrackContainer(int idx);
	void updateGrid();
	void updateVisibleTrackContents();
	void layoutTrackEditors();
	bool onChildOverlayWindowClose(window_main*);
private:
	void onDawCompanionWindowClose(DawWindowCompanion& entry);
};
class DawCtrl : public AppCtrl {
	Menus menus;
protected:
	hires_timer_t timer;
	seq_rand rand;
	int32_t numCallsWaitEvents = 0;

	track_gui_entry_t* lastHoveredTrack = NULL;
	int32_t lastHoveredTrackTicks = 0;
	void* lastHoveredTooltip = nullptr;
	void* lastTooltipSrc = nullptr;
	int32_t lastHoveredTooltipTicks = 0;
public:
	String lastKey;
	DawViewContainers* viewContainers = NULL;
	DawInstance& daw;
	scaled_grid grid;
	clip_view clipView;
	view_mode_t viewMode = view_mode_t::TRACK_TIMELINE;
	DawCtrl(DawInstance& _daw) : AppCtrl(), daw(_daw) {

	}
	scaled_grid& getGrid() {
		return grid;
	}
	clip_view& getClipView() {
		return clipView;
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
	virtual void setEditClip(gui_clip* gclip);
	virtual void resetMouseContext();

	bool filesDropMove(ivec2 pos, int kbmods) override;
    bool filesDropBegin(std::vector<String>& files, ivec2 pos, int kbmods) override;
    bool filesDropFinal(std::vector<String>& files, ivec2 pos, int kbmods) override;
    void mouseMoved(ivec2 mousePos, ivec2 deltaPos) override;
	void menuCommand(const menucmd_t&& command) override;
	void updateMenubar() override;
	void onTick() override;
	void postInit() override;
	void destroy() override;
	void relayout(int32_t w, int32_t h);
	bool processGlobalKeyevent(KeyEvent& event) override;
	bool mouseDownPre() override;
	void uncaptureMouse();
	void onUncaptureMouse();
	void prerender(NVGcontext* nanovgCtxt, int32_t x, int32_t y, int32_t w, int32_t h, float pixelRatio);



	void initApp(int argc, char* argv[]) override;
	bool init(window_main* window, NVGcontext* nanovg) override;

	void focusReceived() {
	}
	void focusLost() {
//		closeContextMenu();
	}
	virtual void setupView() = 0;
	virtual void layoutView(int32_t w, int32_t h) = 0;
	virtual void updateVisibleTrackContents() {

	}
	virtual void updateGrid() {

	}
	virtual void setStatusText(String s) {

	}
	virtual bool isCompanion() const {
		return false;
	}
	virtual void resetAutomationContext() {
	}
	virtual DAW::Cursor& getCursor() = 0;
	DawInstance* getDaw() {
		return &daw;
	}
	virtual void addTrackToView(track_t* track, int flags) = 0;
	virtual void removeTrackFromView(track_t* track, int flags) = 0;
	virtual void resetView() = 0;
	virtual void layoutView() = 0;
	virtual void fixCursor() = 0;
	virtual bool isZooming() = 0;
	view_mode_t getViewMode();
	virtual void setViewMode(view_mode_t mode) = 0;
};

class MainCtrl : public DawCtrl
{
	friend class DawInstance;
	DawViewContainersMain* view = NULL;
	std::array<dawview_layout_t, 10> layouts;
public:
	static MainCtrl* get();
	MainCtrl(DawInstance& _daw);
	~MainCtrl() {
		//my_printf("~MainCtrl destructor\n",0);
	}
	static PlaybackThread* getPlayThread() {
		MainCtrl* ctrl = MainCtrl::get();
		return ctrl ? &ctrl->daw.playThread : nullptr;
	}
	static guictr_plugins* getPluginCtr();
	static guictr_tracks* getGuiTrackCtr();

	void initApp(int argc, char* argv[]) override;
	bool init(window_main* window, NVGcontext* nanovg) override;

	void postInit() override;
	void onTick() override;
	void setupView() override;
	bool isClipEditorVisible();
	bool isPluginViewVisible();
	void showPluginView();
	void showClipEditor();
	void updateGrid() override;
	void updateVisibleTrackContents() override;
	bool processGlobalKeyevent(KeyEvent& event) override;
	guitrack_editor& getTrackEditor();
	void addDebug(String s);
	void resetMouseContext() override;
	void setEditClip(gui_clip* gclip) override;
	void layoutView(int32_t w, int32_t h) override;
	void showAutomation(track_t* tr, automatable_t* at, int32_t paramIdx);
	void setStatusText(String s) override;
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
    std::shared_ptr<guictr_layout> replaceContainerWith(guictr_base* ctr,
    		std::shared_ptr<guictr_layout> newContainer) override;
    void dragContainerRelayout(drag_ctr_event evt) override;
};

class CompanionCtrl : public DawCtrl
{
	DAW::Cursor cursor;
public:
	DawViewContainersCompanion* view = NULL;
	CompanionCtrl(DawInstance& _daw) : DawCtrl(_daw) {
		//my_printf("CompanionCtrl constructor\n",0);
	}
	~CompanionCtrl() {
		//my_printf("~CompanionCtrl destructor\n",0);
	}
	void setupView() override;
	void layoutView(int32_t w, int32_t h) override;
	void resetMouseContext() override;
	void destroy() override;
	bool isCompanion() const override {
		return true;
	}
	void updateVisibleTrackContents() override;
	void updateGrid() override;
	DAW::Cursor& getCursor() override {
		return cursor;
	}
	void addTrackToView(track_t* track, int flags) override;
	void removeTrackFromView(track_t* track, int flags) override;
	void resetView() override;
	void layoutView() override;
	void fixCursor() override;
	bool isZooming() override;
	void setViewMode(view_mode_t mode) override;
	void setEditClip(gui_clip* gclip) override;
};

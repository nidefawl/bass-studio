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

struct automatable_t;
struct KeyEvent;
struct MouseEvent;

struct NVGcontext;
class guibase;
class guictr_base;
class guictr_plugins;
class guictr_pluginview;
class guiplugin;
class guictr_test;
class guictr_tempocontrols;
class guictr_tracks;
class gui_statusbar;
class guictr_clipeditor;
class guictr_clipeditorview;
class guictxtmenu_base;
class appwindow_main;
class DawViewContainers;

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
	Cursor cursorBegin;
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

struct dragdrop_target_indicator {
	int idx = -1;
	void* ptr = nullptr;
	ivec2 targetPos{ -1, -1 };
	void reset() {
		idx = -1;
		ptr = nullptr;
	}
	void set(void* _ptr, int _idx) {
		idx = _idx;
		ptr = _ptr;
	}
	void setPos(ivec2 _targetPos) {
		targetPos = _targetPos;
	}
};
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
		int len = (int) notePitches.size();
		for (int i = 0; i < len; i++) {
			if (notePitches[i] >= (int)note) {
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
		int32_t iNote = floor(note);
		int len = (int) notePitches.size();
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
		int32_t iNote = floor(note);
		int len = (int) notePitches.size();
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
class MainCtrl : public AppCtrl, public delete_cb, public project_controller_t
{
	DawViewContainers* view = NULL;
	Menus menus;

	edithistory hist;
	scaled_grid grid;
	clip_view clipView;
	WorkerThread workerThread;
	PlaybackThread playThread;
	String projectPath;
	hires_timer_t timer;
	track_t* selectedTrack = NULL;
	track_t* lastHoveredTrack = NULL;
	int32_t lastHoveredTrackTicks = 0;
	void* lastHoveredTooltip = nullptr;
	void* lastTooltipSrc = nullptr;
	int32_t lastHoveredTooltipTicks = 0;
	seq_rand rand;
	String loadProject = "";
public:
	int32_t numCallsWaitEvents = 0;
	std::shared_ptr<plugin_clipboard_t> pluginClipboard;
	static MainCtrl* get();
	~MainCtrl() {
		my_printf("~MainCtrl destructor\n",0);
	}
	static PlaybackThread* getPlayThread() {
		MainCtrl* ctrl = MainCtrl::get();
		return ctrl ? &ctrl->playThread : nullptr;
	}
	static guictr_plugins* getPluginCtr();
	static guictr_tracks* getGuiTrackCtr();
	String lastKey;
	dragdrop_midifile dragdropclip;
	dragdrop_target_indicator dragdropTarget;
	plugin_selection pluginSel;
	plugindatabase_t plugindb;
	tick_t tickJmpFrom = 0;
	tick_t tickJmpTo = 0;
	int nextTooltipId = 0;
//	int curTooltip = 0;
	scaled_grid& getGrid() {
		return grid;
	}
	clip_view& getClipView() {
		return clipView;
	}
	dragdrop_target_indicator& getDragDropTarget() {
		return dragdropTarget;
	}
	plugin_selection& getPluginSel() {
		return pluginSel;
	}
	edithistory& getHist() {
		return hist;
	}
	trackallcontainer_t& getTracks() {
		return trackList;
	}
	String& getProjectPath() {
		return projectPath;
	}
	WorkerThread* getWorkerThread() {
		return &workerThread;
	}

	/**
	 * Loads project file at location path
	 * @param path - path to a valid .project file
	 * @param flags - 0 or FLAG_DEFER_LOAD (don't load vst plugins, use placeholders)
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
	void setEmptyProject();
	void pushHist(action_base* action);
	void focusReceived() {
	}
	void focusLost() {
//		closeContextMenu();
	}
	void addDebug(String s);


	void setTempo(int32_t _tempo100) override;
	bool filesDropMove(ivec2 pos, int kbmods) override;
    bool filesDropBegin(std::vector<String>& files, ivec2 pos, int kbmods) override;
    bool filesDropFinal(std::vector<String>& files, ivec2 pos, int kbmods) override;
    void mouseMoved(ivec2 mousePos, ivec2 deltaPos) override;
	void menuCommand(int cmd) override;
	bool onWindowCloseRequest() override;
	void updateMenubar() override;
	void onTick();
	bool init(window_main* window, NVGcontext* nanovg);
	void postInit();
	void destroy();
	void unloadProject();
	void relayout(int32_t w, int32_t h);
	bool processGlobalKeyevent(KeyEvent& event) override;
	bool mouseDownPre() override;
	bool isZooming();
	void uncaptureMouse();
	void onUncaptureMouse();
	/**
	 * addTrackImpl - adds track to trackCtr and creates gui
	 * int32 trackInserPos - track-type-container local pos
	 */
	void addTrackImpl(int32_t trackInsertPos, track_t* t, int flags) override;
	void removeTrackImpl(track_t* t, int flags);
	track_t* getTrackId(uint32_t trackId);
	void removeTrackId(uint32_t trackId);
	void setEditClip(gui_clip* gclip);
	void setSelectedTrack(track_t* track);
	track_t* getSelectedTrack();
	void showPluginView();
	void showClipEditor();
	void prerender(int32_t x, int32_t y, int32_t w, int32_t h, float pixelRatio);

	void showAutomation(track_t* tr, automatable_t* at, int32_t paramIdx);
	bool isClipEditorVisible();
	bool isPluginViewVisible();
	track_t* createNewTrack(int trackType);
	track_t* insertNewTrack(int trackInsertPos, int trackType, int flags = FLG_TRK_CHANGE_USER);
	void updateGrid();
	void updateVisibleTrackContents();
	void setStatusText(String s);
	std::shared_ptr<clip_clipboard> copySelection(const Cursor& cursor);
	void pasteClipboard(clip_clipboard* c, int32_t trackOffset, tick_t tickOffset);
	void pasteClipboard(clip_clipboard* c, Cursor& cursor);
	void cutSelection(const Cursor& cursor);
	void cutIntersecting(track_t* tr, clip_t* mask);
	void cutIntersecting(track_t* tr, tick_t tickBegin, tick_t tickEnd);
//	void copyClipsInRange(trackcontents_t* in, trackcontents_t* out, int32_t srcPos, int32_t dstPos, int32_t len);

	void objectDragMove(guibase* g, MouseEvent& evt);
	void objectDragRelease(guibase* g, MouseEvent& evt);
	void preClipDelete(clip_t* clip);
	void preTrackDelete(track_t* clip);
	void startPlaying();
	void stopPlaying();
	/**
	 * setAudioThreadState - puts audio thread into requested state - synchronized
	 * 						 does not return before audio thread is in requested state
	 */
	void setAudioThreadState(playback_state state);
	bool isPlaying();
	bool toggleLoop();
	void setDragged(guibase* g);
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
	void initApp(int argc, char* argv[]);
};

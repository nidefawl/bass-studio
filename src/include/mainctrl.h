#pragma once
#include <list>
#include <vector>
#include <set>
#include <stdint.h>
#include <memory>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "config.h"
#include "str_util.h"
#include "seq_util.h"
#include "seq_time.h"
#include "seq_math.h"
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
#include "../threads/workerthread.h"
#include "../threads/playbackthread.h"
#include "edithistory.h"
#include "projectfile.h"
#include "hires_timer.h"
#include "../host/plugindatabase.h"
#include "rand.h"


using glm::vec2;
using glm::ivec2;
using glm::vec4;
using glm::ivec4;

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
//template <typename T>
//struct TrackRange
//{
//	TrackRange(std::vector<T*>& src) : vec(src) { }
//	std::vector<T*>& vec;
//    typedef T* iterator;
//    typedef const T* const_iterator;
//    iterator begin() { return vec.begin(); }
//    const_iterator begin() const { return vec.cbegin(); }
//    iterator end() { return vec.end(); }
//    const_iterator end() const { return vec.cend(); }
//};
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
class ContextCtrl
{
	bool isOK = false;
	NVGcontext* vg = NULL;
	guictxtmenu_base *ctxtmenu = NULL;
	window_overlay* window = NULL;
public:
	ivec2 mousepos;
	ContextCtrl()
	{
	};
	static ContextCtrl* get() {
		static ContextCtrl ctrl;
		return &ctrl;
	}
	void destroy();
	bool isOk() const {
		return isOK;
	}
	bool isShown() {
		return this->window->isShown();
	}
	void close();
	void open(guictxtmenu_base *ctxtmenu, ivec2 pos);
	bool init(window_overlay* window, NVGcontext* nanovg);
	void render(int32_t x, int32_t y, int32_t w, int32_t h, float ratio);
	void mouseDown(ivec2 mousePos, int button, bool doubleclick);
	void mouseUp(ivec2 mousePos, int button);
	void mouseMoved(ivec2 mousePos, ivec2 deltaPos);

};

struct clip_cursor_t {
	tick_t start = 0;
	tick_t end = 0;
};
inline bool operator==(const clip_cursor_t& lhs, const clip_cursor_t& rhs){
	return lhs.start == rhs.start && lhs.end == rhs.end;
}
inline bool operator!=(const clip_cursor_t& lhs, const clip_cursor_t& rhs){return !operator==(lhs,rhs);}
class ViewContainers;
class clip_view {
public:
	gui_clip * gui = NULL;
	clip_cursor_t cursor;
	clip_notes_t dragStartNotes;
	std::vector<note_t> draggedSelectionBegin;
	std::vector<note_t> draggedSelection;
	clip_notes_t clipboard;
	tick_t clipboardCursorRange;
	void set(gui_clip* _clip) {
		if (_clip == NULL) {
			this->gui = NULL;
		} else {
			this->gui = _clip;
		}
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
};
struct Menus {
	ngui::Menu file;
	ngui::Menu recent;
	ngui::Menu edit;
	ngui::Menu options;
};
class MainCtrl : public delete_cb, public project_t
{
	bool isOK = false;
	NVGcontext* vg = NULL;
	ViewContainers* view = NULL;
	guictxtmenu_base* ctxtmenu = NULL;
	ngui::MenuBar menubar;
	Menus menus;

	edithistory hist;
	std::vector<guictr_base*> containers;
	scaled_grid grid;
	clip_view clipView;
	WorkerThread workerThread;
	PlaybackThread playThread;
	String projectPath;
	hires_timer_t timer;
	KeyEvent keyEvent(int key, int scancode, int keyState, int mods, const char* key_name);
	track_t* selectedTrack = NULL;
	track_t* lastHoveredTrack = NULL;
	int32_t lastHoveredTrackTicks = 0;
	seq_rand rand;
public:
	int32_t numCallsWaitEvents = 0;
	window_main* mainWindow = NULL;
	static MainCtrl* get();
	static PlaybackThread* getPlayThread() {
		return &get()->playThread;
	}
	static guictr_plugins* getPluginCtr();
	bool isOk() const {
		return isOK;
	}
	int cursorIcon = CURSOR_DEFAULT;
	ivec2 m_size;
	ivec2 m_mousePos;
	guibase *guiOver = NULL;		//updates on mouse move "current mouseover"
	guibase *guiDragged = NULL;		//updates on mouse click "currently dragged", set from guiOver
	guibase *guiCaptured = NULL;	//updates when cursor is hidden, set from guiDragged
	guibase *guiFocused = NULL;		//updates on mouse click, set from guiOver
	guibase *guiCtrFocused = NULL;	//updates on mouse click, handles keyboard input
	String lastKey;
	ivec2 dragStart;
	ivec2 dragOffset;
	ivec2 dragDistance;
	dragdrop_midifile dragdropclip;
	plugindatabase_t plugindb;
	tick_t tickJmpFrom = 0;
	tick_t tickJmpTo = 0;
	scaled_grid& getGrid() {
		return grid;
	}
	clip_view& getClipView() {
		return clipView;
	}
	ngui::MenuBar& getMenubar() {
		return menubar;
	}
	edithistory& getHist() {
		return hist;
	}
	trackallcontainer_t& getTracks() {
		return trackList;
	}
	tick_t& getPlaybackPos() {
		return playbackPos;
	}
	std::shared_ptr<project_file> createProjectFile();
	void loadFile(String path);
	bool setLoadedProject(std::shared_ptr<project_file> file);
	void pushHist(action_base* action);
	void focusLost() {
		closeContextMenu();
	}
	WorkerThread* getWorkerThread() {
		return &workerThread;
	}
	void focusReceived() {
	}
	void resetMouseContext();
	void addDebug(String s);
	void render(int32_t x, int32_t y, int32_t w, int32_t h, float ratio);
	void mouseDown(ivec2 mousePos, int button, bool doubleclick);
	void mouseUp(ivec2 mousePos, int button);
	void onCharInput(unsigned int codepoint);
	void onKeyInput(int key, int scancode, int keyState, int mods, const char* key_name);
	void mouseScrolled(double xoffset, double yoffset);
	void mouseMoved(ivec2 mousePos, ivec2 deltaPos);
	void openContextMenu(guictxtmenu_base *b, ivec2 pos);
	bool filesDropMove(ivec2 pos);
    bool filesDropBegin(std::vector<String>& files, ivec2 pos);
    bool filesDropFinal(std::vector<String>& files, ivec2 pos);
	void menuCommand(int cmd);
	void onMenuOpen(ngui::Menu* menu);
	void onWindowCloseRequest();
	void updateMenubar();
	void closeContextMenu();
	bool hasContextMenu();
	guictxtmenu_base* getContextMenu();
	void onTick();
	void requestRedraw();
	bool init(window_main* window, NVGcontext* nanovg);
	void postInit();
	void destroy();
	void relayout(int32_t w, int32_t h);
	bool captureMouse(guibase* gui);
	void uncaptureMouse();
	void onUncaptureMouse();
	void addTrack(int32_t trackInsertPos, track_t* t);
	void removeTrack(track_t* t);
	void addTrackImpl(int32_t trackInsertPos, track_t* t);
	void removeTrackImpl(track_t* t);
	track_t* getTrackId(uint32_t trackId);
	void removeTrackId(uint32_t trackId);
	void setEditClip(gui_clip* gclip);
	void setSelectedTrack(track_t* track);
	track_t* getSelectedTrack();
	void showPluginView();
	void showClipEditor();
	bool isClipEditorVisible();
	bool isPluginViewVisible();
	track_t* insertNewTrack(int trackInsertPos, int trackType);
	void updateGrid();
	void updateVisibleTrackContents();
	void setStatusText(String s);
	float getCurrentTempoBPM() {
		return tempo100 / 100.0f;
	}
	int32_t getCurrentTempo() {
		return tempo100;
	}
	void setTempo(int32_t _tempo100);
	uint32_t sigNum() {
		return signatureNum;
	}
	uint32_t sigDen() {
		return 1<<signatureDenom;
	}
	uint32_t sigDenExp() {
		return signatureDenom;
	}
	void setNum(uint32_t n) {
		this->signatureNum = CLAMP_I(n, 1, 32);
	}
	void setDen(uint32_t d) {
		for (int i = 0; i <= 4; i++) {
			if (d < (1u << (i + 1u))) {
				this->signatureDenom = i;
				return;
			}
		}
	}
	double getProjectWorkingArea() {
		return 1000.0;
	}
	beatbar16th_t toBeatBar16th(int32_t tick) {
		beatbar16th_t t;
		uint8_t denom = 4-signatureDenom;
		uint8_t num = signatureNum;
		tick = tick / TICKS_16TH;
		t.th = tick & ((1<<denom) - 1);
		int32_t quarters = (tick>>denom);
		t.beat = uint32_t(quarters) % num;
		t.bar = quarters / num;
		if (tick < 0) {
			t.bar -= 1;
		}
		return t;
	}
	std::shared_ptr<clip_clipboard> copySelection(const Cursor& cursor);
	void pasteClipboard(clip_clipboard* c, int32_t trackOffset, tick_t tickOffset);
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
	bool isPlaying();
	bool toggleLoop();

	String getClipboardText();
	void setClipboardText(String s);
	void setJumpFromTo(tick_t tickJmpFrom, tick_t tickJmpTo) {
		this->tickJmpFrom = tickJmpFrom;
		this->tickJmpTo = tickJmpTo;
	}
	void onGuiRemoved(guibase* gui) {
		if (this->guiOver == gui)  {
			this->guiOver = NULL;
		}
		if (this->guiCaptured == gui)  {
			this->guiCaptured = NULL;
		}
		if (this->guiFocused == gui)  {
			this->guiFocused = NULL;
		}
		if (this->guiDragged == gui)  {
			this->guiDragged = NULL;
		}
		if (this->guiCtrFocused == gui)  {
			this->guiCtrFocused = NULL;
		}
	}
};

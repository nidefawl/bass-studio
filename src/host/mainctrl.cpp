#include <nanovg.h>
#include <GLFW/glfw3.h>
#include <time.h>
#include <algorithm>
#include <functional>
#include <vector>
#include <memory>

#include "mainctrl.h"
#include "math/seq_math.h"
#include "error.h"
#include "basectrl.h"
#include "window.h"
#include "platform.h"
#include "keyboard.h"
#include "commands.h"
#include "project.h"
#include "projectfile.h"
#include "grid.h"
#include "note.h"
#include "cursor.h"
#include "exceptions.h"
#include "color_util.h"
#include "str_util.h"
#include "seq_util.h"
#include "appsettings.h"
#include "track.h"
#include "clip.h"
#include "fileloader.h"
#include "edithistory.h"
#include "logging.h"
#include "menu.h"
#include "thread.h"
#include "msgbox.h"
#include "tls.h"

#include "../gui/gui.h"
#include "../gui/guicontainer.h"
#include "../gui/button.h"
#include "../gui/splitter.h"
#include "../gui/guicontextmenu_base.h"
#include "../gui/tempocontrols.h"
#include "../gui/scrollbar.h"
#include "../gui/statusbar.h"
#include "../gui/pluginctr.h"
#include "../gui/clipeditor.h"
#include "../gui/trackctr.h"
#include "../gui/trackctr_nodes.h"
#include "../gui/trackcontent.h"
#include "../gui/list.h"
#include "../gui/pluginlist.h"
#include "../gui/guimenu.h"
#include "../gui/debugctr.h"
#include "../gui/drawwaveform.h"
#include "../gui/guishaderview.h"
#include "../gui/about.h"
#include "../gui/dialog_io.h"
#include "../gui/dialogs.h"
#include "../gui/guicontainer_layout.h"

#include "plugin/base_plugin.h"
#include "plugin/vst_plugin.h"
#include "track_impl.h"
#include "audiocache.h"
#include "seq_time.h"

#include "../gui/guiplugin.h"
#include "../threads/workerthread.h"
#include "../threads/playbackthread.h"
#include "plugindatabase.h"
#include "window_impl.h"

#include "vst_host.h"
#include "audio_host.h"
#include "midi_host.h"

	const int FLAG_DEFER_LOAD = 0x1;
	const int FLAG_INVOKE_USER_CB_DEFERLOAD = 0x2;

	int32_t getNumClipAllocations();

void dragdrop_midifile::reset() {
	if (isLoaded) {

		my_printf("meeeh, reset!\n",0);
	}
	isValidTarget = false;
	isLoaded = false;
	clipboard.reset();
}
void testTask() {

	ThreadTaskTest task;
	task.id = 3;
	task.a = 4;
	task.b = 5;
	WorkerThread* t = MainCtrl::get()->getWorkerThread();
	if (!t->pushTask(&task)) {
		throw new appexception("thread task failed");
	}
	if (task.isInQueue()) {
		task.wait();
	}
	if (task.isError()) {
		printf("task[%d] iserror: %d\n", task.id, task.result);
		std::exception_ptr eptr = task.getException();
		if (eptr != nullptr) {
			printf("task[%d] had exception.. rethrowing\n",task.id);
	        try{
	        	std::rethrow_exception(eptr);
	        }
	        catch(const std::exception &ex)
	        {
				printf("task[%d] had exception: %s\n",task.id,  ex.what());
	        }
		}
	} else if (task.isGood()) {
		printf("task[%d] isGood: %d\n", task.id, task.result);
	} else {
		printf("task[%d] was not processed!\n", task.id);
	}

}
guictr_base* makeCtrProperties(); //guiproperties.cpp
guictr_base* makeCtrTheme(); //guiproperties.cpp
guictr_base* makeCtrHistory(); //guihistory.cpp

class guictr_side_tabs_daw_1 : public guictr_tabbed {
public:
	gui_ctr_debug ctr_dbg;
	guictr_base* const ctr_properties;
	guictr_base* const ctr_theme;
	guictr_base* const ctr_history;
	gui_shaderview shaderView;
	guictr_side_tabs_daw_1() : guictr_tabbed(), ctr_properties(makeCtrProperties()), ctr_theme(makeCtrTheme()), ctr_history(makeCtrHistory()) {
		setBackgroundRendered(true);
		ctr_dbg.setLabel("Debug 1");
		ctr_properties->setLabel("Properties");
		ctr_theme->setLabel("Theme");
		ctr_history->setLabel("History");
		shaderView.setLabel("Shader");
		addEntry(&ctr_dbg, ctr_dbg.label);
		addEntry(ctr_history, ctr_history->label);
		addEntry(ctr_properties, ctr_properties->label);
		addEntry(ctr_theme, ctr_theme->label);
		addEntry(&shaderView, shaderView.label);
		setActiveEntry(0);
	}
	virtual ~guictr_side_tabs_daw_1() {
		//remove the entries we have to delete, base class would see dangling ptr otherwise
		remove(ctr_properties);
		remove(ctr_theme);
		remove(ctr_history);
		delete ctr_properties;
		delete ctr_theme;
		delete ctr_history;
	}
};
class guictr_effectlibrary : public guictr_base {
public:
	guictr_pluginlibrary ctr_pluginlist;
	guictr_modulelibrary ctr_effectlist;
	guictr_effectlibrary() : guictr_base() {
		setBackgroundRendered(false);
		padding = 0;
		margin = 0;
		add(&ctr_pluginlist);
		add(&ctr_effectlist);
	}
	virtual ~guictr_effectlibrary() {
		removeGuis();
	}
	void update() {
		ctr_pluginlist.update();
		ctr_effectlist.update();
	}
	void layout() {
		ctr_pluginlist.size.x = size.x;
		ctr_pluginlist.size.y = size.y/2;
		ctr_effectlist.size.x = size.x;
		ctr_effectlist.size.y = size.y/2;
		ctr_pluginlist.pos = {0, 0};
		ctr_effectlist.pos = {0, ctr_pluginlist.bottom()};
		for (guibase* gui : guis) {
			gui->layout();
		}
	}
};
guictr_base* makeGuiPluginsLoadedList();
class guictr_side_tabs_daw_2 : public guictr_tabbed {
public:

	guictr_effectlibrary& ctr_effectlib;
	guictr_base* const ctr_properties;
	guictr_base* const ctr_loadedplugins;
	gui_ctr_debug ctr_dbg;
	guictr_side_tabs_daw_2(guictr_effectlibrary& _ctr_effectlib)
	: guictr_tabbed(),
	  ctr_effectlib(_ctr_effectlib),
	  ctr_properties(makeCtrProperties()),
	  ctr_loadedplugins(makeGuiPluginsLoadedList()) {
		setBackgroundRendered(true);
		ctr_effectlib.setLabel("Plugins");
		ctr_loadedplugins->setLabel("Instances");
		ctr_properties->setLabel("Properties");
		ctr_dbg.setLabel("Debug 1");
		addEntry(&ctr_dbg, ctr_dbg.label);
		addEntry(&ctr_effectlib, ctr_effectlib.label);
		addEntry(ctr_loadedplugins, ctr_loadedplugins->label);
		addEntry(ctr_properties, ctr_properties->label);
		setActiveEntry(0);
	}
	virtual ~guictr_side_tabs_daw_2() {
		//remove the entries we have to delete, base class would see dangling ptr otherwise
		remove(ctr_properties);
		remove(ctr_loadedplugins);
		delete ctr_properties;
		delete ctr_loadedplugins;
	}
};
class DawViewContainers {
	guictr_noteeditor noteeditor;
public:
	guictr_effectlibrary ctr_effectlib;
	guictr_menubar ctr_menu;
	guictr_tempocontrols ctr_tempo;
	guictr_plugins ctr_plugins;
	guictr_test ctr_test;
	gui_statusbar statusbar;
	guictr_pluginview ctr_pluginview;
	guictr_clipeditorview ctr_clipeditorview;
	guictr_clipeditor ctr_clipeditor;
	guictr_tracks ctr_tracks;
	guictr_nodes ctr_nodes;
	guictr_side_tabs_daw_1 subctr_tabbed;
	guictr_side_tabs_daw_2 subctr_tabbed2;
	guictr_stacked ctr_stack_right;
//	Splitter splitterList;
	Splitter splitterCenter;
	Splitter splitterRight;
	DawViewContainers(ngui::MenuBar& menubar, DAW::Cursor& _cursor, project_t& project, scaled_grid& grid, clip_view& clipView, dragdrop_midifile& dragdropclip)
	  : noteeditor(clipView),
	  ctr_menu(menubar),
	  ctr_tempo(project),
	  ctr_pluginview(&ctr_plugins),
	  ctr_clipeditorview(noteeditor),
	  ctr_clipeditor(noteeditor, clipView),
	  ctr_tracks(_cursor, project, grid, dragdropclip),
	  ctr_nodes(_cursor, project, dragdropclip),
	  subctr_tabbed(),
	  subctr_tabbed2(ctr_effectlib),
	  ctr_stack_right(),
//	  splitterList(0, 0.5f),
	  splitterCenter(0, 0.7f),
	  splitterRight(1, 0.8f)
	{
		ctr_stack_right.addEntry(&subctr_tabbed2, "Top");
		ctr_stack_right.addEntry(&subctr_tabbed, "Bottom");
		ctr_stack_right.setBackgroundRendered(false);
		ctr_stack_right.padding = 0;
		ctr_stack_right.margin = 0;
		ivec2 inset = {guictr_stacked::STACK_ENTRY_BTN_SIZE+INSET_CTR_SPACING*3, INSET_CTR_SPACING};
		subctr_tabbed.setTabMenuInset(inset);
		subctr_tabbed2.setTabMenuInset(inset);
		splitterCenter.setMinMax(0.25f, 0.9f);
//		splitterList.setMinMax(0.1f, 0.9f);
		splitterRight.setMinMax(0.2f, 0.9f);
	}
	void layout(int32_t winW, int32_t winH) {
		int winX = 0; int winY = 0;
		int winBottom = winH;
#if USE_GUI_MENU
		int hMenu = 28;
		winH -= hMenu;
		winY += hMenu;
		ctr_menu.pos = vec2(0, 0);
		ctr_menu.size = vec2(winW, hMenu);
#endif
		int hTopControls = 48;
		int hStatusBar = 60;
		int hCenter = winH - hTopControls - hStatusBar;
		int hRight = winH - hTopControls;
		int hTrackCtr = splitterCenter.leftOrTop(hCenter);
		int hEditor = splitterCenter.rightOrBottom(hCenter);
//		int heightList = splitterList.leftOrTop(hRight);
//		int heightDebug = splitterList.rightOrBottom(hRight);
		int width = splitterRight.leftOrTop(winW);
		int wRight = splitterRight.rightOrBottom(winW);
		ctr_tempo.size = { winW, hTopControls };
		ctr_tracks.size = { width, hTrackCtr };
		ctr_nodes.size = { width, hTrackCtr };
		ctr_clipeditor.size = { width, hEditor };
		ctr_plugins.size = { width, hEditor };
		ctr_pluginview.size = { 300, hStatusBar };
		ctr_clipeditorview.size = { 300, hStatusBar };
		int wbottom = width;
		wbottom -= 60; //rightmost part
		wbottom -= ctr_pluginview.size.x;
		wbottom -= ctr_clipeditorview.size.x;
		statusbar.size = { wbottom, hStatusBar };

		ctr_tempo.pos = { winX, winY };
		ctr_tracks.pos = { winX, winY+hTopControls };
		ctr_nodes.pos = { winX, winY+hTopControls };
		statusbar.pos = { winX, winBottom - hStatusBar };
		ctr_clipeditorview.pos = { statusbar.right(), winBottom - hStatusBar };
		ctr_pluginview.pos = { ctr_clipeditorview.right(), winBottom - hStatusBar };
		ctr_plugins.pos = { winX, winBottom - hStatusBar - hEditor};
		ctr_clipeditor.pos = { winX, winBottom - hStatusBar - hEditor };
		splitterCenter.pos = ivec2(winX, ctr_clipeditor.pos.y - 5);
		splitterCenter.size = ivec2(width, 10);

		ctr_tempo.setSnapSides(ivec4(0, 0, 0, 1));
		statusbar.setSnapSides(ivec4(0, 1, 0, 0));
		ctr_clipeditorview.setSnapSides(ivec4(0, 1, 0, 0));
		ctr_pluginview.setSnapSides(ivec4(0, 1, 0, 0));
		ctr_clipeditor.setSnapSides(ivec4(0, 1, 0, 0));
		ctr_plugins.setSnapSides(ivec4(0, 1, 0, 0));
		subctr_tabbed2.setSnapSides(ivec4(1, 0, 0, 1));
		ctr_stack_right.setSnapSides(ivec4(1, 1, 1, 1));

		subctr_tabbed.setSnapSides(ivec4(1, 0, 0, 0));


//		ctr_tabbed.pos = {width, winY+hTopControls+heightList};
//		ctr_tabbed.size = {wRight, heightDebug};
//		ctr_tabbed2.pos = {width, winY+hTopControls};
//		ctr_tabbed2.size = {wRight, heightList};
		ctr_stack_right.pos = {width, winY+hTopControls};
		ctr_stack_right.size = {wRight, hRight};


		splitterRight.pos = ivec2(ctr_stack_right.pos.x - 5, hTopControls);
		splitterRight.size = ivec2(10, hRight);
//		splitterList.pos = ivec2(ctr_tabbed.pos.x, ctr_tabbed2.bottom()-5);
//		splitterList.size = ivec2(wRight, 10);
	}
	void addTo(std::vector<guictr_base*>& v) {
		 v.push_back(&ctr_tracks);
		 v.push_back(&ctr_clipeditor);
		 v.push_back(&ctr_tempo);
		 v.push_back(&ctr_pluginview);
		 v.push_back(&ctr_clipeditorview);
		 v.push_back(&ctr_stack_right);
//		 v.push_back(&ctr_tabbed2);
		 v.push_back(&statusbar);
//		 v.push_back(&ctr_tabbed);
#if USE_GUI_MENU
		 v.push_back(&ctr_menu);
#endif
		 v.push_back(&splitterCenter);
//		 v.push_back(&splitterList);
		 v.push_back(&splitterRight);
	}
};
void MainCtrl::setViewMode(view_mode_t mode) {
	this->viewMode = mode;
	switch (mode) {
	case MIXER:
	case TRACK_TIMELINE:
		containers[0] = &view->ctr_tracks;
		break;
	case NODE_EDITOR:
		containers[0] = &view->ctr_nodes;
		break;
	}
	view->ctr_tracks.setVisible(containers[0] == &view->ctr_tracks);
	view->ctr_nodes.setVisible(containers[0] == &view->ctr_nodes);
	view->ctr_nodes.refresh();
	focusGui(containers[0]);
}
view_mode_t MainCtrl::getViewMode() {
	return this->viewMode;
}
void MainCtrl::showPluginView() {
	containers[1] = &view->ctr_plugins;
}
void MainCtrl::showClipEditor() {
	containers[1] = &view->ctr_clipeditor;
}
bool MainCtrl::isClipEditorVisible() {
	return containers[1] == &view->ctr_clipeditor;
}
bool MainCtrl::isPluginViewVisible() {
	return containers[1] == &view->ctr_plugins;
}
void MainCtrl::addDebug(String s) {

	view->subctr_tabbed.ctr_dbg.addStr(s);
}

void MainCtrl::resetMouseContext() {
	BaseCtrl::resetMouseContext();
	view->ctr_nodes.reset();
}

void MainCtrl::unloadProject() {
	dbgassert(playThread.isLocked());
	closeContextMenu();
	resetMouseContext();
	projectPath = "";
	setSelectedTrack(NULL);
	clipView.set(NULL);
	cursor.setEmptySelection();

	hist.clear(this);

//	std::shared_ptr<clip_clipboard>& clipboard = view->ctr_tracks.trackView.clipboard;
//	clipboard.reset();
	std::vector<track_t*> _tracks = trackList.getAllTracksFlatVec();  // iterate a copy
	std::vector<track_t*> _rootTracks = trackList.getAllTracksTreeVec();
	my_printf("unloading project with %d tracks\n", _tracks.size());
	for (auto it = _tracks.rbegin(); it != _tracks.rend(); it++) {
		track_t* track = *it;

		// no need to, this is done by trackcontainer_tracktype_t
		//		if (track->parent != nullptr)  {
		//			track->parent->removeChild(track);
		//		}


		my_printf("remove track %s\n", StringAsCStr(track->name));
		removeTrackImpl(track, FLG_TRK_CHANGE_LOAD);
	}
	trackList.clear();
	for (auto it = _tracks.rbegin(); it != _tracks.rend(); it++) {
		track_t* track = *it;
		my_printf("delete track %s\n", StringAsCStr(track->name));
		releaseTrackResources(track, this);
		delete track;
	}

	vsthost::getInstance()->releaseProjectResources();

	this->view->ctr_tracks.trackView.resizePreModifyState.reset();
	this->view->ctr_tracks.trackView.clipboard.reset();
	this->view->ctr_tracks.trackView.action.clipboard.reset();
	this->view->ctr_tracks.trackView.tracksVisibleFlat.clear();

	{

		auto* host = vsthost::getInstance();
		std::vector<effectbase*> pluginsDeferred;
		host->getDeferredEffects(pluginsDeferred);
		dbgassert(pluginsDeferred.empty());
	}

}

bool MainCtrl::onWindowCloseRequest() {
	return true;
}

void MainCtrl::updateMenubar() {
	menubar.disableAll = this->ctxtmenu != NULL;
	ngui::Menu* undo = menus.edit.getByCmd(CMD_UNDO);
	ngui::Menu* redo = menus.edit.getByCmd(CMD_REDO);
	if (hist.canUndo()) {
		undo->disabled = false;
		undo->title = menuName(StringFormat("Undo %s", StringAsCStr(hist.getUndoStep())), KC_UNDO);
	} else {
		undo->disabled = true;
		undo->title = menuName("Undo", KC_UNDO);
	}
	if (hist.canRedo()) {
		redo->disabled = false;
		redo->title = menuName(StringFormat("Redo %s", StringAsCStr(hist.getRedoStep())), KC_REDO);
	} else {
		redo->disabled = true;
		redo->title = menuName("Redo", KC_REDO);
	}
}

static SupportedFileType FILE_TYPE_PROJECT {"Project File", PROJECT_FILE_EXT};
std::vector<SupportedFileType> vFILE_TYPE_PROJECT = { FILE_TYPE_PROJECT };

void MainCtrl::loadFileCStr(const char* str) {
	loadFile(str, 0);
}
void MainCtrl::saveFile(const String& path) {
	if (!path.empty())
	{
		std::shared_ptr<project_file> f = createProjectFile();
		saveProject(f, path);
		projectPath = path;
	}
}
void MainCtrl::loadFile(String path, int flags) {
	timer.reset();
	std::shared_ptr<project_file> f = loadProjectFile(path);
	double l1 = timer.getTimeDoubleReset();
	if (!f) {
		setStatusText(StringFormat("Failed loading %s", StringAsCStr(FileNameFromPath(path))));
	} else {
		const bool wasUserCallback = (flags&FLAG_INVOKE_USER_CB_DEFERLOAD) != 0;
		auto cb = [this, path, l1, projFile=f, wasUserCallback](int n) {
			try {
				timer.reset();
				int loadFlags = 0;
				if (wasUserCallback) {
					loadFlags = n==0 ? FLAG_DEFER_LOAD : 0;
				} else {
					loadFlags = n;
				}
				setLoadedProject(projFile, loadFlags);
				double l2 = timer.getTimeDoubleReset();
				log_printf("Loading file %s took %f %f\n", StringAsCStr(path), l1, l2);
			} catch (std::exception& e) {
				handleStdException(e);
			}
		};
		if ((flags&FLAG_INVOKE_USER_CB_DEFERLOAD) == 0) {
			cb(flags&FLAG_DEFER_LOAD);
		} else {
			guidialog_cb_yes_no* dlg = new guidialog_cb_yes_no();
			dlg->cb = cb;
			dlg->message = "Load plugins?";
			openDialog(dlg);
		}
	}
}
void MainCtrl::setEmptyProject() {
	ThreadLock lock = playThread.lockThread();
	unloadProject();
	int totalAllocs = getNumClipAllocations();
	if (totalAllocs != 0) {
		log_printf("getNumClipAllocations == %d!\n", totalAllocs);
		dbgassert(getNumClipAllocations() == 0);
	}
	insertNewTrack(-1, TRACK_TYPE_MIDI, FLG_TRK_CHANGE_LOAD);
	insertNewTrack(-1, TRACK_TYPE_MASTER, FLG_TRK_CHANGE_LOAD);
}
#if CREATE_DEBUG_COMPANION_WINDOW
void drawDebugWindow(NVGcontext* ctx, int winW, int winH, float pxratio);
int initDebugWindow();
void openDebugWindow(window_main* mainwindow) {
	dbgassert(mainwindow);
	window_dialog* dialog = mainwindow->createDialog("waveform atlas cache", 1280, 720);
	window_init_fn init;
	window_draw_fn drawFn;
	init.initCallback = []() {
		initDebugWindow();
	};
	drawFn.drawCallback = [](NVGcontext* ctx, int winW, int winH, float pxratio) {
		drawDebugWindow(ctx, winW, winH, pxratio);
	};
	dialog->setDrawFunction(drawFn);
	dialog->setInitFunction(init);
	dialog->show();
}
#endif
void MainCtrl::menuCommand(int cmd) {
	try {
	String path = projectPath;
	switch (cmd) {
	case CMD_UNDO:
		if (hist.canUndo()) {
			ThreadLock lock = playThread.lockThread();
			hist.undoStep(this);
			updateVisibleTrackContents();
		}
		break;
	case CMD_REDO:
		if (hist.canRedo()) {
			ThreadLock lock = playThread.lockThread();
			hist.redoStep(this);
			updateVisibleTrackContents();
		}
		break;
	case CMD_FILE_NEW:
	{
		//TODO: stop playback here
		setEmptyProject();
		MainCtrl::getGuiTrackCtr()->layout();
		MainCtrl::get()->updateVisibleTrackContents();
	}
		break;
	case CMD_FILE_OPEN:
		{
			String path;
			if (promptUserFilePath(window, 0, vFILE_TYPE_PROJECT, path)) {
				loadFile(path, FLAG_INVOKE_USER_CB_DEFERLOAD);
			}
		}
		break;
	case CMD_FILE_SAVEAS:
	case CMD_FILE_SAVE: {
			if (cmd == CMD_FILE_SAVEAS || path.empty()) {
				if (!promptUserFilePath(window, 1, vFILE_TYPE_PROJECT, path)) {
					break;
				}
			}
			saveFile(path);
		}
		break;
	case CMD_FILE_CLOSE:
		break;
	case CMD_CUT:
		break;
	case CMD_COPY:
		break;
	case CMD_PASTE:
		break;
	case CMD_DELETE:
		break;
	case CMD_SELECT_ALL:
		break;
	case CMD_DUPLICATE:
		break;
	case CMD_GUI_GLOBAL_ZOOM_DECREASE:
		m_scale = math::max(0.05f, m_scale - 0.05f);
		BaseCtrl::relayout();
		break;
	case CMD_GUI_GLOBAL_ZOOM_INCREASE:
		m_scale = math::min(10.0f-0.05f, m_scale + 0.05f);
		BaseCtrl::relayout();
		break;
	case CMD_INSERT_AUDIO_TRACK:
	case CMD_INSERT_MIDI_TRACK:
	case CMD_INSERT_RETURN_TRACK:
	case CMD_INSERT_MASTER_TRACK:
	{

		int32_t trackType = (cmd-CMD_INSERT_AUDIO_TRACK)%NUM_TRACK_TYPES;
		MainCtrl::get()->insertNewTrack(-1, trackType);
	}
		break;
	case CMD_ABOUT:
		this->openDialog(new guidialog_about());
		break;
	case CMD_SHOW_DEBUG_WINDOW:
#if CREATE_DEBUG_COMPANION_WINDOW
		openDebugWindow(dynamic_cast<window_main*>(this->window));
#endif
		break;
	case CMD_PREFERENCES:
		this->openDialog(new guidialog_settings());
		break;
	case CMD_EXIT:
		mainWindow->requestClose();
		break;

	}
	} catch (std::exception& e) {
		handleStdException(e);
	}
}
void MainCtrl::postInit() {
	audiohost::getInstance()->initPa();
	midihost::getInstance()->initPm();
	if (settings.startEngine) {
		vsthost* host = vsthost::getInstance();
		audiohost* audioHost = audiohost::getInstance();
		if (audioHost->startAudio(settings.iosettings)) {
			host->setOutput(audioHost);
		} else {
			//notify user
			log_printf("audioHost->startAudio() failed\n", 0);
		}
	}
	midihost::getInstance()->startMidi();
//	vsthost::getInstance()->postInit();
	if (!loadProject.empty()) {
		loadFile(loadProject, FLAG_DEFER_LOAD);
	}

	view->ctr_effectlib.update();
	setAudioThreadState(playback_state::status_stop);
}

void MainCtrl::destroy()
{
	if (!isOK) {
		return;
	}
	setAudioThreadState(playback_state::status_no_process);
	dbgassert(playThread.getState() == playback_state::status_no_process);
	ThreadLock lock = playThread.lockThread();
	midihost::getInstance()->stopMidi();
	audiohost::getInstance()->stopAudio();
	unloadProject();
	int totalAllocs = getNumClipAllocations();
	if (totalAllocs != 0) {
		log_printf("getNumClipAllocations == %d!\n", totalAllocs);
		dbgassert(getNumClipAllocations() == 0);
	}
	vsthost::getInstance()->unload();
	vsthost::getInstance()->destroy();
	audiohost::getInstance()->deinitPa();
	midihost::getInstance()->deinitPm();
	waveformrender::getInstance()->destroy();
	settings.dens = grid.grid_dens;
	isOK = false;
	delete view;
	plugindb.closeDatabase();
	this->workerThread.stopThread();
	this->workerThread.joinThread();
	this->playThread.stopThread();
	this->playThread.joinThread();
	daw_tls::tlsinstance& tls = daw_tls::getTls();
	delete tls.waveform;
	delete tls.audioCache;
	delete tls.midiHost;
	delete tls.host;
	delete tls.audioHost;
	tls.host = nullptr;
	tls.midiHost = nullptr;
	tls.audioHost = nullptr;
	tls.mainCtrl = nullptr;
	tls.project = nullptr;
	tls.pluginDatabase = nullptr;
	tls.waveform = nullptr;
	tls.audioCache = nullptr;
}
void MainCtrl::initApp(int argc, char* argv[]) {
	for (int i = 1; i < argc; i++) {
		String s = argv[i];
		if (s == "--load" && i+1 < argc) {
			loadProject = argv[i+1];
		}
	}
	daw_tls::tlsinstance& tls = daw_tls::getTls();
	auto audioHost = new audiohost();
	auto host = new vsthost();
	auto midiHost = new midihost();
	if (!vsthost::assignMasterCallback(host)) {
		delete host;
		dbgassert(0);
		throw applogicexception("no empty vst callback slot");
	}
	host->setSampleFormat(sampleformat_t{static_cast<samplerate_t>(settings.iosettings.samplerate), settings.iosettings.blocksize, sampleformat_bits_t::FLOAT_32});
	tls.project = this;
	tls.mainCtrl = this;
	tls.audioHost = audioHost;
	tls.host = host;
	tls.midiHost = midiHost;
	tls.pluginDatabase = &plugindb;
	tls.audioCache = new audiocache(settings.iosettings.samplerate);
	tls.waveform = new waveformrender();
}
bool MainCtrl::init(window_main* window, NVGcontext* nanovg)
{
	this->mainWindow = window;
	this->window = window;
	this->vg = nanovg;
	plugindb.openDatabase();
	this->playThread.setTls(daw_tls::getTls());
	this->playThread.startThread(this);
	this->workerThread.setTls(daw_tls::getTls());
	this->workerThread.startThread();
	this->workerThread.call([]() {
		my_printf("WorkerThreadCallTest\n", 0);
	})->wait();
	themes.loadThemes();

	getDefaultTheme()->initTheme();
	getDefaultTheme()->bindFonts();

	view = new DawViewContainers(menubar, cursor, *this, grid, clipView, dragdropclip);
	view->addTo(this->containers);
	for (guictr_base *ctr : containers) {
		ctr->setControl(this);
	}
	view->ctr_plugins.setControl(this);
	view->ctr_nodes.setControl(this);
	view->ctr_tracks.setVisible(containers[0] == &view->ctr_tracks);
	view->ctr_nodes.setVisible(containers[0] == &view->ctr_nodes);

	menus.recent.type = ngui::menu_type::submenu;
	menus.recent.title = "Open recent";
	menus.recent.addCommand(CMD_FILE_OPEN, "File 1");
	menus.recent.addCommand(CMD_FILE_OPEN, "File 2");
	menus.recent.addCommand(CMD_FILE_OPEN, "File 4");
	menus.recent.addCommand(CMD_FILE_OPEN, "File 5");
	menus.file.type = ngui::menu_type::submenu;
	menus.file.title = "File";
	menus.file.addCommand(CMD_FILE_NEW, menuName("New", KC_NEW), ICON_FILE);
	menus.file.addCommand(CMD_FILE_OPEN, menuName("Open", KC_OPEN), ICON_FOLDER);
	menus.file.add(&menus.recent);
	menus.file.addCommand(CMD_FILE_SAVE, menuName("Save", KC_SAVE), ICON_SAVE);
	menus.file.addCommand(CMD_FILE_SAVEAS, "Save As");
	menus.file.addSeperator();
	menus.file.addCommand(CMD_EXIT, "Quit");
	menus.edit.type = ngui::menu_type::submenu;
	menus.edit.title = "Edit";
	menus.edit.addCommand(CMD_UNDO, menuName("Undo", KC_UNDO));
	menus.edit.addCommand(CMD_REDO, menuName("Redo", KC_REDO));
	menus.edit.addSeperator();
	menus.edit.addCommand(CMD_CUT, menuName("Cut", KC_CUT));
	menus.edit.addCommand(CMD_COPY, menuName("Copy", KC_COPY));
	menus.edit.addCommand(CMD_PASTE, menuName("Paste", KC_PASTE));
	menus.edit.addCommand(CMD_DUPLICATE, menuName("Duplicate", KC_DUPLICATE));
	menus.edit.addSeperator();
	menus.edit.addCommand(CMD_DELETE, menuName("Delete", KC_DELETE));
	menus.edit.addCommand(CMD_SELECT_ALL, menuName("Select All", KC_SELECTALL));
	menus.tools.type = ngui::menu_type::submenu;
	menus.tools.title = "Tools";
	menus.tools.addCommand(CMD_PREFERENCES, "Preferences");
	menus.tools.addCommand(CMD_SHOW_DEBUG_WINDOW, "Show Debug Window");
	menus.tools.addCommand(CMD_ABOUT, "About");

	menubar.add(&menus.file);
	menubar.add(&menus.edit);
	menubar.add(&menus.tools);
	this->updateMenubar();
#if !USE_GUI_MENU
	this->mainWindow->updateMenu();
#endif

	waveformrender::getInstance()->init();
	setEmptyProject();

	grid.grid_dens = settings.dens;

//	vsthost::getInstance()->setSamplerateBlockSize(settings.iosettings.samplerate, settings.iosettings.blocksize);
	updateGrid();
	isOK = true;
	return isOK;
}
void MainCtrl::onTick()
{
	const bool bWroteMidiData = vsthost::getInstance()->writeRecordedData();

	if (bWroteMidiData) {
		this->updateVisibleTrackContents();
	}
//	double since = timer.getTimeDoubleReset();
	vsthost::getInstance()->onTick();
	for (guictr_base *ctr : containers) {
		ctr->onTick(this);
	}
	for (guictr_base *ctr : containers) {
		ctr->onIdle();
	}
//	if (rand.rng_rand(100000) == 0) {
//		throw std::bad_alloc();
//	}
//	my_printf("onTick %d\n", std::this_thread::get_id());
//	waveformrender::getInstance()->renderUpdates(vg, 0);
//	if (isPlaying()) {
		mainWindow->requestRedraw();
//	}
	if (!guiDragged && !guiCaptured && guiOver && (!this->ctxtmenu || ctxtmenu->isTransient())) {
		int32_t hoverTicks = 0;
		if (ctxtmenu && ctxtmenu->isTransient() && (lastTooltipSrc && guiOver && guiOver != lastTooltipSrc)) {
			closeContextMenu();
		}
		if (ctxtmenu && !ctxtmenu->isTransient()) {
			hoverTicks = 0;
		}
		if (!ctxtmenu && guiOver == lastHoveredTooltip) {
			hoverTicks = lastHoveredTooltipTicks + 1;
			if (lastHoveredTooltipTicks >= 12) {
				auto ctxtmenu = guiOver->getTooltip(this);
				if (ctxtmenu) {
					lastTooltipSrc = guiOver;
					nextTooltipId++;
					openContextMenu(ctxtmenu, m_mousePos+ivec2(-16,26));
				}
				hoverTicks = 0;
			}
		}
		lastHoveredTooltipTicks = hoverTicks;
		lastHoveredTooltip = guiOver;
	} else {
		if (ctxtmenu && ctxtmenu->isTransient()) {
			closeContextMenu();
		}
	}
	if (guiDragged && !guiCaptured && guiDragged->isDragMoveable()) {
		track_t *tr = NULL;
		int32_t hoverTicks = 0;
		ivec2 trackViewLocalPos = toControlsObjectSpace(m_mousePos, &view->ctr_tracks);
		guictr_base& ctrMixers = view->ctr_tracks.trackControls;
		if (ctrMixers.contains(trackViewLocalPos)) {
			ivec2 posRelative = m_mousePos - ctrMixers.toScreenSpace(ivec2(0));
			tr = getTrackFromMouse(this->view->ctr_tracks.trackView, *this, posRelative, false);
			if (tr && tr == lastHoveredTrack && selectedTrack != tr) {
				hoverTicks = lastHoveredTrackTicks + 1;
				if (lastHoveredTrackTicks >= 6) {
					setSelectedTrack(tr);
					showPluginView();
					hoverTicks = 0;
				}
			}
		} else if (view->ctr_pluginview.contains(m_mousePos)) {
			hoverTicks = lastHoveredTrackTicks + 1;
			if (lastHoveredTrackTicks >= 6) {
				showPluginView();
				hoverTicks = 0;
			}
		} else if (view->ctr_clipeditorview.contains(m_mousePos)) {
			hoverTicks = lastHoveredTrackTicks + 1;
			if (lastHoveredTrackTicks >= 6) {
				showClipEditor();
				hoverTicks = 0;
			}
		}
		lastHoveredTrackTicks = hoverTicks;
		lastHoveredTrack = tr;
	}
}

void MainCtrl::pushHist(action_base* action) {
	hist.push(this, action);
}
std::shared_ptr<project_file> MainCtrl::createProjectFile() {
	ThreadLock lock = playThread.lockThread();
	std::shared_ptr<project_file> file = std::make_shared<project_file>();
	file->path = projectPath;
	copyTo(file->project);
	audiocache::getInstance()->store(file->sampleFileIndex);
	file->layout.layoutGrid = grid;
	file->layout.scrollOffsetX = view->ctr_tracks.getScrollOffset();
	return file;
}
void MainCtrl::setDragged(guibase* g) {
	guiDragged = g;
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
 * @param flags - 0 or FLAG_DEFER_LOAD (don't load vst plugins, use placeholders)
 * @return
 */
bool MainCtrl::setLoadedProject(std::shared_ptr<project_file> file, int flags) {

	setAudioThreadState(playback_state::status_no_process);
	my_printf("loading %s: %d tracks\n", StringAsCStr(file->path), trackList.size());

	ThreadLock lock = playThread.lockThread();
	unloadProject();
	/** make sure call to unloadProject unloaded all vst2 instances **/
	dbgassert(vsthost::getInstance()->getVst2Instances().empty());
	//TODO: assert that audiocache is empty
	audiocache::getInstance()->load(file->sampleFileIndex);

	/** populates trackList **/
	project_t::copyFrom(file->project);

	vsthost* host = vsthost::getInstance();
	/** create all audio instances **/
	for (track_t* t : trackList) {
		host->createAudio(t);
	}


	/** create all gui instances **/
	for (track_t* tr : trackList) {
		view->ctr_tracks.addTrack(tr, FLG_TRK_CHANGE_LOAD);
	}
	/** pre-load all plugin instances **/
	trackList.loadPlugins(file->project);

	/** reset maximum stage id and determine new maximum stage id **/
	host->updateMaximumStageId();

	/** remove routings to missing track **/
	DAW::validateTrackRoutings(host, this->getTracksFlatVec());

	/** inform host about track layout changes so it resets and updates internal structures **/
	host->onTrackLayoutChange();


	// is plugin loading not deferred?
	if ((flags&FLAG_DEFER_LOAD) == 0) {
		/**
		 * plugin loading was not deferred.
		 * handle request to load all plugins.
		 */

		/** loading screen guictr class **/
		class guictr_loading : public guictr_base {
			public:
			String text;
	//		int64_t time;
			guictr_loading() {
				setLabel("LOADING ");
	//			time = getTimeHPint64();
			}
			void render(NVGcontext* vg) override {
				guictr_base::render(vg);
				vec2 cs = getSizeContent();
	//			float fSeconds = (getTimeHPint64()-time) / 1000000.0;
	//			String fsince = StringFormat("Loading %.02fs", fSeconds);
				setFont(vg, 32, G_WHITE, NVG_ALIGN_TOP | NVG_ALIGN_LEFT);
				float w = textWidth(vg, "Loading 1234");
				nvgText(vg, cs.x/2-w/2, cs.y/2, StringAsCStr(text), NULL);
			}
		};
		guictr_loading ctr;
		ctr.size = m_size;
		ctr.setControl(this);
		ctr.layout();


		/** precondition: an existing with opengl+nanoVG context **/
		auto windowMain = dynamic_cast<window_main*>(window);
		dbgassert(windowMain);

		/** get the list of all plugins in deferred loading state **/
		std::vector<effectbase*> pluginsDeferred;
		host->getDeferredEffects(pluginsDeferred);

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
		int len = pluginsDeferred.size();
		for (int i = 0; i < len; i++) {

			dbgassert(pluginsDeferred[i]->getModuleType() == PLUGIN_TYPE_DEFERRED);
			auto plugin = dynamic_cast<effect_deferred*>(pluginsDeferred[i]);
			windowMain->preRender();
	//		render(0, 0, m_size.x, m_size.y, 1.0);
			NVGcolor col = getTheme()->getColor(GuiColor::COL_CLEAR_COLOR);
			glClearColor(col.r, col.g, col.b, col.a);
			glClear(GL_COLOR_BUFFER_BIT);
			float ratio = 1.0;
			nvgBeginFrame(vg, m_size.x, m_size.y, ratio);
			nvgLineJoin(vg, NVGlineCap::NVG_BEVEL);

			nvgSave(vg);
			ctr.text = plugin->getDfrdPluginName();
			ctr.render(vg);
			nvgRestore(vg);
			nvgEndFrame(vg);
			windowMain->postRender();
			/** TODO: vsync **/
			threadSleep(16);
			host->activateDeferred(plugin);
		}
		ctr.setControl(nullptr);
	}

	/** load layouts **/
	trackList.loadSubtrackLayouts(file->project);

//	view->ctr_tracks.layout();
	grid.setLayout(file->layout.layoutGrid);
	view->ctr_tracks.layout();
	view->ctr_plugins.layout();

	updateVisibleTrackContents();
	view->ctr_tracks.layout();
	view->ctr_tracks.setScrollOffset(file->layout.scrollOffsetX);


	/** load cursor state **/
	if (cursor.isSubtrackSelection() && trackList.validTrackIdx(cursor.cursorTrack)) {
		track_t* tr = trackList[cursor.cursorTrack];
		fixCursorSubRange(cursor, tr->subtracks.size());
	} else {
		fixCursorTrackRange(cursor, trackList.size());
	}
	/** set as current project **/
	this->projectPath = file->path;

	setAudioThreadState(playback_state::status_stop);
	return true;
}

void MainCtrl::relayout(int32_t w, int32_t h) {
	closeAllAppMenus();
	closeContextMenu();
	w = math::max(640, w);
	h = math::max(480, h);
	view->layout(w, h);

	view->ctr_plugins.layout();
	view->ctr_clipeditor.layout();
	view->ctr_tracks.layout();
	view->ctr_nodes.layout();
	for (guictr_base *ctr : containers) {
		if(ctr == &view->ctr_clipeditor)
			continue;
		if(ctr == &view->ctr_plugins)
			continue;
		if(ctr == &view->ctr_tracks)
			continue;
		if(ctr == &view->ctr_nodes)
			continue;
		ctr->layout();
	}
}
void MainCtrl::setSelectedTrack(track_t* track) {
	selectedTrack = track;
	view->ctr_plugins.showTrack(track ? track->audio : nullptr);
}
track_t* MainCtrl::getSelectedTrack() {
	return selectedTrack;
}
guictr_plugins* MainCtrl::getPluginCtr() {
	auto ctrlThis = get();
	return ctrlThis ? &ctrlThis->view->ctr_plugins : nullptr;
}
guictr_tracks* MainCtrl::getGuiTrackCtr() {
	auto ctrlThis = get();
	return ctrlThis ? &ctrlThis->view->ctr_tracks : nullptr;
}
void MainCtrl::updateGrid() {
	grid.update(view->ctr_tracks.trackView.getSizeContent());
	view->ctr_tracks.updateVisibleTrackContents();
}
guitrack_editor& MainCtrl::getTrackEditor() {
	return view->ctr_tracks.trackView;
}
void MainCtrl::updateVisibleTrackContents() {
	view->ctr_tracks.updateVisibleTrackContents();
}
bool MainCtrl::isZooming() {
	return guiCaptured == &view->ctr_tracks.trackTimeline;
}
void MainCtrl::uncaptureMouse() {
	this->mainWindow->releaseMouse();
}
void MainCtrl::onUncaptureMouse() {
	guiCaptured = NULL;
}
void MainCtrl::mouseMoved(ivec2 mousePos, ivec2 deltaPos) {
	dragdropTarget.reset();
#if USE_GUI_MENU
	if (ctxtmenu && !ctxtmenu->isTransient()) {
		MouseHitEvt evt = mouseHitEvt(MouseHitType::MOUSE_OVER);
		if (view->ctr_menu.mouseHitTest(mousePos, evt)) {
		}
		return;
	}
#endif
	BaseCtrl::mouseMoved(mousePos, deltaPos);
}

void MainCtrl::objectDragMove(guibase* g, MouseEvent& mevt) {
	MouseHitEvt evt = mouseHitEvt(MouseHitType::MOUSE_DRAGDROP_OBJECT);
	evt.setDraggedThing(g);
	for (guictr_base *ctr : containers) {
		if (ctr->mouseHitTest(mevt.mousepos, evt)) {
			break;
		}
	}
	guibase* gui = evt.getGuiHit();
	if (gui) {
		ivec2 mposObj = toControlsObjectSpace(mevt.mousepos, gui);
		g->dragMoveOn(gui, mposObj);
	} else {
	}
}
void MainCtrl::objectDragRelease(guibase* g, MouseEvent& mevt) {
	MouseHitEvt evt = mouseHitEvt(MouseHitType::MOUSE_DRAGDROP_OBJECT);
	evt.setDraggedThing(g);
	for (guictr_base *ctr : containers) {
		if (ctr->mouseHitTest(mevt.mousepos, evt)) {
			break;
		}
	}
	guibase* gui = evt.getGuiHit();
	if (gui) {
		ivec2 mposObj = toControlsObjectSpace(mevt.mousepos, gui);
		g->dragReleaseOn(gui, mposObj);
	}
}
bool MainCtrl::filesDropBegin(std::vector<String>& files, ivec2 mousepos, int kbmods) {
	my_printf("filesDropBegin %d %d isdragging=%d\n", mousepos.x, mousepos.y, dragdropclip.isLoaded);
	dragdropclip.reset();
	if (guiDragged || guiCaptured) {
		return false;
	}
	if (files.size()) {
		String path = files.front();
		if (StrEndsWith(path, ".wav")) {
			String a,b,c, d;
			SplitPath(path, &a, &b, &c, &d);
			audiofile_t* audio = audiocache::getInstance()->loadFile(path);
			if (audio) {
				auto* sample = audio->sample.get();
				if (sample) {
					clip_t clip;
					clip.clipType = CLIP_AUDIO;
					clip.name = b;
					//clip.notes = move(notes);
					clip.audio.id = audio->id;
					clip.setLenSamples(sample->nSamples);
					clip.setLen(samplesToTicks(sample->nSamples));
					clip.loopEnabled = false;
					std::shared_ptr<track_clipboard_t> trClipboard = std::make_shared<track_clipboard_t>();
					trClipboard->clips.push_back(std::make_shared<clip_t>(std::move(clip)));
					std::shared_ptr<clip_clipboard> fileClipboard = std::make_shared<clip_clipboard>();
					fileClipboard->tracks.push_back(trClipboard);
					dragdropclip.reset();
					dragdropclip.clipboard = fileClipboard;
					dragdropclip.isLoaded = true;
				}
			}
		}
		if (StrEndsWith(path, ".mid")) {
			LoadMidiTask task(files.front());
			if (!MainCtrl::get()->getWorkerThread()->pushTask(&task)) {
				return false;
			}
			if (task.isInQueue()) {
				task.wait();
				if (task.isGood()) {
					std::shared_ptr<clip_clipboard> fileloadedClipboard = task.getClipboard();
					if (fileloadedClipboard) {
						dragdropclip.reset();
						dragdropclip.clipboard = fileloadedClipboard;
						dragdropclip.isLoaded = true;
						my_printf("got clip\n",0);
					} else {
						my_printf("FAIL: no clip\n",0);
					}
				}
			}
		}
		if (dragdropclip.isLoaded) {
			MouseHitEvt evt = mouseHitEvt(MouseHitType::MOUSE_DRAGDROP_CLIP);
			evt.setDraggedThing(&dragdropclip);
			for (guictr_base *ctr : containers) {
				if (ctr->mouseHitTest(mousepos, evt)) {
					break;
				}
			}
			guibase* gui = evt.getGuiHit();
			if (gui) {
				ivec2 mposObj = toControlsObjectSpace(mousepos, gui);
				bool result = gui->clipDropBegin(dragdropclip, mposObj, kbmods);
				if (!result) {

				}
				return result;
			}
//
////					MainCtrl::get()->getTrackId(0)->add(clip);
////					MainCtrl::get()->updateVisibleTrackContents();
////					MainCtrl::get()->requestRedraw();
//				return true;
		}
	}
	return false;
}
bool MainCtrl::filesDropMove(ivec2 mousepos, int kbmods) {
	if (guiDragged || guiCaptured) {
		dragdropclip.reset();
		return false;
	}
	if (dragdropclip.isLoaded) {
		dragdropclip.isValidTarget = false;
//		my_printf("filesDropMove %d %d isdragging=%d\n", pos.x, pos.y, dragdropclip.isDragging);
		MouseHitEvt evt = mouseHitEvt(MouseHitType::MOUSE_DRAGDROP_CLIP);
		evt.setDraggedThing(&dragdropclip);
		for (guictr_base *ctr : containers) {
			if (ctr->mouseHitTest(mousepos, evt)) {
				break;
			}
		}
		guibase* gui = evt.getGuiHit();
		if (gui) {
			ivec2 mposObj = toControlsObjectSpace(mousepos, gui);
			bool result = gui->clipDropMove(dragdropclip, mposObj, kbmods);
			if (!result) {

			}
			return result;
		}
	}
	return false;
}

//little helper to reset clip when scope is left
class clipreset {
	dragdrop_midifile& clip;
public:
	clipreset(dragdrop_midifile& _clip) : clip(_clip) { };
	~clipreset() {
		clip.reset();
	}
};
bool MainCtrl::filesDropFinal(std::vector<String>& files, ivec2 mousepos, int kbmods) {
	clipreset rst(dragdropclip);
	if (guiDragged || guiCaptured) {
		return false;
	}
	if (dragdropclip.isLoaded && dragdropclip.isValidTarget) {
		my_printf("filesDropFinal %d %d isdragging=%d\n", mousepos.x, mousepos.y, dragdropclip.isLoaded);
		MouseHitEvt evt = mouseHitEvt(MouseHitType::MOUSE_DRAGDROP_CLIP);
		evt.setDraggedThing(&dragdropclip);
		for (guictr_base *ctr : containers) {
			if (ctr->mouseHitTest(mousepos, evt)) {
				break;
			}
		}
		guibase* gui = evt.getGuiHit();
		if (gui) {
			ivec2 mposObj = toControlsObjectSpace(mousepos, gui);
			bool result = gui->clipDropFinal(dragdropclip, mposObj, kbmods);
			return result;
		}
	}
	return false;
}
#if defined(__GNUC__) && defined(ENABLE_MICHAELS_GLIBCXX_HACKS)
namespace STLVectorDebugTracking {
	void dbgPrintVectorAllocs();
}
#endif

bool MainCtrl::processGlobalKeyevent(KeyEvent& event) {

	if (event.type == KeyEventType::K_PRESS) {
		lastKey = getKeyName(event.scancode);
		if (!lastKey.length()) {
			const char* ca = glfwGetKeyName(event.keyCode, event.scancode);
			if (ca) {
				lastKey = ca;

			}
		}
	}
	if (event.type != KeyEventType::K_RELEASE) {
		if (event.keyCode == KEY_TAB) {
			switch (this->viewMode) {
			case view_mode_t::TRACK_TIMELINE:
			case view_mode_t::MIXER:
				this->setViewMode(view_mode_t::NODE_EDITOR);
				return true;
			case view_mode_t::NODE_EDITOR:
				this->setViewMode(view_mode_t::TRACK_TIMELINE);
				return true;
			}
			return true;
		}
		if (event.keyCode == KEY_M) {
			ThreadLock lock = playThread.lockThread();
#if defined(__GNUC__) && defined(ENABLE_MICHAELS_GLIBCXX_HACKS)
			STLVectorDebugTracking::dbgPrintVectorAllocs();
			return true;
#endif
		}
		if (!event.mods && event.keyCode == KEY_S) {
			logStackTrace();
			return true;
		}
		if (event.keyCode == KEY_SPACE) {
			if (isPlaying()) {
				stopPlaying();
			} else {
				startPlaying();
			}
			return true;
		}
		if (isKC(KC_UNDO, event)) {
			menuCommand(CMD_UNDO);
			return true;
		}
		if (isKC(KC_REDO, event)) {
			menuCommand(CMD_REDO);
			return true;
		}
		if (isKC(KC_NEW, event)) {
			menuCommand(CMD_FILE_NEW);
			return true;
		}
		if (isKC(KC_OPEN, event)) {
			menuCommand(CMD_FILE_OPEN);
			return true;
		}
		if (isKC(KC_SAVE, event)) {
			menuCommand(CMD_FILE_SAVE);
			return true;
		}
		if (isKC(KC_SAVEAS, event)) {
			menuCommand(CMD_FILE_SAVEAS);
			return true;
		}
		if (isKC(KC_ZOOM_IN, event)) {
			menuCommand(CMD_GUI_GLOBAL_ZOOM_INCREASE);
			return true;
		}
		if (isKC(KC_ZOOM_OUT, event)) {
			menuCommand(CMD_GUI_GLOBAL_ZOOM_DECREASE);
			return true;
		}
		if (isKC({ 0, KEY_TAB, nullptr }, event)) {
			menuCommand(CMD_PREFERENCES);
			return true;
		}
	}
	return false;
}
void MainCtrl::startPlaying() {
	setAudioThreadState(playback_state::status_play);
}
void MainCtrl::stopPlaying() {
	setAudioThreadState(playback_state::status_stop);
}
void MainCtrl::setAudioThreadState(playback_state state) {
	playThread.addRequest(REQ_STATE, (int) state, true);
}
bool MainCtrl::toggleLoop() {
	loopEnabled = !loopEnabled;
	return loopEnabled;
}
bool MainCtrl::isPlaying() {
	return playThread.getState() == playback_state::status_play;
}
bool MainCtrl::mouseDownPre() {
	dragdropclip.reset();
	if (this->ctxtmenu && this->ctxtmenu->isDialog()) {
		return false;
	}
	closeAllContextMenus();
	return true;
}

track_t* MainCtrl::createNewTrack(int trackType) {
	dbgassert(trackType >= 0 && trackType < NUM_TRACK_TYPES);
	int32_t tryTypeOffset = trackTypeCtrs[trackType]->size();

	String name = StringFormat("%s %d", TrackTypeToName(trackType), tryTypeOffset + 1);
	track_t* newTrack = new track_t(trackType, name, true);
	newTrack->rgb = colorPalette[rand.rng_rand(COLOR_PALETTE_LEN)];

	switch (trackType) {
		case TRACK_TYPE_MIDI:
			break;
		case TRACK_TYPE_RETURN:
			break;
		case TRACK_TYPE_MASTER:
			break;
	}
	return newTrack;
}
track_t* MainCtrl::insertNewTrack(int trackInsertPos, int trackType, int flags) {
	track_t* newTrack = createNewTrack(trackType);
	ThreadLock lock = playThread.lockThread();
	addTrackImpl(trackInsertPos, newTrack, flags);

	return newTrack;
}

class action_modify_track_add : public action_base {
public:
	int32_t trackIdx = -1;
	int32_t localIdx = -1;
	track_t* trackPtr;
	action_modify_track_add() = delete;
	action_modify_track_add(String description, track_t* _trackPtr) : action_base() {
		desc = description;
		trackPtr = nullptr;
		trackIdx = _trackPtr->idx;
		localIdx = _trackPtr->localIdxFlat;
		dbgassert(MainCtrl::get()->getTrackId(trackIdx) == _trackPtr);
	}
	void releaseResources(MainCtrl* ctrl) override {
		if (trackPtr) {
			releaseTrackResources(trackPtr, ctrl);
			delete trackPtr;
			trackPtr = nullptr;
		}
	}
	void undo(MainCtrl* ctrl) {
		ctrl->resetMouseContext();
		ctrl->setEditClip(NULL);
		trackPtr = ctrl->getTrackId(trackIdx);
		dbgassert(trackPtr && trackPtr->audio && trackPtr->audio->sampleFormat.blockSize%8==0); // see if pointer is valid
		dbgassert(localIdx == trackPtr->localIdxFlat);
		//SERIALIZE TRACK VSTs
		localIdx = trackPtr->localIdxFlat;
		ctrl->removeTrackImpl(trackPtr, FLG_TRK_CHANGE_HISTORY_UNDO);
	}
	void redo(MainCtrl* ctrl) {
		dbgassert(trackPtr);
		ctrl->resetMouseContext();
		ctrl->setEditClip(NULL);
		ctrl->addTrackImpl(localIdx, trackPtr, FLG_TRK_CHANGE_HISTORY_UNDO);
		dbgassert(localIdx == trackPtr->localIdxFlat);
		localIdx = trackPtr->localIdxFlat;
		trackPtr = nullptr;
		//UNSERIALIZE TRACK VSTs
	}
};
class action_modify_track_remove : public action_base {
public:
	int32_t trackIdx = -1;
	int32_t localIdx = -1;
	track_t* trackPtr;
	action_modify_track_remove() = delete;
	action_modify_track_remove(String description, track_t* _trackPtr) : action_base() {
		desc = description;
		trackPtr = _trackPtr;
		trackIdx = _trackPtr->idx;
		localIdx = _trackPtr->localIdxFlat;
		dbgassert(MainCtrl::get()->getTrackId(trackIdx) != trackPtr);
	}
	~action_modify_track_remove() {
	}
	void releaseResources(MainCtrl* ctrl) override {
		if (trackPtr) {
			releaseTrackResources(trackPtr, ctrl);
			delete trackPtr;
			trackPtr = nullptr;
		}
	}

	void undo(MainCtrl* ctrl) {
		ctrl->resetMouseContext();
		ctrl->setEditClip(NULL);
		ctrl->addTrackImpl(localIdx, trackPtr, FLG_TRK_CHANGE_HISTORY_UNDO);
		dbgassert(localIdx == trackPtr->localIdxFlat);
		localIdx = trackPtr->localIdxFlat;
		trackPtr = nullptr;
		//UNSERIALIZE TRACK VSTs
	}
	void redo(MainCtrl* ctrl) {
		ctrl->resetMouseContext();
		ctrl->setEditClip(NULL);
		trackPtr = ctrl->getTrackId(trackIdx);
		dbgassert(trackPtr);
		//SERIALIZE TRACK VSTs
		ctrl->removeTrackImpl(trackPtr, FLG_TRK_CHANGE_HISTORY_UNDO);
		dbgassert(trackPtr && trackPtr->audio && trackPtr->audio->sampleFormat.blockSize%8==0); // see if pointer is valid
		dbgassert(localIdx == trackPtr->localIdxFlat);
	}
};
void MainCtrl::addTrackImpl(int32_t trackInsertPos, track_t* newTrack, int flags) {
	trackList.addTrack(trackInsertPos, newTrack);
	if ((flags&FLG_TRK_CHANGE_HISTORY_UNDO) != 0) {
		dbgassert(newTrack->audio);
	} else {
		dbgassert(!newTrack->audio);
		vsthost* host = vsthost::getInstance();
		host->createAudio(newTrack);
	}
	view->ctr_tracks.addTrack(newTrack, flags);
	if (flags&FLG_TRK_CHANGE_USER) {
		pushHist(new action_modify_track_add(StringFormat("Add %s Track", TrackTypeToName(newTrack->type)), newTrack));
	}

	vsthost::getInstance()->onTrackLayoutChange();
}
void MainCtrl::removeTrackId(uint32_t trackId) {
	if (trackList.validTrackIdx(trackId)) {
		removeTrackImpl(trackList[trackId], FLG_TRK_CHANGE_USER);
	}
}
void MainCtrl::removeTrackImpl(track_t* track, int flags) {
	guictr_plugins* plugins = MainCtrl::getPluginCtr();
	plugins->hideTrack(track->audio);
	if (clipView.gui && clipView.gui->m_track == track){
		clipView.set(NULL);
	}
	trackList.removeTrack(track);
	view->ctr_tracks.removeTrack(track, flags);
	DAW::removeTrackRoutings(this->getTracksFlatVec(), track->audio->stageId);
	if (flags&FLG_TRK_CHANGE_USER) {
		pushHist(new action_modify_track_remove(StringFormat("Remove %s Track", TrackTypeToName(track->type)), track));
	}
	vsthost::getInstance()->onTrackLayoutChange();
}
track_t* MainCtrl::getTrackId(uint32_t trackId) {
	return trackList[trackId]; // operator[] returns NULL on oob
}

void MainCtrl::preClipDelete(clip_t* clip) {
	if (clipView.clip() == clip) {
		clipView.set(NULL);
	}
	onGuiRemoved(clip);
//	resetMouseContext();
}
void MainCtrl::preTrackDelete(track_t* track) {
	if(clipView.gui && clipView.gui->m_track == track) {
		clipView.set(NULL);
	}
	resetMouseContext();
}
void MainCtrl::showAutomation(track_t* tr, automatable_t* at, int32_t paramIdx) {
	view->ctr_tracks.showAutomationLane(tr, at, paramIdx);
}
void MainCtrl::setTempo(int32_t _tempo100) {
	std::function<void()> fn2 = [this, _tempo100]() {
		this->tempo100 = CLAMP_I(_tempo100, 100, 99900);
	};
	playThread.call(fn2, true);

	// Commented the code below because of eclipse cpp indexer error
//	playThread.call([this, _tempo100]() {
//		this->tempo100 = CLAMP_I(_tempo100, 100, 99900);
//	}, true);
}
void MainCtrl::setStatusText(String s) {
	view->statusbar.setTitle(s);
}
void MainCtrl::setEditClip(gui_clip* gclip) {
	view->ctr_clipeditor.storeLayout();
	clipView.set(gclip);
	view->ctr_clipeditor.showEditClip();
}

void MainCtrl::prerender(int32_t x, int32_t y, int32_t w, int32_t h, float pixelRatio) {
//	my_printf("prerender %d\n", std::this_thread::get_id());
	for (guictr_base *ctr : containers) {
		ctr->prerender(vg);
	}
	waveformrender::getInstance()->renderUpdates(vg, 0);
}
track_t* clip_view::track() const {
	if (!this->gui)
		return NULL;
	return this->gui->m_track;
}
clip_t* clip_view::clip() const {
	if (!this->gui)
		return NULL;
	return this->gui->m_clip;
}



GLFWwindow* getGlfwFromWindowBase(window_base* w);
GLFWwindow* getTopLevelGlfwWindow() {
	auto main = MainCtrl::get();
	if (main) {
		return getGlfwFromWindowBase(main->window);
	}
	dbgassert(0);
	return nullptr;
}

int handleFatalError(int type, int implSpecType) {
	seqthreads::thread_base* thread = MainCtrl::getPlayThread();
	if (thread && seqthreads::currentThreadsId() == thread->getThreadId()) {
		host_processing_stats_t processing;
		auto host = vsthost::getInstance();
		host->getProcessingStats(processing);
		if (processing.pluginId) {
			effectbase* eff = host->getPluginById(processing.pluginId);
			if (eff) {
				my_printf("Crash was most likely caused by %s\n", StringAsCStr(eff->getName()));
			}
		}
	}
	return 0;
}

int32_t project_controller_t::tickToSamples(tick_t ticks)
{
	vsthost* host = vsthost::getInstance();
	dbgassert(host);
	return std::round(tickToSamplePrecise(ticks, tempo100, vsthost::getInstance()->sampleFormat.sampleRate));
}
tick_t project_controller_t::samplesToTicks(int32_t sample)
{
	vsthost* host = vsthost::getInstance();
	dbgassert(host);
	return std::round(sampleToTickPrecise(sample, tempo100, vsthost::getInstance()->sampleFormat.sampleRate));
}

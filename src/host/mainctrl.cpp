#include <nanovg.h>
#include <time.h>
#include <algorithm>
#include <functional>
#include <vector>
#include <memory>
#include <GLFW/glfw3.h>
#include "../platform/mingw/mingw.thread.h"

#include "window.h"
#include "platform.h"

#include "keyboard.h"
#include "commands.h"

#include "project.h"

#include "basectrl.h"
#include "mainctrl.h"
#include "grid.h"
#include "note.h"
#include "cursor.h"
#include "exceptions.h"
#include "color_util.h"
#include "str_util.h"
#include "seq_util.h"
#include "settings.h"
#include "track.h"
#include "clip.h"
#include "fileloader.h"
#include "edithistory.h"
#include "logging.h"
#include "menu.h"
#include "msgbox.h"

#include "../gui/gui.h"
#include "../gui/guicontainer.h"
#include "../gui/button.h"
#include "../gui/guicontextmenu.h"
#include "../gui/tempocontrols.h"
#include "../gui/scrollbar.h"
#include "../gui/statusbar.h"
#include "../gui/plugin.h"
#include "../gui/pluginctr.h"
#include "../gui/clipeditor.h"
#include "../gui/trackctr.h"
#include "../gui/trackcontent.h"
#include "../gui/trackctr.h"
#include "../gui/pluginlist.h"
#include "../gui/guimenu.h"
#include "../gui/debugctr.h"
#include "../gui/drawwaveform.h"

#include "vst_host.h"
#include "plugin/base_plugin.h"
#include "plugin/vst_plugin.h"
#include "track_impl.h"
#include "leak_detect.h"
#include "audiocache.h"
#include "seq_time.h"
#include "../threads/workerthread.h"

using glm::vec2;
using glm::ivec2;
using glm::vec4;
using glm::ivec4;
using std::min;
using std::max;
using namespace std;


std::shared_ptr<MainCtrl> mainctrl;
MainCtrl* MainCtrl::get() {
	return mainctrl.get();
}
std::shared_ptr<AppCtrl> makeApp() {
	mainctrl = std::make_shared<MainCtrl>();
	return mainctrl;
}
AppCtrl* AppCtrl::get() {
	return mainctrl.get();
}
void deleteApp() {
	mainctrl = nullptr;
}




//MainCtrl* mainctrl = nullptr;
//AppCtrl* makeApp() {
//	mainctrl = new MainCtrl();
//	return mainctrl;
//}
//MainCtrl* MainCtrl::get() {
//	return mainctrl;
//}
//AppCtrl* AppCtrl::get() {
//	return mainctrl;
//}
//void deleteApp() {
//	if (mainctrl)
//		delete mainctrl;
//	mainctrl = nullptr;
//}

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
		exception_ptr eptr = task.getException();
		if (eptr != nullptr) {
			printf("task[%d] had exception.. rethrowing\n",task.id);
	        try{
	            rethrow_exception(eptr);
	        }
	        catch(const exception &ex)
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
class ViewContainers {
	guictr_noteeditor noteeditor;
public:
	guictr_menubar ctr_menu;
	guictr_tempocontrols ctr_tempo;
	guictr_plugins ctr_plugins;
	guictr_test ctr_test;
	gui_statusbar statusbar;
	guictr_pluginview ctr_pluginview;
	guictr_clipeditorview ctr_clipeditorview;
	guictr_clipeditor ctr_clipeditor;
	guictr_tracks ctr_tracks;
	gui_ctr_debug ctr_dbg;
	guictr_pluginlibrary ctr_pluginlist;
	guictr_modulelibrary ctr_effectlist;
	Splitter splitterList;
	Splitter splitterCenter;
	Splitter splitterRight;
	ViewContainers(ngui::MenuBar& menubar, Cursor& _cursor, project_t& project, scaled_grid& grid, clip_view& clipView, dragdrop_midifile& dragdropclip)
	  : noteeditor(clipView),
	  ctr_menu(menubar),
	  ctr_tempo(project),
	  ctr_pluginview(&ctr_plugins),
	  ctr_clipeditorview(noteeditor),
	  ctr_clipeditor(noteeditor, clipView),
	  ctr_tracks(_cursor, project, grid, dragdropclip),
	  splitterList(0, 0.5f),
	  splitterCenter(0, 0.7f),
	  splitterRight(1, 0.8f)
	{
		splitterCenter.setMinMax(0.25f, 0.9f);
		splitterList.setMinMax(0.1f, 0.9f);
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
		int heightList = splitterList.leftOrTop(hRight);
		int heightDebug = splitterList.rightOrBottom(hRight);
		int width = splitterRight.leftOrTop(winW);
		int wRight = splitterRight.rightOrBottom(winW);
		ctr_tempo.size = { winW, hTopControls };
		ctr_tracks.size = { width, hTrackCtr };
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
		ctr_pluginlist.setSnapSides(ivec4(1, 0, 0, 1));
		ctr_effectlist.setSnapSides(ivec4(1, 0, 0, 0));
		ctr_dbg.setSnapSides(ivec4(1, 0, 0, 0));


		ctr_dbg.pos = {width, winY+hTopControls+heightList};
		ctr_dbg.size = {wRight, heightDebug};
		ctr_pluginlist.pos = {width, winY+hTopControls};
		ctr_pluginlist.size = {wRight, heightList/2};
		ctr_effectlist.pos = {width, ctr_pluginlist.bottom()};
		ctr_effectlist.size = {wRight, heightList/2};
		splitterRight.pos = ivec2(ctr_dbg.pos.x - 5, hTopControls);
		splitterRight.size = ivec2(10, hRight);
		splitterList.pos = ivec2(ctr_dbg.pos.x, ctr_effectlist.bottom()-5);
		splitterList.size = ivec2(wRight, 10);
	}
	void addTo(vector<guictr_base*>& v) {
		 v.push_back(&ctr_tracks);
		 v.push_back(&ctr_clipeditor);
		 v.push_back(&ctr_tempo);
		 v.push_back(&ctr_pluginview);
		 v.push_back(&ctr_clipeditorview);
		 v.push_back(&ctr_pluginlist);
		 v.push_back(&ctr_effectlist);
		 v.push_back(&statusbar);
		 v.push_back(&ctr_dbg);
#if USE_GUI_MENU
		 v.push_back(&ctr_menu);
#endif
		 v.push_back(&splitterCenter);
		 v.push_back(&splitterList);
		 v.push_back(&splitterRight);
	}
};
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

	view->ctr_dbg.addStr(s);
}

void MainCtrl::unloadProject() {
	closeContextMenu();
	resetMouseContext();
	projectPath = "";
	setSelectedTrack(NULL);
	clipView.set(NULL);
//	std::shared_ptr<clip_clipboard>& clipboard = view->ctr_tracks.trackView.clipboard;
//	clipboard.reset();
	vector<track_t*> _tracks = trackList.vec();  // iterate a copy
	my_printf("DELETE _tracks %d\n", _tracks.size());
	for (track_t* tr : _tracks) {
		my_printf("DELETE TRACK %s\n", StringAsCStr(tr->name));
		vsthost::getInstance()->unloadTrack(tr);
		removeTrackImpl(tr);
	}
	trackList.clear();
	for (track_t* tr : _tracks) {
		deleteTrack(tr, this);
	}
	hist.clear();

}
void MainCtrl::destroy()
{
	if (!isOK) {
		return;
	}
	setAudioThreadState(playback_state::status_no_process);
	assert(playThread.getState() == playback_state::status_no_process); // should have been set by window close request
	vsthost::getInstance()->stopAudio();
	unloadProject();
	vsthost::getInstance()->unload();
	vsthost::getInstance()->destroy();
	audiocache::destroy();
	waveformrender::destroy();


	settings.dens = grid.grid_dens;
	isOK = false;
	delete view;
	plugindb.close();
	this->workerThread.stopThread();
	this->workerThread.joinThread();
	this->playThread.stopThread();
	this->playThread.joinThread();
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
void MainCtrl::onWindowCloseRequest() {
}
void MainCtrl::onMenuOpen(ngui::Menu* menu) {
	updateMenubar();
#if !USE_GUI_MENU
	this->mainWindow->updateMenu();
#endif
}
static SupportedFileType FILE_TYPE_PROJECT {"Project File", PROJECT_FILE_EXT};
vector<SupportedFileType> vFILE_TYPE_PROJECT = { FILE_TYPE_PROJECT };
void MainCtrl::loadFile(String path) {
	shared_ptr<project_file> f = loadProjectFile(this, path);
	if (!f) {
		setStatusText(StringFormat("Failed loading %s", StringAsCStr(FileNameFromPath(path))));
	} else {
		setLoadedProject(f);
	}
}
void MainCtrl::setEmptyProject() {
	ThreadLock lock = playThread.lockThread();
	unloadProject();
	insertNewTrack(-1, TRACK_TYPE_MIDI, false);
	insertNewTrack(-1, TRACK_TYPE_MASTER, false);
}
void MainCtrl::menuCommand(int cmd) {
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
		setEmptyProject();
		MainCtrl::getGuiTrackCtr()->layout();
		MainCtrl::get()->updateVisibleTrackContents();
	}
		break;
	case CMD_FILE_OPEN:
		{
			String path;
			if (promptUserFilePath(window, 0, vFILE_TYPE_PROJECT, path)) {
				loadFile(path);
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
			if (!path.empty())
			{
				shared_ptr<project_file> f = createProjectFile();
				saveProject(f, path);
				projectPath = path;
			}
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
	case CMD_EXIT:
		mainWindow->requestClose();
		break;

	}
}
void MainCtrl::postInit() {
	vsthost::getInstance()->postInit();
	loadFile("singlewave.project");
//	for (int i = 0; i < 32; i++) {
//		loadFile("muuure.project");
//	}
	for (int i = 0; i < 2; i++) {
		track_t* track = getTrackId(i);
		if (track) {
//			track->audio = vsthost::getInstance()->createAudio(track);
//			vstpluginloadres res = vsthost::getInstance()->loadPlugin("C:/PluginManager/configs/default/hosts/Ableton/categories/DUNE 2.dll");
//			if (res.result == 0) {
//				track->audio->setInstrument(res.plugin);
//				track->audio->instrument->show();
//
//				void* ptr;
//				int32_t size = res.plugin->dispatch(effGetChunk, 0, 0, &ptr);
//				if (ptr) {
//					my_printf("effGetChunk: %llu, %d\n", ptr, size);
//				}
//			}
//			res = vsthost::getInstance()->loadPlugin("C:/VstPlugins/fabfilter/FabFilter Pro-Q 2.dll");
//			if (res.result == 0) {
//				track->audio->insertEffect(-2, res.plugin);
//	//			track->audio->instrument->show();
//			}
//			res = vsthost::getInstance()->loadPlugin("C:/VstPlugins/fabfilter/FabFilter Pro-L.dll");
//			if (res.result == 0) {
//				track->audio->insertEffect(-2, res.plugin);
//	//			track->audio->instrument->show();
//			}
		}
	}
	view->ctr_pluginlist.update();
	view->ctr_effectlist.update();
}
void MainCtrl::initApp() {

	vsthost::setInstance(std::make_unique<vsthost>(44100, 256));
	audiocache::setInstance(std::make_unique<audiocache>(vsthost::getInstance()->lSampleRate));
	audiocache::getInstance()->loadFile("PHFT_Drum Loop_130_099.wav");
	audiocache::getInstance()->loadFile("C:\\Users\\Michael\\Desktop\\left right.wav");
	waveformrender::setInstance(std::make_unique<waveformrender>());
}
bool MainCtrl::init(window_main* window, NVGcontext* nanovg)
{
	this->mainWindow = window;
	this->window = window;
	this->vg = nanovg;
	plugindb.open();
	this->playThread.startThread();
	this->workerThread.startThread();
	initColor();

	view = new ViewContainers(menubar, cursor, *this, grid, clipView, dragdropclip);
	view->addTo(this->containers);

	menus.recent.type = ngui::menu_type::submenu;
	menus.recent.title = "Open recent";
	menus.recent.addCommand(CMD_FILE_OPEN, "File 1");
	menus.recent.addCommand(CMD_FILE_OPEN, "File 2");
	menus.recent.addCommand(CMD_FILE_OPEN, "File 4");
	menus.recent.addCommand(CMD_FILE_OPEN, "File 5");
	menus.file.type = ngui::menu_type::submenu;
	menus.file.title = "File";
	menus.file.addCommand(CMD_FILE_NEW, menuName("New", KC_NEW));
	menus.file.addCommand(CMD_FILE_OPEN, menuName("Open", KC_OPEN));
	menus.file.add(&menus.recent);
	menus.file.addCommand(CMD_FILE_SAVE, menuName("Save", KC_SAVE));
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

	menubar.add(&menus.file);
	menubar.add(&menus.edit);
	this->updateMenubar();
#if !USE_GUI_MENU
	this->mainWindow->updateMenu();
#endif

	waveformrender::getInstance()->init();
	setEmptyProject();
//	int w = 120;
//	int x = 0;
//	for (int i = 0; i < 10; i++) {
//		guiplugin *gui = new guiplugin({ x, 0 }, { w, w });
//		if (i % 2 == 0) {
//			gui->setTitleCstr("���!�$&/\\");
//		}
//		else {
//			gui->setTitle(StringFormat("module %d", i));
//		}
//		view->ctr_test.add(gui);
//		x += (int)(w*1.2);
//	}
//	for (int i = 0; i < 19; i++) {
//		guiplugin *gui = new guiplugin({ x, 0 }, { w, w });
//		gui->setTitle(StringFormat("Plugin %d", i));
//		view->ctr_plugins.add(gui);
//		x += (int)(w*1.2);
//	}
	grid.grid_dens = settings.dens;
	updateGrid();
	isOK = true;
	return isOK;
}
String MainCtrl::getClipboardText()
{
	return this->mainWindow->getClipboardText();
}
void MainCtrl::setClipboardText(String s)
{
	this->mainWindow->setClipboardText(s);
}
void MainCtrl::requestRedraw()
{
	this->mainWindow->requestRedraw();
}
int32_t MainCtrl::tickToSamples(tick_t ticks)
{
	vsthost* host = vsthost::getInstance();
	return std::round(tickToSamplePrecise(ticks, tempo100, host->lSampleRate));
}
tick_t MainCtrl::samplesToTicks(int32_t sample)
{
	vsthost* host = vsthost::getInstance();
	return std::round(sampleToTickPrecise(sample, tempo100, host->lSampleRate));
}
void MainCtrl::onTick()
{
//	double since = timer.getTimeDoubleReset();
	vsthost::getInstance()->onTick();
	for (guictr_base *ctr : containers) {
		ctr->onTick(this);
	}
	for (guictr_base *ctr : containers) {
		ctr->onIdle();
	}
//	my_printf("onTick %d\n", std::this_thread::get_id());
//	waveformrender::getInstance()->renderUpdates(vg, 0);
//	if (isPlaying()) {
		mainWindow->requestRedraw();
//	}
	if (!guiDragged && !guiCaptured && guiOver && (!this->ctxtmenu || ctxtmenu->isTransient())) {
		int32_t hoverTicks = 0;
		if (ctxtmenu && (ctxtmenu->canClose() || guiOver->curTooltip != nextTooltipId-1)) {
			closeContextMenu();
		}
		if (!ctxtmenu && guiOver == lastHoveredTooltip) {
			hoverTicks = lastHoveredTooltipTicks + 1;
			if (lastHoveredTooltipTicks >= 12) {
				auto tooltip = guiOver->getTooltip(this);
				guiOver->curTooltip = nextTooltipId++;
				if (tooltip) {
					openContextMenu(tooltip, m_mousePos+ivec2(0,6));
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
			tr = getTrackFromMouse(*this, posRelative, false);
			if (tr)
			my_printf("hovered %s, ticks %d\n", StringAsCStr(tr->name), lastHoveredTrackTicks);
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
	hist.push(action);
}
shared_ptr<project_file> MainCtrl::createProjectFile() {
	shared_ptr<project_file> file = make_shared<project_file>();
	file->path = projectPath;
	copyTo(file->project);
	audiocache::getInstance()->store(file->sampleFileIndex);
	file->layout.layoutGrid = grid;
	file->layout.scrollOffsetX = view->ctr_tracks.getScrollOffset();
	return file;
}
void MainCtrl::resetMouseContext() {
	if (guiCtrFocused) {
		if (!guiCtrFocused->isStaticContainer()) {
			guiCtrFocused = NULL;
		}
	}
	guiCaptured = guiFocused = guiOver = guiDragged = NULL;
}
void MainCtrl::setDragged(guibase* g) {
	guiDragged = g;
}
bool MainCtrl::setLoadedProject(shared_ptr<project_file> file) {
	setAudioThreadState(playback_state::status_no_process);
	ThreadLock lock = playThread.lockThread();
	unloadProject();
	audiocache::getInstance()->load(file->sampleFileIndex);
	copyFrom(file->project);
	my_printf("NUM TRACKS: %d\n", trackList.size());
	for (track_t* tr : trackList) {
		view->ctr_tracks.addTrack(tr);
	}
	trackList.loadPlugins(file->project);
	//	for (track_t* tr : trackList) {
	//		if (tr->audio) {
	//			std::vector<automatable_t*> targets;
	//			tr->audio->getAutomatableTargets(targets);
	//			for (automatable_t* at : targets) {
	//				std::vector<int32_t> targetsIdx;
	//				at->getAutomated(targetsIdx);
	//				for (int32_t idx : targetsIdx) {
	//					view->ctr_tracks.addAutomationLane(tr, at, idx, false);
	//				}
	//			}
	//		}
	//	}
	view->ctr_tracks.layout();
	grid.setLayout(file->layout.layoutGrid);
	view->ctr_tracks.setScrollOffset(file->layout.scrollOffsetX);
	view->ctr_plugins.layout();
	updateVisibleTrackContents();
	if (cursor.isSubtrackSelection() && trackList.validTrackIdx(cursor.cursorTrack)) {
		track_t* tr = trackList[cursor.cursorTrack];
		fixCursorSubRange(cursor, tr->subtracks.size());
	} else {
		fixCursorTrackRange(cursor, trackList.size());
	}
	this->projectPath = file->path;
	setAudioThreadState(playback_state::status_stop);

	return true;
}

void MainCtrl::relayout(int32_t w, int32_t h) {
	closeContextMenu();
	m_size = ivec2(w, h);
	view->layout(w, h);

	view->ctr_plugins.layout();
	view->ctr_clipeditor.layout();
	for (guictr_base *ctr : containers) {
		if(ctr == &view->ctr_plugins)
			continue;
		if(ctr == &view->ctr_clipeditor)
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
	return &get()->view->ctr_plugins;
}
guictr_tracks* MainCtrl::getGuiTrackCtr() {
	return &get()->view->ctr_tracks;
}
void MainCtrl::updateGrid() {
	grid.update(view->ctr_tracks.trackView.getSizeContent());
	view->ctr_tracks.updateVisibleTrackContents();
}
void MainCtrl::updateVisibleTrackContents() {
	view->ctr_tracks.updateVisibleTrackContents();
}
bool MainCtrl::captureMouse(guibase* gui) {
	if (guiCaptured == NULL) {
		guiCaptured = gui;
		this->mainWindow->captureMouse();
		return true;
	}
	return false;
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
void MainCtrl::closeAppMenus() {
	for (auto w : menuWindows) {
		w->getCtrl()->close();
	}
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
void MainCtrl::closeAppMenus(int startlvl) {
	for (int i = startlvl; i < (int)menuWindows.size(); i++) {
		auto w = menuWindows[i];
		w->getCtrl()->close();
	}
}
void MainCtrl::openAppMenu(int lvl, guictxtmenu_base *b, ivec2 pos) {
	if ((int)menuWindows.size() <= lvl) {
		menuWindows.push_back(this->mainWindow->createOverlay());
	}
//	ivec2 windowPos;
//	this->mainWindow->getPos(&windowPos);
	menuWindows[lvl]->getCtrl()->open(b, pos);
}
void MainCtrl::openContextMenu(guictxtmenu_base *b, ivec2 pos) {
	if (this->ctxtmenuOld)
		DELETE_PTR(this->ctxtmenuOld) //horrible lifetime management
	this->ctxtmenu = b;
	ivec2 windowPos;
	this->mainWindow->getPos(&windowPos);
	if (!contextWindow) {
		contextWindow = this->mainWindow->createOverlay();
	}
	contextWindow->getCtrl()->open(b, windowPos+pos);
}
void MainCtrl::closeContextMenu() {
	if (contextWindow) {
		contextWindow->getCtrl()->close();
	}
	this->ctxtmenuOld = this->ctxtmenu;
	this->ctxtmenu = nullptr;
}
bool MainCtrl::hasContextMenu() {
	return this->contextWindow && this->contextWindow->isShown();
//	return PopupCtrl::get()->isShown();
}
guictxtmenu_base* MainCtrl::getContextMenu() {
	return this->ctxtmenu;
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
//		bool result = guiOver->pluginDragMove(g, mposObj);
//		if (!result) {
//
//		}
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
//		bool result = guiOver->pluginDragRelease(g, mposObj);
//		if (!result) {
//
//		}
	}
}
bool MainCtrl::filesDropBegin(vector<string>& files, ivec2 mousepos, int kbmods) {
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
			cachedaudio_t* audio = audiocache::getInstance()->loadFile(path);
			if (audio) {
				auto* sample = audio->sample.get();
				if (sample) {
					clip_t clip(CLIP_AUDIO, b);
					//clip.notes = move(notes);
					clip.audio.id = audio->id;
					clip.setLenSamples(sample->nSamples);
					clip.setLen(samplesToTicks(sample->nSamples));
					clip.loopEnabled = false;
					shared_ptr<track_clipboard_t> trClipboard = make_shared<track_clipboard_t>();
					trClipboard->clips.push_back(make_shared<clip_t>(move(clip)));
					shared_ptr<clip_clipboard> fileClipboard = make_shared<clip_clipboard>();
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
					shared_ptr<clip_clipboard> fileloadedClipboard = task.getClipboard();
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
bool MainCtrl::filesDropFinal(vector<string>& files, ivec2 mousepos, int kbmods) {
	clipreset rst(dragdropclip);
	if (guiDragged || guiCaptured) {
		my_printf("filesDropFinal guiDragged || guiCaptured\n",0);
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
		} else {
			my_printf("!guiOver\n",0);
		}
	} else {

		my_printf("!dragdropclip.isLoaded || !dragdropclip.isValidTarget: %d && %d\n",dragdropclip.isLoaded, dragdropclip.isValidTarget);
	}
	return false;
}

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
		if (event.keyCode == KEY_M) {
//			ngui::show("Hello, you pressed the M key. Thanks you for your efforts", "Well done!", ngui::Style::Info, ngui::Buttons::OK);
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
	if (ctxtmenu != NULL) {
		closeContextMenu();
	}
	return true;
}

track_t* MainCtrl::insertNewTrack(int trackInsertPos, int trackType, int wasUserAction) {
	assert(trackType >= 0 && trackType < NUM_TRACK_TYPES);
	int32_t tryTypeOffset = trackTypeCtrs[trackType]->size();

	String name = StringFormat("%s %d", TrackTypeToName(trackType), tryTypeOffset + 1);
	track_t* newTrack = new track_t(trackType, name, true);
	newTrack->rgb = colorPalette[rand.rng_rand(COLOR_PALETTE_LEN)];

	switch (trackType) {
	case TRACK_TYPE_MIDI:
	{
		clip_t* c = new clip_t(CLIP_MIDI, StringFormat("%s-clip", StringAsCStr(name)));
		c->time = TICKS_BAR * 4;
		c->setLen(TICKS_BAR * 10);
		c->loopStart = 0;
		c->loopLen = c->getLen();
		for (int i = 0; i < 6 ; i++) {
			note_t note;
			note.pitch = 40+(((i%3)%2))*4+(i%3) + (i/3)*12*2;
			note.time = (i+1+(i/3)*3)*TICKS_BAR;
			note.len = TICKS_BAR;
			c->notes.addSingle(note);
		}
		newTrack->getMidi().clips.push_back(c);
	}
		break;
	case TRACK_TYPE_RETURN:
		break;
	case TRACK_TYPE_MASTER:
		break;
	}


//	if (trackin)
	if (wasUserAction) {
		addTrack(trackInsertPos, newTrack);
	} else {
		addTrackImpl(trackInsertPos, newTrack, 0);
	}

	return newTrack;
}

class action_modify_addtrack : public action_base {
public:
	int32_t trackIdx;
	track_t* trackPtr = NULL;
	action_modify_addtrack(String description, track_t* _trackPtr) : action_base() {
		desc = description;
		trackPtr = _trackPtr;
		trackIdx = _trackPtr->idx;
	}
	void undo(MainCtrl* ctrl) {
		ctrl->resetMouseContext();
		ctrl->setEditClip(NULL);
		trackallcontainer_t& trackListAll = ctrl->getTracks();
		track_t* t = trackListAll[trackIdx];
		if (t) {
			ctrl->removeTrackImpl(t);
		}
		trackPtr = t;
	}
	void redo(MainCtrl* ctrl) {
		ctrl->resetMouseContext();
		ctrl->setEditClip(NULL);
		ctrl->addTrackImpl(trackIdx, trackPtr);
		trackPtr = NULL;
	}
};
class action_modify_removetrack : public action_base {
public:
	int32_t trackIdx = -1;
	track_t* trackPtr;
	action_modify_removetrack(String description, track_t* _trackPtr) : action_base() {
		desc = description;
		trackPtr = _trackPtr;
		trackIdx = _trackPtr->idx;
	}
	void undo(MainCtrl* ctrl) {
		ctrl->resetMouseContext();
		ctrl->setEditClip(NULL);
		ctrl->addTrackImpl(trackIdx, trackPtr);
		//UNSERIALIZE TRACK VSTs
	}
	void redo(MainCtrl* ctrl) {
		ctrl->resetMouseContext();
		ctrl->setEditClip(NULL);
		//SERIALIZE TRACK VSTs
		vsthost::getInstance()->unloadTrack(trackPtr);
		ctrl->removeTrackImpl(trackPtr);
	}
};
void MainCtrl::addTrackImpl(int32_t trackInsertPos, track_t* newTrack, int triggerupdate) {
	trackList.addTrack(trackInsertPos, newTrack);
	if (triggerupdate&1) {
		view->ctr_tracks.addSingleTrack(newTrack);
	} else {
		view->ctr_tracks.addTrack(newTrack);
	}

}
void MainCtrl::addTrack(int32_t trackInsertPos, track_t* newTrack) {
	addTrackImpl(trackInsertPos, newTrack, 1);
	pushHist(new action_modify_addtrack(StringFormat("Add %s Track", TrackTypeToName(newTrack->type)), newTrack));
}
void MainCtrl::removeTrackImpl(track_t* track) {
	guictr_plugins* plugins = MainCtrl::getPluginCtr();
	plugins->hideTrack(track->audio);
	if (clipView.gui && clipView.gui->m_track == track){
		clipView.set(NULL);
	}
//	trackList.moveTrack(track);
	trackList.removeTrack(track);
	view->ctr_tracks.removeSingleTrack(track);
}
void MainCtrl::removeTrack(track_t* track) {
	//SERIALIZE TRACK
	vsthost::getInstance()->unloadTrack(track);
	removeTrackImpl(track);
	pushHist(new action_modify_removetrack(StringFormat("Remove %s Track", TrackTypeToName(track->type)), track));
}
void MainCtrl::removeTrackId(uint32_t trackId) {
	if (trackList.validTrackIdx(trackId)) {
		track_t* t = trackList[trackId]; // operator[] returns NULL on oob
		removeTrack(t);
	}
}
track_t* MainCtrl::getTrackId(uint32_t trackId) {
	return trackList[trackId]; // operator[] returns NULL on oob
}


void cutIntersectingClips(trackdata_midi_t& midi, tick_t tickBegin, tick_t tickEnd, delete_cb *cb) {
	vector<clip_t*>::iterator it = midi.clips.begin();
	
	while (it != midi.clips.end()) {
		clip_t* c = *it;
		if (c->start() >= tickEnd || c->end() <= tickBegin) {
			it++;
			continue;
		}
		if (c->start() >= tickBegin && c->end() <= tickEnd) {
			it = midi.removeClip(c);
			deleteClip(c, cb);
			continue;
		} else if (c->time >= tickBegin) {
			//cut left
			cutClipLeft(c, tickEnd-c->time);
			c->setDirty();
		} else if (c->end() <= tickEnd) {
			//cut right
			cutClipRight(c, c->end() - tickBegin);
			c->setDirty();
		} else {
			clip_t* c2 = c->clone();
			cutClipRight(c, c->end() - tickBegin);
			cutClipLeft(c2, tickEnd-c->time);
			it = midi.clips.insert(it, c2);
			c->setDirty();
		}
		it++;
	}
	midi.sortClips();
}
void MainCtrl::cutIntersecting(track_t* tr, tick_t tickBegin, tick_t tickEnd) {
//	if (tr->type == TRACK_TYPE_MIDI) {
		cutIntersectingClips(tr->getMidi(), tickBegin, tickEnd, this);
//	}
}
void MainCtrl::preClipDelete(clip_t* clip) {
	if (clipView.clip() == clip) {
		clipView.set(NULL);
	}
	resetMouseContext();
}
void MainCtrl::preTrackDelete(track_t* track) {
	if(clipView.gui && clipView.gui->m_track == track) {
		clipView.set(NULL);
	}
	resetMouseContext();
}
void MainCtrl::cutIntersecting(track_t* tr, clip_t* mask) {
	tick_t tickBegin = mask->time;
	tick_t tickEnd = mask->end();
	cutIntersecting(tr, tickBegin, tickEnd);
}
void MainCtrl::showAutomation(track_t* tr, automatable_t* at, int32_t paramIdx) {
	view->ctr_tracks.showAutomationLane(tr, at, paramIdx);
}
void MainCtrl::cutSelection(const Cursor& _cursor) {
	int32_t tickBegin = _cursor.getTickBegin();
	int32_t tickEnd = _cursor.getTickEnd();
	int32_t trackBegin = _cursor.getTrackBegin();
	int32_t trackEnd = _cursor.getTrackEnd();
	if (!cursor.isSubtrackSelection()) {
		for (int i = trackBegin; i <= trackEnd; i++) {
			if (trackList.validTrackIdx(i)) {
				track_t* tr = trackList[i];
//				if (tr->type == TRACK_TYPE_MIDI) {
					cutIntersecting(tr, tickBegin, tickEnd);
//				}
			}
		}
	} else {
		int32_t trackSBegin = _cursor.getSubTrackBegin();
		int32_t trackSEnd = _cursor.getSubTrackEnd();
		if (trackList.validTrackIdx(trackBegin)) {
			track_t* tr = trackList[trackBegin];
			std::vector<automation_point_t> empty(0);
			for (int i = 0; i <= trackSEnd-trackSBegin; i++) {
				int32_t subTrackIdx = trackSBegin + i;
				if (tr->validSubtrack(subTrackIdx)) {
					gui_track_automationlane* subtrack = tr->subtracks[subTrackIdx];
					automation_t* automation = subtrack->getAutomation();
					if (automation) {
						automation->setRange(tickBegin, tickEnd, empty);
					}
				}

			}
		}
	}
}
void MainCtrl::pasteClipboard(clip_clipboard* clipboard, int32_t track, tick_t tick) {

	tick_t tickOffset = tick - clipboard->srcPos;
	tick_t trackOffset = track;
	for (int i = 0; i <= clipboard->selTrackRange; i++) {
		track_clipboard_t* trClipboard = clipboard->tracks[i].get();
		if (!trackList.validTrackIdx(i + trackOffset)) {
			continue;
		}
		int32_t trackIdx = trackList.clampTrackIdx(i + trackOffset);
		track_t* tr = trackList[trackIdx];
//		if (tr->type == TRACK_TYPE_MIDI) {
			trackdata_midi_t& midi = tr->getMidi();
			for (auto it = trClipboard->clips.begin(); it != trClipboard->clips.end(); it++) {
				clip_t* cl = (*it).get();
				clip_t* cloned = cl->clone();
				cloned->time += tickOffset;
				cutIntersecting(tr, cloned);
				midi.addClip(cloned);
			}
			midi.sortClips();
//		}
	}

}
void MainCtrl::setTempo(int32_t _tempo100) {
	playThread.call([this, _tempo100]() {
		this->tempo100 = CLAMP_I(_tempo100, 100, 99900);
	}, true);
}
void MainCtrl::setStatusText(String s) {
	view->statusbar.setTitle(s);
}
void MainCtrl::setEditClip(gui_clip* gclip) {
	view->ctr_clipeditor.storeLayout();
	clipView.set(gclip);
	view->ctr_clipeditor.showEditClip();
}
void copyClipsInRange(trackdata_midi_t& in, track_clipboard_t& out, int32_t srcPos, int32_t dstPos, int32_t len) {

	auto it = in.clips.cbegin();
	while (it != in.clips.cend()) {
		const clip_t* c = *it;
		if (c->end() > srcPos && c->time < srcPos+len) {
			clip_t clone(*c);
			if (c->time < srcPos && c->end() > srcPos) {
				cutClipLeft(&clone, srcPos - c->time);
			}
			if (c->time < srcPos + len && c->end() > srcPos + len) {
				cutClipRight(&clone, (c->end()) - (srcPos+len));
			}
			out.clips.push_back(make_shared<clip_t>(move(clone)));
		}
		it++;
	}
	stable_sort(out.clips.begin(), out.clips.end(), [](
			shared_ptr<clip_t> const & a, shared_ptr<clip_t> const & b) {
		return a->time < b->time;
	});
//	if (clips.size() > 1) {
//		if (!(clips[0]->start() < clips[1]->start())) {
//			for (int i = 0; i < clips.size(); i++) {
//				my_printf("clip[%d] = %d\n", i, clips[i]->start());
//			}
//		}
//		assert(clips[0]->start() < clips[1]->start());
//	}
//	out->sortClips();
}
void MainCtrl::pasteClipboard(clip_clipboard* clipboard, Cursor& cursor) {
	if (clipboard->type == clip_clipboard::ClipboardFull) {
		if (cursor.isSubtrackSelection())
			return;
		pasteClipboard(clipboard, cursor.cursorTrack, cursor.getTickBegin());
	} else  if (clipboard->type == clip_clipboard::ClipboardAutomation) {
		if (!cursor.isSubtrackSelection())
			return;
		int32_t tickBegin = cursor.getTickBegin();
		int32_t tickLen = clipboard->selRange;
		int32_t trackBegin = cursor.getTrackBegin();
		if (trackList.validTrackIdx(trackBegin)) {
			track_t* tr = trackList[trackBegin];
			int32_t subTrackOffset = cursor.getSubTrackBegin();
			for (int i = 0; i <= clipboard->selTrackRange; i++) {
				int32_t subTrackIdx = subTrackOffset + i;
				if (tr->validSubtrack(subTrackIdx)) {
					gui_track_automationlane* subtrack = tr->subtracks[subTrackIdx];
					std::vector<automation_point_t>& data = clipboard->automationLanes[i];
					automatable_t* automatable = subtrack->at;
					automation_t* automation = NULL;
					if (automatable) {
						automation = automatable->getAutomation(subtrack->param);
					}
					if (automation) {
						automation->setRange(tickBegin, tickBegin+tickLen, data);
					}
				}

			}
		}
	}
}
shared_ptr<clip_clipboard> MainCtrl::copySelection(const Cursor& _cursor) {
	int32_t tickBegin = _cursor.getTickBegin();
	int32_t tickEnd = _cursor.getTickEnd();
	int32_t trackBegin = _cursor.getTrackBegin();
	int32_t trackEnd = _cursor.getTrackEnd();
	int32_t trackSubBegin = _cursor.getSubTrackBegin();
	int32_t trackSubEnd = _cursor.getSubTrackEnd();
	shared_ptr<clip_clipboard> clipboard = make_shared<clip_clipboard>();
	clipboard->srcPos = tickBegin;
	clipboard->srcTrack = trackBegin;
	clipboard->selRange = tickEnd - tickBegin;
	if (_cursor.isSubtrackSelection()) {
		clipboard->selTrackRange = trackSubEnd - trackSubBegin;
		clipboard->type = clip_clipboard::ClipboardAutomation;
		if (trackList.validTrackIdx(trackBegin)) {
			track_t* tr = trackList[trackBegin];
			for (int i = trackSubBegin; i <= trackSubEnd; i++) {
				if (tr->validSubtrack(i)) {
					gui_track_automationlane* subtrack = tr->subtracks[i];
					automatable_t* automatable = subtrack->at;
					automation_t* automation = NULL;
					if (automatable) {
						automation = automatable->getAutomation(subtrack->param);
					}

					std::vector<automation_point_t> data;
					if (automation)
					automation->copyRange(tickBegin, tickEnd, data);
					clipboard->automationLanes.push_back(std::move(data));

				}
			}
		}
	} else {
		clipboard->selTrackRange = trackEnd - trackBegin;
		clipboard->selRange = tickEnd - tickBegin;
		clipboard->type = clip_clipboard::ClipboardFull;
		for (int i = 0; i <= clipboard->selTrackRange; i++) {
			track_clipboard_t trackClipboard;
			if (trackList.validTrackIdx(trackBegin + i)) {
				track_t* tr = trackList[trackBegin + i];
//				if (tr->type == TRACK_TYPE_MIDI) {
					copyClipsInRange(tr->getMidi(), trackClipboard, clipboard->srcPos, 0, clipboard->selRange);
//				}
			}
			clipboard->tracks.push_back(make_shared<track_clipboard_t>(move(trackClipboard)));
		}
	}
	return clipboard;
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


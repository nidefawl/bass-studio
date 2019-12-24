#include <vector>
#include "assert_dbg.h"

#include "error.h"
#include "math/seq_math.h"
#include "debugctr.h"
#include "str_util.h"
#include "knob.h"
#include "guiglobals.h"
#include "gui.h"
#include "button.h"
#include "knob.h"
#include "guicontainer.h"
#include "guicolors.h"
#include "guiconstant.h"
#include "trackctr.h"
#include "theme.h"
#include "button.h"
#include "track.h"
#include "track_impl.h"
#include "clip.h"
#include "../host/mainctrl.h"
#include "../host/vst_host.h"
#include "../host/audio_host.h"
#include "../host/plugin/vst_plugin.h"
#include "../host/plugin/vst_plugin_handles.h"
#include "edithistory.h"
#include "guiplugin.h"
#include "util/debug_alloc.h"
#ifdef _WIN32
#include "platform/win/debug_msg_count.h"
#endif
using namespace std;

#define DISPLAY_HWND_DRAWS 1
#define DISPLAY_WIN_MSG_STATS 1
//
//#if DISPLAY_WIN_MSG_STATS
//int getNumMsg();
//int getMsgId(int i);
//int getMsgCnt(int i);
//#endif
//#if DISPLAY_HWND_DRAWS
//int getHWNDMapSize();
//String getHWNDName(int i);
//int getHWNDCnt(int i);
//#endif
namespace GuiColor {
void initConstants(int colorVal);
}
constexpr int ID_BTN_RESET_HIST = 1;
constexpr int ID_KNOB_SET_COLOR = 2;
constexpr int ID_BTN_INJECT_SEGFAULT_AUDIO_THREAD = 3;
constexpr int ID_BTN_INJECT_BAD_MALLOC_AUDIO_THREAD = 4;
constexpr int ID_BTN_INJECT_SEGFAULT_MAIN_THREAD = 5;
constexpr int ID_BTN_INJECT_BAD_MALLOC_MAIN_THREAD = 6;
constexpr int ID_BTN_TOGGLE_STACKTRACE = 8;
constexpr int ID_BTN_TOGGLE_PROCESSING = 7;
constexpr int BTN_FONT_SIZE = 16;
gui_ctr_debug::gui_ctr_debug() : guictr_base() {
	setBackgroundRendered(true);
	auto knob = new guiknob;
	auto btn = new guibutton;
	btn->id = ID_BTN_RESET_HIST;
	btn->setText("Reset history");
	btn->setFontSize(BTN_FONT_SIZE);
	knob->id = ID_KNOB_SET_COLOR;
	knob->fnSetValue = [this](float f, int flags) {
		curVal = 0+math::max(0, math::min(255, (int32_t)math::floor(f*255)));
		GuiColor::initConstants(curVal);
		parentCtrl->getTheme()->initTheme();
	};
	knob->fnGetValue = [this](void) {
		return math::max(0.0f, math::min(1.0f, curVal/255.0f));
	};
	debugGuis.push_back(btn);
	debugGuis.push_back(knob);
	{

		auto btn2 = new guibutton;
		btn2->id = ID_BTN_INJECT_SEGFAULT_AUDIO_THREAD;
		btn2->setText("Segfault on Audiothread");
		btn2->setFontSize(BTN_FONT_SIZE);
		debugGuis.push_back(btn2);
		auto btn3 = new guibutton;
		btn3->id = ID_BTN_INJECT_BAD_MALLOC_AUDIO_THREAD;
		btn3->setText("BadAlloc on Audiothread");
		btn3->setFontSize(BTN_FONT_SIZE);
		debugGuis.push_back(btn3);
	}
	{

		auto btn2 = new guibutton;
		btn2->id = ID_BTN_INJECT_SEGFAULT_MAIN_THREAD;
		btn2->setText("Segfault on Mainthread");
		btn2->setFontSize(BTN_FONT_SIZE);
		debugGuis.push_back(btn2);
		auto btn3 = new guibutton;
		btn3->id = ID_BTN_INJECT_BAD_MALLOC_MAIN_THREAD;
		btn3->setText("BadAlloc on Mainthread");
		btn3->setFontSize(BTN_FONT_SIZE);
		debugGuis.push_back(btn3);
	}
	{
		auto btn3 = new guibutton;
		btn3->id = ID_BTN_TOGGLE_STACKTRACE;
		btn3->setText("Enable Stacktraces");
		btn3->setFontSize(BTN_FONT_SIZE);
		debugGuis.push_back(btn3);
	}
	{
		auto btn3 = new guibutton;
		btn3->id = ID_BTN_TOGGLE_PROCESSING;
		btn3->setText("Enable Processing");
		btn3->setFontSize(BTN_FONT_SIZE);
		debugGuis.push_back(btn3);
	}
	for (auto g : debugGuis) {
		add(g);
	}
}
void gui_ctr_debug::render(NVGcontext* vg) {
	if (isBackgroundRendered()){
		renderBackground(vg);
	}
	if (!setScissorTransform(vg)) {
		return;
	}
	MainCtrl *ctrl = MainCtrl::get();

	vector<String> strings;
	String str;
	str = StringFormat("%012X", (int64_t) ctrl->getTheme());
	strings.push_back(String("ctrl->getTheme: ") + str);
	str = StringFormat("%012X", (int64_t) (parentCtrl?parentCtrl->getTheme():0));
	strings.push_back(String("parentCtrl->getTheme: ") + str);
	str = StringFormat("%012X", (int64_t) (theme));
	strings.push_back(String("this->theme: ") + str);
	str = ctrl->guiOver ? ctrl->guiOver->getClassName() : "<null>";
	strings.push_back(String("guiOver: ") + str);
	str = ctrl->guiDragged ? ctrl->guiDragged->getClassName() : "<null>";
	strings.push_back(String("guiDragged: ") + str);
	str = ctrl->guiCaptured ? ctrl->guiCaptured->getClassName() : "<null>";
	strings.push_back(String("guiCaptured: ") + str);
	str = ctrl->guiCtrFocused ? ctrl->guiCtrFocused->getClassName() : "<null>";
	strings.push_back(String("guiCtrFocused: ") + str);
	str = ctrl->guiFocused ? ctrl->guiFocused->getClassName() : "<null>";
	strings.push_back(String("guiFocused: ") + str);
	str = "<null>";
	if (ctrl->getDragDropTarget().src) {
		str = static_cast<guibase*>(ctrl->getDragDropTarget().src)->getClassName();
		str += StringFormat(" %d", ctrl->getDragDropTarget().src);
	}
	strings.push_back(String("target: ") + str);

	guibase* p = ctrl->guiFocused;
	int lvl = 0;
	while (p != NULL) {
		String s = "";
		if (lvl == 0) {
			s = "guiFocused: ";
		}
		for (int i = 0; i < lvl; i++) {
			s += "  ";
		}
		strings.push_back(s + p->getClassName());
		p = p->parent;
		lvl++;
	}

	strings.push_back(String("lastKey: ") + ctrl->lastKey);
	strings.push_back(StringFormat("undo size: %d", ctrl->getHist().getNumUndoSteps()));
	strings.push_back(StringFormat("redo size: %d", ctrl->getHist().getNumRedoSteps()));
	clip_view& clipView = ctrl->getClipView();
	if (clipView.clip()) {
		strings.push_back(StringFormat("Clip: %s", StringAsCStr(clipView.clip()->name)));
		strings.push_back(StringFormat("Notes: %d", clipView.clip()->notes.m_list.size()));
		strings.push_back(StringFormat("Selection size: %d", clipView.clip()->notes.selection.size()));
	}

	strings.push_back("sample format");
	strings.push_back(StringFormat(" samplerate: %u", vsthost::getInstance()->sampleFormat.sampleRate));
	strings.push_back(StringFormat(" blockSize : %u", vsthost::getInstance()->sampleFormat.blockSize));
	strings.push_back(StringFormat(" bit depth : %u", static_cast<int32_t>(vsthost::getInstance()->sampleFormat.sampleformat)));

	track_t* track = ctrl->getTrackId(0);
	if (track && track->audio) {
		strings.push_back(StringFormat("level: %.4f", track->audio->meter.getMaxRMS()));
	}
	if (ctrl->guiFocused && ctrl->guiFocused->parent == (guibase*)ctrl->getPluginCtr()) {
		guiplugin* gplugin = dynamic_cast<guiplugin*>(ctrl->guiFocused);
		if (gplugin) {
			effectbase* vst = gplugin->getModule();
			strings.push_back("\n\n");
			vst->getInfo(strings);
		}
	}
	struct win32_msg {
		int id;
		int cnt;
	};
#if DISPLAY_HWND_DRAWS
	std::vector<win32_msg> wnd;
	for (int i = 0; i < msgCounter.getHWNDMapSize(); i++) {
		int cnt = msgCounter.getHWNDCnt(i);
		wnd.push_back( { i, cnt });
	}
	std::sort(wnd.begin(), wnd.end(), [](win32_msg const & a, win32_msg const & b) {
		return a.cnt > b.cnt;
	});
	for (win32_msg& msg : wnd) {
		String s = msgCounter.getHWNDName(msg.id);
		strings.push_back(StringFormat("%s: %d", StringAsCStr(s), msg.cnt));

	}
#endif

#if DISPLAY_WIN_MSG_STATS
	std::vector<win32_msg> msgs;
	for (int i = 0; i < msgCounter.getNumMsg(); i++) {
		int id = msgCounter.getMsgId(i);
		int cnt = msgCounter.getMsgCnt(i);
		msgs.push_back( { id, cnt });

	}
	std::sort(msgs.begin(), msgs.end(), [](win32_msg const & a, win32_msg const & b) {
		if (a.cnt == b.cnt) {
			return a.id < b.id;
		}
		return a.cnt > b.cnt;
	});
	for (win32_msg& msg : msgs) {
		strings.push_back(StringFormat("WM_ 0x%04X: %d", msg.id, msg.cnt));

	}
#endif
	int x = 5;

	setFont(vg, 14, G_WHITE, NVG_ALIGN_TOP | NVG_ALIGN_LEFT);
	float lineh;
	nvgTextMetrics(vg, NULL, NULL, &lineh);


	nvgText(vg, x, 0, StringAsCStr(label), NULL);
	String proj = StringFormat("Project: %s", StringAsCStr(ctrl->getProjectPath()));
	nvgText(vg, x, lineh, StringAsCStr(proj), NULL);
	int y = lineh*3;
	for (String& s : strings) {
		nvgText(vg, x, y, StringAsCStr(s), NULL);
		y += lineh;
	}
	for (String& s : g_debugStrings) {
		nvgText(vg, x, y, StringAsCStr(s), NULL);
		y += lineh;
	}
	for (auto c : guis) {
		nvgSave(vg);
		c->render(vg);
		nvgRestore(vg);
	}
	g_debugStrings.clear();
}
void gui_ctr_debug::layout() {
	ivec2 cs = getSizeContent();
	int32_t size = 32;
	auto knobTest = getByID(ID_KNOB_SET_COLOR);
	knobTest->size = ivec2(size);
	knobTest->pos = ivec2(cs.x-knobTest->size.x, cs.y-knobTest->size.y);
	auto posY = cs.y;
	auto posX = 0;
	for (auto gui : guis) {
		gui->layout();
		if (gui == knobTest)
			continue;
		gui->size = ivec2(size*5, size);
		gui->pos = ivec2(posX, posY-gui->size.y);
		posY = gui->top()-INSET_TRACK_CONTENT;
	}
}

int32_t getNumClipAllocations(); //clip.cpp
void resetHistAndCheck() {
	auto ctrl = MainCtrl::get();
	auto& trackEditor = MainCtrl::getGuiTrackCtr()->trackView;
	trackEditor.action.clipboard.reset();
	trackEditor.clipboard.reset();
	ctrl->getHist().clear(ctrl);

	auto& tracks = ctrl->trackList;
	int n = 0;
	for (auto track : tracks) {
		int nTrackClips = track->getMidi().getConstClips().size();
		my_printf("track %s %d %s has %d clips\n", TrackTypeToName(track->type), track->idx, StringAsCStr(track->name), nTrackClips);
		n += nTrackClips;
	}

	int nAlloc = getNumClipAllocations();

	dbgassert(n == nAlloc);
}
void gui_ctr_debug::buttonClicked(guibase* button) {
	switch (button->id) {
	case ID_BTN_RESET_HIST:
		resetHistAndCheck();
		break;
	case ID_BTN_INJECT_SEGFAULT_AUDIO_THREAD:
		MainCtrl::getPlayThread()->call([]() {
			debugRaiseSegFault();
		}, true);
		break;
	case ID_BTN_INJECT_BAD_MALLOC_AUDIO_THREAD:
		MainCtrl::getPlayThread()->call([]() {
			throw std::bad_alloc();
		}, true);
		break;
	case ID_BTN_INJECT_SEGFAULT_MAIN_THREAD:
		debugRaiseSegFault();
		break;
	case ID_BTN_INJECT_BAD_MALLOC_MAIN_THREAD:
		throw std::bad_alloc();
		break;

	case ID_BTN_TOGGLE_STACKTRACE:
		{
			auto tracker = DebugAlloc::getTracker<clip_t>();
			tracker->setPrintAllocationStackTraces(!tracker->getPrintAllocationStackTraces());
			static_cast<guibutton*>(button)->setText(String(tracker->getPrintAllocationStackTraces()?"Disable Stacktraces":"Enable Stacktraces"));

		}
		break;
	case ID_BTN_TOGGLE_PROCESSING:
		vsthost::getInstance()->enableProcessing = !vsthost::getInstance()->enableProcessing;
		static_cast<guibutton*>(button)->setText(String(vsthost::getInstance()->enableProcessing?"Disable Processing":"Enable Processing"));

		break;
	}
}
bool gui_ctr_debug::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
	if (this->contains(mpos)) {
		ivec2 localMouse = this->toContainerSpace(mpos);
		for (guibase* gui : guis) {
			if (gui->mouseHitTest(localMouse, evt)) {
				return true;
			}
		}
	}
	return false;
}

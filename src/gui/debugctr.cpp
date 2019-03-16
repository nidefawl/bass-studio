#include <vector>

#include "debugctr.h"
#include "str_util.h"
#include "knob.h"
#include "guicontainer.h"
#include "track.h"
#include "track_impl.h"
#include "clip.h"
#include "../host/mainctrl.h"
#include "../host/vst_host.h"
#include "../host/plugin/vst_plugin.h"
#include "../host/plugin/vst_plugin_handles.h"
#include "edithistory.h"
#include "leak_detect.h"
#include "guiplugin.h"

using namespace std;

#define DISPLAY_HWND_DRAWS 1
#define DISPLAY_WIN_MSG_STATS 1

struct win32_msg {
	int id;
	int cnt;
};
#if DISPLAY_WIN_MSG_STATS
int getNumMsg();
int getMsgId(int i);
int getMsgCnt(int i);
#endif
#if DISPLAY_HWND_DRAWS
int getHWNDMapSize();
String getHWNDName(int i);
int getHWNDCnt(int i);
#endif
namespace GuiColor {
extern int colorVal;
void initConstants(int colorVal);
}
gui_ctr_debug::gui_ctr_debug() : guictr_base() {
	add(&knobTest);
	add(&knobTest2);
	add(&btn);
	btn.setText("Reset history");
	btn.setFontSize(24);
	knobTest.fnSetValue = [this](float f, int flags) {
		GuiColor::colorVal = 0+max(0, min(255, (int32_t)std::floor(f*255)));
		GuiColor::initConstants(GuiColor::colorVal);
		parentCtrl->getTheme()->initDefaultTheme();
	};
	knobTest.fnGetValue = [](void) {
		return max(0.0f, min(1.0f, GuiColor::colorVal/255.0f));
	};
	knobTest2.setValueInit(theme->get(G_PLUGIN_TITLE_HEIGHT)/255.0f);
	knobTest2.fnSetValue = [this](float f, int flags) {
		theme->set(G_PLUGIN_TITLE_HEIGHT, (int32_t)(knobTest2.getValueInternal()*255.0f));
	};
}
void gui_ctr_debug::render(NVGcontext* vg) {
	renderBackground(vg);
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
	if (ctrl->getDragDropTarget().ptr) {
		str = static_cast<guibase*>(ctrl->getDragDropTarget().ptr)->getClassName();
		str += StringFormat(" %d", ctrl->getDragDropTarget().idx);
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
	strings.push_back(StringFormat("Samplerate: %u", vsthost::getInstance()->lSampleRate));
	strings.push_back(StringFormat("BlockSize: %u", vsthost::getInstance()->lBlockSize));
	strings.push_back(StringFormat("blockReads: %u", vsthost::getInstance()->blockReads));
	strings.push_back(StringFormat("bufferUnderuns: %u", vsthost::getInstance()->bufferUnderuns));
	strings.push_back(StringFormat("numCallsWaitEvents: %u", ctrl->numCallsWaitEvents));

	track_t* track = ctrl->getTrackId(0);
	if (track && track->audio) {
		strings.push_back(StringFormat("level: %.4f", track->audio->meter.getRms(0)));
	}
	if (ctrl->guiFocused && ctrl->guiFocused->parent == (guibase*)ctrl->getPluginCtr()) {
		guiplugin* gplugin = dynamic_cast<guiplugin*>(ctrl->guiFocused);
		if (gplugin) {
			effectbase* vst = gplugin->getModule();
			strings.push_back("\n\n");
			vst->getInfo(strings);
		}
	}
#if DISPLAY_HWND_DRAWS
	std::vector<win32_msg> wnd;
	for (int i = 0; i < getHWNDMapSize(); i++) {
		int cnt = getHWNDCnt(i);
		wnd.push_back( { i, cnt });
	}
	std::sort(wnd.begin(), wnd.end(), [](win32_msg const & a, win32_msg const & b) {
		return a.cnt > b.cnt;
	});
	for (win32_msg& msg : wnd) {
		String s = getHWNDName(msg.id);
		strings.push_back(StringFormat("%s: %d", StringAsCStr(s), msg.cnt));

	}
#endif

#if DISPLAY_WIN_MSG_STATS
	std::vector<win32_msg> msgs;
	for (int i = 0; i < getNumMsg(); i++) {
		int id = getMsgId(i);
		int cnt = getMsgCnt(i);
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

	setFont(vg, 26, G_WHITE, NVG_ALIGN_TOP | NVG_ALIGN_LEFT);
	nvgText(vg, x, 0, StringAsCStr(label), NULL);
	String proj = StringFormat("Project: %s", StringAsCStr(ctrl->getProjectPath()));
	nvgText(vg, x, 30, StringAsCStr(proj), NULL);
	float lineh;
	nvgTextMetrics(vg, NULL, NULL, &lineh);
	int y1 = 60;
	int y = y1 + lineh;
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
	int32_t size = 64;
	knobTest.size = ivec2(size);
	knobTest2.size = ivec2(size);
	knobTest.pos = ivec2(cs.x-knobTest.size.x, cs.y-knobTest.size.y);
	knobTest2.pos = ivec2(knobTest.pos.x-knobTest2.size.x, cs.y-knobTest2.size.y);
	btn.size = ivec2(size*3.5, size);
	btn.pos = ivec2(0, cs.y-btn.size.y);
}

void gui_ctr_debug::buttonClicked(guibase* button) {
	if (button == &btn) {
		MainCtrl::get()->getHist().clear();
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

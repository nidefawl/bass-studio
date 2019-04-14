#pragma once
#include <list>
#include <vector>
#include <set>
#include <stdint.h>
#include <memory>

#include "math/vec.h"
#include "config.h"
#include "str_util.h"
#include "seq_util.h"
#include "seq_time.h"
#include "window.h"
#include "menu.h"
#include "mouse.h"
#include "keyboard.h"
#include "event.h"
#include "note.h"
#include "logging.h"
#include "hires_timer.h"
#include "rand.h"
#include "theme.h"
#include "thememgr.h"
#include "saferef.h"

struct NVGcontext;
class guibase;
class guictr_base;
class guictxtmenu_base;
class appwindow_main;

KeyEvent keyEvent(int key, int scancode, int keyState, int mods, const char* key_name);
String getModKeyName(int modKey);
String menuName(String s, KeyCombo combo);

class BaseCtrl : public SafeRefHandler<guibase> {
protected:
	guitheme_mgr themes;
public:
	BaseCtrl() {
		themes.parent = this;
	}
	window_base* window = NULL;
	NVGcontext* vg = NULL;
	std::vector<guictr_base*> containers;
	guictxtmenu_base *ctxtmenu = NULL;
//	guictxtmenu_base *ctxtmenuOld = NULL;
	int cursorIcon = CURSOR_DEFAULT;
	ivec2 m_size = { -1, -1 };
	ivec2 m_mousePos = { -1, -1 };
	guibase *guiOver = NULL;		//updates on mouse move "current mouseover"
	guibase *guiDragged = NULL;		//updates on mouse click "currently dragged", set from guiOver
	guibase *guiCaptured = NULL;	//updates when cursor is hidden, set from guiDragged
	guibase *guiFocused = NULL;		//updates on mouse click, set from guiOver
	guibase *guiCtrFocused = NULL;	//updates on mouse click, handles keyboard input
	guibase* getGuiFocused() {
		return guiFocused;
	}
	struct stored_ref {
		guibase* ptr;
		int32_t refId;
	};
	int32_t refIdNext = 1;
	std::vector<stored_ref> refs;
	int safeRefCreate(guibase* gui) override {
		stored_ref ref{gui, (int32_t)refIdNext++};
		refs.push_back(ref);
		return ref.refId;
	}
	guibase* safeRefGetPtr(int32_t refId) override {
		auto it = std::find_if(refs.begin(), refs.end(), [refId](const stored_ref& ref) {
			return ref.refId == refId;
		});
		if (it != refs.end()) {
			stored_ref& ref = *it;
			return ref.ptr;
		}
		return nullptr;
	}
	void safeRefDestroy(int32_t refId) override {
		auto it = std::find_if(refs.begin(), refs.end(), [refId](const stored_ref& ref) {
			return ref.refId == refId;
		});
		if (it != refs.end()) {
			it->ptr = nullptr;
			refs.erase(it);
			return;
		}
		assert(0);
	}

	ivec2 dragStart;
	ivec2 dragOffset;
	ivec2 dragDistance;
	bool mouseInside = false;
	bool isOK = false;
	bool isOk() const {
		return isOK;
	}
	virtual ~BaseCtrl() { }
	virtual guitheme_t* getTheme() {
		return &themes.getRef();
	}
	guitheme_mgr* getThemeMgr() {
		return &themes;
	}
	virtual void prerender(int32_t x, int32_t y, int32_t w, int32_t h, float ratio);
	void render(int32_t x, int32_t y, int32_t w, int32_t h, float ratio);
	virtual bool processGlobalKeyevent(KeyEvent& event) {
		return false;
	}
	virtual bool mouseDownPre() {
		return true;
	}
	MouseHitEvt mouseHitEvt(MouseHitType _type);
	void focusGui(guibase* g);
	void mouseDown(ivec2 mousePos, int button, bool doubleclick);
	void mouseUp(ivec2 mousePos, int button);
	virtual void onCharInput(unsigned int codepoint);
	virtual void onKeyInput(int key, int scancode, int keyState, int mods, const char* key_name);
	void mouseScrolled(double xoffset, double yoffset);
	virtual void mouseMoved(ivec2 mousePos, ivec2 deltaPos);

	bool isCtrOrChildFocused(guibase* gui);
	bool isMouseInside() {
		return mouseInside;
	}
	virtual void onCursorEnter(int entered) {
		mouseInside = entered;
	}
	virtual void relayout() { relayout(m_size.x, m_size.y); };
	virtual void relayout(int32_t w, int32_t h) { };
	virtual void openContextMenu(guictxtmenu_base *b, ivec2 pos);
	virtual void closeContextMenu() { };
	void closeAllAppMenus()  { closeAppMenusAtLvl(0); };
	virtual void closeAppMenusAtLvl(int startlvl)  { };
	virtual void closeAllContextMenus() {
		closeContextMenu();
		closeAllAppMenus();
	}
	virtual void openAppMenu(int lvl, guictxtmenu_base *b, ivec2 pos) { };
	virtual void closePopup() { }; // close this window if its a popup window
	virtual bool hasContextMenu() { return false; };
	virtual void objectDragMove(guibase* g, MouseEvent& evt) { };
	virtual void objectDragRelease(guibase* g, MouseEvent& evt) { };
	bool captureMouse(guibase* gui);
	virtual String getClipboardText();
	virtual void setClipboardText(String s);
	virtual void requestRedraw() {
		this->window->requestRedraw();
	}
	// Only use this pointer for comparison!
	void onGuiRemoved(void* gui);
	void resetMouseContext();
};
class AppCtrl : public BaseCtrl {
protected:
	struct appmenu_window_entry {
		window_overlay* wnd;
		guictxtmenu_base *ctxt;
	};
	std::vector<appmenu_window_entry> menuWindows;
	std::vector<guibase*> garbageGuis;
public:
	bool hasCtxtMenu() {
		return this->ctxtmenu!=NULL;
	}
	bool hasMenuWindow() {
		for (auto& w : menuWindows) {
			if (w.ctxt)
				return true;
		}
		return false;
	}
	window_main* mainWindow = NULL;
	window_overlay* contextWindow = NULL;
#if WINDOW_HAS_MENUBAR
	ngui::MenuBar menubar;
#endif
	AppCtrl();
	virtual ~AppCtrl();
	virtual void relayout(int32_t w, int32_t h) override = 0;
	void onChildOverlayWindowClose(window_overlay*);
	void openContextMenu(guictxtmenu_base *b, ivec2 pos) override;
	void openDialog(guictxtmenu_base *b);
	void closeContextMenu() override;
	void openAppMenu(int lvl, guictxtmenu_base *b, ivec2 pos) override;
	void closeAppMenusAtLvl(int startlvl) override;
	bool hasContextMenu() override;
	virtual void onKeyInput(int key, int scancode, int keyState, int mods, const char* key_name) override;
	virtual void onCharInput(unsigned int codepoint) override;
	virtual void onMenuOpen(ngui::Menu* menu);
	virtual void updateMenubar();
	guictxtmenu_base* getContextMenu();
#if WINDOW_HAS_MENUBAR
	virtual ngui::MenuBar& getMenubar();
#endif

	virtual void closePopup() { }; // close this window if its a popup window

	virtual void focusReceived() = 0;
	virtual void focusLost() = 0;
	virtual bool filesDropMove(ivec2 pos, int kbmods) { return false; };
	virtual bool filesDropBegin(std::vector<String>& files, ivec2 pos, int kbmods) { return false; };
	virtual bool filesDropFinal(std::vector<String>& files, ivec2 pos, int kbmods) { return false; };
	virtual void menuCommand(int cmd) { };
	virtual void onWindowClose() { };
	virtual bool onWindowCloseRequest() { return true; };

	virtual void onTick() = 0;
	virtual void initApp(int argc, char* argv[]) = 0;
	virtual bool init(window_main* window, NVGcontext* nanovg) = 0;
	virtual void postInit() = 0; /* OpenGL context exists in postInit */
	virtual void destroy() = 0;
	void destroyControl();
protected:
	void openOverlayGui(guictxtmenu_base *b, ivec2 pos, int flags);
};
class guictr_scrollbar;
class PopupCtrl : public BaseCtrl
{
	guictr_scrollbar* popupCtrs = nullptr;
	bool canTakeInputFocus = false;
public:
	PopupCtrl();
	~PopupCtrl();
	void destroy();
	bool isShown() {
		return this->window && this->window->isShown();
	}
	void closePopup() override;
	void relayout(int32_t w, int32_t h) override;
	void open(guictxtmenu_base *ctxtmenu, ivec2 pos);
	bool init(window_overlay* window, NVGcontext* nanovg);
	void focusReceived() { };
	void focusLost();
	bool hasInputFocus();
	void onWindowClose();
	bool onWindowCloseRequest();
};

#pragma once
#include <list>
#include <vector>
#include <set>
#include <stdint.h>
#include <memory>

#include <glm/glm.hpp>
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
#include "note.h"
#include "logging.h"
#include "hires_timer.h"
#include "rand.h"


using glm::vec2;
using glm::ivec2;
using glm::vec4;
using glm::ivec4;

struct NVGcontext;
class guibase;
class guictr_base;
class guictxtmenu_base;
class appwindow_main;

KeyEvent keyEvent(int key, int scancode, int keyState, int mods, const char* key_name);
String getModKeyName(int modKey);
String menuName(String s, KeyCombo combo);

class BaseCtrl {
public:
	window_base* window = NULL;
	NVGcontext* vg = NULL;
	std::vector<guictr_base*> containers;
	guictxtmenu_base *ctxtmenu = NULL;
	guictxtmenu_base *ctxtmenuOld = NULL;
	int cursorIcon = CURSOR_DEFAULT;
	ivec2 m_size;
	ivec2 m_mousePos;
	guibase *guiOver = NULL;		//updates on mouse move "current mouseover"
	guibase *guiDragged = NULL;		//updates on mouse click "currently dragged", set from guiOver
	guibase *guiCaptured = NULL;	//updates when cursor is hidden, set from guiDragged
	guibase *guiFocused = NULL;		//updates on mouse click, set from guiOver
	guibase *guiCtrFocused = NULL;	//updates on mouse click, handles keyboard input

	ivec2 dragStart;
	ivec2 dragOffset;
	ivec2 dragDistance;
	bool mouseInside = false;
	bool isOK = false;
	bool isOk() const {
		return isOK;
	}
	virtual ~BaseCtrl() { }
	virtual void prerender(int32_t x, int32_t y, int32_t w, int32_t h, float ratio);
	virtual void render(int32_t x, int32_t y, int32_t w, int32_t h, float ratio);

	virtual bool processGlobalKeyevent(KeyEvent& event) {
		return false;
	}
	virtual bool mouseDownPre() {
		return true;
	}
	MouseHitEvt mouseHitEvt(MouseHitType _type);
	void mouseDown(ivec2 mousePos, int button, bool doubleclick);
	void mouseUp(ivec2 mousePos, int button);
	void onCharInput(unsigned int codepoint);
	void onKeyInput(int key, int scancode, int keyState, int mods, const char* key_name);
	void mouseScrolled(double xoffset, double yoffset);
	virtual void mouseMoved(ivec2 mousePos, ivec2 deltaPos);

	bool isCtrOrChildFocused(guibase* gui);
	bool isMouseInside() {
		return mouseInside;
	}
	virtual void onCursorEnter(int entered) {
		mouseInside = entered;
	}
	virtual void relayout(int32_t w, int32_t h) { };
	virtual void openContextMenu(guictxtmenu_base *b, ivec2 pos) { };
	virtual void closeContextMenu() { };
	virtual void closeAppMenus()  { };
	virtual void closeAppMenus(int startlvl)  { };
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
	void onGuiRemoved(guibase* gui);
	void resetMouseContext();
};
class AppCtrl : public BaseCtrl {
public:
	window_main* mainWindow = NULL;
	window_overlay* contextWindow = NULL;
	std::vector<window_overlay*> menuWindows;
#if WINDOW_HAS_MENUBAR
	ngui::MenuBar menubar;
#endif
	AppCtrl() { }
	virtual ~AppCtrl() { }
	virtual void relayout(int32_t w, int32_t h) = 0;
	void openContextMenu(guictxtmenu_base *b, ivec2 pos);
	void closeContextMenu();
	void openAppMenu(int lvl, guictxtmenu_base *b, ivec2 pos);
	void closeAppMenus();
	void closeAppMenus(int startlvl);
	bool hasContextMenu();
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
	virtual void onWindowCloseRequest() { };

	virtual void onTick() = 0;
	virtual void initApp(int argc, char* argv[]) = 0;
	virtual bool init(window_main* window, NVGcontext* nanovg) = 0;
	virtual void postInit() = 0; /* OpenGL context exists in postInit */
	virtual void destroy() = 0;
};
class guictr_popup;
class PopupCtrl : public BaseCtrl
{
	guictr_popup* popupCtrs;
public:
	PopupCtrl();
	~PopupCtrl();
	static PopupCtrl* get() {
		static PopupCtrl ctrl;
		return &ctrl;
	}
	void destroy();
	bool isShown() {
		return this->window->isShown();
	}
	void closePopup() override;
	void open(guictxtmenu_base *ctxtmenu, ivec2 pos);
	bool init(window_overlay* window, NVGcontext* nanovg);
	void focusReceived() { };
	void focusLost();

};

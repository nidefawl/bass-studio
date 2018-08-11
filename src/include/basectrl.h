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
	void mouseDown(ivec2 mousePos, int button, bool doubleclick);
	void mouseUp(ivec2 mousePos, int button);
	void onCharInput(unsigned int codepoint);
	void onKeyInput(int key, int scancode, int keyState, int mods, const char* key_name);
	void mouseScrolled(double xoffset, double yoffset);
	virtual void mouseMoved(ivec2 mousePos, ivec2 deltaPos);

	bool isCtrOrChildFocused(guibase* gui);
	virtual void onCursorEnter(int entered) {

	}
};
class AppCtrl : public BaseCtrl {
public:
	AppCtrl() { }
	virtual ~AppCtrl() { }
	virtual void relayout(int32_t w, int32_t h) = 0;
	virtual void requestRedraw() = 0;
	virtual void closeAppMenus(int startlvl) = 0;
	virtual void openContextMenu(guictxtmenu_base *b, ivec2 pos) = 0;
	virtual void openAppMenu(int lvl, guictxtmenu_base *b, ivec2 pos) = 0;
	virtual void closeAppMenus() = 0;
	virtual void menuCommand(int cmd) = 0;
	virtual void onMenuOpen(ngui::Menu* menu) = 0;
	virtual void updateMenubar() = 0;
	virtual void closeContextMenu() = 0;
	virtual bool hasContextMenu() = 0;
	virtual bool captureMouse(guibase* gui) = 0;
	virtual void objectDragMove(guibase* g, MouseEvent& evt) = 0;
	virtual void objectDragRelease(guibase* g, MouseEvent& evt) = 0;
	virtual String getClipboardText() = 0;
	virtual void setClipboardText(String s) = 0;
	virtual void onWindowCloseRequest() = 0;
	virtual void focusReceived() = 0;
	virtual void focusLost() = 0;
	virtual bool filesDropMove(ivec2 pos, int kbmods) = 0;
	virtual bool filesDropBegin(std::vector<String>& files, ivec2 pos, int kbmods) = 0;
	virtual bool filesDropFinal(std::vector<String>& files, ivec2 pos, int kbmods) = 0;
	virtual ngui::MenuBar& getMenubar() = 0;
	virtual void onTick() = 0;
	virtual void initApp() = 0;
	virtual bool init(window_main* window, NVGcontext* nanovg) = 0;
	/* OpenGL context exists at this point */
	virtual void postInit() = 0;
	virtual void destroy() = 0;
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
	static AppCtrl* get();
};
class guictr_popup;
class PopupCtrl : public BaseCtrl
{
	guictr_popup* popupCtrs;
	bool mouseInside = false;
public:
	PopupCtrl();
	~PopupCtrl();
	static PopupCtrl* get() {
		static PopupCtrl ctrl;
		return &ctrl;
	}
	bool isMouseInside() {
		return mouseInside;
	}
	void destroy();
	bool isShown() {
		return this->window->isShown();
	}
	void close();
	void open(guictxtmenu_base *ctxtmenu, ivec2 pos);
	bool init(window_overlay* window, NVGcontext* nanovg);
	void focusReceived() {
	}
	void focusLost();
	void onCursorEnter(int entered) override;

};

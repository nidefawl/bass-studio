#pragma once
#include "math/vec.h"
#include "str_util.h"
#include <memory>

#define WINDOW_BORDERLESS_POPUP 1
#define WINDOW_IS_MAINWINDOW_MASTER 2
#define WINDOW_IS_MAINWINDOW_SLAVE 4

class BaseCtrl;
class AppCtrl;
class PopupCtrl;
struct window_draw_fn;
struct window_init_fn;

class window_base {
public:
	window_base() {}
	virtual ~window_base() {}
	virtual bool isShown() = 0;
	virtual void getPos(ivec2* pos) = 0;
	virtual void getSize(ivec2* size) = 0;
	virtual void setSize(ivec2 size) = 0;
	virtual void setPos(ivec2 pos) = 0;
	virtual void requestRedraw() = 0;
	virtual void setClipboardText(String s) = 0;
	virtual String getClipboardText() = 0;
	virtual int getKeyMods() = 0;
	virtual void hideSystemCursor() = 0;
	virtual void captureMouse() = 0;
	virtual void releaseMouse() = 0;
	virtual bool isMouseCaptured() = 0;
	virtual void updateWindowFromDlg() = 0;
	virtual void fireMouseMoved() = 0;
};
class window_dialog : public window_base {
public:
	window_dialog() : window_base() {}
	virtual ~window_dialog() {}
	virtual void show() = 0;
	virtual void setDrawFunction(const window_draw_fn& fn) = 0;
	virtual void setInitFunction(const window_init_fn& fn) = 0;
};

class window_overlay : public window_base {
public:
	window_overlay() : window_base() {}
	virtual ~window_overlay() {}
	virtual void show() = 0;
	virtual void hide() = 0;
	virtual void positionOnScreen(ivec2 pos, ivec2 size) = 0;
	virtual PopupCtrl* getCtrl() = 0;
};
class window_main : public window_base {
public:
	window_main() : window_base() {}
	virtual ~window_main() {}
	virtual window_dialog* createDialog(const String& sTitle, int w, int h) = 0;
	virtual window_main* createOverlay(std::shared_ptr<AppCtrl> ctrl, int flags) = 0;
	virtual void closeOverlay(window_main* wnd) = 0;
	virtual void show() = 0;
	virtual void hide() = 0;
	virtual void requestClose() = 0;
	virtual void updateMenu() = 0;
	virtual void preRender() = 0;
	virtual void postRender() = 0;
	virtual AppCtrl* getCtrl() = 0;
	virtual void positionOnScreen(ivec2 pos, ivec2 size) = 0;
	virtual bool canResize() = 0;
};

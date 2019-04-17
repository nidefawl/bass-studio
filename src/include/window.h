#pragma once
#include "math/vec.h"
#include "str_util.h"

class BaseCtrl;
class PopupCtrl;
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
	virtual window_dialog* createDialog() = 0;
	virtual window_overlay* createOverlay() = 0;
	virtual void requestClose() = 0;
	virtual void updateMenu() = 0;
	virtual void preRender() = 0;
	virtual void postRender() = 0;
};

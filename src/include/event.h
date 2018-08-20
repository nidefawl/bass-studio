#pragma once

#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include "mouse.h"
using glm::ivec2;


class guibase;

enum MouseEventType {
	M_EVT_BTN_DOWN,
	M_EVT_BTN_UP,
	M_EVT_DOUBLECLICK,
	M_EVT_MOVE,
	M_EVT_CAPTURED_MOVE,
	M_EVT_SCROLL,
};
enum MouseHitType {
	MOUSE_OVER,
	MOUSE_LEFT,
	MOUSE_RIGHT,
	MOUSE_DRAGDROP_CLIP,
	MOUSE_DRAGDROP_OBJECT,
	MOUSE_SCROLL,
};
struct MouseEvent {
	MouseEventType type;
	int button;
	guibase* guiDragged;
	ivec2 mousepos;
	ivec2 relMousepos;
	ivec2 dragStart;
	ivec2 dragOffset;
	ivec2* dragDistance;
	int kbmods;
};
class MouseHitEvt {
	guibase *guiHit = nullptr;
	void *draggedThing = nullptr;
	int cursorIcon = CURSOR_DEFAULT;
	bool cursorChanged = false;
public:
	MouseHitType type;
	int kbmods;
	MouseHitEvt(MouseHitType _type, int _kbmods) : type(_type), kbmods(_kbmods) {
	}
	void requestFocus(guibase* gui) {
		guiHit = gui;
	}
	guibase* getGuiHit() {
		return guiHit;
	}
	int getCursor() {
		return cursorIcon;
	}
	bool hasCursorChanged() {
		return cursorChanged;
	}
	void requestCursor(int _cursorIcon) {
		cursorChanged = true;
		cursorIcon = _cursorIcon;
	}
	void* getDraggedThing() {
		return draggedThing;
	}
	void setDraggedThing(void* _draggedThing) {
		draggedThing = _draggedThing;
	}
};
enum KeyEventType {
	K_RELEASE,
	K_PRESS,
	K_REPEAT,
};
struct KeyEvent {
	KeyEventType type;
	int keyCode;
	int scancode;
	int mods;
	const char* keyname;
};

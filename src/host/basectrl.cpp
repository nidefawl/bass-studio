#include <nanovg.h>
#include <time.h>
#include <algorithm>
#include <functional>
#include <vector>
#include <memory>
#include <GLFW/glfw3.h>
#include "basectrl.h"
#include "../gui/gui.h"
#include "../gui/guicontainer.h"
#include "../gui/guicontextmenu.h"

#include "window.h"
#include "platform.h"

#include "keyboard.h"
#include "mouse.h"
#include "event.h"
#include "commands.h"

#include "project.h"

using glm::vec2;
using glm::ivec2;
using glm::vec4;
using glm::ivec4;
using std::min;
using std::max;
using namespace std;

String getModKeyName(int modKey) {
	switch (modKey) {
	case KB_MOD_SHIFT:
		return "Shift";
	case KB_MOD_CTRL:
		return "Ctrl";
	case KB_MOD_ALT:
		return "Alt";
	}
	return "";
}
String menuName(String s, KeyCombo combo) {
	String modName = getModKeyName(combo.keyMod);
	String keyName = "";
	if (combo.keyChar) {
		keyName = StringToUpper(combo.keyChar);
	}
	if (!keyName.length()) {
		return s;
	}
	if (modName.length()) {
		modName = modName+"+";
		keyName = modName + keyName;
	}
	return StringFormat("%s\t%s", StringAsCStr(s), StringAsCStr(keyName));
}
MouseEvent mouseEvent(BaseCtrl* ctrl, guibase* gui, ivec2 mousePos, int button, MouseEventType evtType) {
	MouseEvent mevt;
	/*MouseEventType type;
	int button;
	guibase* guiDragged;
	ivec2 mousepos;
	ivec2 localpos;
	ivec2& dragStart;
	ivec2& dragOffset;
	ivec2& dragDistance;*/
	mevt.type = evtType;
	mevt.guiDragged = gui;
	mevt.button = button;
	mevt.mousepos = mousePos;
	mevt.relMousepos = toControlsObjectSpace(mousePos, gui);
	mevt.dragStart = ctrl->dragStart;
	mevt.dragOffset = ctrl->dragOffset;
	mevt.dragDistance = &ctrl->dragDistance;
	mevt.kbmods = ctrl->window->getKeyMods();
	return mevt;
}

KeyEvent keyEvent(int key, int scancode, int keyState, int mods, const char* key_name) {
	KeyEvent kevt;
	switch (keyState) {
	case STATE_PRESS:
		kevt.type = KeyEventType::K_PRESS;
		break;
	case STATE_REPEAT:
		kevt.type = KeyEventType::K_REPEAT;
		break;
	case STATE_RELEASE:
		kevt.type = KeyEventType::K_RELEASE;
		break;
	}
	kevt.keyCode = key;
	kevt.scancode = scancode;
	kevt.mods = mods;
	kevt.keyname = key_name;
	return kevt;
}
ivec2 toControlsObjectSpace(ivec2& pos, guibase* gui) {
	vector<guibase*> guiHierachy;
	gui->getHierachy(guiHierachy);
	ivec2 posOS = pos;
	while (!guiHierachy.empty()) {
		guibase* b = guiHierachy.back(); guiHierachy.pop_back();
		posOS = b->toContainerSpace(posOS);
	}
//	return posOS - gui->pos;
	return gui->toContainerSpace(posOS);
}
void processScrollEvt(BaseCtrl* ctrl, guibase* gui, ivec2 mousePos, double xoffset, double yoffset) {
	MouseEvent evt = mouseEvent(ctrl, gui, mousePos, -1, M_EVT_SCROLL);
	if (!gui->handleMouseScroll(evt, xoffset, yoffset)) {
		if (gui->parent) {
			processScrollEvt(ctrl, gui->parent, mousePos, xoffset, yoffset);
		}
	}
}
void BaseCtrl::mouseUp(ivec2 mousePos, int button) {
	if (guiCaptured != NULL) {
		this->window->releaseMouse();
		guiCaptured = NULL;
	}
	if (guiDragged) {
		cursorIcon = CURSOR_DEFAULT;
//		if (guiDragged!=guiFocused&&guiFocused) {
//			MouseEvent evt = mouseEvent(this, guiFocused, mousePos, button, M_EVT_BTN_UP);
//			guiFocused->handleDraggedRelease(evt);
//		}
		cursorIcon = CURSOR_DEFAULT;
		MouseEvent evt = mouseEvent(this, guiDragged, mousePos, button, M_EVT_BTN_UP);
		guiDragged->handleDraggedRelease(evt);
		guiDragged = NULL;
	}
}
MouseHitEvt BaseCtrl::mouseHitEvt(MouseHitType _type) {
	return {_type, window->getKeyMods()};

}
void BaseCtrl::mouseDown(ivec2 mousePos, int button, bool doubleclick) {
	if (!mouseDownPre()) {
		return;
	}
	if (guiCaptured != NULL) {
		return;
	}
	MouseHitEvt evt = mouseHitEvt(button == 0 ? MouseHitType::MOUSE_LEFT : MouseHitType::MOUSE_RIGHT);
	for (guictr_base *ctr : containers) {
		if (ctr->mouseHitTest(mousePos, evt)) {
			break;
		}
	}
	guiOver = evt.getGuiHit();

	guibase* gui = evt.getGuiHit();
	guibase* oldFocused = guiFocused;
	guibase* newFocus = gui != NULL ? gui->getFocusedControl() : NULL;
	guiCtrFocused = gui != NULL ? gui->getFocusedContainer() : NULL;
	if (oldFocused != newFocus) {
		if (oldFocused) {
			oldFocused->focusEvent(evt,false);
		}
		if (newFocus && newFocus->focusEvent(evt, true)) {
			guiFocused = newFocus;
		}
	}
//	if (evt.hasCursorChanged()) {
		cursorIcon = evt.getCursor();
//	}
		if (button == 0) {
			//left button gets focus from mouse move only
			guiDragged = !!(gui) ? gui->getDraggedControl() : nullptr;
		}
	if (gui != NULL) {
		dragDistance = ivec2(0);
		dragStart = mousePos;
		dragOffset = gui->toScreenSpace(ivec2(0)) - mousePos;
		MouseEvent evt = mouseEvent(this, gui, mousePos, button, doubleclick ? M_EVT_DOUBLECLICK : M_EVT_BTN_DOWN);
		if (button == 0) {
			gui->handleDraggedBegin(evt);
		} else if (button == 1) {
			gui->handleRightClick(evt);
		}
	}
}

void BaseCtrl::mouseScrolled(double xoffset, double yoffset) {
	ivec2 mousePos = this->m_mousePos;
	MouseHitEvt evt = mouseHitEvt(MouseHitType::MOUSE_SCROLL);
	for (guictr_base *ctr : containers) {
		if (ctr->mouseHitTest(mousePos, evt)) {
			break;
		}
	}
	guibase* gui = evt.getGuiHit();
	if (gui) {
		processScrollEvt(this, gui, mousePos, xoffset, yoffset);
	}
}

bool BaseCtrl::isCtrOrChildFocused(guibase* gui) {
	if (gui == this->guiCtrFocused)
		return true;
	guibase* p = this->guiFocused;
	while (p != NULL) {
		if (p == gui)
			return true;
		p = p->parent;
	}
	return false;
}

void BaseCtrl::mouseMoved(ivec2 mousePos, ivec2 deltaPos) {
	if (ctxtmenu && !ctxtmenu->isTransient()) {
		return;
	}
	this->m_mousePos = mousePos;
	if (ctxtmenu == NULL) {
		if (guiCaptured != NULL) {
			dragDistance += deltaPos;
			MouseEvent evt = mouseEvent(this, guiCaptured, mousePos, -1, M_EVT_CAPTURED_MOVE);
			guiCaptured->handleDraggedMove(evt);
			return;
		}
		if (guiDragged != NULL) {
			dragDistance += deltaPos;
			MouseEvent evt = mouseEvent(this, guiDragged, mousePos, -1, M_EVT_MOVE);
			guiDragged->handleDraggedMove(evt);
			return;
		}
	}
	MouseHitEvt evt = mouseHitEvt(MouseHitType::MOUSE_OVER);
	for (guictr_base *ctr : containers) {
		if (ctr->mouseHitTest(mousePos, evt)) {
			break;
		}
	}
//	if (evt.hasCursorChanged()) {
		cursorIcon = evt.getCursor();
//	}
	guiOver = evt.getGuiHit();
}

void BaseCtrl::onCharInput(unsigned int codepoint) {
	if (guiCaptured) {
		return;
	}
	if (guiFocused && guiFocused->handleCharInput(codepoint)) {
		return;
	}
	if (guiCtrFocused && guiCtrFocused != guiFocused && guiCtrFocused->handleCharInput(codepoint)) {
		return;
	}
	if (guiCtrFocused != NULL && guiCtrFocused != guiFocused) {
		if (guiCtrFocused->handleCharInput(codepoint)) {
			return;
		}
	}
}

void BaseCtrl::onKeyInput(int key, int scancode, int keyState, int mods, const char* key_name)
{
	if (guiCaptured) {
		return;
	}
	KeyEvent event = keyEvent(key, scancode, keyState, mods, key_name);
	if (guiDragged) {
		if (guiDragged->handleKeyInput(event)) {
			return;
		}
		return;
	}
	if (processGlobalKeyevent(event)) {
		return;
	}
	if (guiFocused && guiFocused->handleKeyInput(event)) {
		return;
	}
	if (guiCtrFocused && guiCtrFocused != guiFocused && guiCtrFocused->handleKeyInput(event)) {
		return;
	}
//	if (action == STATE_RELEASE)
//		return;
//
//	if (key == KEY_ESCAPE) {
//		return;
//	}
//	if (key == KEY_SPACE) {
////		window_dialog* dialog = this->mainWindow->createDialog();
////		dialog->show();
//
//
//		return;
//	}
}
void BaseCtrl::prerender(int32_t x, int32_t y, int32_t w, int32_t h, float pixelRatio) {
	for (guictr_base *ctr : containers) {
		ctr->prerender(vg);
	}
}
void BaseCtrl::render(int32_t x, int32_t y, int32_t w, int32_t h, float ratio) {
	static int test = 0;
	nvgBeginFrame(vg, w, h, ratio);
	nvgLineJoin(vg, NVGlineCap::NVG_BEVEL);


	for (guictr_base *ctr : containers) {
		nvgSave(vg);
		ctr->render(vg);
		nvgRestore(vg);
	}
	if (guiDragged) {
		nvgSave(vg);
		guiDragged->renderDragged(vg, this->m_mousePos + dragOffset);
		nvgRestore(vg);
	}
#if RENDER_DBG_BRD
	int colorIdx = 0;
	for (guictr_base *ctr : containers) {
		ctr->renderDebug(vg, dbgcolors[colorIdx++ % 5]);
	}
#endif

//	int lx = 20; int ly = 20; int lw = 300;
//	renderDashedLineFrame(vg, lx, ly, lw, lw, 1.0f);
//	RenderResources::NvgImageTexture& image = RenderResources::imgDashedLine;
//
//
//		nvgBeginPath(vg);
//		nvgRect(vg, 0, 0, 100, 100);
//		nvgFillColor(vg, rgbToNvg(0x333333));
//		nvgFill(vg);
//		NVGpaint paintDown = nvgImagePattern(vg, 0, 0, image.width, image.height, 0, image.id, 1.0f);
//
//		nvgBeginPath(vg);
//		nvgRect(vg, 20, 20, 60, 60);
//		nvgFillPaint(vg, paintDown);
//		nvgFill(vg);

	nvgEndFrame(vg);
	test++;
	if (test > 100) {
		test = 0;
	}
}

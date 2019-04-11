#include "glheaders.h"
#include <nanovg.h>
#include <time.h>
#include <algorithm>
#include <functional>
#include <vector>
#include <memory>
#include <GLFW/glfw3.h>
#include "basectrl.h"
#include "theme.h"
#include "../gui/gui.h"
#include "../gui/guicontainer.h"
#include "../gui/guicontextmenu_base.h"

#include "window.h"
#include "platform.h"

#include "keyboard.h"
#include "mouse.h"
#include "event.h"
#include "commands.h"
#include <assert.h>

#include "project.h"

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
void BaseCtrl::focusGui(guibase* gui) {
	if (guiCaptured != NULL) {
		return;
	}
	guibase* oldFocused = guiFocused;
	guibase* newFocus = gui != NULL ? gui->getFocusedControl() : NULL;
	guiCtrFocused = gui != NULL ? gui->getFocusedContainer() : NULL;
	if (oldFocused != newFocus) {
		MouseHitEvt evt(MouseHitType::MOUSE_LEFT, 0);
		if (oldFocused) {
			oldFocused->focusEvent(evt, false);
		}
		if (newFocus && newFocus->focusEvent(evt, true)) {
			guiFocused = newFocus;
		} else if (!newFocus) {
			guiFocused = nullptr;
		}
	}
}
void BaseCtrl::mouseDown(ivec2 mousePos, int button, bool doubleclick) {
	if (!mouseDownPre()) {
		return;
	}
	if (guiCaptured != NULL) {
		return;
	}
	MouseHitEvt evt = mouseHitEvt(fromButton(button));
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
		} else if (!newFocus) {
			guiFocused = nullptr;
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
		gui->handleMouseDownBegin(evt);
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
	if (guiFocused && guiFocused->handleKeyInput(event)) {
		return;
	}
	if (guiCtrFocused && guiCtrFocused != guiFocused && guiCtrFocused->handleKeyInput(event)) {
		return;
	}
	if (processGlobalKeyevent(event)) {
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
	NVGcolor col = getTheme()->getColor(GuiColor::COL_CLEAR_COLOR);
	glClearColor(col.r, col.g, col.b, col.a);
	glClear(GL_COLOR_BUFFER_BIT);
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
		guiDragged->renderDragged(vg, this->m_mousePos, dragOffset);
		nvgRestore(vg);
	}
#if RENDER_DBG_BRD
	int colorIdx = 0;
	auto renderDebug = [](NVGcontext* vg, guictr_base *ctr, NVGcolor color) {
		nvgBeginPath(vg);
		nvgRect(vg, ctr->pos.x, ctr->pos.y, ctr->size.x, ctr->size.y);
		nvgFillColor(vg, color);
		nvgFill(vg);
		ivec2 posInset = ctr->getPosContent();
		ivec2 sizeInset = ctr->getSizeContent();
		nvgBeginPath(vg);
		nvgRect(vg, posInset.x, posInset.y, sizeInset.x, sizeInset.y);
		nvgFillColor(vg, color);
		nvgFill(vg);
	};
	static NVGcolor dbgcolorsa[5] = {
		nvgRGBA(255, 0, 0, 55),
		nvgRGBA(0, 255, 0, 55),
		nvgRGBA(0, 0, 255, 55),
		nvgRGBA(255, 0, 255, 55),
		nvgRGBA(255, 255, 0, 55)
	};

	for (guictr_base *ctr : containers) {
		renderDebug(vg, ctr, dbgcolorsa[colorIdx++ % 5]);
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
// Only use this pointer for comparison!
void BaseCtrl::onGuiRemoved(void* gui) {
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
void BaseCtrl::resetMouseContext() {
	if (guiCtrFocused) {
		if (!guiCtrFocused->isStaticContainer()) {
			guiCtrFocused = NULL;
		}
	}
	guiCaptured = guiFocused = guiOver = guiDragged = NULL;
}

bool BaseCtrl::captureMouse(guibase* gui) {
	if (guiCaptured == NULL) {
		guiCaptured = gui;
		this->window->captureMouse();
		return true;
	}
	return false;
}
String BaseCtrl::getClipboardText()
{
	return this->window->getClipboardText();
}
void BaseCtrl::openContextMenu(guictxtmenu_base *b, ivec2 pos)
{
	delete b; //TODO: defer delete
}
void BaseCtrl::setClipboardText(String s)
{
	this->window->setClipboardText(s);
}

AppCtrl::AppCtrl() {

}
AppCtrl::~AppCtrl() {
}
void AppCtrl::destroyControl() {
	destroy();
	for (auto gui : garbageGuis) {
		delete gui;
	}
}
void AppCtrl::closeAppMenusAtLvl(int startlvl) {
	for (int i = startlvl; i < (int)menuWindows.size(); i++) {
		auto menuWnd = menuWindows[i];
		if (menuWnd.ctxt) {
			menuWnd.wnd->getCtrl()->closePopup();
		}
	}
}
void AppCtrl::openAppMenu(int lvl, guictxtmenu_base *b, ivec2 pos) {
	if ((int)menuWindows.size() <= lvl) {
		auto newWnd = this->mainWindow->createOverlay();
		menuWindows.push_back({ newWnd, nullptr });
	}
	//TODO: menu change on same level will let his assertation fail
	auto& entry = menuWindows[lvl];
	assert(entry.wnd && !entry.ctxt);
	entry.ctxt = b;
	ivec2 windowPos;
	this->mainWindow->getPos(&windowPos);
	entry.wnd->getCtrl()->open(b, windowPos+pos);
}

void AppCtrl::openOverlayGui(guictxtmenu_base *b, ivec2 pos, int flags) {
	assert(!this->ctxtmenu);
	 //move this in some garbageCollect() methdo and trigger garbage collection after every window-msg on win32 (linux?)
	for (auto gui : garbageGuis) {
		delete gui;
	}
	garbageGuis.clear();
	this->ctxtmenu = b;
	ivec2 windowPos;
	ivec2 windowSize;
	this->mainWindow->getPos(&windowPos);
	this->mainWindow->getSize(&windowSize);
	ivec2 wndPos = windowPos;
	if (flags&1) {
		wndPos = windowPos+pos;
	} else {
		wndPos = windowPos+(windowSize-b->size)/2;
	}
	if (!contextWindow) {
		contextWindow = this->mainWindow->createOverlay();
	}
	if (contextWindow) {
		auto* ctxtWindowTheme = contextWindow->getCtrl()->getTheme();
		//copy theme from this control to contextWindows control
		*ctxtWindowTheme = *getTheme();
		contextWindow->getCtrl()->open(b, wndPos);
	}

}
void AppCtrl::openDialog(guictxtmenu_base *b) {
	openOverlayGui(b, ivec2(0), 0);
}
void AppCtrl::openContextMenu(guictxtmenu_base *b, ivec2 pos) {
	openOverlayGui(b, pos, 1);
}
void AppCtrl::closeContextMenu() {
	if (this->ctxtmenu) {
		assert(contextWindow);
		contextWindow->getCtrl()->closePopup();
	}
}
void AppCtrl::onChildOverlayWindowClose(window_overlay* ptr) {
	if (ptr == this->contextWindow) {
		assert(this->ctxtmenu);
		this->ctxtmenu->onParentWindowClose();
		this->ctxtmenu->setControl(nullptr);
		// ctxtmenu can't be deleted at this point, some point in the call chain may dereference it again
		garbageGuis.push_back(this->ctxtmenu);
		this->ctxtmenu = nullptr;
		return;
	}
	auto it = std::find_if(menuWindows.begin(), menuWindows.end(), [ptr](const auto& entry) {
		return entry.wnd == ptr;
	});
	if (it != menuWindows.end()) {
		auto lvl = it-menuWindows.begin();
		auto& menuWnd = *it;
		assert(menuWnd.ctxt);
		menuWnd.ctxt->onParentWindowClose();
		menuWnd.ctxt->setControl(nullptr);
		// ctxtmenu can't be deleted at this point, some point in the call chain may dereference it again
		garbageGuis.push_back(menuWnd.ctxt);
		menuWnd.ctxt = nullptr;
		return;
	}
	assert(0);
}
bool AppCtrl::hasContextMenu() {
	return this->contextWindow && this->contextWindow->isShown();
}
void AppCtrl::onCharInput(unsigned int codepoint) {
	window_overlay* wnd = this->contextWindow;
	if (wnd && wnd->isShown()) {
		if (wnd->getCtrl()->hasInputFocus()) {
			wnd->getCtrl()->onCharInput(codepoint);
			wnd->requestRedraw();
			return;
		}
	}
	BaseCtrl::onCharInput(codepoint);
}
void AppCtrl::onKeyInput(int key, int scancode, int keyState, int mods, const char* key_name)
{
	window_overlay* wnd = this->contextWindow;
	if (wnd && wnd->isShown()) {
		if (wnd->getCtrl()->hasInputFocus()) {
			wnd->getCtrl()->onKeyInput(key, scancode, keyState, mods, key_name);
			wnd->requestRedraw();
			return;
		}
	}
	BaseCtrl::onKeyInput(key, scancode, keyState, mods, key_name);

}

void AppCtrl::updateMenubar() {
#if WINDOW_HAS_MENUBAR
	menubar.disableAll = this->ctxtmenu != NULL;
#endif
}
void AppCtrl::onMenuOpen(ngui::Menu* menu) {
	updateMenubar();
#if !USE_GUI_MENU
	this->mainWindow->updateMenu();
#endif
}
guictxtmenu_base* AppCtrl::getContextMenu() {
	return this->ctxtmenu;
}
#if WINDOW_HAS_MENUBAR
ngui::MenuBar& AppCtrl::getMenubar() {
	return menubar;
}
#endif

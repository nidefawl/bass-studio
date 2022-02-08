#include "glheaders.h"
#include <nanovg.h>
#include <ctime>
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
#include "../gui/container/guicontainer_dnd_layout.h"
#include "../gui/container/guicontainer_layout_types.h"

#include "window.h"
#include "platform.h"

#include "keyboard.h"
#include "mouse.h"
#include "event.h"
#include "commands.h"
#include "gui/dialog.h"
#include "assert_dbg.h"

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
        modName = modName + "+";
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
    mevt.type         = evtType;
    mevt.guiDragged   = gui;
    mevt.button       = button;
    mevt.mousepos     = mousePos;
    mevt.relMousepos  = toControlsObjectSpace(mousePos, gui);
    mevt.dragStart    = ctrl->dragStart;
    mevt.dragOffset   = ctrl->dragOffset;
    mevt.dragDistance = &ctrl->dragDistance;
    mevt.kbmods       = ctrl->window->getKeyMods();
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
    kevt.keyCode  = key;
    kevt.scancode = scancode;
    kevt.mods     = mods;
    kevt.keyname  = key_name;
    return kevt;
}
ivec2 toControlsObjectSpace(ivec2& pos, guibase* gui) {
    vector<guibase*> guiHierachy;
    gui->getHierachy(guiHierachy);
    ivec2 posOS = pos;
    while (!guiHierachy.empty()) {
        guibase* b = guiHierachy.back();
        guiHierachy.pop_back();
        posOS = b->toContainerSpace(posOS);
    }
    // return posOS - gui->pos;
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
    if (guiCaptured != nullptr) {
        this->window->releaseMouse();
        guiCaptured = nullptr;
    }
    if (guiDragged) {
        //  cursorIcon = CURSOR_DEFAULT;
        //  if (guiDragged!=guiFocused&&guiFocused) {
        //   MouseEvent evt = mouseEvent(this, guiFocused, mousePos, button, M_EVT_BTN_UP);
        //   guiFocused->handleDraggedRelease(evt);
        //  }
        cursorIcon     = CURSOR_DEFAULT;
        MouseEvent evt = mouseEvent(this, guiDragged, mousePos, button, M_EVT_BTN_UP);
        guiDragged->handleDraggedRelease(evt);
        guiDragged = nullptr;
    }
}
MouseHitEvt BaseCtrl::mouseHitEvt(MouseHitType _type) {
    return {_type, window->getKeyMods()};
}
void BaseCtrl::focusGui(guibase* gui) {
    if (guiCaptured != nullptr) {
        return;
    }
    guibase* oldFocused = guiFocused;
    guibase* newFocus   = gui != nullptr ? gui->getFocusedControl() : nullptr;
    guiCtrFocused       = gui != nullptr ? gui->getFocusedContainer() : nullptr;
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
    if (guiCaptured != nullptr) {
        return;
    }
    MouseHitEvt evt = mouseHitEvt(fromButton(button));
    for (guictr_base* ctr : containers) {
        if (ctr->mouseHitTest(mousePos, evt)) {
            break;
        }
    }
    guiOver = evt.getGuiHit();

    guibase* gui        = evt.getGuiHit();
    guibase* oldFocused = guiFocused;
    guibase* newFocus   = gui != nullptr ? gui->getFocusedControl() : nullptr;
    guiCtrFocused       = gui != nullptr ? gui->getFocusedContainer() : nullptr;
    if (oldFocused != newFocus) {
        if (oldFocused) {
            oldFocused->focusEvent(evt, false);
        }
        if (newFocus && newFocus->focusEvent(evt, true)) {
            guiFocused = newFocus;
        } else if (!newFocus) {
            guiFocused = nullptr;
        }
    }
    // if (evt.hasCursorChanged()) {
    cursorIcon = evt.getCursor();
    // }
    if (button == 0) {
        // left button gets focus from mouse move only
        guiDragged = !!(gui) ? gui->getDraggedControl() : nullptr;
    }
    if (gui != nullptr) {
        dragDistance   = ivec2(0);
        dragStart      = mousePos;
        dragOffset     = gui->toScreenSpace(ivec2(0)) - mousePos;

        MouseEvent mouseEvt = mouseEvent(this, gui, mousePos, button, doubleclick ? M_EVT_DOUBLECLICK : M_EVT_BTN_DOWN);
        gui->handleMouseDownBegin(mouseEvt);
    }
}

void BaseCtrl::mouseScrolled(double xoffset, double yoffset) {
    ivec2 mousePos  = this->m_mousePos;
    MouseHitEvt evt = mouseHitEvt(MouseHitType::MOUSE_SCROLL);
    for (guictr_base* ctr : containers) {
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
    if (gui == this->guiCtrFocused) return true;
    guibase* p = this->guiFocused;
    while (p != nullptr) {
        if (p == gui) return true;
        p = p->parent;
    }
    return false;
}

void BaseCtrl::mouseMoved(ivec2 mousePos, ivec2 deltaPos) {
    if (ctxtmenu && !ctxtmenu->isTransient()) {
        return;
    }
    this->m_mousePos = mousePos;
    if (ctxtmenu == nullptr) {
        if (guiCaptured != nullptr) {
            dragDistance += deltaPos;
            MouseEvent evt = mouseEvent(this, guiCaptured, mousePos, -1, M_EVT_CAPTURED_MOVE);
            guiCaptured->handleDraggedMove(evt);
            return;
        }
        if (guiDragged != nullptr) {
            dragDistance += deltaPos;
            MouseEvent evt = mouseEvent(this, guiDragged, mousePos, -1, M_EVT_MOVE);
            guiDragged->handleDraggedMove(evt);
            return;
        }
    }
    MouseHitEvt evt = mouseHitEvt(MouseHitType::MOUSE_OVER);
    for (guictr_base* ctr : containers) {
        if (ctr->mouseHitTest(mousePos, evt)) {
            break;
        }
    }
    // if (evt.hasCursorChanged()) {
    cursorIcon = evt.getCursor();
    // }
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
    if (guiCtrFocused != nullptr && guiCtrFocused != guiFocused) {
        if (guiCtrFocused->handleCharInput(codepoint)) {
            return;
        }
    }
}

void BaseCtrl::onKeyInput(int key, int scancode, int keyState, int mods, const char* key_name) {
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
}
void BaseCtrl::prerender(NVGcontext* nanovgCtxt, int32_t x, int32_t y, int32_t w, int32_t h, float pixelRatio) {
    for (guictr_base* ctr : containers) {
        ctr->prerender(vg);
    }
}
void BaseCtrl::render(NVGcontext* nanovgCtxt, int32_t x, int32_t y, int32_t w, int32_t h, float ratio) {
    NVGcolor col = getTheme()->getColor(GuiColor::COL_CLEAR_COLOR);
    glClearColor(col.r, col.g, col.b, col.a);
    glClear(GL_COLOR_BUFFER_BIT);
    static int test = 0;
    nvgBeginFrame(vg, w, h, ratio);
    nvgScale(vg, m_scale, m_scale);
    nvgLineJoin(vg, NVGlineCap::NVG_BEVEL);


    for (guictr_base* ctr : containers) {
        if (ctr->size == ivec2{0, 0}) {
            log_printf("warning, rendering container with size 0 0\n", 0);
            continue;
        }
        nvgSave(vg);
        ctr->render(vg);
        nvgRestore(vg);
    }
    if (dragDropTargets_ContainerMove.size()) {
        for (std::weak_ptr<i_ctr_drop_area>& weakPtrTarget : dragDropTargets_ContainerMove) {

            if (!weakPtrTarget.expired()) {
                // TODO: I don't even want to lock here, BaseCtrl::render() is considered const, not changing state of objects.
                //  Thus the lifetime of a i_ctr_drop_area is not allowed to end within BaseCtrl::render() or asynchronously on another
                //  thread.
                auto shrdPtrTarget = weakPtrTarget.lock();
                if (shrdPtrTarget.get()) {
                    if (shrdPtrTarget->size == ivec2{0, 0}) {
                        log_printf("warning, rendering container with size 0 0\n", 0);
                        continue;
                    }
                    if (shrdPtrTarget->contains(ctrDragHandler.pos)) {
                        nvgSave(vg);
                        shrdPtrTarget->render(vg);
                        nvgRestore(vg);
                        break;
                    }
                }
            }
        }
    }
    if (guiDragged) {
        if (guiDragged->size == ivec2{0, 0}) {
            log_printf("warning, rendering container with size 0 0\n", 0);
        } else {
            nvgSave(vg);
            guiDragged->renderDragged(vg, this->m_mousePos, dragOffset);
            nvgRestore(vg);
        }
    }
#if RENDER_DBG_BRD
    int colorIdx     = 0;
    auto renderDebug = [](NVGcontext* vg, guictr_base* ctr, NVGcolor color) {
        nvgBeginPath(vg);
        nvgRect(vg, ctr->pos.x, ctr->pos.y, ctr->size.x, ctr->size.y);
        nvgFillColor(vg, color);
        nvgFill(vg);
        ivec2 posInset  = ctr->getPosContent();
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

    for (guictr_base* ctr : containers) {
        renderDebug(vg, ctr, dbgcolorsa[colorIdx++ % 5]);
    }
#endif

//    int lx = 20;
//    int ly = 20;
//    int lw = 300;
//    renderDashedLineFrame(vg, lx, ly, lw, lw, 1.0f);
//    RenderResources::NvgImageTexture& image = RenderResources::imgDashedLine;
//
//
//    nvgBeginPath(vg);
//    nvgRect(vg, 0, 0, 100, 100);
//    nvgFillColor(vg, rgbToNvg(0x333333));
//    nvgFill(vg);
//    NVGpaint paintDown = nvgImagePattern(vg, 0, 0, image.width, image.height, 0, image.id, 1.0f);
//
//    nvgBeginPath(vg);
//    nvgRect(vg, 20, 20, 60, 60);
//    nvgFillPaint(vg, paintDown);
//    nvgFill(vg);

    nvgEndFrame(vg);
    test++;
    if (test > 100) {
        test = 0;
    }
}
void BaseCtrl::onGuiRemoved(void* gui) {
    // Only use gui pointer for comparison!
    if (this->guiOver == gui) {
        this->guiOver = nullptr;
    }
    if (this->guiCaptured == gui) {
        this->guiCaptured = nullptr;
    }
    if (this->guiFocused == gui) {
        this->guiFocused = nullptr;
    }
    if (this->guiDragged == gui) {
        this->guiDragged = nullptr;
    }
    if (this->guiCtrFocused == gui) {
        this->guiCtrFocused = nullptr;
    }
}
void BaseCtrl::resetMouseContext() {
    if (guiCtrFocused) {
        if (!guiCtrFocused->isStaticContainer()) {
            guiCtrFocused = nullptr;
        }
    }
    guiCaptured = guiFocused = guiOver = guiDragged = nullptr;
}

bool BaseCtrl::captureMouse(guibase* gui) {
    if (guiCaptured == nullptr) {
        guiCaptured = gui;
        this->window->captureMouse();
        return true;
    }
    return false;
}
String BaseCtrl::getClipboardText() {
    return this->window->getClipboardText();
}
void BaseCtrl::openContextMenu(guictxtmenu_base* b, ivec2 pos, int flags) {
    delete b; // TODO: defer delete
}
void BaseCtrl::closeDialogs() {
}
void BaseCtrl::closeAllContextMenus() {
    if (!this->ctxtmenu || !this->ctxtmenu->isDialog()) {
        closeContextMenu();
    }
    closeAllAppMenus();
}
void BaseCtrl::setClipboardText(String s) {
    this->window->setClipboardText(s);
}

void AppCtrl::onAppTick() {
    getTheme()->updateAnimation();
    onTick();

    // move this in some garbageCollect() methdo and trigger garbage collection after every window-msg on win32 (linux?)
    for (auto gui : garbageGuis) {
        delete gui;
    }
    garbageGuis.clear();
}
void AppCtrl::destroyControl() {
    closeAllContextMenus();
    contextWindow = nullptr;
    dbgassert(!this->ctxtmenu);
    for (auto gui : garbageGuis) {
        delete gui;
    }
    garbageGuis.clear();
    dbgassert(isOk());
    destroy();
}
void AppCtrl::closeAppMenusAtLvl(int startlvl) {
    for (int i = startlvl; i < (int)menuWindows.size(); i++) {
        auto menuWnd = menuWindows[i];
        if (menuWnd.ctxt) {
            menuWnd.wnd->getCtrl()->closePopup();
        }
    }
}
void AppCtrl::openAppMenu(int lvl, guictxtmenu_base* b, ivec2 pos) {
    while (menuWindows.size() <= lvl) {
        menuWindows.push_back({nullptr, nullptr});
    }
    if (!menuWindows[lvl].wnd) {
        int createflags = 0;
        createflags |= WINDOW_BORDERLESS_POPUP;
        menuWindows[lvl].wnd = this->mainWindow->createOverlay(std::make_shared<PopupCtrl>(), createflags);
    }
    // TODO: menu change on same level will let this assertion fail
    auto& entry = menuWindows[lvl];
    dbgassert(entry.wnd && !entry.ctxt);
    entry.ctxt = b;
    ivec2 windowPos;
    this->mainWindow->getPos(&windowPos);
    entry.wnd->getCtrl()->m_scale = m_scale;
    ivec2 childMenuPos            = pos;
    // TODO: this OS specific handling should be abstracted away into window.cpp
#ifdef _WIN32
#endif
    childMenuPos += windowPos;
    static_cast<PopupCtrl*>(entry.wnd->getCtrl())->open(b, childMenuPos, false);
}
namespace {
    template <typename T>
    void determineWindowPos(T* b, window_main* mainWindow, float m_scale, int flags, ivec2 pos, ivec2& wndPos) {
        ivec2 windowPos;
        ivec2 windowSize;
        mainWindow->getPos(&windowPos);
        mainWindow->getSize(&windowSize);
        wndPos = windowPos;
        if (flags & BASECTRL_WND_POS_ABSOLUTE) {
            wndPos = pos;
        } else if (flags & BASECTRL_WND_POS_RELATIVE) {
            wndPos = windowPos + ivec2(pos.x * m_scale, pos.y * m_scale);
        } else {
            wndPos = windowPos + (windowSize - b->size) / 2;
        }
    }
} // namespace
void AppCtrl::openOverlayGui(guictxtmenu_base* b, ivec2 pos, int flags) {
    if (!(flags & BASECTRL_OVERLAY_TYPE_CONTEXTMENU)) {
        dbgassert(0);
        return;
    }
    if (this->ctxtmenu) {
        closeContextMenu();
    }

    b->setFontSize(getTheme()->getFloat(GuiConstant::CONST_FONT_SIZE_CONTEXT_MENU));

    dbgassert(!this->ctxtmenu);
    this->ctxtmenu = b;

    ivec2 wndPos(0);
    determineWindowPos(b, mainWindow, m_scale, flags, pos, wndPos);

    int createflags         = WINDOW_BORDERLESS_POPUP;
    window_main* ctxtWindow = this->contextWindow;
    if (!ctxtWindow || ctxtWindow->getCreationFlags() != createflags) {
        if (ctxtWindow) {
            this->mainWindow->closeOverlay(ctxtWindow);
        }
        dbgassert(!this->contextWindow);
        ctxtWindow = this->mainWindow->createOverlay(std::make_shared<PopupCtrl>(), createflags);
    }
    this->contextWindow = ctxtWindow;
    if (ctxtWindow) {
        auto* ctxtWindowTheme = ctxtWindow->getCtrl()->getTheme();
        // copy theme from this control to contextWindows control
        *ctxtWindowTheme               = *getTheme();
        ctxtWindow->getCtrl()->m_scale = m_scale;
        static_cast<PopupCtrl*>(ctxtWindow->getCtrl())->open(b, wndPos, false);
    } else {
        dbgassert(0);
    }
}
void AppCtrl::openDialog(guidialog_base* _guidialog) {
    if (this->dialog) {
        return;
    }
    this->dialog = _guidialog;
    ivec2 wndPos(0);
    determineWindowPos(_guidialog, mainWindow, m_scale, 0, ivec2(0), wndPos);
    window_main* dialogWindow = this->mainWindow->createOverlay(std::make_shared<PopupCtrl>(), WINDOW_IS_DIALOG | WINDOW_IS_RESIZABLE);

    auto* ctxtWindowTheme = dialogWindow->getCtrl()->getTheme();
    // copy theme from this control to contextWindows control
    *ctxtWindowTheme                 = *getTheme();
    dialogWindow->getCtrl()->m_scale = m_scale;
    log_printf("open dialogWindow\n", 0);
    static_cast<PopupCtrl*>(dialogWindow->getCtrl())
            ->open(_guidialog, wndPos, (dialogWindow->getCreationFlags() & WINDOW_IS_RESIZABLE)); // ugly cast
}
void AppCtrl::openContextMenu(guictxtmenu_base* b, ivec2 pos, int flags) {
    // log_printf("open ctxtmenu_base %s\n", StringAsCStr(b->getLabel()));
    openOverlayGui(b, pos, flags | BASECTRL_OVERLAY_TYPE_CONTEXTMENU);
}
void AppCtrl::closeContextMenu() {
    if (this->ctxtmenu) {
        dbgassert(contextWindow);
        contextWindow->getCtrl()->closePopup();
    }
}
void AppCtrl::closeDialogs() {
    if (this->dialog) {
        this->dialog->closeContextMenu();
    }
}
void AppCtrl::onChildOverlayWindowClose(window_main* ptr) {

    if (ptr == this->contextWindow) {
        if (this->ctxtmenu) {
            this->ctxtmenu->onParentWindowClose();
            this->ctxtmenu->setControl(nullptr);
            /**
             * ctxtmenu can't be deleted at this point.
             * somewhere in the call chain it might get dereferenced again.
             */
            /* garbage collect context menu in onTick handler */
            garbageGuis.push_back(this->ctxtmenu);
            this->ctxtmenu = nullptr;
        }
        return;
    }
    auto it = std::find_if(menuWindows.begin(), menuWindows.end(), [ptr](const auto& entry) { return entry.wnd == ptr; });
    if (it != menuWindows.end()) {
        auto& menuWnd = *it;
        dbgassert(menuWnd.ctxt);
        menuWnd.ctxt->onParentWindowClose();
        menuWnd.ctxt->setControl(nullptr);
        /* garbage collect menu in onTick handler */
        garbageGuis.push_back(menuWnd.ctxt);
        menuWnd.ctxt = nullptr;
        return;
    }
    dbgassert(this->dialog && (ptr->getCreationFlags() & WINDOW_IS_DIALOG));
    this->dialog->onParentWindowClose();
    this->dialog->setControl(nullptr);
    garbageGuis.push_back(this->dialog);
    this->dialog = nullptr;
}

bool AppCtrl::hasContextMenu() {
    return this->contextWindow && this->contextWindow->isShown();
}
void AppCtrl::onCharInput(unsigned int codepoint) {
    window_main* wnd = this->contextWindow;
    if (wnd && wnd->isShown()) {
        if (wnd->getCtrl()->hasInputFocus()) {
            wnd->getCtrl()->onCharInput(codepoint);
            wnd->requestRedraw();
            return;
        }
    }
    BaseCtrl::onCharInput(codepoint);
}
void AppCtrl::onKeyInput(int key, int scancode, int keyState, int mods, const char* key_name) {
    window_main* wnd = this->contextWindow;
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
    menubar.disableAll = this->ctxtmenu != nullptr;
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

void BaseCtrl::relayout() {
    relayout(m_size.x * 1.0 / m_scale, m_size.y * 1.0 / m_scale);
};
void BaseCtrl::relayout(int32_t w, int32_t h){};
void BaseCtrl::windowSizeChanged(int32_t w, int32_t h) {
    m_size = ivec2(w, h);
    relayout(m_size.x * 1.0 / m_scale, m_size.y * 1.0 / m_scale);
}

void BaseCtrl::objectDragMove(guibase* g, MouseEvent& mevt) {
    MouseHitEvt evt = mouseHitEvt(MouseHitType::MOUSE_DRAGDROP_OBJECT);
    evt.setDraggedThing(g);
    for (guictr_base* ctr : containers) {
        if (ctr->mouseHitTest(mevt.mousepos, evt)) {
            break;
        }
    }
    guibase* gui = evt.getGuiHit();
    if (gui) {
        g->dragMoveOn(gui, mevt.mousepos);
    } else {
    }
}
void BaseCtrl::objectDragRelease(guibase* g, MouseEvent& mevt) {
    MouseHitEvt evt = mouseHitEvt(MouseHitType::MOUSE_DRAGDROP_OBJECT);
    evt.setDraggedThing(g);
    for (guictr_base* ctr : containers) {
        if (ctr->mouseHitTest(mevt.mousepos, evt)) {
            break;
        }
    }
    guibase* gui = evt.getGuiHit();
    if (gui) {
        g->dragReleaseOn(gui, mevt.mousepos);
    }
}
void BaseCtrl::dragContainerBegin(MouseEvent& evt, guictr_layout_entry* ctrDragSrc) {
    dbgassert(!ctrContent.get());
    // get a shared pointer reference to ctrDragSrc, this is a bit awkward, as are all interfaces using shared_ptr
    // stores the reference to ctrDragSrc in shared_ptr ctrContent.
    // extends the lifetime of that container so we can safely access it in render and mouse move callbacks.
    if (ctrDragSrc->getContainerRef(ctrContent, false)) {
        dbgassert(ctrContent.get());
        auto* szLabel1 = StringAsCStr(ctrContent->getGui()->label);
        log_printf("dragContainerBegin %s\n", szLabel1);
        //            ctrDragHandler.pos = ctrContent->getGui()->pos;
        auto vecSizeScaled  = vec2(ctrContent->getGui()->size) * 0.3f;
        ctrDragHandler.size = math::maxvec2(ivec2(32, 12), vecSizeScaled);
        ctrDragHandler.setLabel("Move " + ctrDragSrc->getGui()->label);
        setDragged(&ctrDragHandler);
        //            ctrDragHandler.validPreview = false;
        dragContainerMove(evt);
        dragContainerRelayout(drag_ctr_event{drag_ctr_event_type::DRAG_BEGIN});
    }
}
void BaseCtrl::dragContainerMove(MouseEvent& evt) {
    dragContainerRelayout(drag_ctr_event{drag_ctr_event_type::DRAG_MOVE});
    std::vector<i_ctr_layout*> list                     = getContainers();
    std::vector<std::weak_ptr<i_ctr_drop_area>> targets = getTargets(evt, list);
    dragDropTargets_ContainerMove                       = targets;
    ctrDragHandler.pos                                  = evt.mousepos;
}
void BaseCtrl::dragContainerRelease(MouseEvent& evt) {
    bool hasRemovedContainer = false;
    bool hasPlacedContainer  = false;
    i_ctr_drop_area* area    = determineDropCtrArea(evt);
    if (area && ctrContent) {
        auto* szLabel1 = StringAsCStr(ctrContent->getGui()->label);
        auto layoutCtr = dynamic_cast<guictr_layout*>(area->getLayoutCtr());
        auto* szLabel2 = StringAsCStr(layoutCtr->label);

        dock_pos dockPos                  = area->getDockPos();
        container_layout ctrLayout        = layoutCtr->getLayout();
        container_layout updatedCtrLayout = dock_pos_to_container_layout(dockPos);
        if (area->childContainerIndex > -1 || (ctrLayout != updatedCtrLayout && ctrLayout != container_layout::SOLE)) {

            auto newContainer = std::make_shared<guictr_layout>();
            newContainer->setLayout(updatedCtrLayout);
            log_printf("replace guictr_layout container\n", 0);
            if (updatedCtrLayout == container_layout::TABBED) {
                auto& ctrEntries = layoutCtr->getEntries();
                dbgassert(area->dockPosOffset >= 0 && area->dockPosOffset <= ctrEntries.size());
                auto entryToReplace            = ctrEntries[math::min<int32_t>(ctrEntries.size() - 1, area->dockPosOffset)];
                auto oldEntryThatIsNowTabEntry = layoutCtr->replaceContainerWith(entryToReplace->getGui(), newContainer);

                newContainer->placeContainer(ctrContent, area);
                newContainer->placeContainer(oldEntryThatIsNowTabEntry, area);

            } else { // SPLIT_V or SPLIT_H
                auto newDockPos = dock_pos::NONE;

                switch (area->dockPos) {
                    case dock_pos::LEFT:
                        newDockPos = dock_pos::RIGHT;
                        break;
                    case dock_pos::RIGHT:
                        newDockPos = dock_pos::LEFT;
                        break;
                    case dock_pos::TOP:
                        newDockPos = dock_pos::BOTTOM;
                        break;
                    case dock_pos::BOTTOM:
                        newDockPos = dock_pos::TOP;
                        break;
                    default:
                        dbgassert(0);
                        break;
                }
                auto containerToReplace =
                        area->childContainerIndex > -1 ? layoutCtr->getEntries()[area->childContainerIndex]->getGui() : layoutCtr;
                auto parentLayoutCtr = dynamic_cast<guictr_layout*>(containerToReplace->parent);
                if (!parentLayoutCtr) {
                    log_printf("replace top level container\n", 0);
                    std::shared_ptr<guictr_layout> prevCtr = this->replaceContainerWith(containerToReplace, newContainer);
                    if (prevCtr) {
                        std::shared_ptr<guictr_layout_entry> entry1 = createGuiCtrLayoutEntry(prevCtr);
                        newContainer->placeContainer(ctrContent, area);
                        area->dockPos = newDockPos;
                        newContainer->placeContainer(entry1, area);
                    }

                } else {
                    auto layoutCtrEntry = parentLayoutCtr->replaceContainerWith(containerToReplace, newContainer);

                    newContainer->placeContainer(ctrContent, area);
                    area->dockPos = newDockPos;
                    newContainer->placeContainer(layoutCtrEntry, area);
                }
            }
            log_printf("cannot directly insert into %s. need to change layout from %d to %d first\n", szLabel2, ctrLayout,
                       updatedCtrLayout);

        } else {
            hasRemovedContainer = true;
            hasPlacedContainer  = area->getLayoutCtr()->placeContainer(ctrContent, area);
            log_printf("attempt to place container %s on %s result %d\n", szLabel1, szLabel2, hasPlacedContainer);
        }
    }
    if (hasRemovedContainer && !hasPlacedContainer) {
        log_printf("Container was removed from its parent could not be placed on target. Container is now dangling %s\n",
                   StringAsCStr(ctrContent->getGui()->label));
    }
    // end the extension of the dragged containers lifetime
    ctrContent                  = nullptr;
    ctrDragHandler.validPreview = false;
    dragDropTargets_ContainerMove.clear();
    dragContainerRelayout(drag_ctr_event{drag_ctr_event_type::DRAG_END});
}
std::vector<std::weak_ptr<i_ctr_drop_area>> BaseCtrl::getTargets(MouseEvent& mevt, std::vector<i_ctr_layout*> ifMatches) {
    std::vector<std::weak_ptr<i_ctr_drop_area>> targets;
    for (i_ctr_layout* ctr : ifMatches) {
        std::vector<std::weak_ptr<i_ctr_drop_area>> ctrtargets;
        ctr->getOverlays(mevt, ctrtargets);
        for (auto& target : ctrtargets) {
            if (!target.expired()) {
                // TODO: no need to lock here...
                auto shrdPtrTarget = target.lock();

                if (shrdPtrTarget->pos.x > m_size.x * 1.0 / m_scale - 10) {
                    //shrdPtrTarget->pos.x -= 10;
                }
                if (shrdPtrTarget->pos.x + shrdPtrTarget->size.x < 10) {
                    //shrdPtrTarget->pos.x += 10;
                }
            }
        }
        //      log_printf("ctrtargets %X %d\n", reinterpret_cast<int64_t>(ctr), ctrtargets.size());
        targets.insert(targets.begin(), ctrtargets.begin(), ctrtargets.end());
    }
    return targets;
}
std::vector<i_ctr_layout*> BaseCtrl::getContainers() {
    MouseHitEvt evtDragObj = mouseHitEvt(MouseHitType::MOUSE_DRAGDROP_OBJECT);
    evtDragObj.setDraggedThing(nullptr);
    evtDragObj.requestFocus(nullptr);
    std::vector<i_ctr_layout*> ifMatches;
    std::deque<guictr_base*> stack;
    std::vector<guictr_base*> ctrMatches;
    dbgassert(stack.empty());
    stack.insert(stack.begin(), containers.begin(), containers.end());
    while (!stack.empty()) {
        guictr_base* current = stack.front();
        stack.pop_front();
        if (current->guis.size()) {
            ctrMatches.clear();
            for (auto* tChildTest : current->guis) {
                // TODO: check for visibility, or redesing guiyctr_layout in tabbed mode to not have inactive containers in its guis list
                if (tChildTest->isVisible()) {
                    guictr_base* ctrMatch = dynamic_cast<guictr_base*>(tChildTest);
                    if (ctrMatch) {
                        ctrMatches.push_back(ctrMatch);
                    }
                }
            }
            if (ctrMatches.size()) {
                stack.insert(stack.begin(), ctrMatches.begin(), ctrMatches.end());
            }
        }
        i_ctr_layout* ifMatch = dynamic_cast<i_ctr_layout*>(current);
        if (ifMatch) {
            ifMatches.push_back(ifMatch);
        }
    }
    //  log_printf("matched %d instances\n", ifMatches.size());

    return ifMatches;
}

void guictr_dragged_container_instance::handleDraggedMove(MouseEvent& evt) {
    parentCtrl->dragContainerMove(evt);
}

void guictr_dragged_container_instance::handleDraggedRelease(MouseEvent& evt) {
    parentCtrl->dragContainerRelease(evt);
}

void guictr_dragged_container_instance::renderDragged(NVGcontext* vg, ivec2 mousepos, ivec2 dragOffset) {
    // pos = dummyPreview.pos;
    // size = dummyPreview.size;
    // nvgSave(vg);
    // render(vg);
    // nvgRestore(vg);
    renderContainerLabel(vg);
    if (isDragRendered() && validPreview) {
        nvgLineCap(vg, NVGlineCap::NVG_ROUND);
        //  for (ivec4& box : boxes) {
        //   nvgBeginPath(vg);
        //   nvgRect(vg, box.x, box.y, box.z, box.w);
        //   nvgStrokeColor(vg, G_MOVE_HIGHLIGHT);
        //   nvgStrokeWidth(vg, 4.0);
        //   nvgStroke(vg);
        //  }
        nvgLineCap(vg, NVGlineCap::NVG_BUTT);
    }
}

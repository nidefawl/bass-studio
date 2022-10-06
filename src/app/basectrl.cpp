#include "appconfig.h"
#include "glheaders.h"
#include <cstddef>
#include <nanovg.h>
#include <ctime>
#include <algorithm>
#include <functional>
#include <vector>
#include <memory>
#include <GLFW/glfw3.h>
#include "assert_dbg.h"
#include "basectrl.h"
#include "commands.h"
#include "event.h"
#include "gui/container/container_dnd_layout.h"
#include "gui/container/container_layout_types.h"
#include "gui/container/container.h"
#include "gui/contextmenu/contextmenu_base.h"
#include "gui/dialog/dialog.h"
#include "gui/gui.h"
#include "keyboard.h"
#include "logging.h"
#include "math/seq_math.h"
#include "mouse.h"
#include "platform.h"
#include "project.h"
#include "str_util.h"
#include "theme.h"
#include "tls.h"
#include "types.h"
#include "window.h"

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
String GetMenuNameWithKeybind(const String& s, const KeyCombo* combo) {
    if (!combo || combo->keyCode == KeyboardKey::DAW_KB_INVALID) {
        return s;
    }
    return s + "\t" + combo->toString();
}
MouseEvent mouseEvent(BaseCtrl* ctrl, guibase* gui, ivec2 mousePos, int button, KeyboardMods kbmods, MouseEventType evtType) {
    MouseEvent mevt;
    mevt.type         = evtType;
    mevt.guiDragged   = gui;
    mevt.button       = button;
    mevt.mousepos     = mousePos;
    mevt.relMousepos  = toControlsObjectSpace(mousePos, gui);
    mevt.dragStart    = ctrl->dragStart;
    mevt.dragOffset   = ctrl->dragOffset;
    mevt.dragDistance = &ctrl->dragDistance;
    dbgassert(kbmods >= 0);
    mevt.kbmods       = static_cast<KeyboardMods>(kbmods);
    return mevt;
}

KeyEvent keyEvent(int key, int scancode, int keyState, int mods, const char* key_name) {
    KeyEvent kevt;
    kevt.type = static_cast<KeyboardState>(keyState);
    kevt.keyCode  = static_cast<KeyboardKey>(key);
    kevt.scancode = scancode;
    kevt.mods     = static_cast<KeyboardMods>(mods);
    kevt.keyname  = key_name;
    return kevt;
}
ivec2 toControlsObjectSpace(ivec2 pos, guibase* gui) {
    std::vector<guibase*> guiHierachy;
    gui->getHierachy(guiHierachy);
    while (!guiHierachy.empty()) {
        guibase* b = guiHierachy.back();
        guiHierachy.pop_back();
        pos = b->toContainerSpace(pos);
    }
    return gui->toContainerSpace(pos);
}
namespace {
void processScrollEvt(BaseCtrl* ctrl, guibase* gui, ivec2 mousePos, double xoffset, double yoffset, KeyboardMods kbmods) {
    MouseEvent evt = mouseEvent(ctrl, gui, mousePos, -1, kbmods, M_EVT_SCROLL);
    if (!gui->handleMouseScroll(evt, xoffset, yoffset)) {
        if (gui->parent) {
            processScrollEvt(ctrl, gui->parent, mousePos, xoffset, yoffset, kbmods);
        }
    }
}
}

BaseCtrl::BaseCtrl(AppCtrl* parent)
    : parentCtrl(parent) {
    if (parent) {
        this->commands = parent->commands;
    } else if (daw_tls::isTlsInitialized()) {
        this->commands = daw_tls::getTls().commandManager;
    }
    themes.parent = this;
    ctrDragHandler.setControl(this);
    ctrDragHandler.setFlag(FLG_RENDER_LABEL, true);
}


BaseCtrl::~BaseCtrl() {
}

void BaseCtrl::mouseUp(ivec2 mousePos, int button, KeyboardMods kbmods) {
    if (guiCaptured != nullptr) {
        this->window->releaseMouse();
        guiCaptured = nullptr;
    }
    if (guiDragged) {
        cursorIcon     = CURSOR_DEFAULT;
        lastMouseEvent = mouseEvent(this, guiDragged, mousePos, button, kbmods, M_EVT_BTN_UP);
        guiDragged->handleDraggedRelease(lastMouseEvent);
        guiDragged = nullptr;
    }
}
MouseHitEvt BaseCtrl::mouseHitEvt(MouseHitType _type, KeyboardMods kbmods) {
    return { _type, static_cast<KeyboardMods>(kbmods) };
}
void BaseCtrl::focusGui(guibase* gui) {
    if (guiCaptured != nullptr) {
        return;
    }
    guibase* oldFocused = guiFocused;
    guibase* newFocus   = gui != nullptr ? gui->getFocusedControl() : nullptr;
    guiCtrFocused       = gui != nullptr ? gui->getFocusedContainer() : nullptr;
    if (oldFocused != newFocus) {
        MouseHitEvt evt(MouseHitType::MOUSE_LEFT, KeyboardMods::KB_MODS_NONE);
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
void BaseCtrl::mouseDown(ivec2 mousePos, int button, KeyboardMods kbmods, bool doubleclick) {
    if (!mouseDownPre()) {
        return;
    }
    if (guiCaptured != nullptr) {
        return;
    }
    MouseHitEvt evt = mouseHitEvt(fromButton(button), kbmods);
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
        dragDistance = ivec2(0);
        dragStart    = mousePos;
        dragOffset   = gui->toScreenSpace(ivec2(0)) - mousePos;

        lastMouseEvent = mouseEvent(this, gui, mousePos, button, evt.kbmods, doubleclick ? M_EVT_DOUBLECLICK : M_EVT_BTN_DOWN);
        gui->handleMouseDownBegin(lastMouseEvent);
    }
}

void BaseCtrl::mouseScrolled(double xoffset, double yoffset, KeyboardMods kbmods) {
    ivec2 mousePos  = this->m_mousePos;
    MouseHitEvt evt = mouseHitEvt(MouseHitType::MOUSE_SCROLL, kbmods);
    for (guictr_base* ctr : containers) {
        if (ctr->mouseHitTest(mousePos, evt)) {
            break;
        }
    }
    guibase* gui = evt.getGuiHit();
    if (gui) {
        processScrollEvt(this, gui, mousePos, xoffset, yoffset, evt.kbmods);
    }
}

bool BaseCtrl::isCtrOrChildFocused(const guibase* gui) const {
    if (gui == this->guiCtrFocused) return true;
    guibase* p = this->guiFocused;
    while (p != nullptr) {
        if (p == gui) return true;
        p = p->parent;
    }
    return false;
}

void BaseCtrl::mouseMoved(ivec2 mousePos, ivec2 deltaPos, KeyboardMods kbmods) {
    if (ctxtmenu && !ctxtmenu->isTransient()) {
        return;
    }
    this->m_mousePos = mousePos;
    MouseHitEvt evt = mouseHitEvt(MouseHitType::MOUSE_OVER, kbmods);
    for (guictr_base* ctr : containers) {
        if (ctr->mouseHitTest(mousePos, evt)) {
            break;
        }
    }
    // if (evt.hasCursorChanged()) {
    cursorIcon = evt.getCursor();
    // }
    if (!window->isMouseCaptured()) {
        guiOver = evt.getGuiHit();
    }
    if (ctxtmenu == nullptr) {
        if (guiCaptured != nullptr) {
            dragDistance += deltaPos;
            lastMouseEvent = mouseEvent(this, guiCaptured, mousePos, -1, kbmods, M_EVT_CAPTURED_MOVE);
            guiCaptured->handleDraggedMove(lastMouseEvent);
            return;
        }
        if (guiDragged != nullptr) {
            dragDistance += deltaPos;
            lastMouseEvent = mouseEvent(this, guiDragged, mousePos, -1, kbmods, M_EVT_MOVE);
            guiDragged->handleDraggedMove(lastMouseEvent);
            return;
        }
    }
}

bool BaseCtrl::onCharInput(uint32_t codepoint) {
    if (guiCaptured) {
        return false;
    }
    if (guiFocused && guiFocused->handleCharInput(codepoint)) {
        return true;
    }
    if (guiCtrFocused && guiCtrFocused != guiFocused && guiCtrFocused->handleCharInput(codepoint)) {
        return true;
    }
    if (guiCtrFocused != nullptr && guiCtrFocused != guiFocused) {
        if (guiCtrFocused->handleCharInput(codepoint)) {
            return true;
        }
    }
    if (parentCtrl && parentCtrl->onCharInput(codepoint)) {
        return true;
    }
    return false;
}

bool BaseCtrl::onKeyInput(int key, int scancode, int keyState, int mods, const char* key_name) {
    using namespace DAW::UI;
    if (guiCaptured) {
        return false;
    }
    KeyEvent event = keyEvent(key, scancode, keyState, mods, key_name);
    auto boundCommand = commands->matchKeyCombo(event);
    CommandContext commandContext;
    if (boundCommand) {
        event.cmd = boundCommand;
        commandContext = boundCommand->getContext();
    }
    if (guiDragged && commandContext.matchesFocusedGui(guiDragged) && guiDragged->handleKeyInput(event)) {
        return true;
    }
    if (guiFocused && commandContext.matchesFocusedGui(guiFocused) && guiFocused->handleKeyInput(event)) {
        return true;
    }
    if (guiCtrFocused && guiCtrFocused != guiFocused && commandContext.matchesFocusedGui(guiCtrFocused) && guiCtrFocused->handleKeyInput(event)) {
        return true;
    }
    if (commandContext.ctxtType == CommandContextType::CMD_CTXT_GLOBAL && processGlobalKeyevent(event)) {
        return true;
    }
    if (parentCtrl && parentCtrl->onKeyInput(key, scancode, keyState, mods, key_name)) {
        return true;
    }
    return false;
}
void BaseCtrl::prerender(NVGcontext* nanovgCtxt, int32_t x, int32_t y, int32_t w, int32_t h, float pixelRatio) {
    for (guictr_base* ctr : containers) {
        ctr->prerender(vg);
    }
}
void BaseCtrl::render(NVGcontext* nanovgCtxt, int32_t x, int32_t y, int32_t w, int32_t h, float ratio) {
    NVGcolor col = getTheme()->getColor(GuiColor::COL_CLEAR_COLOR);
    glClearColor(col.r, col.g, col.b, col.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    nvgBeginFrame(vg, w, h, ratio);
    nvgScale(vg, m_scale, m_scale);
    nvgLineJoin(vg, NVGlineCap::NVG_BEVEL);


    for (guictr_base* ctr : containers) {
        if (ctr->size == ivec2{ 0, 0 }) {
            log_lf(Log::L_WARN, "warning, rendering container with size 0 0\n");
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
                    if (shrdPtrTarget->size == ivec2{ 0, 0 }) {
                        log_lf(Log::L_WARN, "warning, rendering container with size 0 0\n");
                        continue;
                    }
                    bool bContains = shrdPtrTarget->contains(ctrDragHandler.pos);
                    if (bContains || shrdPtrTarget->isAlwaysShow()) {
                        nvgSave(vg);
                        shrdPtrTarget->render(vg);
                        nvgRestore(vg);
                    }
                    if (bContains) {
                        break;
                    }
                }
            }
        }
    }
    if (guiDragged) {
        if (guiDragged->size == ivec2{ 0, 0 }) {
            log_lf(Log::L_WARN, "warning, rendering container with size 0 0\n");
        } else {
            nvgSave(vg);
            guiDragged->renderDragged(vg, this->m_mousePos, dragOffset);
            nvgRestore(vg);
        }
    }

#define RENDER_DBG_BRD 0
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

    for (guictr_base* ctr : containers) {
        renderDebug(vg, ctr, dbgcolorsArray[colorIdx++ % 8]);
    }
#endif

    nvgEndFrame(vg);
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
void BaseCtrl::openContextMenu(guictxtmenu_base* b, ivec2 pos) {
    delete b;// TODO: defer delete
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

AppCtrl::AppCtrl(AppCtrl* parent)
    : BaseCtrl(parent) {
}

void AppCtrl::onAppTick() {
    getTheme()->updateAnimation();
    onTick();
    if (!guiDragged && !guiCaptured && guiOver && (!this->ctxtmenu || ctxtmenu->isTransient())) {
        auto hoverTime = tmLastHoveredTooltip;
        if (ctxtmenu && ctxtmenu->isTransient() && (lastTooltipSrc && guiOver && guiOver != lastTooltipSrc)) {
            closeContextMenu();
        }
        if (ctxtmenu && !ctxtmenu->isTransient()) {
            hoverTime = 0;
        }
        if (!ctxtmenu && bIsVisible) {
            auto timeNow = getTimeMillis();
            if (guiOver == lastHoveredTooltip && timeNow - tmLastHoveredTooltip >= 360) {
                auto newContextMenu = guiOver->getTooltip(this);
                if (newContextMenu) {
                    newContextMenu->theme = getTheme();
                    lastTooltipSrc        = guiOver;
                    nextTooltipId++;
                    openOverlayGui(newContextMenu, m_mousePos + ivec2(-16, 26), WINDOW_POS_RELATIVE | WINDOW_IS_TOOLTIP | WINDOW_IS_BORDERLESS);
                }
                hoverTime = 0;
            } else if (guiOver != lastHoveredTooltip) {
                hoverTime = timeNow;
            }
        }
        tmLastHoveredTooltip = hoverTime;
        lastHoveredTooltip   = guiOver;
    } else {
        if (ctxtmenu && ctxtmenu->isTransient()) {
            closeContextMenu();
        }
    }
    /* run deferred delete of contextmenus */
    releaseGarbageGuis();
}
void AppCtrl::releaseGarbageGuis() {
    for (auto gui : garbageGuis) {
        delete gui;
    }
    garbageGuis.clear();
}
void AppCtrl::destroyControl() {
    dbgassert(!this->ctxtmenu);
    releaseGarbageGuis();
    dbgassert(garbageGuis.empty());
    this->contextWindow = nullptr;
    menuWindows.clear();
    dbgassert(isOk());
    destroy();
    releaseGarbageGuis();
    dbgassert(garbageGuis.empty());
}
void AppCtrl::closeAppMenusAtLvl(int startlvl) {
    for (int i = startlvl; i < CtrSize(menuWindows); i++) {
        auto menuWnd = menuWindows[i];
        if (menuWnd.ctxt) {
            menuWnd.wnd->getCtrl()->closePopup();
        }
    }
}

void determineWindowPos(guibase* guicontextmenu, window_main* mainWindow, float m_scale, int flags, ivec2 pos, ivec2& wndPos) {
    ivec2 windowPos;
    mainWindow->getPos(&windowPos);
    wndPos = windowPos;
    if (flags & WINDOW_POS_ABSOLUTE) {
        wndPos = pos;
    } else if (flags & WINDOW_POS_RELATIVE) {
        wndPos = windowPos + ivec2(pos.x * m_scale, pos.y * m_scale);
    } else {
        ivec2 windowSize;
        mainWindow->getSize(&windowSize);
        wndPos = windowPos + (windowSize - guicontextmenu->size) / 2;
    }
}

void AppCtrl::openAppMenu(int lvl, guictxtmenu_base* guicontextmenu, ivec2 pos, int createflags) {
    closeAppMenusAtLvl(lvl);
    while (CtrSize(menuWindows) <= lvl) {
        menuWindows.push_back({ nullptr, nullptr });
    }
    // TODO: allow caller/guicontextmenu to decide what font size to apply here
    // guicontextmenu->setFontSize(getTheme()->getFloat(GuiConstant::CONST_FONT_SIZE_CONTEXT_MENU));

    ivec2 wndPos(0);
    determineWindowPos(guicontextmenu, mainWindow, m_scale, createflags, pos, wndPos);

    if (!menuWindows[lvl].wnd) {
        auto popupCtrl        = std::make_shared<PopupCtrl>(this);
#if BUILD_DAW_HOST
        popupCtrl->setDawCtrl(this->parentDawCtrl);
#endif
        *popupCtrl->getTheme() = *getTheme();
        popupCtrl->m_scale     = m_scale;
        popupCtrl->m_size      = math::maxvec2(ivec2(20, 20), guicontextmenu->size);
        const ivec2 windowSize = ivec2(vec2(popupCtrl->m_size) * popupCtrl->m_scale);
        menuWindows[lvl].wnd   = this->mainWindow->createOverlay(popupCtrl, windowSize, createflags);
    }
    // TODO: menu change on same level will let this assertion fail
    auto& entry = menuWindows[lvl];
    dbgassert(entry.wnd && !entry.ctxt);
    entry.ctxt = guicontextmenu;

    auto popupCtrl     = entry.wnd->getCtrl();
    popupCtrl->m_scale = m_scale;
    // copy theme (again) from this control to contextWindows control
    *popupCtrl->getTheme() = *getTheme();
    auto label             = guicontextmenu->getLabel();
    if (!label.empty()) {
        popupCtrl->setWindowName(label);
    } else {
        popupCtrl->setWindowName("Menu");
    }
    {
        *popupCtrl->getTheme() = *getTheme();
        popupCtrl->m_scale     = m_scale;
    }
    static_cast<PopupCtrl*>(popupCtrl)->open(guicontextmenu, wndPos, (entry.wnd->getCreationFlags() & WINDOW_IS_RESIZABLE), entry.wnd->getCreationFlags() & WINDOW_IS_FOCUSED);
}

void AppCtrl::openOverlayGui(guictxtmenu_base* guicontextmenu, ivec2 pos, int createflags) {
    if (this->ctxtmenu) {
        closeContextMenu();
        if (!bIsVisible) {
            return;
        }
    }
    dbgassert(!this->ctxtmenu);

    guicontextmenu->setFontSize(getTheme()->getFloat(GuiConstant::CONST_FONT_SIZE_CONTEXT_MENU));


    ivec2 wndPos(0);
    determineWindowPos(guicontextmenu, mainWindow, m_scale, createflags, pos, wndPos);

    window_main* ctxtWindow = this->contextWindow;
    if (!ctxtWindow || ctxtWindow->getCreationFlags() != createflags) {
        if (ctxtWindow) {
            this->mainWindow->closeOverlay(ctxtWindow);
        }
        auto popupCtrl = std::make_shared<PopupCtrl>(this);
#if BUILD_DAW_HOST
        popupCtrl->parentDawCtrl = this->parentDawCtrl;
#endif
        popupCtrl->m_scale     = m_scale;
        popupCtrl->m_size      = math::maxvec2(ivec2(20, 20), guicontextmenu->size);
        *popupCtrl->getTheme() = *getTheme();
        const ivec2 windowSize = ivec2(vec2(popupCtrl->m_size) * popupCtrl->m_scale);
        ctxtWindow             = this->mainWindow->createOverlay(popupCtrl, windowSize, createflags);
    }
    this->ctxtmenu = guicontextmenu;
    this->contextWindow = ctxtWindow;
    if (ctxtWindow) {
        auto popupCtrl = ctxtWindow->getCtrl();
        dbgassert(popupCtrl->isOk());
        popupCtrl->m_scale = m_scale;
        // copy theme (again) from this control to contextWindows control
        auto themePopup = popupCtrl->getTheme();
        auto themeThis  = getTheme();
        dbgassert(themePopup);
        dbgassert(themeThis);
        *themePopup = *themeThis;
        auto label  = guicontextmenu->getLabel();
        if (!label.empty()) {
            popupCtrl->setWindowName(label);
        } else {
            popupCtrl->setWindowName("Tooltip");
        }
        {
            *popupCtrl->getTheme() = *getTheme();
            popupCtrl->m_scale     = m_scale;
        }
        static_cast<PopupCtrl*>(popupCtrl)->open(guicontextmenu, wndPos, (ctxtWindow->getCreationFlags() & WINDOW_IS_RESIZABLE), (createflags & WINDOW_IS_FOCUSED));
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

    auto popupCtrl = std::make_shared<PopupCtrl>(this);
#if BUILD_DAW_HOST
    popupCtrl->parentDawCtrl = this->parentDawCtrl;
#endif
    popupCtrl->m_scale     = m_scale;
    popupCtrl->m_size      = math::maxvec2(ivec2(20, 20), _guidialog->size);
    *popupCtrl->getTheme() = *getTheme();
    const ivec2 windowSize = ivec2(vec2(popupCtrl->m_size) * popupCtrl->m_scale);

    window_main* dialogWindow = this->mainWindow->createOverlay(popupCtrl, windowSize, WINDOW_IS_DIALOG | WINDOW_IS_RESIZABLE);

    dialogWindow->setSizeLimits(windowSize, windowSize * 2);

    auto label = _guidialog->getLabel();
    if (!label.empty()) {
        popupCtrl->setWindowName(label);
    } else {
        popupCtrl->setWindowName("Dialog");
    }
    {
        *popupCtrl->getTheme() = *getTheme();
        popupCtrl->m_scale     = m_scale;
    }
    popupCtrl->open(_guidialog, wndPos, (dialogWindow->getCreationFlags() & WINDOW_IS_RESIZABLE), true);
}
void AppCtrl::openContextMenu(guictxtmenu_base* b, ivec2 pos) {
    openOverlayGui(b, pos, WINDOW_POS_RELATIVE | WINDOW_IS_BORDERLESS);
    // openAppMenu(0, b, pos);
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
void AppCtrl::onChildOverlayWindowDestroy(window_main* ptr) {
    // This will only be called after onChildOverlayWindowClose
    if (ptr == contextWindow) {
        contextWindow = nullptr;
    }
    auto it = std::remove_if(menuWindows.begin(), menuWindows.end(), [ptr](const auto& entry) { return entry.wnd == ptr; });
    if (it != menuWindows.end()) {
        menuWindows.erase(it);
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
bool AppCtrl::onCharInput(uint32_t codepoint) {
    return BaseCtrl::onCharInput(codepoint);
}
bool AppCtrl::onKeyInput(int key, int scancode, int keyState, int mods, const char* key_name) {
    return BaseCtrl::onKeyInput(key, scancode, keyState, mods, key_name);
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
    MouseHitEvt evt = mouseHitEvt(MouseHitType::MOUSE_DRAGDROP_OBJECT, mevt.kbmods);
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
    MouseHitEvt evt = mouseHitEvt(MouseHitType::MOUSE_DRAGDROP_OBJECT, mevt.kbmods);
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
        auto vecSizeScaled  = vec2(ctrContent->getGui()->size) * 0.3f;
        ctrDragHandler.size = math::maxvec2(ivec2(32, 12), vecSizeScaled);
        ctrDragHandler.setLabel("Move " + ctrDragSrc->getGui()->label);
        setDragged(&ctrDragHandler);
        dragContainerMove(evt);
        dragContainerRelayout(drag_ctr_event{ drag_ctr_event_type::DRAG_BEGIN });
    }
}
void BaseCtrl::dragContainerMove(MouseEvent& evt) {
    dragContainerRelayout(drag_ctr_event{ drag_ctr_event_type::DRAG_MOVE });
    std::vector<i_ctr_layout*> list                     = getContainers();
    std::vector<std::weak_ptr<i_ctr_drop_area>> targets = getTargets(evt, list);
    dragDropTargets_ContainerMove                       = targets;
    ctrDragHandler.pos                                  = evt.mousepos;
}
void BaseCtrl::dropContainer(std::shared_ptr<guictr_layout_entry>& ctrContent, i_ctr_drop_area* area) {
    bool hasRemovedContainer = false;
    bool hasPlacedContainer  = false;
    auto* szLabel1           = StringAsCStr(ctrContent->getGui()->label);
    auto layoutCtr           = dynamic_cast<guictr_layout*>(area->getLayoutCtr());
    auto* szLabel2           = StringAsCStr(layoutCtr->label);

    dock_pos dockPos                  = area->getDockPos();
    container_layout ctrLayout        = layoutCtr->getLayout();
    container_layout updatedCtrLayout = dock_pos_to_container_layout(dockPos);
    if (area->childContainerIndex > -1 || (ctrLayout != updatedCtrLayout && ctrLayout != container_layout::SOLE)) {
        auto newContainer = std::make_shared<guictr_layout>();
        newContainer->setLayout(updatedCtrLayout);
        if (updatedCtrLayout == container_layout::TABBED) {
            auto& ctrEntries = layoutCtr->getEntries();
            dbgassert(area->dockPosOffset >= 0 && area->dockPosOffset <= CtrSize(ctrEntries));
            auto entryToReplace            = ctrEntries[math::min<int32_t>(CtrSize(ctrEntries) - 1, area->dockPosOffset)];
            auto oldEntryThatIsNowTabEntry = layoutCtr->replaceContainerWith(entryToReplace->getGui(), newContainer);

            newContainer->placeContainer(ctrContent, area);
            newContainer->placeContainer(oldEntryThatIsNowTabEntry, area);

        } else {// SPLIT_V or SPLIT_H
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
        log_lf(Log::L_DEBUG, "cannot directly insert into %s. need to change layout from %d to %d first\n", szLabel2, static_cast<int32_t>(ctrLayout), static_cast<int32_t>(updatedCtrLayout));
    } else {
        hasRemovedContainer = true;
        hasPlacedContainer  = area->getLayoutCtr()->placeContainer(ctrContent, area);
        log_lf(Log::L_DEBUG, "attempt to place container %s on %s result %d\n", szLabel1, szLabel2, hasPlacedContainer);
    }
    if (hasRemovedContainer && !hasPlacedContainer) {
        log_lf(Log::L_DEBUG, "Container was removed from its parent could not be placed on target. Container is now dangling %s\n", StringAsCStr(ctrContent->getGui()->label));
    }
}
void BaseCtrl::dragContainerRelease(MouseEvent& evt) {
    i_ctr_drop_area* area = determineDropCtrArea(evt);

    if (area && ctrContent) {
        this->dropContainer(ctrContent, area);
    }
    // end the extension of the dragged containers lifetime
    ctrContent                  = nullptr;
    ctrDragHandler.validPreview = false;
    dragDropTargets_ContainerMove.clear();
    dragContainerRelayout(drag_ctr_event{ drag_ctr_event_type::DRAG_END });
}
std::vector<std::weak_ptr<i_ctr_drop_area>> BaseCtrl::getTargets(MouseEvent& mevt, std::vector<i_ctr_layout*> ifMatches) {
    std::vector<std::weak_ptr<i_ctr_drop_area>> targets;
    for (i_ctr_layout* ctr : ifMatches) {
        std::vector<std::weak_ptr<i_ctr_drop_area>> ctrtargets;
        ctr->getOverlays(mevt, ctrtargets);
        targets.insert(targets.begin(), ctrtargets.begin(), ctrtargets.end());
    }
    return targets;
}
std::vector<i_ctr_layout*> BaseCtrl::getContainers() {
    MouseHitEvt evtDragObj = mouseHitEvt(MouseHitType::MOUSE_DRAGDROP_OBJECT, KeyboardMods::KB_MODS_NONE);
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
        // nvgLineCap(vg, NVGlineCap::NVG_ROUND);
        //  for (ivec4& box : boxes) {
        //   nvgBeginPath(vg);
        //   nvgRect(vg, box.x, box.y, box.z, box.w);
        //   nvgStrokeColor(vg, G_MOVE_HIGHLIGHT);
        //   nvgStrokeWidth(vg, 4.0);
        //   nvgStroke(vg);
        //  }
        // nvgLineCap(vg, NVGlineCap::NVG_BUTT);
    }
}
void BaseCtrl::closeAllAppMenus() {
    closeAppMenusAtLvl(0);
}
SafeRefStorage<guibase>& BaseCtrl::getRefStorage() {
    if (daw_tls::isTlsInitialized()) {
        auto runtime = daw_tls::getTls().runtime;
        dbgassert(runtime);
        return runtime->safeRefs;
    }
    return localRefs;
}

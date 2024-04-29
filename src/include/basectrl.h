#pragma once
#include <deque>
#include <list>
#include <map>
#include <memory>
#include <set>
#include "commands.h"
#include "math/seq_math.h"
#include "tls.h"
#include "types.h"
#include <vector>

#include "assert_dbg.h"
#include "config.h"
#include "event.h"
#include "gui/gui.h"
#include "gui/container/container.h"
#include "gui/container/container_layout_types.h"
#include "hires_timer.h"
#include "keyboard.h"
#include "logging.h"
#include "math/vec.h"
#include "menu.h"
#include "mouse.h"
#include "note.h"
#include "rand.h"
#include "saferef.h"
#include "seq_time.h"
#include "seq_util.h"
#include "str_util.h"
#include "theme.h"
#include "thememgr.h"
#include "window.h"

struct NVGcontext;
class guidialog_base;
class guictxtmenu_base;
class appwindow_main;
class guictr_layout;

KeyEvent keyEvent(int key, int scancode, int keyState, int mods, const char* key_name);
String getModKeyName(int modKey);
String GetMenuNameWithKeybind(const String& s, const KeyCombo* combo);

void determineWindowPos(guibase* guicontextmenu, window_main* mainWindow, float m_scale, int flags, ivec2 pos, ivec2& wndPos);

class determine_string_width {
    BaseCtrl* ctrl;
    guitheme_t* theme;
public:
    determine_string_width(BaseCtrl* _ctrl, guitheme_t* _theme) : ctrl(_ctrl), theme(_theme) {
    }
    float getStringWidth(const String& text, float fontSize, int alignment = 0);
};
class determine_table_string_width {
    BaseCtrl* ctrl;
    guitheme_t* theme;
    float fontSize = 0;
    int textAlignment = 0;
public:
    determine_table_string_width(BaseCtrl* _ctrl, guitheme_t* _theme, float _fontSize, int _alignment) : ctrl(_ctrl), theme(_theme), fontSize(_fontSize), textAlignment(_alignment) {
    }
    float getStringWidth(const String& text);
};


class guictr_dragged_container_instance final : public guictr_base {
public:
    std::vector<DropAreaUILayout> boxes;
    dock_pos dockPos  = dock_pos::NONE;
    bool validPreview = false;
    guictr_dragged_container_instance() : guictr_base() { setDragRendered(true); }
    ~guictr_dragged_container_instance() override = default;
    void layout() override {}
    bool isDragMoveable() override { return true; }
    void renderDragged(NVGcontext* vg, ivec2 mousepos, ivec2 dragOffset) override;
    void handleDraggedMove(MouseEvent& evt) override;
    void handleDraggedRelease(MouseEvent& evt) override;
};

class BaseCtrl {
public:
    enum drag_ctr_event_type { DRAG_BEGIN, DRAG_MOVE, DRAG_END };
    struct drag_ctr_event {
        drag_ctr_event_type evtType;
    };
protected:
    guitheme_mgr themes;
    DAW::UI::CommandManager* commands = nullptr;
    AppCtrl* const parentCtrl = nullptr;
#if BUILD_DAW_HOST
    DawCtrl* parentDawCtrl = nullptr;
#endif
public:
    String windowName;
    window_base* window = nullptr;
    NVGcontext* vg      = nullptr;
    std::vector<guictr_base*> containers;
    /* list of target areas where the currently dragged object can be moved to */
    std::vector<std::weak_ptr<DropAreaUILayout>> dragDropTargets_ContainerMove;
    guictr_dragged_container_instance ctrDragHandler;
    SPLayoutEntry draggedLayoutContainer;
    int32_t refIdNext = 1;
    SafeRefStorage<guibase> localRefs;
    int cursorIcon         = CURSOR_DEFAULT;
    ivec2 m_size           = {-1, -1};
    ivec2 m_mousePos       = {-1, -1};
    float m_scale          = 1.0f;
    ivec2 dragStart{0};
    ivec2 dragOffset{0};
    ivec2 dragDistance{0};

    guictxtmenu_base* ctxtmenu = nullptr;
protected:
    SafeRef<guibase> guiOver;       // updates on mouse move "current mouseover"
    SafeRef<guibase> guiDragged;    // updates on mouse click "currently dragged", set from guiOver
    SafeRef<guibase> guiCaptured;   // updates when cursor is hidden, set from guiDragged
    SafeRef<guibase> guiFocused;    // updates on mouse click, set from guiOver
    SafeRef<guibase> guiCtrFocused; // updates on mouse click, handles keyboard input
public:
    guibase* getGuiOver() { return safeRefGet(guiOver); }
    guibase* getGuiDragged() { return safeRefGet(guiDragged); }
    guibase* getGuiCaptured() { return safeRefGet(guiCaptured); }
    guibase* getGuiFocused() { return safeRefGet(guiFocused); }
    guibase* getGuiCtrFocused() { return safeRefGet(guiCtrFocused); }
    const guibase* getGuiOver() const { return safeRefGet(guiOver); }
    const guibase* getGuiDragged() const { return safeRefGet(guiDragged); }
    const guibase* getGuiCaptured() const { return safeRefGet(guiCaptured); }
    const guibase* getGuiFocused() const { return safeRefGet(guiFocused); }
    const guibase* getGuiCtrFocused() const { return safeRefGet(guiCtrFocused); }
    const SafeRef<guibase>& getGuiOverRef() const { return guiOver; }
    const SafeRef<guibase>& getGuiDraggedRef() const { return guiDragged; }
    const SafeRef<guibase>& getGuiCapturedRef() const { return guiCaptured; }
    const SafeRef<guibase>& getGuiFocusedRef() const { return guiFocused; }
    const SafeRef<guibase>& getGuiCtrFocusedRef() const { return guiCtrFocused; }

    MouseEvent lastMouseEvent{};

    bool bShowDebugFrames      = false;
    bool canTakeInputFocus = true; // TODO: use flags
    bool bIsVisible        = true;
    bool bHasFocus         = false;
    bool mouseInside = false;
    bool isOK        = false;
public:
    static MouseHitEvt mouseHitEvt(MouseHitType _type, KeyboardMods kbmods);
    explicit BaseCtrl(AppCtrl* parent);
    virtual ~BaseCtrl();
    BaseCtrl(const BaseCtrl& other) = delete;
    BaseCtrl& operator=(const BaseCtrl& other) = delete;
    BaseCtrl(BaseCtrl&& other) = delete;
    BaseCtrl& operator=(BaseCtrl&& other) = delete;
    
    DropAreaUILayout* determineDropCtrArea(MouseEvent& mevt) {
        MouseHitEvt evtDragObj = mouseHitEvt(MouseHitType::MOUSE_DRAGDROP_OBJECT, mevt.kbmods);
        evtDragObj.setDraggedThing(nullptr);
        evtDragObj.requestFocus(nullptr);
        DropAreaUILayout* gui = nullptr;

        if (gui == nullptr) {
            for (std::weak_ptr<DropAreaUILayout>& weakPtrTarget : dragDropTargets_ContainerMove) {
                if (!weakPtrTarget.expired()) {
                    // TODO: I don't even need to lock here
                    auto shrdPtrTarget = weakPtrTarget.lock();
                    if (shrdPtrTarget) {
                        if (shrdPtrTarget->size == ivec2{0, 0}) {
                            log_lf(Log::L_WARN, "warning, rendering container with size 0 0\n");
                            continue;
                        }

                        evtDragObj.setDraggedThing(nullptr);
                        evtDragObj.requestFocus(nullptr);
                        if (shrdPtrTarget->contains(mevt.mousepos)) {
                            gui = shrdPtrTarget.get();
                            break;
                        }
                    }
                }
            }
        }
        return gui;
    }
    std::vector<guictr_layout_base*> getContainers();
    std::vector<std::weak_ptr<DropAreaUILayout>> getTargets(MouseEvent& mevt, std::vector<guictr_layout_base*> ifMatches);
    /**
     * Begin a container drag-drop action.
     * This function replaces the current dragged gui object with member BaseCtrl::ctrDragHandler.
     *
     * @param evt - The mouse event that initiated the drag-drop action.
     * @param ctrDragSrc - The container that is getting moved
     */
    void dragContainerBegin(MouseEvent& evt, GuiCtrLayoutEntry* ctrDragSrc);
    void dragContainerMove(MouseEvent& evt);
    void dragContainerRelease(MouseEvent& evt);
    void dropContainer(SPLayoutEntry& ctrContent, DropAreaUILayout* area);
    virtual void dragContainerRelayout(drag_ctr_event evt) = 0;
    bool isDraggingContainer() const { return draggedLayoutContainer != nullptr || bShowDebugFrames; }
    SafeRefStorage<guibase>& getRefStorage();
    // int safeRefCreate(guibase* gui) override;
    // guibase* safeRefGetPtr(int32_t refId) override;
    // void safeRefDestroy(int32_t refId) override;
    bool isOk() const { return isOK; }
    virtual guitheme_t* getTheme() { return &themes.getRef(); }
    guitheme_mgr* getThemeMgr() { return &themes; }
    ivec2 getScaledSize() const { return {m_size.x * 1.0 / m_scale, m_size.y * 1.0 / m_scale}; }
    virtual const std::vector<guictr_base*>& getRenderContainers() const { return containers; }
    virtual void prerender(NVGcontext* nanovgCtxt, int32_t x, int32_t y, int32_t w, int32_t h, float ratio);
    virtual void render(NVGcontext* nanovgCtxt, int32_t x, int32_t y, int32_t w, int32_t h, float ratio);
    virtual void renderContainers(NVGcontext* nanovgCtxt, int32_t x, int32_t y, int32_t w, int32_t h, float ratio);
    virtual bool processGlobalKeyevent(const KeyEvent& event) { return false; }
    virtual bool handleGlobalCommand(DAW::UI::CommandContext& ctxt) {
        return false;
    }
    virtual bool mouseDownPre() { return true; }
    bool hasInputFocus() const { return bHasFocus && canTakeInputFocus; }
    void focusGui(guibase* g);
    virtual void focusChanged(guibase* oldFocused, guibase* newFocused);
    void mouseDown(ivec2 mousePos, int button, KeyboardMods kbmods, bool doubleclick);
    void mouseUp(ivec2 mousePos, int button, KeyboardMods kbmods);
    virtual bool onCharInput(uint32_t codepoint);
    virtual bool onKeyInput(int key, int scancode, int keyState, int mods, const char* key_name);
    void mouseScrolled(double xoffset, double yoffset, KeyboardMods kbmods);
    virtual void mouseMoved(ivec2 mousePos, ivec2 deltaPos, KeyboardMods kbmods);

    bool isCtrOrChildFocused(const guibase* gui) const;
    bool isMouseInside() const { return mouseInside; }
    virtual void focusReceived() { bHasFocus = true; };
    virtual void focusLost() { bHasFocus = false; };
    virtual void onCursorEnter(int entered) { mouseInside = entered; }
    virtual void relayout();
    virtual void relayout(int32_t w, int32_t h);
    virtual void windowSizeChanged(int32_t w, int32_t h);
    virtual void openContextMenu(guictxtmenu_base* b, ivec2 pos);
    virtual void closeContextMenu(){};
    void closeAllAppMenus();
    virtual void closeAppMenusAtLvl(int startlvl){};
    virtual void closeAllContextMenus();
    virtual void closeDialogs();
    virtual void openAppMenu(int lvl, guictxtmenu_base* b, ivec2 pos, int flags){};
    virtual void closePopup(){}; // close this window if its a popup window
    virtual bool hasContextMenu() { return false; };
    virtual void objectDragMove(guibase* g, MouseEvent& mevt);
    virtual void objectDragRelease(guibase* g, MouseEvent& mevt);
    bool captureMouse(guibase* gui);
    virtual String getClipboardText();
    virtual void setClipboardText(String s);
    virtual void requestRedraw() { this->window->requestRedraw(); }
    virtual void resetMouseContext();
    virtual void onMenuOpen(ngui::Menu* menu) {}
    virtual ivec2 toScreenSpace(ivec2 p) = 0;
    void setDragged(guibase* g) {
        if (!g) {
            guiDragged = {};
        } else {
            guiDragged = g->toRef();
        }
    }
    virtual std::shared_ptr<guictr_layout> replaceContainerWith(guictr_base* ctr, std::shared_ptr<guictr_layout> newContainer) {
        return nullptr;
    }
    bool isVisible() const { return bIsVisible; }
    void setVisible(bool b) { this->bIsVisible = b; }
    virtual bool hasDialogWindows() { return false; }
    virtual AppCtrl* getParentCtrl() {
        return parentCtrl;
    }
    DAW::UI::CommandManager* getCommandManager() { return commands; }
#if BUILD_DAW_HOST
    virtual DawCtrl* getDawCtrl() {
        return parentDawCtrl;
    }
    virtual void setDawCtrl(DawCtrl* dawCtrl) {
        parentDawCtrl = dawCtrl;
    }
#endif
    virtual String getWindowName() {
        return windowName;
    }
    virtual void setWindowName(String name) {
        windowName = std::move(name);
        if (this->window) {
            window->setWindowTitle(windowName);
        }
    }
    virtual bool isGlobalKeybindCodepoint(uint32_t codepoint) {
        return false;
    }
    virtual void updateZoomLevel(float f) {
        m_scale = f == 0.0f ? 1.0f : (math::clamp(f, 0.25f, 2.0f));
    }
    virtual size_t getAppWindowIndex() { return 0; }
};

class AppCtrl : public BaseCtrl {
protected:
    struct appmenu_window_entry {
        window_main* wnd{nullptr};
        guictxtmenu_base* ctxt{nullptr};
    };
    std::vector<appmenu_window_entry> menuWindows;
    std::vector<guibase*> garbageGuis;
    guidialog_base* dialog = nullptr;
    bool bIsVisible = false;

    int nextTooltipId = 0;
    int64_t tmLastHoveredTooltip        = 0;
    void* lastHoveredTooltip            = nullptr;
    void* lastTooltipSrc                = nullptr;
public:
    bool hasMenuWindow() {
        for (auto& w : menuWindows) {
            if (w.ctxt) return true;
        }
        return false;
    }
    window_main* mainWindow    = nullptr;
    window_main* contextWindow = nullptr;
#if WINDOW_HAS_MENUBAR
    ngui::MenuBar menubar;
#endif
    explicit AppCtrl(AppCtrl* parent);
    ~AppCtrl() override = default;
    void relayout(int32_t w, int32_t h) override = 0;
    virtual void onChildOverlayWindowDestroy(window_main*);
    virtual void onChildOverlayWindowClose(window_main*);
    void openContextMenu(guictxtmenu_base* b, ivec2 pos) override;
    void openDialog(guidialog_base* b);
    void closeContextMenu() override;
    void releaseGarbageGuis();
    void closeDialogs() override;
    void openAppMenu(int lvl, guictxtmenu_base* b, ivec2 pos, int flags) override;
    void closeAppMenusAtLvl(int startlvl) override;
    bool hasContextMenu() override;
    bool onKeyInput(int key, int scancode, int keyState, int mods, const char* key_name) override;
    bool onCharInput(uint32_t codepoint) override;
    void onMenuOpen(ngui::Menu* menu) override;
    virtual void updateMenubar();
    guictxtmenu_base* getContextMenu();
#if WINDOW_HAS_MENUBAR
    virtual ngui::MenuBar& getMenubar();
#endif

    void closePopup() override { }; // close this window if its a popup window

    virtual void filesDropCancel() { };
    virtual bool filesDropMove(ivec2 pos, KeyboardMods kbmods) { return false; };
    virtual bool filesDropBegin(std::vector<String>& files, ivec2 pos, KeyboardMods kbmods) { return false; };
    virtual bool filesDropFinal(std::vector<String>& files, ivec2 pos, KeyboardMods kbmods) { return false; };

    virtual bool menuCommand(const menucmd_t& command) {
        return false;
    }

    virtual void onBeforeShowWindow() {
        bIsVisible = true;
    }

    virtual void onWindowClose() {
        bIsVisible = false;
        if (this->ctxtmenu) {
            dbgassert(contextWindow);
            contextWindow->getCtrl()->closePopup();
            dbgassert(!this->ctxtmenu);
        }
    }

    virtual bool onWindowCloseRequest() {
        return true;
    }

    virtual void onTick()       = 0;
    virtual void initApp(const std::vector<String>& args) = 0;
    virtual bool initAppWindow(window_main* window, NVGcontext* nanovg) = 0;
    virtual void startApp()     = 0; /* OpenGL context exists in startApp */
    virtual void destroy()      = 0;
    virtual void onPreDestroy() { };
    void onAppTick();
    virtual void onFastTick() {};
    void destroyControl();
    bool hasDialogWindows() override {
        if (hasMenuWindow()) return true;
        return dialog != nullptr;
    }

    ivec2 toScreenSpace(ivec2 p) override {
        ivec2 windowPos;
        this->mainWindow->getPos(&windowPos);
        return windowPos + ivec2(vec2(p) * m_scale);
    }
    /**
     * openOverlayGui
     * @param b
     * @param pos
     * @param flags @see BASECTRL_WND_* defines
     */
    void openOverlayGui(guictxtmenu_base* b, ivec2 pos, int flags);
    void dragContainerRelayout(drag_ctr_event evt) override {}
};
class guictr_scrollbar;
class PopupCtrl final : public AppCtrl {
    guictr_scrollbar* popupCtrs = nullptr;
    bool bResizeable            = false;

public:
    explicit PopupCtrl(AppCtrl* parent) : AppCtrl(parent) {};
    ~PopupCtrl() override = default;

    void destroy() override;
    bool isShown() { return this->window && this->window->isShown(); }
    void closePopup() override;
    void relayout(int32_t w, int32_t h) override;
    void open(guictxtmenu_base* ctxtmenu, ivec2 pos, bool bResizeable, bool bFocused);
    void initApp(const std::vector<String>& args) override { };
    bool initAppWindow(window_main* window, NVGcontext* nanovg) override;
    void startApp() override {};
    void onWindowClose() override;
    void onTick() override;
    bool mouseDownPre() override;
    void render(NVGcontext* nanovgCtxt, int32_t x, int32_t y, int32_t w, int32_t h, float ratio) override;
};

class AppInstanceService {
public:
    virtual ~AppInstanceService() = default;
    virtual std::shared_ptr<AppCtrl> makeApp(const std::vector<String>& args) = 0;
    virtual void startApp(std::shared_ptr<AppCtrl>& app) = 0;
    virtual void deleteApp() = 0;
};
int startApplication(const std::vector<String>& args, AppInstanceService& service);

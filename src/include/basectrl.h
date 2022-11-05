#pragma once
#include <deque>
#include <list>
#include <map>
#include <memory>
#include <set>
#include "commands.h"
#include "tls.h"
#include "types.h"
#include <vector>

#include "assert_dbg.h"
#include "config.h"
#include "event.h"
#include "gui/gui.h"
#include "gui/container/container.h"
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

class i_ctr_layout;
class guictr_layout_entry_handle;
class guictr_dragged_container_instance;
class i_ctr_layout;
class i_ctr_drop_area {
    i_ctr_layout* const parent;

public:
    ivec2 pos{0, 0};
    ivec2 size{0, 0};
    int32_t priority            = 0;
    dock_pos dockPos            = dock_pos::NONE;
    int32_t dockPosOffset       = -1;
    int32_t childContainerIndex = -1;
    String label;
    bool bAlwaysShow = false;
    explicit i_ctr_drop_area(i_ctr_layout* _parent) : parent(_parent) {}
    void render(NVGcontext* vg);
    bool contains(ivec2 mpos) const { return mpos.x >= pos.x && mpos.y >= pos.y && mpos.x < pos.x + size.x && mpos.y < pos.y + size.y; }
    i_ctr_layout* getLayoutCtr() { return parent; }
    dock_pos getDockPos() const { return dockPos; }
    void setAlwaysShow(bool b) { bAlwaysShow = b; }
    bool isAlwaysShow() const { return bAlwaysShow; }
};
enum layout_ctr_type { GUICTR_LAYOUT, GUICTR_BASE };
struct guictr_layout_entry {
    const gui_type type;
    const layout_ctr_type frameType;
    ivec2 pos{0};
    ivec2 size{0};
    std::shared_ptr<guictr_base> ctr;
    std::shared_ptr<guictr_layout> selfLayoutCtr;
    guictr_layout_entry_handle* ctrHandle;
    String label;
    bool hasHandle = true;
    i_ctr_layout* parentLayoutContainer = nullptr;
    int32_t entryTag = -1;
    guictr_layout_entry(String label, const std::shared_ptr<guictr_base>& _ctr);
    ~guictr_layout_entry();
    guictr_base* getGui();
    std::shared_ptr<guictr_base> getSharedGui() const { return ctr; }
    guibase* getHandle();
    gui_type getType() const { return type; }
    layout_ctr_type getFrameType() const { return frameType; }
    String getLabel() const { return label; }
    bool getContainerRef(std::shared_ptr<guictr_layout_entry>& out, bool remove);
    void removeEntryFromParent();
    std::shared_ptr<guictr_layout>& getAsLayoutCtr() { return selfLayoutCtr; }
    int32_t getEntryTag() const { return entryTag; }
    void setEntryTag(int32_t tag) { entryTag = tag; }
    void assertState() const {
        dbgassert(!!parentLayoutContainer == !!ctr->parent);
    }
};

class i_ctr_layout {
public:
    virtual ~i_ctr_layout() = default;
    virtual void getOverlays(MouseEvent& evt, std::vector<std::weak_ptr<i_ctr_drop_area>>& handles)                = 0;
    virtual bool placeContainer(std::shared_ptr<guictr_layout_entry> ctr, i_ctr_drop_area* area)                   = 0;
    virtual bool getContainerRef(guictr_layout_entry* ctr, std::shared_ptr<guictr_layout_entry>& out, bool remove) = 0;
    virtual std::shared_ptr<guictr_layout_entry> replaceContainerWith(guictr_base* ctr, std::shared_ptr<guictr_layout_entry>& newEntry) = 0;
    virtual container_layout getLayout() const = 0;
    virtual void postContentChanged() = 0;
    virtual bool activateEntry(guictr_layout_entry* entry) = 0;
};

inline container_layout dock_pos_to_container_layout(dock_pos pos) {
    switch (pos) {
        case dock_pos::TOP:
        case dock_pos::BOTTOM:
            return container_layout::SPLIT_H;
        case dock_pos::LEFT:
        case dock_pos::RIGHT:
            return container_layout::SPLIT_V;
        case dock_pos::STACK:
            return container_layout::TABBED;
        default:
            break;
    }
    return container_layout::SOLE;
}
class guictr_dragged_container_instance : public guictr_base {
public:
    std::vector<i_ctr_drop_area> boxes;
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
    std::vector<std::weak_ptr<i_ctr_drop_area>> dragDropTargets_ContainerMove;
    guictr_dragged_container_instance ctrDragHandler;
    std::shared_ptr<guictr_layout_entry> ctrContent;
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
    guibase* guiOver       = nullptr; // updates on mouse move "current mouseover"
    guibase* guiDragged    = nullptr; // updates on mouse click "currently dragged", set from guiOver
    guibase* guiCaptured   = nullptr; // updates when cursor is hidden, set from guiDragged
    guibase* guiFocused    = nullptr; // updates on mouse click, set from guiOver
    guibase* guiCtrFocused = nullptr; // updates on mouse click, handles keyboard input
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
    i_ctr_drop_area* determineDropCtrArea(MouseEvent& mevt) {
        MouseHitEvt evtDragObj = mouseHitEvt(MouseHitType::MOUSE_DRAGDROP_OBJECT, mevt.kbmods);
        evtDragObj.setDraggedThing(nullptr);
        evtDragObj.requestFocus(nullptr);
        i_ctr_drop_area* gui = nullptr;

        if (gui == nullptr) {
            for (std::weak_ptr<i_ctr_drop_area>& weakPtrTarget : dragDropTargets_ContainerMove) {
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
    guibase* getGuiFocused() const { return guiFocused; }
    std::vector<i_ctr_layout*> getContainers();
    std::vector<std::weak_ptr<i_ctr_drop_area>> getTargets(MouseEvent& mevt, std::vector<i_ctr_layout*> ifMatches);
    /**
     * Begin a container drag-drop action.
     * This function replaces the current dragged gui object with member BaseCtrl::ctrDragHandler.
     *
     * @param evt - The mouse event that initiated the drag-drop action.
     * @param ctrDragSrc - The container that is getting moved
     */
    void dragContainerBegin(MouseEvent& evt, guictr_layout_entry* ctrDragSrc);
    void dragContainerMove(MouseEvent& evt);
    void dragContainerRelease(MouseEvent& evt);
    void dropContainer(std::shared_ptr<guictr_layout_entry>& ctrContent, i_ctr_drop_area* area);
    virtual void dragContainerRelayout(drag_ctr_event evt) = 0;
    bool isDraggingContainer() const { return ctrContent != nullptr || bShowDebugFrames; }
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
    virtual bool processGlobalKeyevent(const KeyEvent& event) { return false; }
    virtual bool handleGlobalCommand(DAW::UI::CommandContext& ctxt) {
        return false;
    }
    virtual bool mouseDownPre() { return true; }
    bool hasInputFocus() const { return bHasFocus && canTakeInputFocus; }
    void focusGui(guibase* g);
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
    // Only use this pointer for comparison!
    void onGuiRemoved(void* gui);
    virtual void resetMouseContext();
    virtual void onMenuOpen(ngui::Menu* menu) {}
    virtual ivec2 toScreenSpace(ivec2 p) = 0;
    void setDragged(guibase* g) { guiDragged = g; }
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
        m_scale = f;
    }
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
    };

    bool onWindowCloseRequest() {
        return true;
    };

    virtual void onTick()                                               = 0;
    virtual void initApp(const std::vector<String>& args)               = 0;
    virtual bool initAppWindow(window_main* window, NVGcontext* nanovg) = 0;
    virtual void startApp()                                             = 0; /* OpenGL context exists in startApp */
    virtual void destroy()                                              = 0;
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
protected:
    void dragContainerRelayout(drag_ctr_event evt) override {}
};
class guictr_scrollbar;
class PopupCtrl : public AppCtrl {
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
    bool initPopup(window_overlay* window, NVGcontext* nanovg);
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

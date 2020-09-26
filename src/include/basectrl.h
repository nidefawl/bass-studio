#pragma once
#include <deque>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <stdint.h>
#include <stdint.h>
#include <vector>

#include "assert_dbg.h"
#include "config.h"
#include "event.h"
#include "gui/gui.h"
#include "gui/guicontainer.h"
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
String menuName(String s, KeyCombo combo);
ivec2 toControlsObjectSpace(ivec2& pos, guibase* gui);

#define BASECTRL_WND_POS_RELATIVE 1
#define BASECTRL_WND_POS_ABSOLUTE 2
#define BASECTRL_WND_RESIZEABLE 4

enum class dock_pos : int32_t { NONE=0, CENTER, LEFT, RIGHT, TOP, BOTTOM, STACK };
enum class container_layout : int32_t { SOLE, SPLIT_H, SPLIT_V, TABBED };
class i_ctr_layout;

class guictr_layout_entry_handle;
class guictr_dragged_container_instance;
class i_ctr_layout;
class i_ctr_drop_area {
	i_ctr_layout* const parent;
public:
	ivec2 pos{0,0};
	ivec2 size{0,0};
	int32_t priority = 0;
	dock_pos dockPos = dock_pos::NONE;
	int32_t dockPosOffset = -1;
	int32_t childContainerIndex = -1;
	String label;
	i_ctr_drop_area(i_ctr_layout* _parent)
	  : parent(_parent) {

	}
    ~i_ctr_drop_area(){};
    void render(NVGcontext* vg);
	bool contains(ivec2 mpos) {
		return mpos.x >= pos.x &&
			mpos.y >= pos.y &&
			mpos.x < pos.x + size.x &&
			mpos.y < pos.y + size.y;
	}
	i_ctr_layout* getLayoutCtr() {
		return parent;
	}
	dock_pos getDockPos() {
		return dockPos;
	}
};
enum layout_ctr_type {
	GUICTR_LAYOUT,
	GUICTR_BASE
};
struct guictr_layout_entry {
	const container_type type;
	const layout_ctr_type frameType;
	ivec2 pos{0};
	ivec2 size{0};
	std::shared_ptr<guictr_base> ctr; /* non-owning */ //TODO: make this owning, unique ptr
	guictr_layout_entry_handle* ctrHandle;
	String label;
	bool hasHandle = true;
    i_ctr_layout* parentLayoutContainer = nullptr;
    guictr_layout_entry(String label, std::shared_ptr<guictr_base> _ctr);
    ~guictr_layout_entry();
	guictr_base* getGui();
	guibase* getHandle();
	container_type getType() const {
		return type;
	}
	layout_ctr_type getFrameType() const {
		return frameType;
	}
	String getLabel() {
		return label;
	}
	bool getContainerRef(std::shared_ptr<guictr_layout_entry>& out, bool remove);
};

class i_ctr_layout {
public:
    virtual ~i_ctr_layout(){};
    virtual void getOverlays(MouseEvent& evt, std::vector<std::weak_ptr<i_ctr_drop_area>>& handles) = 0;
    virtual bool placeContainer(std::shared_ptr<guictr_layout_entry> ctr, i_ctr_drop_area* area) = 0;
    virtual bool getContainerRef(guictr_layout_entry* ctr, std::shared_ptr<guictr_layout_entry>& out, bool remove) = 0;
    virtual container_layout getLayout() const = 0;
    virtual void postContentChanged() = 0;
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
    dock_pos dockPos = dock_pos::NONE;
    bool validPreview = false;
    guictr_dragged_container_instance() : guictr_base()
    {
        setDragRendered(true);
    }
    ~guictr_dragged_container_instance()
    {
    }
    void layout() override
    {
    }
    bool isDragMoveable()
    {
        return true;
    }
    void renderDragged(NVGcontext* vg, ivec2 mousepos, ivec2 dragOffset) override;
    void handleDraggedMove(MouseEvent& evt);
    void handleDraggedRelease(MouseEvent& evt);
};
class BaseCtrl : public SafeRefHandler<guibase> {
protected:
    guitheme_mgr themes;

public:
    BaseCtrl()
    {
        themes.parent = this;
        ctrDragHandler.setControl(this);
        ctrDragHandler.setFlag(FLG_RENDER_LABEL, true);
    }
    window_base* window = NULL;
    NVGcontext* vg = NULL;
    std::vector<guictr_base*> containers;
    /* list of target areas where the currently dragged object can be moved to */
    std::vector<std::weak_ptr<i_ctr_drop_area>> dragDropTargets_ContainerMove;
    guictr_dragged_container_instance ctrDragHandler;
    std::shared_ptr<guictr_layout_entry> ctrContent;
    std::vector<i_ctr_layout*> getContainers();
    std::vector<std::weak_ptr<i_ctr_drop_area>> getTargets(MouseEvent& mevt, std::vector<i_ctr_layout*> ifMatches);
	enum drag_ctr_event_type {
		DRAG_BEGIN, DRAG_MOVE, DRAG_END
	};
    struct drag_ctr_event {
		drag_ctr_event_type evtType;
    };
    i_ctr_layout* determineTarget(MouseEvent& mevt)
	{
		MouseHitEvt evtDragObj = mouseHitEvt(MouseHitType::MOUSE_DRAGDROP_OBJECT);
		evtDragObj.setDraggedThing(nullptr);
		evtDragObj.requestFocus(nullptr);
		i_ctr_layout* gui = nullptr;
		for (guictr_base* ctr : containers) {
			evtDragObj.setDraggedThing(nullptr);
			evtDragObj.requestFocus(nullptr);
			if (ctr->mouseHitTest(mevt.mousepos, evtDragObj)) {
				auto* guihit = evtDragObj.getGuiHit();
				gui = dynamic_cast<i_ctr_layout*>(guihit);
				if (gui) {
					break;
				}
				gui = dynamic_cast<i_ctr_layout*>(guihit->parent);
				if (gui) {
					break;
				}
			}
		}
		return gui;
	}
    i_ctr_drop_area* determineDropCtrArea(MouseEvent& mevt)
	{
		MouseHitEvt evtDragObj = mouseHitEvt(MouseHitType::MOUSE_DRAGDROP_OBJECT);
		evtDragObj.setDraggedThing(nullptr);
		evtDragObj.requestFocus(nullptr);
		i_ctr_drop_area* gui = nullptr;

		if (gui == nullptr) {
			for (std::weak_ptr<i_ctr_drop_area>& weakPtrTarget : dragDropTargets_ContainerMove) {
				if (!weakPtrTarget.expired()) {
					//TODO: I don't even want to lock here
					auto shrdPtrTarget = weakPtrTarget.lock();
					if (shrdPtrTarget.get()) {
						if (shrdPtrTarget->size == ivec2{0, 0}) {
							log_printf("warning, rendering container with size 0 0\n", 0);
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
    virtual void dragContainerRelayout(drag_ctr_event evt) = 0;
    bool isDraggingContainer() const {
    	return ctrContent.get() != nullptr;
    }
    guictxtmenu_base* ctxtmenu = NULL;
    //	guictxtmenu_base *ctxtmenuOld = NULL;
    int cursorIcon = CURSOR_DEFAULT;
    ivec2 m_size = {-1, -1};
    ivec2 m_mousePos = {-1, -1};
    float m_scale = 1.0f;
    guibase* guiOver = NULL;       // updates on mouse move "current mouseover"
    guibase* guiDragged = NULL;    // updates on mouse click "currently dragged", set from guiOver
    guibase* guiCaptured = NULL;   // updates when cursor is hidden, set from guiDragged
    guibase* guiFocused = NULL;    // updates on mouse click, set from guiOver
    guibase* guiCtrFocused = NULL; // updates on mouse click, handles keyboard input
    guibase* getGuiFocused()
    {
        return guiFocused;
    }
    struct stored_ref {
        guibase* ptr;
        int32_t refId;
    };
    int32_t refIdNext = 1;
    std::vector<stored_ref> refs;
    bool canTakeInputFocus = false; // TODO: use flags
    int safeRefCreate(guibase* gui) override
    {
        stored_ref ref{gui, (int32_t)refIdNext++};
        refs.push_back(ref);
        return ref.refId;
    }
    guibase* safeRefGetPtr(int32_t refId) override
    {
        auto it = std::find_if(refs.begin(), refs.end(), [refId](const stored_ref& ref) { return ref.refId == refId; });
        if (it != refs.end()) {
            stored_ref& ref = *it;
            return ref.ptr;
        }
        return nullptr;
    }
    void safeRefDestroy(int32_t refId) override
    {
        auto it = std::find_if(refs.begin(), refs.end(), [refId](const stored_ref& ref) { return ref.refId == refId; });
        if (it != refs.end()) {
            it->ptr = nullptr;
            refs.erase(it);
            return;
        }
        dbgassert(0);
    }

    ivec2 dragStart;
    ivec2 dragOffset;
    ivec2 dragDistance;
    bool mouseInside = false;
    bool isOK = false;
    bool isOk() const
    {
        return isOK;
    }
    virtual ~BaseCtrl()
    {
    }
    virtual guitheme_t* getTheme()
    {
        return &themes.getRef();
    }
    guitheme_mgr* getThemeMgr()
    {
        return &themes;
    }
    ivec2 getScaledSize()
    {
        return ivec2(m_size.x * 1.0 / m_scale, m_size.y * 1.0 / m_scale);
    }
    virtual void prerender(NVGcontext* nanovgCtxt, int32_t x, int32_t y, int32_t w, int32_t h, float ratio);
    void render(NVGcontext* nanovgCtxt, int32_t x, int32_t y, int32_t w, int32_t h, float ratio);
    virtual bool processGlobalKeyevent(KeyEvent& event)
    {
        return false;
    }
    virtual bool mouseDownPre()
    {
        return true;
    }
    bool hasInputFocus()
    {
        return guiFocused && canTakeInputFocus;
    }
    MouseHitEvt mouseHitEvt(MouseHitType _type);
    void focusGui(guibase* g);
    void mouseDown(ivec2 mousePos, int button, bool doubleclick);
    void mouseUp(ivec2 mousePos, int button);
    virtual void onCharInput(unsigned int codepoint);
    virtual void onKeyInput(int key, int scancode, int keyState, int mods, const char* key_name);
    void mouseScrolled(double xoffset, double yoffset);
    virtual void mouseMoved(ivec2 mousePos, ivec2 deltaPos);

    bool isCtrOrChildFocused(guibase* gui);
    bool isMouseInside()
    {
        return mouseInside;
    }
    virtual void onCursorEnter(int entered)
    {
        mouseInside = entered;
    }
    virtual void relayout();
    virtual void relayout(int32_t w, int32_t h);
    virtual void windowSizeChanged(int32_t w, int32_t h);
    virtual void openContextMenu(guictxtmenu_base* b, ivec2 pos, int flags = 1);
    virtual void closeContextMenu(){};
    void closeAllAppMenus()
    {
        closeAppMenusAtLvl(0);
    };
    virtual void closeAppMenusAtLvl(int startlvl){};
    virtual void closeAllContextMenus();
    virtual void openAppMenu(int lvl, guictxtmenu_base* b, ivec2 pos){};
    virtual void closePopup(){}; // close this window if its a popup window
    virtual bool hasContextMenu()
    {
        return false;
    };
    virtual void objectDragMove(guibase* g, MouseEvent& mevt);
    virtual void objectDragRelease(guibase* g, MouseEvent& mevt);
    bool captureMouse(guibase* gui);
    virtual String getClipboardText();
    virtual void setClipboardText(String s);
    virtual void requestRedraw()
    {
        this->window->requestRedraw();
    }
    // Only use this pointer for comparison!
    void onGuiRemoved(void* gui);
    virtual void resetMouseContext();
    virtual void onMenuOpen(ngui::Menu* menu)
    {
    }
    virtual ivec2 toScreenSpace(ivec2 p) = 0;
    void setDragged(guibase* g)
    {
        guiDragged = g;
    }
    virtual std::shared_ptr<guictr_layout> replaceContainerWith(guictr_base* ctr,
    		std::shared_ptr<guictr_layout> newContainer) {
    	return nullptr;
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
    bool closed = false;

public:
    bool hasCtxtMenu()
    {
        return this->ctxtmenu != NULL;
    }
    bool hasMenuWindow()
    {
        for (auto& w : menuWindows) {
            if (w.ctxt)
                return true;
        }
        return false;
    }
    window_main* mainWindow = NULL;
    window_main* contextWindow = NULL;
    // std::map<window_main*,window_main*> contextWindows;
    // std::map<window_main*,guictxtmenu_base*> ctxtmenus;
#if WINDOW_HAS_MENUBAR
    ngui::MenuBar menubar;
#endif
    AppCtrl();
    virtual ~AppCtrl();
    virtual void relayout(int32_t w, int32_t h) override = 0;
    virtual void onChildOverlayWindowClose(window_main*);
    void openContextMenu(guictxtmenu_base* b, ivec2 pos, int flags = 1) override;
    void openDialog(guidialog_base* b);
    void closeContextMenu() override;
    void openAppMenu(int lvl, guictxtmenu_base* b, ivec2 pos) override;
    void closeAppMenusAtLvl(int startlvl) override;
    bool hasContextMenu() override;
    virtual void onKeyInput(int key, int scancode, int keyState, int mods, const char* key_name) override;
    virtual void onCharInput(unsigned int codepoint) override;
    virtual void onMenuOpen(ngui::Menu* menu);
    virtual void updateMenubar();
    guictxtmenu_base* getContextMenu();
#if WINDOW_HAS_MENUBAR
    virtual ngui::MenuBar& getMenubar();
#endif

    virtual void closePopup(){}; // close this window if its a popup window

    virtual void focusReceived() = 0;
    virtual void focusLost() = 0;
    virtual bool filesDropMove(ivec2 pos, int kbmods)
    {
        return false;
    };
    virtual bool filesDropBegin(std::vector<String>& files, ivec2 pos, int kbmods)
    {
        return false;
    };
    virtual bool filesDropFinal(std::vector<String>& files, ivec2 pos, int kbmods)
    {
        return false;
    };

    virtual void menuCommand(const menucmd_t&& command){};
    virtual void onWindowClose()
    {
        // if (contextWindow && this->ctxtmenu) {
        // 	this->mainWindow->closeOverlay(contextWindow);
        // }
        if (this->ctxtmenu) {
            dbgassert(contextWindow);
            contextWindow->getCtrl()->closePopup();
            dbgassert(!this->ctxtmenu);
        }
    };

    bool onWindowCloseRequest()
    {
        if (!closed) {
            closed = true;
            return true;
        }
        return false;
    };

    virtual void onTick() = 0;
    virtual void initApp(int argc, char* argv[]) = 0;
    virtual bool init(window_main* window, NVGcontext* nanovg) = 0;
    virtual void postInit() = 0; /* OpenGL context exists in postInit */
    virtual void destroy() = 0;
    void onAppTick();
    void destroyControl();

protected:
    /**
     * openOverlayGui
     * @param b
     * @param pos
     * @param flags @see BASECTRL_WND_* defines
     */
    void openOverlayGui(guictxtmenu_base* b, ivec2 pos, int flags);
    ivec2 toScreenSpace(ivec2 p) override
    {
        ivec2 windowPos;
        this->mainWindow->getPos(&windowPos);
        return windowPos + ivec2(vec2(p) * (1.0f / m_scale));
    }
    void dragContainerRelayout(drag_ctr_event evt) override {
    }
};
class guictr_scrollbar;
class PopupCtrl : public AppCtrl {
    guictr_scrollbar* popupCtrs = nullptr;
    bool bResizeable = false;

public:
    PopupCtrl();
    ~PopupCtrl();
    void destroy();
    bool isShown()
    {
        return this->window && this->window->isShown();
    }
    void closePopup() override;
    void relayout(int32_t w, int32_t h) override;
    void open(guictxtmenu_base* ctxtmenu, ivec2 pos, bool bResizeable);
    bool init(window_main* window, NVGcontext* nanovg);
    virtual void initApp(int argc, char* argv[]){};
    bool initPopup(window_overlay* window, NVGcontext* nanovg);
    void focusReceived(){};
    void focusLost();
    void onWindowClose();
    void onTick();
    void postInit(){}; /* OpenGL context exists in postInit */
    bool mouseDownPre();
};

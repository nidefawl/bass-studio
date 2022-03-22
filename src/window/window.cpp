#include "glheaders.h"
#include "tls.h"
#include "util/profiling.h"
#include <GLFW/glfw3.h>
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif
#include <nanovg.h>
#include <nanovg_gl.h>
#include "types.h"

#ifdef _WIN32
#include <Windows.h>
#include <Ole2.h>
#endif
#define WIN32API_CALLBACK_TYPE __stdcall

#include <cmath>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <map>
#include <glm/geometric.hpp>

#include "config.h"
#include "math/vec.h"
#include "math/seq_math.h"
#include "str_util.h"
#include "exceptions.h"
#include "color_util.h"
#include "mouse.h"
#include "keyboard.h"
#include "window.h"
#include "msgbox.h"
#include "menu.h"
#include "basectrl.h"
#include "droptargetlistener.h"
#include "platform.h"
#include "logging.h"
#include "appsettings.h"
#include "renderresources.h"
#include "mousecursor.h"
#include "fileio.h"
#include "thread.h"
#include "error.h"
#include "buildinfo.h"
#include "threads/workerthread.h"
#include "gl/gl_framebuffer.h"
#include "gl/gl_vbo.h"
#include "gl/gl_util.h"
#include "window_impl.h"

#ifdef _WIN32
#include "platform/win/windowsize.h"
#include "platform/win/platform_win.h"
#include "platform/win/DropTarget.h"
#endif
#ifdef __linux__
#include "platform/linux/x11_gtk_util.h"
#endif

class appwindow;
static std::vector<appwindow*> windowTimerHandleList;
static prof_stats_applicaton_t appStats;

void registerWindowTimer(appwindow* wnd) {
    windowTimerHandleList.push_back(wnd);
}

void unregisterWindowTimer(appwindow* wnd) {
    dbgassert(removeEntry(windowTimerHandleList, wnd));
}

void windowTickTimerRun();

volatile bool fatalError = false;

class reentrantblocker {
    bool& boolField;

public:
    explicit reentrantblocker(bool& _boolField) : boolField(_boolField) {
    }
    ~reentrantblocker() {
        boolField = false;
    }
    bool isReentrant() {
        return boolField;
    }
    bool check() {
        bool b    = boolField;
        boolField = true;
        return !b;
    }
};

#define PREVENT_REENTRANT(reentrant_err_msg)        \
    reentrantblocker block(reentrant);              \
    if (!block.check()) {                           \
        dbgassert(0 && (reentrant_err_msg));        \
        throw applogicexception(reentrant_err_msg); \
    }


void handleStdException(std::exception& e) {
    try {
        log_lf(Log::L_ERROR, "std::exception: %s\n", e.what());
        logStackTrace();
    } catch (std::exception&) {
        dbgassert(0);
    }
    fatalError = true;
}

void on_terminate() {
    log_lf(Log::L_ERROR, "on_terminate was called\n");
    //exit(1); // required on mingw (at least)
}
namespace RenderResources {
    void initResources(NVGcontext* vg);// renderresources.cpp
}

namespace MouseCursors {
    void initCursors();// mousecursor.cpp
}

static void glfw_runtime_error_callback(int error, const char* description) {
    log_lf(Log::L_ERROR, "glfw-error %d: %s\n", error, description);
}

static void setAppWindowHints() {
    glfwDefaultWindowHints();

#ifdef NANOVG_GL3
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);
#endif

#ifndef NDEBUG
    // debug context
    glfwWindowHint(GLFW_CONTEXT_DEBUG, GL_TRUE);
#else
    glfwWindowHint(GLFW_CONTEXT_DEBUG, GL_FALSE);
#endif

    glfwWindowHint(GLFW_SAMPLES, 0);
    glfwWindowHint(GLFW_STENCIL_BITS, 8);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
}

static void initOGL() {
    static bool gladInitialized = false;
    if (!gladInitialized) {
        gladInitialized = true;
        //TODO: check actual required extensions availability
        if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
            throw appexception("Required OpenGL extensions not present.\nConsider updating graphics drivers");
        }
    }
}

static void showerror(const char* description) {
    ngui::show(description, "Error", ngui::Style::Error, ngui::Buttons::OK);
}

void invalidateWindowContents(GLFWwindow* glfw) {
#ifdef _WIN32
    InvalidateRect(glfwGetWin32Window(glfw), nullptr, FALSE);
#endif
#ifdef __linux__
    sendExposeEvent(glfw);
#endif
}

class appwindow_dialog;
class appwindow_overlay;

#ifdef _WIN32

void syncMenu(HWND hwnd, ngui::MenuBar& menubar);       // menu_win32.cpp
ngui::Menu* getUserDataFromMenu(HMENU hmenu, UINT uPos);// menu_win32.cpp
LRESULT WIN32API_CALLBACK_TYPE appWndProc(HWND hwnd, UINT Msg, WPARAM wParam, LPARAM lParam);
bool restoreWindowPos(HWND hwnd, windowsize* size);
void saveWindowPos(HWND hwnd, windowsize* size);
#define IDT_TIMER1 0

#endif

class appwindow : protected DropTargetListener {
protected:
    appwindow* const parent;
    NVGcontext* nanovgCtxt = nullptr;
#ifdef _WIN32
    DropTarget* dropTarget = nullptr;
    HWND hwnd              = nullptr;
    WNDPROC defWndProc     = nullptr;
#endif

    std::vector<appwindow*> children;

    bool isSharedContextSlave = false;
    bool noRawInput           = false;
    bool bCanResize           = false;
    bool bIsVisible           = false;
    bool valid                = true;

    bool redrawFlagged       = false;
    bool reentrant           = false;
    bool bIsFirstTimeReload  = true;
    bool bFameRendered = false;
    int frameNumber          = 0;
    int frameCountFPS        = 0;
    int64_t tmLastFps        = 0;
    int64_t tmLastDrawMicros = 0;
    // int skipFrames           = 0;

    vec2 mousepos{ -10000, -10000 };
    int cursorIcon = CURSOR_DEFAULT;
    vec2 lastclickpos{ -10000, -10000 };
    vec2 lastmousepos{ -10000, -10000 };

    char nameDbg[128]{ 0 };
    char name[128]{ 0 };
    String fpsStats;
    prof_stats_window_t renderStatsWindow{};
    hires_timer_t timerProfileWindow;

public:
    GLFWwindow* glfw = nullptr;


    explicit appwindow(appwindow* _parent) 
        : parent(_parent),
        tmLastFps(getTimeMillis()) 
    {
#if BUILD_VSTHOST
        //TODO: settings might not be loaded at this point
        noRawInput = daw_tls::getSettings().vmmode;
#endif
    }

private:

    void reloadCustomShaders() {
        String strShaderSrcVertex;
        String strShaderSrcFragment;
        int64_t ret1 = ReadFileText("nanovg.vsh", strShaderSrcVertex);
        int64_t ret2 = ReadFileText("nanovg.fsh", strShaderSrcFragment);
        if (ret1 != -1 && ret2 != -1) {
            int statusErr = nvgReloadShaders(nanovgCtxt, StringAsCStr(strShaderSrcVertex), StringAsCStr(strShaderSrcFragment));
            if (statusErr && bIsFirstTimeReload) {
                throw appexception("Couldn't initialize nanovg");
            }
            bIsFirstTimeReload = false;
        }
    }

    void initContext() {
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
        glEnable(GL_BLEND);
        glEnable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_STENCIL_TEST);
        glDisable(GL_MULTISAMPLE);  
        glDepthFunc(GL_LEQUAL);
        glClearColor(0, 0, 0, 0);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBindVertexArray(0);
        glActiveTexture(GL_TEXTURE0);
        glfwSwapInterval(-1);
        // glfwSwapInterval(0);
        int flags = NVG_ANTIALIAS;
#ifndef NDEBUG
        flags |= NVG_DEBUG;
#endif
#ifdef NANOVG_GL2
        nanovgCtxt = nvgCreateGL2(flags);
#elif defined(NANOVG_GL3)
        nanovgCtxt = nvgCreateGL3(flags);
#endif
        if (!nanovgCtxt) {
            throw appexception("Couldn't initialize nanovg");
        }
        nvgShapeAntiAlias(nanovgCtxt, USE_NANOVG_AA);

        RenderResources::initResources(nanovgCtxt);
        MouseCursors::initCursors();//TODO: call MouseCursors::destroy() on exit of last instance
        reloadCustomShaders();
    }

public:

    bool isValid() {
        return valid;
    }

    GLFWwindow* getGLFW() {
        return glfw;
    }

#ifdef _WIN32
    HWND getHWND() {
        return hwnd;
    }
#endif

    void centerOnScreen(int screenIdx) {
        int monitors_length    = 0;
        GLFWmonitor** monitors = glfwGetMonitors(&monitors_length);
        if (monitors_length > screenIdx) {
            const GLFWvidmode* monitor_vidmode = glfwGetVideoMode(monitors[screenIdx]);
            if (monitor_vidmode != nullptr) {
                int monitor_x = 0, monitor_y = 0;
                int ww = 0, wh = 0;

                glfwGetMonitorPos(monitors[screenIdx], &monitor_x, &monitor_y);
                glfwGetWindowSize(glfw, &ww, &wh);
                setPos(ivec2(math::roundfS32(monitor_x + (monitor_vidmode->width * 0.5f) - ww / 2.0f),
                             math::roundfS32(monitor_y + (monitor_vidmode->height * 0.5f) - wh / 2.0f)));
            }
        }
    }

    void centerWindowOnParent() {
#ifdef _WIN32
        if (parent) {
            RECT rcOwner;
            RECT rcDlg;
            RECT rc;
            GetWindowRect(this->parent->getHWND(), &rcOwner);
            GetWindowRect(hwnd, &rcDlg);
            CopyRect(&rc, &rcOwner);
            OffsetRect(&rcDlg, -rcDlg.left, -rcDlg.top);
            OffsetRect(&rc, -rc.left, -rc.top);
            OffsetRect(&rc, -rcDlg.right, -rcDlg.bottom);
            SetWindowPos(hwnd,
                         HWND_TOP,
                         rcOwner.left + (rc.right / 2),
                         rcOwner.top + (rc.bottom / 2),
                         0, 0,// Ignores size arguments.
                         SWP_NOSIZE);
        }
#endif
    }

    virtual void renderWindowAndChildren() {
        PREVENT_REENTRANT("REENTRANT IN RENDER MAIN")
        for (appwindow* ow : this->children) {
            ow->renderWindowAndChildren();
        }
        if (valid && bIsVisible) {
            renderWindow();
            if (bFameRendered) {
                // log_printf("render %012zx %s\n", (uint64_t)this, nameDbg);
                updateStats();
            }
        }
    }

    virtual void swapBufferAndChildren() {
        PREVENT_REENTRANT("REENTRANT IN RENDER MAIN")
        for (appwindow* ow : this->children) {
            ow->swapBufferAndChildren();
        }
        if (valid && bIsVisible) {
            swapBuffers();
        }
    }

    virtual void flagNeedsRedraw() {
        appStats.numRedrawReq++;
        // invalidateWindowContents(glfw);
    }

    void updateStats() {
        auto tmNowMicros = getTimeMicros();
        auto tmNow = tmNowMicros / 1000LL;
        if (frameCountFPS > 0 && tmNow - tmLastFps >= 1000) {
            float fps = frameCountFPS*1000.0f / static_cast<float>(tmNow - tmLastFps);
            fpsStats = StringFormat("%.2f fps, %f", fps, static_cast<double>(tmLastDrawMicros)/1.0e6);
            glfwSetWindowTitle(glfw, StringAsCStr(fpsStats));
            tmLastFps    = tmNow;
            frameCountFPS = 0;
        }
        tmLastDrawMicros = tmNowMicros;
        redrawFlagged   = false;
        frameCountFPS++;
        frameNumber++;
    }

    void killTimer() {
        unregisterWindowTimer(this);
    }

    void destroyGL() {
        if (glfw) {
            glfwMakeContextCurrent(glfw);
        }
        if (nanovgCtxt) {
#ifdef NANOVG_GL2
            nvgDeleteGL2(nanovgCtxt);
#elif defined(NANOVG_GL3)
            nvgDeleteGL3(nanovgCtxt);
#endif
            nanovgCtxt = nullptr;
        }
#ifdef _WIN32
        if (hwnd) {
            RemovePropW(hwnd, L"GLFW");
            hwnd = nullptr;
        }
#endif
        if (glfw) {
            glfwSetWindowUserPointer(glfw, nullptr);
            glfwDestroyWindow(glfw);
            glfw = nullptr;
        }
    }

    void _onMouseMoved(double x, double y) {
        lastmousepos = mousepos;
        mousepos.x   = (float) x;
        mousepos.y   = (float) y;
        vec2 delta   = mousepos - lastmousepos;
        ivec2 idelta = ivec2((int) delta.x, (int) delta.y);
        onMouseMoved(idelta);
    }

    /* glfw callbacks */
    virtual void renderWindow() = 0;
    void swapBuffers() {
        if (bFameRendered) {
            // log_printf("swapBuffers %012zx %s\n", (uint64_t)this, nameDbg);
            timerProfileWindow.reset();
            glfwMakeContextCurrent(glfw);
            glfwSwapBuffers(glfw);
            renderStatsWindow.timeSwapBuffers = timerProfileWindow.getTimeReset();
        }
        bFameRendered = false;
    }
    virtual void onKeyInput(int key, int scancode, int action, int mods, const char* key_name) = 0;
    virtual void onMouseMoved(ivec2 deltapos) {
    }
    virtual void onMouseButton(int button, int action, int mods) {
    }
    virtual void onMouseScrolled(double xoffset, double yoffset) {
    }
    virtual void onCursorEnter(int entered) {
    }
    virtual void onCharInput(unsigned int codepoint) {
    }
    virtual void onWindowSizeChanged(int width, int height) {
    }
    virtual void onFramebufferSizeChanged(int width, int height) {
    }

    virtual void onWindowFocusChanged(int focused) {
        if (!focused) {
            releaseMouse();// fix mouse sometimes not getting released from controls that capture the mouse cursor
        }
    }

    /* not from glfw */
    virtual void onWindowClose()        = 0;
    virtual void onWindowCloseRequest() = 0;
    virtual void destroy()              = 0;
    virtual void onTick()               = 0;

    virtual void captureMouse() {
        if (!noRawInput) {
            glfwSetInputMode(glfw, GLFW_RAW_MOUSE_MOTION, 1);
        }
        glfwSetInputMode(glfw, GLFW_CURSOR, noRawInput ? GLFW_CURSOR_HIDDEN : GLFW_CURSOR_DISABLED);
    }

    virtual void releaseMouse() {
        if (!noRawInput) {
            glfwSetInputMode(glfw, GLFW_RAW_MOUSE_MOTION, 0);
        }
        glfwSetInputMode(glfw, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

    virtual bool isMouseCaptured() {
        return glfwGetInputMode(glfw, GLFW_CURSOR) != GLFW_CURSOR_NORMAL;
    }

    void createBaseWindow(const char* title, int w, int h, GLFWwindow* share = nullptr, void* parentWindowHandle = nullptr);

    void showWindow() {
        if (bIsVisible)
            return;
        bIsVisible = true;
        glfwShowWindow(glfw);
#ifdef __linux__
        glfwFocusWindow(glfw);
#endif
    }

    void hideWindow() {
        if (!bIsVisible)
            return;
        bIsVisible = false;
        glfwHideWindow(glfw);
        onWindowClose();
    }

    bool isWindowNotHidden() {
        return bIsVisible;
//         //TODO: keep track of window visible state locally
// #ifdef _WIN32
//         if (hwnd == nullptr)
//             return false;
//         return IsWindowVisible(hwnd) == 1;
// #else
//         //TODO: implement linux
//         return true;
// #endif
    }

    void maximize() {
        glfwMaximizeWindow(glfw);
    }

    bool filesDropBegin(std::vector<String>& files, ivec2 pos, int kbmods) override {
        return true;
    }

    bool filesDropMove(ivec2 pos, int kbmods) override {
        return true;
    }

    bool filesDropFinal(std::vector<String>& files, ivec2 pos, int kbmods) override {
        return true;
    }

    virtual void menuCommand(menucmd_t command) {
    }
    virtual void onMenuOpen(ngui::Menu* menu) {
    }

#ifdef _WIN32
    virtual LRESULT windowProc(HWND _hwnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
        switch (Msg) {
            case WM_COMMAND:
                menuCommand(menucmd_t{ LOWORD(wParam), "" });
                return 0;
#if WINDOW_HAS_MENUBAR
            case WM_INITMENUPOPUP: {
                HMENU hmenuPopup = (HMENU) wParam;       // handle of submenu
                UINT uPos        = (UINT) LOWORD(lParam);// submenu item position
                BOOL fSystemMenu = (BOOL) HIWORD(lParam);// window menu flag
                if (!fSystemMenu) {
                    ngui::Menu* menu = getUserDataFromMenu(hmenuPopup, uPos);
                    if (menu && menu->parent) {
                        onMenuOpen(menu->parent);
                    }
                    if (menu && !menu->parent) {
                        log_printf("menu %s has no parent \n", StringAsCStr(menu->title));
                    }
                }
                return 0;
            }
#endif
            default:
                break;
        }
        return CallWindowProc(defWndProc, _hwnd, Msg, wParam, lParam);
    }
#endif

    virtual void onChildDialogCreate(appwindow* child) {
        this->children.push_back(child);
    }

    virtual void onChildDialogClose(appwindow* child) {
        auto it = std::find(children.begin(), children.end(), child);
        if (it != children.end())
            children.erase(it);

        child->destroy();
        delete child;
    }

    virtual void onChildOverlayClose(appwindow* child) {
    }

    ivec2 getMousePos(float scale) {
        return ivec2(math::roundfS32(mousepos.x * scale), math::roundfS32(mousepos.y * scale));
    }

    void setPos(ivec2 pos) {
        glfwSetWindowPos(glfw, pos.x, pos.y);
    }

    void setSize(ivec2 size) {
        glfwSetWindowSize(glfw, size.x, size.y);
    }

    void getSize(ivec2* size) {
        glfwGetWindowSize(glfw, &size->x, &size->y);
    }

    void getPos(ivec2* pos) {
        int x = 0, y = 0;
        glfwGetWindowPos(glfw, &x, &y);
        pos->x = x;
        pos->y = y;
    }

    int getKeyMods_() {
        int shiftL = glfwGetKey(glfw, GLFW_KEY_LEFT_SHIFT);
        int shiftR = glfwGetKey(glfw, GLFW_KEY_RIGHT_SHIFT);
        int ctrlL  = glfwGetKey(glfw, GLFW_KEY_LEFT_CONTROL);
        int ctrlR  = glfwGetKey(glfw, GLFW_KEY_RIGHT_CONTROL);
        int altL   = glfwGetKey(glfw, GLFW_KEY_LEFT_ALT);
        int altR   = glfwGetKey(glfw, GLFW_KEY_RIGHT_ALT);
        int mods   = 0;
        if (altL || altR) {
            mods |= KB_MOD_ALT;
        }
        if (ctrlL || ctrlR) {
            mods |= KB_MOD_CTRL;
        }
        if (shiftL || shiftR) {
            mods |= KB_MOD_SHIFT;
        }
        return mods;
    }
};

class appwindow_main : public appwindow, public window_main {
    AppCtrl* const ctrl;
    std::shared_ptr<AppCtrl> sharedCtrl;
    int32_t windowCreationFlags      = 0;
    int64_t tmDblClick               = 0;
    // int64_t tmLastShaderReloadMillis = 0;
    bool bEnableWindowProfiling = false;
protected:
    void destroyOverlayWindows();

public:
    std::vector<std::shared_ptr<appwindow>> overlayWindows;
    std::vector<std::shared_ptr<appwindow>> overlayWindowsToClose;
    appwindow_main(appwindow_main* _parent, const std::shared_ptr<AppCtrl>& _ctrl)
        : appwindow(_parent),
          window_main(),
          ctrl(_ctrl.get()),
          sharedCtrl(_ctrl) {
        String windowName = typeName(*_ctrl);
        strncpy(name, StringAsCStr(windowName), sizeof(name));
        if (_parent) {
            windowName = String(_parent->nameDbg) + ".child[" + windowName + "]";
        }
        strncpy(nameDbg, StringAsCStr(windowName), sizeof(nameDbg));
        bEnableWindowProfiling = true;
        if (bEnableWindowProfiling) {
            window_base* ptr = this;
            Profiling::profilingRegisterEntry<prof_stats_window_t>(ptr, nameDbg);
        }
    }

    AppCtrl* getCtrl() override {
        return ctrl;
    }

    void postRender() override {
        swapBuffers();
    }

    void preRender() override {
        glfwMakeContextCurrent(glfw);

        int fbwidth = 0, fbheight = 0;
        glfwGetFramebufferSize(glfw, &fbwidth, &fbheight);

        glViewport(0, 0, fbwidth, fbheight);
    }

    void createMainWindow(int w, int h, appwindow_main* parentWindowHandle, int flags = 0);
    void initControl() override;

    void updateMenu() override {
#if WINDOW_HAS_MENUBAR
        ngui::MenuBar& menubar = ctrl->getMenubar();
#ifdef _WIN32
        syncMenu(hwnd, menubar);
#endif// _WIN32
#ifdef __linux__
        //TODO: implement linux
#endif// __linux__
#endif// WINDOW_HAS_MENUBAR
    }

    void flagNeedsRedraw() override {
        appwindow::flagNeedsRedraw();
    }

    window_main* createOverlay(std::shared_ptr<AppCtrl> ctrl, ivec2 windowSize, int flags) override;

    void closeOverlay(window_main* wnd) override {
        assert(wnd);
        auto it = std::find_if(overlayWindows.begin(), overlayWindows.end(), [wnd](const auto& e) {
            return dynamic_cast<window_base*>(e.get()) == dynamic_cast<window_base*>(wnd);
        });
        dbgassert(it != overlayWindows.end());
        wnd->hide();
        // extend lifetime of window to a point where click handlers have returned
        overlayWindowsToClose.push_back(*it);
        overlayWindows.erase(it);
    }

    bool canResize() override {
        return this->bCanResize;
    }

    int getCreationFlags() override {
        return this->windowCreationFlags;
    }

    void destroy() override;

    void setInvalid() override {
        this->valid = false;
    }

    void onTick() override {
        static bool reentrant = false;
        reentrantblocker block(reentrant);
        if (!block.check()) {
            return;
        }

        //overlay/child window lifetime management
        releaseOverlayWindows();

        /* if (getTimeMillis() - tmLastShaderReloadMillis >= 1000) {
            tmLastShaderReloadMillis = getTimeMillis();
            reloadCustomShaders();
        } */

        ctrl->onAppTick();
    }
    void releaseOverlayWindows() {
        if (!overlayWindowsToClose.empty()) {
            for (auto& window : overlayWindowsToClose) {
                auto handlerListSize = windowTimerHandleList.size();
                dbgassert(std::find(windowTimerHandleList.begin(), windowTimerHandleList.end(), window.get()) != windowTimerHandleList.end());
                window->destroy();
                dbgassert(std::find(windowTimerHandleList.begin(), windowTimerHandleList.end(), window.get()) == windowTimerHandleList.end());
                window.reset();
                dbgassert(handlerListSize != windowTimerHandleList.size());
            }
            overlayWindowsToClose.clear();
        }
    }

    void renderWindowAndChildren() override {
        if (cursorIcon != ctrl->cursorIcon) {
            glfwSetCursor(glfw, MouseCursors::cursors[ctrl->cursorIcon]);
            cursorIcon = ctrl->cursorIcon;
        }
        for (auto& w : overlayWindows)
            w->renderWindowAndChildren();
        appwindow::renderWindowAndChildren();
    }

    void swapBufferAndChildren() override {
        for (auto& w : overlayWindows)
            w->swapBufferAndChildren();
        appwindow::swapBufferAndChildren();
    }

    void renderWindow() override {
        timerProfileWindow.reset();
        glfwMakeContextCurrent(glfw);

        int winwidth = 0, winheight = 0;
        int fbwidth = 0, fbheight = 0;
        glfwGetWindowSize(glfw, &winwidth, &winheight);
        glfwGetFramebufferSize(glfw, &fbwidth, &fbheight);

        if (winwidth > 0 && winheight > 0 && fbwidth > 0 && fbheight > 0) {
            if (!ctrl->isOk()) {
                throw std::logic_error("invalid application state");
            }
            float pxratio = fbwidth / (float) winwidth;
            glViewport(0, 0, fbwidth, fbheight);
            ctrl->prerender(this->nanovgCtxt, 0, 0, winwidth, winheight, pxratio);
            renderStatsWindow.timePrerender = timerProfileWindow.getTimeReset();
            glViewport(0, 0, fbwidth, fbheight);
            if (ctrl->isVisible()) {
                ctrl->render(this->nanovgCtxt, 0, 0, winwidth, winheight, pxratio);
                bFameRendered = true;
            }
            renderStatsWindow.timeRender = timerProfileWindow.getTimeReset();
#if BUILD_VSTHOST
            if (bEnableWindowProfiling) {
                // NVGGLRenderStats nvglRenderStats;
                // nvglGetRenderStats(this->nanovgCtxt, &nvglRenderStats);
                window_base* ptr = this;
                Profiling::profilingCommitStats(ptr, frameNumber, renderStatsWindow);
            }
#endif
            renderStatsWindow = {};
        }
    }

    void onMouseMoved(ivec2 deltapos) override {
        if (math::abs(deltapos.x) + math::abs(deltapos.y) > 2)
            this->tmDblClick = 0;
        ctrl->mouseMoved(getMousePos(1.0f / ctrl->m_scale), deltapos);
    }

    void onMouseScrolled(double xoffset, double yoffset) override {
        ctrl->mouseScrolled(xoffset, yoffset);
    }

    void onMouseButton(int button, int action, int mods) override {
        if (action == GLFW_PRESS) {
            auto tmNow    = getTimeMillis();
            bool dblClick = this->tmDblClick != 0 && tmNow - this->tmDblClick < 500;
            dblClick &= glm::distance(lastclickpos, mousepos) < 4;
            this->tmDblClick = dblClick ? 0 : tmNow;
            ctrl->mouseDown(getMousePos(1.0f / ctrl->m_scale), button, dblClick);
        } else if (action == GLFW_RELEASE) {
            ctrl->mouseUp(getMousePos(1.0f / ctrl->m_scale), button);
        }
        lastclickpos = mousepos;
    }

    void onWindowSizeChanged(int width, int height) override {
        if (ctrl->isOK) {
            if (ctrl->m_size.x != width || ctrl->m_size.y != height) {
                ctrl->windowSizeChanged(width, height);
            }
        }
    }

    void onWindowFocusChanged(int focused) override {
        if (focused) {
            ctrl->focusReceived();
        } else {
            releaseMouse();// fix mouse sometimes not getting released from controls that capture the mouse cursor
            ctrl->focusLost();
        }
    }

    void onWindowCloseRequest() override {
        bool b = ctrl->onWindowCloseRequest();
        if (!b) {
            glfwSetWindowShouldClose(glfw, 0);
        }

        if (b) {
            if (!this->parent) {
                onWindowClose();
            } else {
                hideWindow();
            }
        }
    }

    void onWindowClose() override {
        ctrl->onWindowClose();
        if (parent) {
            parent->onChildOverlayClose(this);
        }
    }

    bool filesDropBegin(std::vector<String>& files, ivec2 pos, int kbmods) override {
        return ctrl->filesDropBegin(files, pos, kbmods);
    }

    bool filesDropMove(ivec2 pos, int kbmods) override {
        return ctrl->filesDropMove(pos, kbmods);
    }

    bool filesDropFinal(std::vector<String>& files, ivec2 pos, int kbmods) override {
        return ctrl->filesDropFinal(files, pos, kbmods);
    }

    void requestClose() override {
        glfwSetWindowShouldClose(glfw, 1);
    }

    void menuCommand(menucmd_t command) override {
#if WINDOW_HAS_MENUBAR
        ctrl->menuCommand(std::move(command));
#endif
    }

    void onMenuOpen(ngui::Menu* menu) override {
#if WINDOW_HAS_MENUBAR
        ctrl->onMenuOpen(menu);
#endif
    }

#ifdef _WIN32
    LRESULT windowProc(HWND _hwnd, UINT Msg, WPARAM wParam, LPARAM lParam) override {
        switch (Msg) {
            case WM_MOVING:
            case WM_MOVE:
            case WM_WINDOWPOSCHANGING:
            case WM_WINDOWPOSCHANGED:
            case WM_NCLBUTTONDOWN:
                if (this->ctrl->window == this) {
                    this->ctrl->closeAllContextMenus();
                }
                break;
        }
        return appwindow::windowProc(_hwnd, Msg, wParam, lParam);
    }
#endif

    void onCharInput(unsigned int codepoint) override {
        //log_lf(Log::L_DEBUG, "main onCharInput 0x%04X\n", codepoint);
        ctrl->onCharInput(codepoint);
    }
    void onKeyInput(int key, int scancode, int action, int mods, const char* key_name) override {
        /*if (action == GLFW_PRESS)*/
        //log_lf(Log::L_DEBUG, "keyname %s, key %d, scancode %d\n", key_name, key, scancode);
        //log_lf(Log::L_DEBUG, "mods %08X\n", mods);
        //log_lf(Log::L_DEBUG, "main onKeyInput %d (%c) %d\n", key, key, scancode);
        ctrl->onKeyInput(key, scancode, action, mods, key_name);
    }

    void onChildDialogClose(appwindow* child) override {
        appwindow::onChildDialogClose(child);
    }

    void onChildOverlayClose(appwindow* child) override;

    void captureMouse() override {
        appwindow::captureMouse();
    }

    void releaseMouse() override {
        appwindow::releaseMouse();
    }

    bool isMouseCaptured() override {
        return appwindow::isMouseCaptured();
    }

    void onCursorEnter(int entered) override {
        ctrl->onCursorEnter(entered);
        if (entered)
            glfwSetCursor(glfw, MouseCursors::cursors[cursorIcon]);
    }

    window_dialog* createDialog(const String& sTitle, int w, int h, std::shared_ptr<window_abstract_t>&& windowImpl) override;

    bool isShown() override {
        return appwindow::isWindowNotHidden();
    }

    void getSize(ivec2* size) override {
        return appwindow::getSize(size);
    }

    void getPos(ivec2* pos) override {
        return appwindow::getPos(pos);
    }

    void setPos(ivec2 pos) override {
        return appwindow::setPos(pos);
    }

    void setSize(ivec2 size) override {
        return appwindow::setSize(size);
    }

    void requestRedraw() override {
    }

    void setClipboardText(String s) override {
        glfwSetClipboardString(glfw, StringAsCStr(s));
    }

    String getClipboardText() override {
        const char* text = glfwGetClipboardString(glfw);
        String str;
        if (text) {
            str = text;
        }
        return str;
    }

    int getKeyMods() override {
        return getKeyMods_();
    }

    void updateWindowFromDlg() override {
        renderWindowAndChildren();
        swapBufferAndChildren();
    }

    void fireMouseMoved() override {
        onMouseMoved(ivec2(0));
    }

    void positionOnScreen(ivec2 pos, ivec2 size) override {
#ifdef _WIN32
        POINT p{pos.x, pos.y};
        HMONITOR hMonitor = MonitorFromPoint(p, MONITOR_DEFAULTTONEAREST);

        MONITORINFO mi;
        mi.cbSize = sizeof(mi);
        GetMonitorInfo(hMonitor, &mi);
        if (pos.x + size.x > mi.rcWork.right) {
            pos.x -= (pos.x + size.x) - mi.rcWork.right;
        }
        if (pos.y + size.y > mi.rcWork.bottom) {
            pos.y -= (pos.y + size.y) - mi.rcWork.bottom;
        }
#endif
        appwindow::setPos(pos);
#ifdef __linux__
        /* calling setSize on a hidden window makes the window visible (at the wrong location!)
         * As workaround on linux positionOnScreen is must be called twice:
         * Once before and once after appwindow::show() */
        if (bIsVisible) {
            appwindow::setSize(size);
        }
#else
        appwindow::setSize(size);
#endif
    }
    
    void setSizeLimits(ivec2 minSize, ivec2 maxSize) override {
        glfwSetWindowSizeLimits(glfw, minSize.x, minSize.y, maxSize.x, maxSize.y);
    }

    void focus() override {
        glfwFocusWindow(glfw);
    }

    void show() override {
        appwindow::showWindow();

        //TODO: add this function to GLFW
        //glfwBringWindowToTop(glfw);
#ifdef _WIN32
        BringWindowToTop(hwnd);
        if ((getCreationFlags() & WINDOW_IS_DIALOG) && parent) {
            SetActiveWindow(hwnd);
        }
#else
        glfwFocusWindow(glfw);
#endif
    }

    void hide() override {
        appwindow::hideWindow();
    }
};


void appwindow_main::onChildOverlayClose(appwindow* child) {

    appwindow::onChildOverlayClose(child);
    //TODO: add enum type field to appwindow
    appwindow_main* wndOverlay = dynamic_cast<appwindow_main*>(child);
    dbgassert(wndOverlay);
    if (wndOverlay) {
        this->ctrl->onChildOverlayWindowClose(wndOverlay);
    }
}

class appwindow_dialog : public appwindow, public window_dialog {
    std::shared_ptr<window_abstract_t> impl;
    const bool disablesParent = false;
public:
    explicit appwindow_dialog(appwindow* _parent, std::shared_ptr<window_abstract_t>&& windowImpl)
    : appwindow(_parent),
    impl(windowImpl)
    {
    }

    void createDialogWindow(const char* title, int w, int h, GLFWwindow* share = nullptr) {
        bCanResize = true;
        setAppWindowHints();
        glfwWindowHint(GLFW_RESIZABLE, bCanResize);
        glfwWindowHint(GLFW_VISIBLE, GL_FALSE);
        glfwWindowHint(GLFW_FOCUSED, GL_TRUE);
        //glfwWindowHint(GLFW_FLOATING, 1);
        strncpy(this->nameDbg, title, sizeof(this->nameDbg));
        appwindow::createBaseWindow(title, w, h, share);
#if 0
        LONG l = GetWindowLong(hwnd, GWL_EXSTYLE);
        if (parent) {
            SetWindowLong(hwnd, GWL_EXSTYLE, l & ~WS_EX_APPWINDOW);
        }
        SetWindowLong(hwnd, GWL_STYLE, WS_CAPTION | WS_POPUP | WS_CLIPSIBLINGS | WS_SYSMENU);
#endif
        if (parent) {
            this->parent->onChildDialogCreate(this);
            //glfwSetWindowAttrib(glfw, GLFW_FLOATING, GL_TRUE);
        }
        //glfwWindowHint(GLFW_FLOATING, 0);
        if (0 != impl->init(nanovgCtxt)) {
            glfwSetWindowShouldClose(glfw, 1);
        }
    }

    void destroy() override {
        if (!glfw)
            throw appexception("window null");
        appwindow::killTimer();
        glfwMakeContextCurrent(glfw);
        impl->destroy(nanovgCtxt);
        appwindow::destroyGL();
    }

    void renderWindow() override {
        glfwMakeContextCurrent(glfw);
        int winwidth = 0, winheight = 0;
        int fbwidth = 0, fbheight = 0;
        glfwGetWindowSize(glfw, &winwidth, &winheight);
        glfwGetFramebufferSize(glfw, &fbwidth, &fbheight);
        float pxratio = fbwidth / (float) winwidth;
        glViewport(0, 0, fbwidth, fbheight);
        bFameRendered = impl->render(nanovgCtxt, winwidth, winheight, pxratio) > 0;
    }

    void onWindowCloseRequest() override {
        onWindowClose();
    }

    void onWindowClose() override {
#ifdef _WIN32
        if (parent && disablesParent)
            EnableWindow(parent->getHWND(), TRUE);
#endif
#ifdef __linux__
            //TODO: implement linux
#endif
        glfwSetWindowUserPointer(glfw, nullptr);
        if (parent)
            this->parent->onChildDialogClose(this);
    }

    void onTick() override {
    }

    void onKeyInput(int key, int scancode, int action, int mods, const char* key_name) override {
        if (action == GLFW_PRESS) {
            if (key == GLFW_KEY_ESCAPE) {
                onWindowCloseRequest();
                return;
            }
        }
    }

    void show() override {
        appwindow::showWindow();
#ifdef _WIN32
        if (parent && disablesParent)
            EnableWindow(parent->getHWND(), FALSE);
#endif
#ifdef __linux__
            //TODO: implement linux window enable/disable
#endif
    }

    bool isShown() override {
        return appwindow::isWindowNotHidden();
    }

    void getSize(ivec2* size) override {
        return appwindow::getSize(size);
    }

    void getPos(ivec2* pos) override {
        return appwindow::getPos(pos);
    }

    void setPos(ivec2 pos) override {
        return appwindow::setPos(pos);
    }

    void setSize(ivec2 size) override {
        return appwindow::setSize(size);
    }

    void requestRedraw() override {
    }

    void setClipboardText(String s) override {
        glfwSetClipboardString(glfw, StringAsCStr(s));
    }

    String getClipboardText() override {
        const char* text = glfwGetClipboardString(glfw);
        String str;
        if (text) {
            str = text;
        }
        return str;
    }

    int getKeyMods() override {
        return getKeyMods_();
    }

    void captureMouse() override {
        appwindow::captureMouse();
    }

    void releaseMouse() override {
        appwindow::releaseMouse();
    }

    bool isMouseCaptured() override {
        return appwindow::isMouseCaptured();
    }

    void updateWindowFromDlg() override {
        renderWindowAndChildren();
        swapBufferAndChildren();
    }

    void fireMouseMoved() override {
        onMouseMoved(ivec2(0));
    }
};

window_main* appwindow_main::createOverlay(std::shared_ptr<AppCtrl> overlayCtrl, ivec2 windowSize, int flags) {


    //TODO: document lifetime of control
    std::shared_ptr<appwindow_main> ow = std::make_shared<appwindow_main>(this, overlayCtrl);

    //NOTE: GL context sharing is disabled (commented next line)
    // pass down parent window handle if overlayCtrl is companion overlayCtrl of daw (signaled by WINDOW_IS_MAINWINDOW_SLAVE)
    // appwindow_main* parentHandle = ((flags & WINDOW_IS_MAINWINDOW_SLAVE) != 0) ? this : nullptr;

    appwindow_main* parentHandle = nullptr;
    ow->createMainWindow(windowSize.x, windowSize.y, parentHandle, flags);
    if (!(flags & WINDOW_IS_MAINWINDOW_SLAVE)) {
        ow->initControl();
    }

    auto* ret = ow.get();
    this->overlayWindows.push_back(std::move(ow));
    return ret;
}

void appwindow_main::destroyOverlayWindows() {
    releaseOverlayWindows();
    for (std::shared_ptr<appwindow>& ow : this->overlayWindows) {
        ow->destroy();
        ow.reset();
    }
    this->overlayWindows.clear();
    std::vector<appwindow*> childWindowsCopy = this->children;
    for (appwindow* ow : childWindowsCopy) {
        ow->onWindowCloseRequest();
    }
    dbgassert(this->children.empty());
}

void appwindow_main::destroy() {
    dbgassert(glfw);
    if (this->ctrl) {
        glfwMakeContextCurrent(glfw);
        this->ctrl->closeAllContextMenus();
        glfwMakeContextCurrent(glfw);
        this->ctrl->releaseGarbageGuis();
        glfwMakeContextCurrent(glfw);
    }
    destroyOverlayWindows();
    appwindow::killTimer();
#if BUILD_VSTHOST
#ifdef _WIN32
    if (this->dropTarget) {
        UnregisterDropWindow(hwnd, this->dropTarget);
        this->dropTarget = nullptr;
    }
    if (!parent) {
        auto& settings = daw_tls::getSettings();
        if (windowCreationFlags & WINDOW_IS_MAINWINDOW_SLAVE) {
            saveWindowPos(hwnd, settings.wndCompanion.size.get());
        } else {
            saveWindowPos(hwnd, settings.wndMain.size.get());
        }
    }
#endif
#ifdef __linux__
    //TODO: implement linux
#endif
#endif
    if (this->ctrl) {
        glfwMakeContextCurrent(glfw);
        this->ctrl->destroyControl();
    }
    appwindow::destroyGL();
}

void appwindow_main::initControl() {
    if (!ctrl->initAppWindow(this, this->nanovgCtxt)) {
        throw appexception("Couldn't start application");
    }
#if BUILD_VSTHOST
#ifdef _WIN32
    this->dropTarget = RegisterDropWindow(hwnd, this);
    if (!parent) {
        auto& settings = daw_tls::getSettings();
        if (windowCreationFlags & WINDOW_IS_MAINWINDOW_SLAVE) {
            if (!restoreWindowPos(hwnd, settings.wndCompanion.size.get())) {
                this->maximize();
            }
        } else {

            if (!restoreWindowPos(hwnd, settings.wndMain.size.get())) {
                this->maximize();
            }
        }
    }
#endif
#ifdef __linux__
    //TODO: implement linux window pos
#endif
#endif
    int w, h;
    glfwGetWindowSize(glfw, &w, &h);
    this->onWindowSizeChanged(w, h);
}

void appwindow_main::createMainWindow(int w, int h, appwindow_main* parentWindowHandle, int flags) {
    windowCreationFlags = flags;
    bCanResize          = flags & WINDOW_IS_RESIZABLE;
    setAppWindowHints();
    glfwWindowHint(GLFW_VISIBLE, GL_FALSE);
    if (flags & WINDOW_BORDERLESS_POPUP) {
        glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
        glfwWindowHint(GLFW_VISIBLE, GL_FALSE);
        glfwWindowHint(GLFW_FOCUSED, GL_FALSE);
        glfwWindowHint(GLFW_DECORATED, GL_FALSE);
        //glfwWindowHint(GLFW_UTILITY_WINDOW, GL_TRUE);
        //glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GL_TRUE);
        glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GL_FALSE);
    }

    //glfwWindowHint(GLFW_FLOATING, parent != nullptr);
    appwindow::createBaseWindow(this->name, w, h, parentWindowHandle ? parentWindowHandle->glfw : nullptr, nullptr);

    if (flags & WINDOW_IS_MAINWINDOW_SLAVE) {
        //NOTE: GL context sharing with companion window is disabled!
        //this->nanovgCtxt = parentWindowHandle->nanovgCtxt;
    }

    if (flags & (WINDOW_IS_MAINWINDOW_SLAVE | WINDOW_IS_MAINWINDOW_MASTER)) {
        glfwSetWindowSizeLimits(glfw, 640, 480, GLFW_DONT_CARE, GLFW_DONT_CARE);
    } else {
        //glfwSetWindowAttrib(glfw, GLFW_FLOATING, GL_TRUE);
    }

    if ((flags & WINDOW_IS_DIALOG) && parent) {
#ifdef _WIN32
        SetWindowLongPtr(hwnd, GWLP_HWNDPARENT, (__int3264) (LONG_PTR) parent->getHWND());
#endif
    }
    if (flags & WINDOW_BORDERLESS_POPUP) {
        glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GL_FALSE);//set global state back to default
        glfwSetWindowAttrib(glfw, GLFW_FOCUS_ON_SHOW, 0);
#ifdef _WIN32
        //SetWindowLongPtr(hwnd, GWLP_HWNDPARENT, (__int3264) (LONG_PTR)parent->getHWND());
        LONG l = GetWindowLong(hwnd, GWL_EXSTYLE);
        l      = l & ~WS_EX_APPWINDOW;
        l      = l | WS_EX_TOOLWINDOW;
        SetWindowLong(hwnd, GWL_EXSTYLE, l);
        SetWindowLong(hwnd, GWL_STYLE, WS_CHILD | WS_CLIPSIBLINGS);
#endif
    } else {
        glfwSetWindowAttrib(glfw, GLFW_FOCUS_ON_SHOW, 1);
    }
}

#if defined(__linux__) || defined(__APPLE__)
void AppWndProc_enableBlockReentrant() {
}
void AppWndProc_disableBlockReentrant() {
}
#endif


static appwindow* getUserPointerFromGlfw(GLFWwindow* w) {
    if (!w) return nullptr;
    return static_cast<appwindow*>(glfwGetWindowUserPointer(w));
}

#ifdef _WIN32
int32_t AppWndProc_BlockReentrantEnabled = 0;

void AppWndProc_enableBlockReentrant() {
    AppWndProc_BlockReentrantEnabled++;
}

void AppWndProc_disableBlockReentrant() {
    AppWndProc_BlockReentrantEnabled--;
}

LRESULT WIN32API_CALLBACK_TYPE appWndProc(HWND hwnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
    if (AppWndProc_BlockReentrantEnabled > 0) {
        return DefWindowProc(hwnd, Msg, wParam, lParam);
    }
    try {
        auto* glfwWindow = static_cast<GLFWwindow*>(GetPropW(hwnd, L"GLFW"));
        appwindow* wu = getUserPointerFromGlfw(glfwWindow);
        if (wu && wu->isValid()) {
            return wu->windowProc(hwnd, Msg, wParam, lParam);
        }
        return DefWindowProc(hwnd, Msg, wParam, lParam);
    } catch (std::exception & e) {
        handleStdException(e);
    }
    return 0;
}
#endif

window_dialog* appwindow_main::createDialog(const String& sTitle, int w, int h, std::shared_ptr<window_abstract_t>&& windowImpl) {
    auto* windowDialog = new appwindow_dialog(this, std::move(windowImpl));
    windowDialog->createDialogWindow(StringAsCStr(sTitle), w, h, this->glfw);
    return windowDialog;
}

/* Wire up glfw C style callbacks to cpp class instance */
static void glfw_cb_mousepos(GLFWwindow* w, double x, double y) {
    try {
        appwindow* wu = getUserPointerFromGlfw(w);
        if (wu && wu->isValid())
            wu->_onMouseMoved(x, y);
    } catch (std::exception& e) {
        handleStdException(e);
    }
}

static void glfw_cb_mousebutton(GLFWwindow* w, int button, int action, int mods) {
    try {
        appwindow* wu = getUserPointerFromGlfw(w);
        if (wu && wu->isValid())
            wu->onMouseButton(button, action, mods);
    } catch (std::exception& e) {
        handleStdException(e);
    }
}

static void glfw_cb_cursorenter(GLFWwindow* w, int entered) {
    try {
        appwindow* wu = getUserPointerFromGlfw(w);
        if (wu && wu->isValid())
            wu->onCursorEnter(entered);
    } catch (std::exception& e) {
        handleStdException(e);
    }
}

static void glfw_cb_mousescroll(GLFWwindow* w, double xoffset, double yoffset) {
    try {
        appwindow* wu = getUserPointerFromGlfw(w);
        if (wu && wu->isValid())
            wu->onMouseScrolled(xoffset, yoffset);
    } catch (std::exception& e) {
        handleStdException(e);
    }
}

static void glfw_cb_keyinput(GLFWwindow* w, int key, int scancode, int action, int mods) {
    try {
        const char* key_name = glfwGetKeyName(key, scancode);
        appwindow* wu        = getUserPointerFromGlfw(w);
        if (wu && wu->isValid())
            wu->onKeyInput(key, scancode, action, mods, key_name);
    } catch (std::exception& e) {
        handleStdException(e);
    }
}

static void glfw_cb_charinput(GLFWwindow* w, unsigned int codepoint, int mods) {
    try {
        appwindow* wu = getUserPointerFromGlfw(w);
        if (wu && wu->isValid())
            wu->onCharInput(codepoint);
    } catch (std::exception& e) {
        handleStdException(e);
    }
}

/* static void glfw_cb_refresh(GLFWwindow* w) {
    try {
        appwindow* wu = getUserPointerFromGlfw(w);
        if (wu && wu->isValid())
            wu->renderWindowAndChildren();
    } catch (std::exception& e) {
        handleStdException(e);
    }
} */

static void glfw_cb_windowclose(GLFWwindow* w) {
    try {
        appwindow* wu = getUserPointerFromGlfw(w);
        if (wu && wu->isValid())
            wu->onWindowCloseRequest();
    } catch (std::exception& e) {
        handleStdException(e);
    }
}

static void glfw_cb_windowfocus(GLFWwindow* w, int focused) {
    try {
        appwindow* wu = getUserPointerFromGlfw(w);
        if (wu && wu->isValid())
            wu->onWindowFocusChanged(focused);
    } catch (std::exception& e) {
        handleStdException(e);
    }
}

static void glfw_cb_windowwize(GLFWwindow* w, int width, int height) {
    try {
        appwindow* wu = getUserPointerFromGlfw(w);
        if (wu && wu->isValid())
            wu->onWindowSizeChanged(width, height);
    } catch (std::exception& e) {
        handleStdException(e);
    }
}

static void glfw_cb_framebuffersize(GLFWwindow* w, int width, int height) {
    try {
        appwindow* wu = getUserPointerFromGlfw(w);
        if (wu && wu->isValid())
            wu->onFramebufferSizeChanged(width, height);
    } catch (std::exception& e) {
        handleStdException(e);
    }
}

void appwindow::createBaseWindow(const char* title, int w, int h, GLFWwindow* share, void* parentWindowHandle) {
    if (title != this->name) {
        strncpy(this->name, title, sizeof(this->name));
    }
    if (glfw)
        throw appexception("window not null");
    if (parentWindowHandle) {
        glfw = glfwCreateChildWindow(parentWindowHandle, w, h, title, share);
    } else {
        glfw = glfwCreateWindow(w, h, title, nullptr, share);
    }

    //TODO: glfw can be null here: happened when main window is minimized and DawCtrl::onTick tries to open tooltip after mouse-over timeout
    if (share) {
        this->isSharedContextSlave = true;
    }

    if (!glfw)
        throw appexception("Couldn't create window");
    glfwSetWindowUserPointer(glfw, this);
    glfwSetWindowCloseCallback(glfw, glfw_cb_windowclose);
    glfwSetWindowSizeCallback(glfw, glfw_cb_windowwize);
    // glfwSetWindowRefreshCallback(glfw, glfw_cb_refresh);
    glfwSetWindowFocusCallback(glfw, glfw_cb_windowfocus);
    glfwSetFramebufferSizeCallback(glfw, glfw_cb_framebuffersize);
    glfwSetCursorPosCallback(glfw, glfw_cb_mousepos);
    glfwSetMouseButtonCallback(glfw, glfw_cb_mousebutton);
    glfwSetScrollCallback(glfw, glfw_cb_mousescroll);
    glfwSetKeyCallback(glfw, glfw_cb_keyinput);
    //glfwSetCharCallback(glfw, glfw_cb_charinput);
    glfwSetCharModsCallback(glfw, glfw_cb_charinput);
    glfwSetCursorEnterCallback(glfw, glfw_cb_cursorenter);

    double mposx = 0., mposy = 0.;
    glfwGetCursorPos(glfw, &mposx, &mposy);
    mousepos = ivec2((int) mposx, (int) mposy);

#ifdef _WIN32
    hwnd = glfwGetWin32Window(glfw);
    if (!hwnd)
        throw appexception("Couldn't get win32 window handle");
#endif

    glfwMakeContextCurrent(glfw);

#ifdef _WIN32
    defWndProc = (WNDPROC) GetWindowLongPtr(hwnd, GWLP_WNDPROC);
    SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR) appWndProc);
#endif

    initOGL();
    initContext();

    ImageBuf imgBufDawIcon;
    if (ReadImage(StringFormat("icons/daw_icon.png"), imgBufDawIcon) > 0) {
        GLFWimage images[1];
        images[0].width  = imgBufDawIcon.w;
        images[0].height = imgBufDawIcon.h;
        images[0].pixels = imgBufDawIcon.bytes.data();
        glfwSetWindowIcon(glfw, 1, images);
    }

    registerWindowTimer(this);
}

void printLeakedGuiBase();

GLFWwindow* getGlfwFromWindowBase(window_base* w) {
    dbgassert(w);
    auto glfwWindow = dynamic_cast<appwindow*>(w);
    return glfwWindow ? glfwWindow->getGLFW() : nullptr;
}

void makeWindowContextCurrent(window_base* w) {
    if (w) {
        auto glfw = getGlfwFromWindowBase(w);
        dbgassert(glfw);
        if (glfw) {
            glfwMakeContextCurrent(glfw);
        }
    } else {
        glfwMakeContextCurrent(nullptr);
    }
}


#if HAS_MAIN_LOOP
#include "platform/win/debug_msg_count.h"
win32_hwnd_msg_counter_t msgCounter;
bool msgCounterEnabled = false;

#if defined(_WIN32) && BUILD_VSTHOST
namespace vst_window_mgr {
    void destroyAllVSTWindows();
    bool isVstWindow(HWND hwnd);
}
#endif


void dawinstance_startup_commands(const std::vector<String>& args, daw_tls::tlsinstance& tls);// Forward declare from startup.cpp
void initColor();                               // Forward declare from gui/gui.cpp
void openGlobalLog(const String& logFileName);  // Forward declare from util/logging.cpp
void closeGlobalLog();                          // Forward declare from util/logging.cpp

int startApplication(const std::vector<String>& args, AppInstanceService& appInstance) {
    seqthreads::registerThread("mainthread");

#if !defined(NDEBUG) && defined(_WIN32)
    _dup2(1, 2);//workaround: redirect stderr to stdout so stderr is visible when using gdb on eclipse (bug)
#endif

#ifdef _WIN32
    OleInitialize(0);
#endif

    std::set_terminate(on_terminate);

#ifdef USE_WIN32_EXC_HOOKS
    setExceptionHandler();
#endif

    bool openConsole      = false;
    int centerScreenIdx   = -1;
    String strLogFilename;
    auto logLevel = Log::LEVEL_ALL;
    try {
        for (size_t i = 0; i < args.size(); ++i) {
            if (args[i] == "--center" && i + 1 < args.size()) {
                centerScreenIdx = atoi(StringAsCStr(args[i + 1]));
                i++; 
                continue;
            }
            if (args[i] == "--logfile" && i + 1 < args.size()) {
                strLogFilename = args[i + 1]; 
                i++; 
                continue;
            }
            if (args[i] == "--log" && i + 1 < args.size()) {
                String level = StringToUpper(args[i + 1]);
                if (StrStartsWith(level, "ALL"))
                    logLevel = Log::LEVEL_ALL;
                if (StrStartsWith(level, "TRACE"))
                    logLevel = Log::L_TRACE;
                if (StrStartsWith(level, "DEBUG"))
                    logLevel = Log::L_DEBUG;
                if (StrStartsWith(level, "INFO"))
                    logLevel = Log::L_INFO;
                if (StrStartsWith(level, "WARN"))
                    logLevel = Log::L_WARN;
                if (StrStartsWith(level, "ERROR"))
                    logLevel = Log::L_ERROR;
                i++;
                continue;
            }
            if (args[i] == "--console") {
                openConsole = true;
                continue;
            }
        }

        App::Platform::initPlatformEnvironment("daw");

        //TODO: fix rpath/dll path to avoid loading unrelated dyn libs

        if (openConsole) {
            allocConsole();
        }
#ifdef _WIN32
        enableVirtTermProc();
#endif

        if (strLogFilename.length()) {
            openGlobalLog(App::Platform::toUserdataPath(strLogFilename));
        }

        getGlobalLogger()->setLevel(logLevel);

        if (logLevel <= Log::L_DEBUG) {
            char* pPath = getenv("PATH");
            if (pPath != nullptr) {
                log_out("PATH: %s\n", pPath);
            }

            log_out("BUILD_BINARY_NAME %s\n", BuildInfo::BUILD_BINARY_NAME);
            log_out("COMPILER_ID %s\n", BuildInfo::COMPILER_ID);
            log_out("COMPILE_OPTIONS %s\n", BuildInfo::COMPILE_OPTIONS);
            log_out("COMPILE_DEFS %s\n", BuildInfo::COMPILE_DEFS);
#ifdef _ITERATOR_DEBUG_LEVEL
            log_out("_ITERATOR_DEBUG_LEVEL %d\n", (int) _ITERATOR_DEBUG_LEVEL);
#endif
        }

        setMinimumResolutionTimer();
        initColor();

        glfwSetErrorCallback(glfw_runtime_error_callback);

#ifdef _WIN32
        glfwSetWin32WindowClassName(L"DAWWINDOW01");
#endif
        // glfwInitHint(GLFW_CONTEXT_KEEPCURRENT, 1);
        if (!glfwInit()) {
            showerror("Initialization failed. Couldn't initialize glfw");
            exit(EXIT_FAILURE);
        }

        std::shared_ptr<AppCtrl> ctrl = appInstance.makeApp(args);

        std::unique_ptr<appwindow_main> mainWindow = std::make_unique<appwindow_main>(nullptr, ctrl);
        mainWindow->createMainWindow(1280, 720, nullptr, WINDOW_IS_MAINWINDOW_MASTER);
#ifdef _WIN32
        setMainHWND(mainWindow->getHWND());
#endif
        mainWindow->initControl();

        mainWindow->showWindow();
        if (centerScreenIdx >= 0) {
            mainWindow->centerOnScreen(centerScreenIdx);
        }

        Profiling::profilingRegisterEntry<prof_stats_applicaton_t>(&appInstance, "Application Stats");

#ifndef NDEBUG
        enableGlDebugCallback();
#endif

        glfwSetErrorCallback(glfw_runtime_error_callback);

        appInstance.startApp(ctrl);

#if BUILD_VSTHOST
        daw_tls::tlsinstance& tls = daw_tls::getTls();
        dawinstance_startup_commands(args, tls);
#endif

        hires_timer_t hiresRuntime;
        hires_timer_t hiresTimer1;
        GLFWwindow* glfwHandle = mainWindow->getGLFW();
        int64_t tmHRLastTick   = hiresRuntime.getTime();
        int64_t tmHRLastDraw   = 0;
        int frameNumberStats   = 0;

#ifdef _WIN32
        int64_t tmLRLastCheck = tmHRLastTick / 1000L;
        int64_t tmHRMsgSent   = 0;
        int64_t tmLRDbgPrint  = tmHRLastTick / 1000L;
        char clsName_v[256];
        const bool debugMessageLoop = false;
        const DWORD timeout = 1;
#else
        const double timeoutEvent = 0.001;
#endif
        while (!fatalError) {
#ifdef _WIN32
            hiresTimer1.reset();
            /*auto timeoutResult = */MsgWaitForMultipleObjects(0, nullptr, FALSE, timeout, QS_ALLINPUT);
            int64_t maxMsgProcess = 1024;
            while (!fatalError && maxMsgProcess > 0) {
                MSG msg{};
                if (!PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                    break;
                }
                maxMsgProcess--;
                appStats.numMessagesProcessed++;
                switch (msg.message) {
                    case WM_QUIT:
                        glfwSetWindowShouldClose(glfwHandle, 1);
                        break;
                    case (WM_APP + 42):
                        log_lf(Log::L_DEBUG, "MSG took %d ms to get through\n", (hiresRuntime.getTime() - tmHRMsgSent));
                        tmHRMsgSent = 0;
                        break;
                    case WM_PAINT: 
                        if (msgCounterEnabled) {
                            if (GetClassNameA(msg.hwnd, clsName_v, 256))
                                msgCounter.incrPaints(clsName_v);
                        }
                        appStats.numMessagesWmPaint++;
                        [[fallthrough]];
                    default:
                        if (msgCounterEnabled) {
                            msgCounter.incrMessage(msg.message);
                        }
                        TranslateMessage(&msg);
                        DispatchMessageW(&msg);
                        break;
                }
            }
            int64_t tmMsgLoop = hiresTimer1.getTimeReset();
            glfwUpdateWin32Internals();
#endif
            int64_t tmUpdateInternals = hiresTimer1.getTimeReset();
#ifndef _WIN32
            glfwWaitEventsTimeout(timeoutEvent);
#endif
            if (glfwWindowShouldClose(glfwHandle)) {
                break;
            }
            int64_t tmHRNow = hiresRuntime.getTime();
            int64_t tmLRNow = tmHRNow/1000L;
            const auto timerDelayTarget_us = 20000L;
            if (tmHRNow - tmHRLastTick >= timerDelayTarget_us) {//TODO: figure out good tick rate
                appStats.tickTimerDelay = tmHRNow - tmHRLastTick - timerDelayTarget_us;
                tmHRLastTick            = tmHRNow;
                hiresTimer1.reset();
                windowTickTimerRun();
                appStats.tickTimerDuration = hiresTimer1.getTimeReset();
                Profiling::profilingCommitStats(&appInstance, frameNumberStats, appStats);
                appStats = prof_stats_applicaton_t{};
                frameNumberStats++;
                tmHRNow = hiresRuntime.getTime();
            }
            const int64_t minFrameDelayMicros = 1'000'000LL / 100LL;
            if (tmHRNow - tmHRLastDraw >= minFrameDelayMicros) {
                hiresTimer1.reset();

                // glfwMakeContextCurrent(mainWindow->getGLFW());
                mainWindow->renderWindowAndChildren();
                appStats.timeRefreshAll = hiresTimer1.getTimeReset();
                mainWindow->swapBufferAndChildren();
                appStats.timeSwapBuffersAll = hiresTimer1.getTimeReset();
                glfwMakeContextCurrent(nullptr);

                tmHRLastDraw = tmHRNow = hiresRuntime.getTime();
            }
#ifdef _WIN32
            if (debugMessageLoop) {
                if (tmLRNow - tmLRDbgPrint >= 1000) {
                    tmLRDbgPrint = tmLRNow;
                    log_lf(Log::L_DEBUG, "maxMsgProcessesed %d tmMsgLoop %zd, tmUpdateInternals %zd\n", static_cast<int>(1024 - maxMsgProcess), tmMsgLoop, tmUpdateInternals);
                }
                if (tmHRMsgSent > 0 && tmHRNow - tmHRMsgSent >= 1000000L) {
                    tmHRMsgSent = 0;
                }
                if (tmLRNow - tmLRLastCheck >= 1000 && tmHRMsgSent == 0) {
                    tmLRLastCheck = tmLRNow;
                    log_lf(Log::L_DEBUG, "PostMessage WM_APP + 42\n");
                    tmHRMsgSent = hiresRuntime.getTime();
                    PostMessage(mainWindow->getHWND(), WM_APP + 42, 0, 0);
                }
            }
#endif
        }
        mainWindow->destroy();

#if defined(_WIN32) && BUILD_VSTHOST
        vst_window_mgr::destroyAllVSTWindows();
#endif

        mainWindow.reset();

        glfwTerminate();

    } catch (std::exception& e) {
        handleStdException(e);
    }

    appInstance.deleteApp();
    printLeakedGuiBase();
    FrameBuffer::printLeaked();
    DrawVBO::printLeaked();
    if (fatalError) {
        log_printf("EXIT_FAILURE\n");
    } else {
        log_printf("EXIT_SUCCESS\n");
    }
    if (strLogFilename.length()) {
        closeGlobalLog();
    }
#ifdef _WIN32
    OleUninitialize();
#endif
    return fatalError ? 1 : 0;
}

#endif// HAS_MAIN_LOOP

void windowTickTimerRun() {
    std::vector<appwindow*> localWindowTimerHandleList = windowTimerHandleList;
    for (appwindow* window : localWindowTimerHandleList) {
        if (STL_CONTAINS(windowTimerHandleList, window)) {
            if (window->isWindowNotHidden()) {
                window->onTick();
            }
        }
    }
}

#if (BUILD_VSTHOST || BUILD_EXTERNAL_PLUGIN)
#include "plugins/plugin-window.h"
#include "plugins/plugincontrol.h"
#include <vstsdk-host-2.4/aeffect.h>
#include <vstsdk-host-2.4/aeffectx.h>
#include <vstsdk-plugin-2.4/aeffeditor.h>

class appwindow_plugin : public appwindow_main, public pluginwindow {
    bool isInitialized = false;

public:
    ERect _rect{ 0 };
    appwindow_plugin(AudioEffect* _effect, std::shared_ptr<PluginControl> _ctrl, int w, int h)
        : appwindow_main(nullptr, _ctrl),
          pluginwindow(_ctrl) {
        this->effect = _effect;
        setRect(0, 0, w, h);
    }

    ~appwindow_plugin() override {
        if (isInitialized) {
            log_printf("Plugin window was not correctly de-initialized\n");
        }
    }

    // start pluginwindow overrides
    void onSetParameter(int32_t index, float value) override {
        this->ctrlShared->onSetParameter(index, value);
    }

    void destroyContextAndWindow() override {
        isInitialized = false;
        if (!glfw)
            throw appexception("glfw null");
        destroyOverlayWindows();
        appwindow::killTimer();
        glfwMakeContextCurrent(glfw);
        // appwindow::destroyGL();
#ifdef _WIN32
        if (hwnd) {
            RemovePropW(hwnd, L"GLFW");
            hwnd = nullptr;
        }
#endif
        glfwDestroyWindow(glfw);
        glfw = nullptr;
    }
    // end pluginwindow overrides

    //start aeffect AEffEditor overrides
    void setRect(int x, int y, int width, int height) {
        _rect.left   = x;
        _rect.top    = y;
        _rect.right  = x + width;
        _rect.bottom = y + height;
    }

    bool getRect(ERect** rect) override {
        *rect = &_rect;
        return true;
    }

    void createPluginWindow(const char* title, int w, int h, void* parentWindowHandle) {
        setAppWindowHints();
        appwindow::createBaseWindow(title, w, h, nullptr, parentWindowHandle);
        RenderResources::initResources(nanovgCtxt);

        if (!ctrlShared->initAppWindow(this, this->nanovgCtxt)) {
            throw appexception("Couldn't start application");
        }
    }

    bool open(void* ptr) override {
        try {
            AEffEditor::open(ptr);
            if (ptr) {
                isInitialized = true;
                setAppWindowHints();
                int windowWidth  = _rect.right - _rect.left;
                int windowHeight = _rect.bottom - _rect.top;
                createPluginWindow("plugin-window", windowWidth, windowHeight, ptr);
                this->valid = true;
                glfwGetWindowSize(glfw, &windowWidth, &windowHeight);
                this->onWindowSizeChanged(windowWidth, windowHeight);
                this->valid = false;
                ctrlShared->onGuiOpen(effect);
                return true;
            }
        } catch (std::exception& e) {
            String excDesc = StringFormat("Fatal error: %s", e.what());
            ngui::show(StringAsCStr(excDesc), "Error", ngui::Style::Error, ngui::Buttons::OK);
        } catch (...) {
            ngui::show("FATAL", "Error", ngui::Style::Error, ngui::Buttons::OK);
        }
        AEffEditor::close();
        return false;
    }

    void close() override {
        try {
            if (isInitialized) {
                glfwMakeContextCurrent(glfw);
            }
            ctrlShared->onGuiClose(effect);
            if (isInitialized) {
                hideWindow();
                destroyContextAndWindow();
            }
            this->valid = false;
        } catch (std::exception& e) {
            String excDesc = StringFormat("Fatal error: %s", e.what());
            ngui::show(StringAsCStr(excDesc), "Error", ngui::Style::Error, ngui::Buttons::OK);
        } catch (...) {
            ngui::show("FATAL", "Error", ngui::Style::Error, ngui::Buttons::OK);
        }
        AEffEditor::close();
    }

    ///< Receive key down event. Return true only if key was really used!
    bool onKeyDown(VstKeyCode & keyCode) override {
        return false;
    }
    ///< Receive key up event. Return true only if key was really used!
    bool onKeyUp(VstKeyCode & keyCode) override {
        return false;
    }
    ///< Handle mouse wheel event, distance is positive or negative to indicate wheel direction.
    bool onWheel(float distance) override {
        return false;
    }
    ///< Set knob mode (if supported by Host). See CKnobMode in VSTGUI.
    bool setKnobMode(VstInt32 val) override {
        return false;
    }

    //end aeffect overrides

    void idle() override {
        if (isInitialized) {
            flagNeedsRedraw();
        }
    }
};

pluginwindow* createPluginWindow(AudioEffect * _effect, std::shared_ptr<PluginControl> _ctrl, int w, int h) {
    return new appwindow_plugin(_effect, std::move(_ctrl), w, h);
}

#endif

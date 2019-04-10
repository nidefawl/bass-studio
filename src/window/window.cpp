#include "glheaders.h"
#include <GLFW/glfw3.h>
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif
#define NANOVG_GL3 1
#include <nanovg.h>
#include <nanovg_gl.h>
#include <nanovg_gl_utils.h>
#include <stdint.h>

#ifdef _WIN32
#include <windows.h>
#include <ole2.h>
#endif
#define WIN32API_CALLBACK_TYPE __stdcall

#include <math.h>
#include <chrono>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <map>
#include <glm/geometric.hpp>

#ifdef _WIN32
#include "../platform/win/winheaders.h"
#include "../platform/win/DropTarget.h"
#endif
#ifdef __linux__
#include "../platform/linux/x11_gtk_util.h"
#endif

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
#include "threads.h"
#include "error.h"


volatile bool fataError = false;
void enableGlDebugCallback();

class reentrantblocker {
	bool& boolField;
public:
	reentrantblocker(bool& _boolField) : boolField(_boolField) {
	}
	~reentrantblocker() {
		boolField = false;
	}
	bool isReentrant() {
		return boolField;
	}
	bool check() {
		bool b = boolField;
		boolField = true;
		return !b;
	}
};
#define PREVENT_REENTRANT(reentrant_err_msg) 	\
	static bool reentrant = false; 				\
	reentrantblocker block(reentrant); 			\
	if (!block.check()) {						\
		assert(0&&reentrant_err_msg);		    \
		throw new applogicexception(reentrant_err_msg); \
	}
#define EXC_TRY try {
#define EXC_CATCH \
	} catch (std::exception& e) { 									\
		handleStdException(e);										\
	}

void handleStdException(std::exception& e) {
	getGlobalLogger()->logStr(StringFormat("Exception: %s\n", e.what()));
	logStackTrace();
	fataError = true;
	std::terminate();
}

void on_terminate() {
	getGlobalLogger()->logStr("on_terminate\n");
	exit(1); // required on mingw (at least)
}
void on_unexpected() {
	getGlobalLogger()->logStr("on_unexpected\n");
	logStackTrace();
	fataError = true;
	exit(1); // required on mingw (at least)
}

namespace RenderResources {
void initResources(NVGcontext* vg); // renderresources.cpp
}
namespace MouseCursors {
void initCursors(); // mousecursor.cpp
}


static void glfw_cb_mousepos(GLFWwindow *w, double x, double y);
static void glfw_cb_mousebutton(GLFWwindow *w, int button, int action, int mods);
static void glfw_cb_mousescroll(GLFWwindow *w, double xoffset, double yoffset);
static void glfw_cb_cursorenter(GLFWwindow *w, int entered);
static void glfw_cb_keyinput(GLFWwindow *w, int key, int scancode, int action, int mods);
static void glfw_cb_charinput(GLFWwindow *w, unsigned int codepoint);
static void glfw_cb_refresh(GLFWwindow *w);
static void glfw_cb_windowclose(GLFWwindow *w);
static void glfw_cb_windowfocus(GLFWwindow *w, int focused);
static void glfw_cb_windowwize(GLFWwindow *w, int width, int height);
static void glfw_cb_framebuffersize(GLFWwindow *w, int width, int height);

static void glfw_startup_error_callback(int error, const char* description) {
	my_printf("glfw-error %d: %s\n", error, description);
}
static void glfw_runtime_error_callback(int error, const char* description) {
	my_printf("glfw-error %d: %s\n", error, description);
}
static void setAppWindowHints() {
	glfwDefaultWindowHints();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_STENCIL_BITS, 8);
	glfwWindowHint(GLFW_DEPTH_BITS, 24);
	glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_FALSE);
}
static void showerror(const char* description) {
	ngui::show(description, "Error", ngui::Style::Error, ngui::Buttons::OK);
}

void invalidateWindowContents(GLFWwindow* glfw) {
#ifdef _WIN32
		InvalidateRect(glfwGetWin32Window(glfw), NULL, FALSE);
#endif
#ifdef __linux__
		sendExposeEvent(glfw);
#endif
}

#ifdef DAWFRAMEWORK_PLUGIN
#define HAS_APP_SETTINGS 0
#else
#define HAS_APP_SETTINGS 1
#endif

#if HAS_APP_SETTINGS
appsettings settings;
#endif

class appwindow_dialog;
class appwindow_overlay;

#ifdef _WIN32
void syncMenu(HWND hwnd, ngui::MenuBar& menubar); // menu_win32.cpp
ngui::Menu* getUserDataFromMenu(HMENU hmenu, UINT uPos); // menu_win32.cpp
LRESULT WIN32API_CALLBACK_TYPE appWndProc(HWND hwnd, UINT Msg, WPARAM wParam, LPARAM lParam);
static VOID WIN32API_CALLBACK_TYPE timerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);
bool restoreWindowPos(HWND hwnd, windowsize* size);
void saveWindowPos(HWND hwnd, windowsize* size);
#define IDT_TIMER1 0
#endif

class appwindow : protected DropTargetListener {
private:
	std::vector<appwindow*> children;
	uint64_t last = 0;
protected:
	char name[32]{ 0 };
	int cursorIcon = CURSOR_DEFAULT;
	vec2 lastclickpos{ -10000, -10000 };
	vec2 lastmousepos{ -10000, -10000 };
	vec2 mousepos{ -10000, -10000 };
public:
	GLFWwindow *glfw = NULL;
protected:
	NVGcontext* nanovgCtxt = NULL;
	bool isExternalWindow = false;
	bool noRawInput = false;
#ifdef _WIN32
	UINT_PTR timer = 0;
	DropTarget* dropTarget = NULL;
	HWND hwnd = NULL;
	WNDPROC defWndProc = NULL;
#endif
	bool valid = true;
public:
	bool isValid() {
		return valid;
	}
	void setInvalid() {
		this->valid = false;
	}
	void setValid() {
		this->valid = true;
	}
private:
	int calls = 0;
	uint64_t tm_lastfps = 0;
	String fpsStats;
	double secondsLastDraw = 0.0;
//	double secondsLastDrawReq = 0.0;
	const double minFrameDelay = 1/288.0;
	bool redrawFlagged = false;
	void initOGL() {
		//static code
		static bool gladInitialized = false;
		if (!gladInitialized) {
			gladInitialized = true;
			// doesn't actually check availability
			//TODO: check actual required extensions availability
#ifdef USE_GLAD_GL_HEADERS
			if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
				throw appexception("Required OpenGL extensions not present.\nConsider updating graphics drivers");
			}
#endif
		}
	}
	void initContext() {
//		glfwSwapInterval(-1);
		nanovgCtxt = nvgCreateGL3(NVG_ANTIALIAS | NVG_DEBUG);
		if (!nanovgCtxt) {
			throw appexception("Couldn't initialize nanovg");
		}
		int font = nvgCreateFont(nanovgCtxt, "sans", StringAsCStr(toCWDPath("res/fonts/Roboto-Regular.ttf")));
		if (font == -1) {
			throw appexception("Failed loading font");
		}
		glEnable(GL_BLEND);
		glDisable(GL_CULL_FACE);
		glDisable(GL_DEPTH_TEST);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
public:
	appwindow() :
	  tm_lastfps(getTimeMillis()) {
		name[0] = 0;
#if HAS_APP_SETTINGS
		noRawInput = settings.vmmode;
#endif
	}
	virtual ~appwindow() {
		if (hwnd) {
			RemovePropW(hwnd, L"GLFW");
		}
		if (glfw) {
			glfwSetWindowUserPointer(glfw, nullptr);
			glfwDestroyWindow(glfw);
		}
	}
	GLFWwindow* getGLFW() {
		return glfw;
	}
#ifdef _WIN32
	HWND getHWND() {
		return hwnd;
	}
#endif
	void onRefresh()
	{
		PREVENT_REENTRANT("REENTRANT IN RENDER MAIN")
		double delay = getSince(secondsLastDraw);
		if (delay > minFrameDelay) {
			render();
			endFrame();
		}
	}
	bool needsRefresh() {
		double delay = getSince(secondsLastDraw);
		return delay > minFrameDelay;
	}
	virtual void flagNeedsRedraw() {
//		double delay = getSince(secondsLastDrawReq);
//		if(delay > minFrameDelay*2) {
//			secondsLastDrawReq = getTimeHPC();
			invalidateWindowContents(glfw);
//		}
	}
	void endFrame() {

		uint64_t tm = getTimeMillis();
		double since = (tm - tm_lastfps) / 1000.0;
		if (calls > 0 && since >= 1.0) {
			double fps = calls / since;
			fpsStats = StringFormat("%.2f fps", fps);
			glfwSetWindowTitle(glfw, StringAsCStr(fpsStats));
			tm_lastfps = tm;
			calls = 0;
		}
		calls++;
		secondsLastDraw = getTimeHPC();
		redrawFlagged = false;
	}
	void killTimer() {
#ifdef _WIN32
		if (timer && hwnd) {
			KillTimer(hwnd, this->timer);
			my_printf("KillTimer\n", 0);
		}
#endif
	}
	void destroyGL() {
		if (nanovgCtxt) {
			nvgDeleteGL3(nanovgCtxt);
			nanovgCtxt = nullptr;
		}
	}
	void _onMouseMoved(double x, double y) {
		lastmousepos = mousepos;
		mousepos.x = (float)x;
		mousepos.y = (float)y;
		vec2 delta = mousepos - lastmousepos;
		ivec2 idelta = ivec2((int)delta.x, (int)delta.y);
		onMouseMoved(idelta);
	}

	/* glfw callbacks */
	virtual void render() = 0;
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
	virtual void onWindowFocusChanged(int focused) {
	}
	virtual void onWindowSizeChanged(int width, int height) {
	}
	virtual void onFramebufferSizeChanged(int width, int height) {
	}
	/* not from glfw */
	virtual void onWindowClose() = 0;
	virtual void onWindowCloseRequest() = 0;


	virtual void onTick() = 0;

	virtual void hideSystemCursor() {
		glfwSetInputMode(glfw, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
		if (!noRawInput) {
			glfwSetInputMode(glfw, GLFW_RAW_MOUSE_MOTION, 0);
		}
	}
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

	void createBaseWindow(const char* title, int w, int h, GLFWwindow* share = nullptr, void* parentWindowHandle = nullptr) {
		strncpy(this->name, title, 32);
		if (glfw)
			throw appexception("window not null");
		if (parentWindowHandle) {
			glfw = glfwCreateChildWindow(parentWindowHandle, w, h, title, share);
		} else {
			glfw = glfwCreateWindow(w, h, title, NULL, share);
		}
		if (!glfw)
			throw appexception("Couldn't create window");
		glfwSetWindowUserPointer(glfw, this);
		glfwSetWindowCloseCallback(glfw, glfw_cb_windowclose);
		glfwSetWindowSizeCallback(glfw, glfw_cb_windowwize);
		glfwSetWindowRefreshCallback(glfw, glfw_cb_refresh);
		glfwSetWindowFocusCallback(glfw, glfw_cb_windowfocus);
		glfwSetFramebufferSizeCallback(glfw, glfw_cb_framebuffersize);
		glfwSetCursorPosCallback(glfw, glfw_cb_mousepos);
		glfwSetMouseButtonCallback(glfw, glfw_cb_mousebutton);
		glfwSetScrollCallback(glfw, glfw_cb_mousescroll);
		glfwSetKeyCallback(glfw, glfw_cb_keyinput);
		glfwSetCharCallback(glfw, glfw_cb_charinput);
		glfwSetCursorEnterCallback(glfw, glfw_cb_cursorenter);
		double mposx, mposy;
		glfwGetCursorPos(glfw, &mposx, &mposy);
		mousepos = ivec2((int)mposx, (int)mposy);
#ifdef _WIN32
		hwnd = glfwGetWin32Window(glfw);
		if (!hwnd)
			throw appexception("Couldn't get win32 window handle");
#endif
		glfwMakeContextCurrent(glfw);
#ifdef _WIN32
		defWndProc = (WNDPROC)GetWindowLongPtr(hwnd, GWLP_WNDPROC);
		SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)appWndProc);
#endif
		initOGL();
		initContext();
#ifdef _WIN32
		this->timer = SetTimer(hwnd, 0, 1, (TIMERPROC)timerProc);
#endif
		last = getTimeMillis();
	}
	void showWindow() {
		glfwShowWindow(glfw);
	}
	void centerOnScreen(int screenIdx) {
	    int monitors_length;
	    GLFWmonitor **monitors = glfwGetMonitors(&monitors_length);
	    if (monitors_length > screenIdx) {
	        GLFWvidmode *monitor_vidmode = (GLFWvidmode*) glfwGetVideoMode(monitors[screenIdx]);
	        if(monitor_vidmode != NULL) {
	            int monitor_x, monitor_y;
	            glfwGetMonitorPos(monitors[screenIdx], &monitor_x, &monitor_y);
	    		int ww, wh;
	    		glfwGetWindowSize(glfw, &ww, &wh);
	            setPos(ivec2(
	            		monitor_x + (monitor_vidmode->width * 0.5) - ww/2,
						monitor_y + (monitor_vidmode->height * 0.5) - wh/2));
	        }
	    }
	}
	void hideWindow() {
		glfwHideWindow(glfw);
		//this->timer = SetTimer(hwnd, 0, 1, (TIMERPROC)NULL);
		onWindowClose();
	}
	bool isWindowNotHidden() {
		//TODO: keep track of window visible state locally
#ifdef _WIN32
		if (hwnd == NULL)
			return false;
		return IsWindowVisible(hwnd) == 1;
#else
		//TODO: implement linux
		return true;
#endif
	}
	void maximize() {
		glfwMaximizeWindow(glfw);
	}
    virtual bool filesDropBegin(std::vector<String>& files, ivec2 pos, int kbmods) {
    	return true;
    }
    virtual bool filesDropMove(ivec2 pos, int kbmods) {
    	return true;
    }
    virtual bool filesDropFinal(std::vector<String>& files, ivec2 pos, int kbmods) {
    	return true;
    }
    virtual void menuCommand(int cmd) {
    }
    virtual void onMenuOpen(ngui::Menu* menu) {
    }
#ifdef _WIN32
	virtual LRESULT windowProc(HWND _hwnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
		switch (Msg) {
		case WM_COMMAND:
			menuCommand(LOWORD(wParam));
			return 0;
#if WINDOW_HAS_MENUBAR
		case WM_INITMENUPOPUP:
		{
			HMENU hmenuPopup = (HMENU) wParam; // handle of submenu
			UINT uPos = (UINT) LOWORD(lParam); // submenu item position
			BOOL fSystemMenu = (BOOL) HIWORD(lParam); // window menu flag
			if (!fSystemMenu) {
				ngui::Menu* menu = getUserDataFromMenu(hmenuPopup, uPos);
				if (menu && menu->parent) {
					onMenuOpen(menu->parent);
				}
				if (menu && !menu->parent) {
					my_printf("menu %s has no parent \n", StringAsCStr(menu->title));
				}

			}
		}
#endif
		return 0;
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

		delete child;
	}
	virtual void onChildOverlayClose(appwindow* child) {
	}
	ivec2 getMousePos() {
		return ivec2((int)mousepos.x, (int)mousepos.y);
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
		int x, y;
		glfwGetWindowPos(glfw, &x, &y);
		pos->x = x;
		pos->y = y;
	}
	int getKeyMods_() {
		int shiftL = glfwGetKey(glfw, GLFW_KEY_LEFT_SHIFT);
		int shiftR = glfwGetKey(glfw, GLFW_KEY_RIGHT_SHIFT);
		int ctrlL = glfwGetKey(glfw, GLFW_KEY_LEFT_CONTROL);
		int ctrlR = glfwGetKey(glfw, GLFW_KEY_RIGHT_CONTROL);
		int altL = glfwGetKey(glfw, GLFW_KEY_LEFT_ALT);
		int altR = glfwGetKey(glfw, GLFW_KEY_RIGHT_ALT);
		int mods = 0;
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
class appwindow_main : public appwindow, public window_main  {
	AppCtrl* const ctrl;
	uint64_t dblclicktimer;
public:
	std::vector<std::unique_ptr<appwindow_overlay>> overlayWindows;
	appwindow_main(AppCtrl* _ctrl)
		: appwindow(),
		  window_main(),
		  ctrl(_ctrl) {
		dblclicktimer = 0;
	}
	void createMainWindow(const char* title, int w, int h, void* parentWindowHandle);
	void updateMenu();
	void flagNeedsRedraw() override {
		appwindow::flagNeedsRedraw();
		if (cursorIcon != ctrl->cursorIcon) {
			glfwSetCursor(glfw, MouseCursors::cursors[ctrl->cursorIcon]);
			cursorIcon = ctrl->cursorIcon;
		}
	}
	window_overlay* createOverlay();
	void destroyOverlayWindows();
	void destroy();
	void onTick() {
		PREVENT_REENTRANT("REENTRANT IN onTick")
//		flagNeedsRedraw();
		glfwMakeContextCurrent(glfw);
		ctrl->onTick();
	}
	void render()
	{
		glfwMakeContextCurrent(glfw);
		int winwidth, winheight;
		int fbwidth, fbheight;
		glfwGetWindowSize(glfw, &winwidth, &winheight);
		glfwGetFramebufferSize(glfw, &fbwidth, &fbheight);
		if (winwidth>0&&winheight>0&&fbwidth>0&&fbheight>0) {
			if (!ctrl->isOk()) {
				throw std::logic_error("invalid application state");
			}
			float pxratio = fbwidth / (float)winwidth;
			glViewport(0, 0, fbwidth, fbheight);
			glEnable(GL_BLEND);
			glDisable(GL_CULL_FACE);
			glDisable(GL_DEPTH_TEST);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			ctrl->prerender(0, 0, winwidth, winheight, pxratio);
			glViewport(0, 0, fbwidth, fbheight);
			static const vec4 clearc = int32vec4(0xff121212);
			glClearColor(clearc[0], clearc[1], clearc[2], clearc[3]);
			glStencilMask(~0);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
			ctrl->render(0, 0, winwidth, winheight, pxratio);
			glfwSwapBuffers(glfw);
		}
	}
	void onMouseMoved(ivec2 deltapos) {
		if (math::abs(deltapos.x)+math::abs(deltapos.y) > 2)
			this->dblclicktimer = 0;
		ctrl->mouseMoved(getMousePos(), deltapos);
		flagNeedsRedraw();
	}
	virtual void onMouseScrolled(double xoffset, double yoffset) {
		ctrl->mouseScrolled(xoffset, yoffset);
		flagNeedsRedraw();
	}
	void onMouseButton(int button, int action, int mods) {
		if (action == GLFW_PRESS) {
			uint64_t timeMillis = getTimeMillis();
			bool dblClick = this->dblclicktimer != 0 && timeMillis - this->dblclicktimer < 500;
			dblClick &= glm::distance(lastclickpos, mousepos) < 4;
			this->dblclicktimer = dblClick ? 0 : timeMillis;
			ctrl->mouseDown(getMousePos(), button, dblClick);
		} else if (action == GLFW_RELEASE) {
			ctrl->mouseUp(getMousePos(), button);
		}
		lastclickpos = mousepos;
		flagNeedsRedraw();
	}
	void onWindowSizeChanged(int width, int height) {
		ctrl->relayout(width, height);
		flagNeedsRedraw();
	}
	void onWindowFocusChanged(int focused) {
		if (focused) {
			ctrl->focusReceived();
		} else {
			ctrl->focusLost();
		}
		flagNeedsRedraw();
	}

	void onWindowCloseRequest() override {
		bool b = ctrl->onWindowCloseRequest();
		glfwSetWindowShouldClose(glfw, b ? 1 : 0);
		if (b) {
			onWindowClose();
		}
	}
	void onWindowClose() override {
		ctrl->onWindowClose();
	}
	bool filesDropBegin(std::vector<String>& files, ivec2 pos, int kbmods) {
		flagNeedsRedraw();
		return ctrl->filesDropBegin(files, pos, kbmods);
    }
	bool filesDropMove(ivec2 pos, int kbmods) {
		flagNeedsRedraw();
		return ctrl->filesDropMove(pos, kbmods);
    }
	bool filesDropFinal(std::vector<String>& files, ivec2 pos, int kbmods) {
		flagNeedsRedraw();
		return ctrl->filesDropFinal(files, pos, kbmods);
    }
    void requestClose() override {
		glfwSetWindowShouldClose(glfw, 1);
		onWindowClose();
    }
    void menuCommand(int cmd) {
#if WINDOW_HAS_MENUBAR
    	ctrl->menuCommand(cmd);
#endif
    }
    virtual void onMenuOpen(ngui::Menu* menu) {
#if WINDOW_HAS_MENUBAR
    	ctrl->onMenuOpen(menu);
#endif
    }
#ifdef _WIN32
	virtual LRESULT windowProc(HWND _hwnd, UINT Msg, WPARAM wParam, LPARAM lParam) override {
		if (this->ctrl->hasMenuWindow()) {
			bool dbg;
			dbg = !!_hwnd;
		}
		switch (Msg) {
		case WM_MOVING:
		case WM_MOVE:
		case WM_WINDOWPOSCHANGING:
		case WM_WINDOWPOSCHANGED:
		case WM_NCLBUTTONDOWN:
			this->ctrl->closeAllContextMenus();
			break;

		}
		return appwindow::windowProc(_hwnd, Msg, wParam, lParam);
	}
#endif

	void onCharInput(unsigned int codepoint) {
//		my_printf("main onCharInput 0x%04X\n", codepoint);
		ctrl->onCharInput(codepoint);
		flagNeedsRedraw();
	}
	void onKeyInput(int key, int scancode, int action, int mods, const char* key_name)
	{
		/*if (action == GLFW_PRESS)*/
//		my_printf("keyname %s, key %d, scancode %d\n", key_name, key, scancode);
//		my_printf("mods %08X\n", mods);
//		my_printf("main onKeyInput %d (%c) %d\n", key, key, scancode);
		ctrl->onKeyInput(key, scancode, action, mods, key_name);
		flagNeedsRedraw();
	}
	void onChildDialogClose(appwindow* child) override {
		glfwFocusWindow(this->glfw);
		appwindow::onChildDialogClose(child);
	}
	void onChildOverlayClose(appwindow* child) override;
	void captureMouse() {
		appwindow::captureMouse();
	}
	void releaseMouse() {
		appwindow::releaseMouse();
	}
	void hideSystemCursor() {
//		appwindow::hideSystemCursor();
	}
	bool isMouseCaptured() {
		return appwindow::isMouseCaptured();
	}
	void onCursorEnter(int entered) {
		ctrl->onCursorEnter(entered);
		if (entered)
			glfwSetCursor(glfw, MouseCursors::cursors[cursorIcon]);
	}
	window_dialog* createDialog();
	bool isShown() {
		return appwindow::isWindowNotHidden();
	}

	void getSize(ivec2* size) override {
		return appwindow::getSize(size);
	}
	void getPos(ivec2* pos) {
		return appwindow::getPos(pos);
	}
	void setPos(ivec2 pos) {
		return appwindow::setPos(pos);
	}
	void setSize(ivec2 size) {
		return appwindow::setSize(size);
	}
	void requestRedraw() {
		flagNeedsRedraw();
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
	void updateWindowFromDlg() {
		onRefresh();
	}

	void fireMouseMoved() override {
		onMouseMoved(ivec2(0));
	}
};

class appwindow_overlay : public appwindow, public window_overlay {
	uint64_t dblclicktimer;
public:
	appwindow* const parent;
	std::unique_ptr<PopupCtrl> popupCtrl;
	appwindow_overlay(appwindow* _parent)
		: appwindow(),
		  window_overlay(),
		  parent(_parent),
		  popupCtrl(std::make_unique<PopupCtrl>())
	{
		dblclicktimer = 0;
	}
	~appwindow_overlay() {

	}
	void destroy() {
		popupCtrl->destroy();
		glfwMakeContextCurrent(glfw);
		destroyGL();
		killTimer();
	}
	void createOverlayWindow(const char* title, int w, int h, void* parentHandle);
	void render()
	{
		glfwMakeContextCurrent(glfw);
		int winwidth, winheight;
		int fbwidth, fbheight;
		glfwGetWindowSize(glfw, &winwidth, &winheight);
		glfwGetFramebufferSize(glfw, &fbwidth, &fbheight);
		float pxratio = fbwidth / (float)winwidth;
		glViewport(0, 0, fbwidth, fbheight);
		static const vec4 clearc = int32vec4(0xff126612);
		glClearColor(clearc[0], clearc[1], clearc[2], clearc[3]);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		if (!popupCtrl->isOk()) {
			throw std::logic_error("invalid application state");
		}
		popupCtrl->render(0, 0, winwidth, winheight, pxratio);
		glfwSwapBuffers(glfw);
	}

	PopupCtrl* getCtrl() {
		return popupCtrl.get();
	}

	void onTick() {
	//	uint64_t tm = getTimeMillis();
	//	float f = (float)(tm / 1000.0);
	//	this->rgb[1] = 0.2f + sin(f*2.0f)*0.1f;
		//
#ifdef _WIN32
		UpdateWindow(this->hwnd);
#endif
		//TODO: implement linux
	}

	void show() {
		appwindow::showWindow();
		if (parent) {
//		    SetForegroundWindow(parent->getHWND());
//		    SetFocus(parent->getHWND());
//		    SetActiveWindow(parent->getHWND());
//		    SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
//		    BringWindowToTop(getHWND());

		}
		ShowWindow(hwnd, SW_SHOW);
//		if (parent)
//		SetParent(hwnd, parent->getHWND());
	}

	void hide() {
		appwindow::hideWindow();
	}

	bool isShown() {
		return appwindow::isWindowNotHidden();
	}

	void onWindowCloseRequest() override {
		bool b = popupCtrl->onWindowCloseRequest();
		glfwSetWindowShouldClose(glfw, b ? 1 : 0);
		if (b) {
			onWindowClose();
		}
	}
	void onWindowClose() override {
		popupCtrl->onWindowClose();
		parent->onChildOverlayClose(this);
	}

	void onMouseMoved(ivec2 deltapos) {
		if (math::abs(deltapos.x)+math::abs(deltapos.y) > 2)
			this->dblclicktimer = 0;
		popupCtrl->mouseMoved(getMousePos(), deltapos);
		flagNeedsRedraw();
	}
	void onMouseButton(int button, int action, int mods) {
		if (action == GLFW_PRESS) {
			uint64_t timeMillis = getTimeMillis();
			bool dblClick = this->dblclicktimer != 0 && timeMillis - this->dblclicktimer < 500;
			dblClick &= glm::distance(lastclickpos, mousepos) < 4;
			this->dblclicktimer = dblClick ? 0 : timeMillis;
			popupCtrl->mouseDown(getMousePos(), button, dblClick);
		} else if (action == GLFW_RELEASE) {
			popupCtrl->mouseUp(getMousePos(), button);
		}
		lastclickpos = mousepos;
		flagNeedsRedraw();
	}
	void onWindowFocusChanged(int focused) {
		if (focused) {
			popupCtrl->focusReceived();
		} else {
			popupCtrl->focusLost();
		}
		flagNeedsRedraw();
	}
	virtual void onMouseScrolled(double xoffset, double yoffset) {
		popupCtrl->mouseScrolled(xoffset, yoffset);
		flagNeedsRedraw();
	}
	void onWindowSizeChanged(int width, int height) {
		flagNeedsRedraw();
	}


	void onCharInput(unsigned int codepoint) {
//		my_printf("overlay onCharInput 0x%04X\n", codepoint);
		popupCtrl->onCharInput(codepoint);
		flagNeedsRedraw();
	}
	void onKeyInput(int key, int scancode, int action, int mods, const char* key_name)
	{
		/*if (action == GLFW_PRESS)*/
//		my_printf("keyname %s, key %d, scancode %d\n", key_name, key, scancode);
//		my_printf("mods %08X\n", mods);
//		my_printf("overlay onKeyInput %d (%c) %d\n", key, key, scancode);
		popupCtrl->onKeyInput(key, scancode, action, mods, key_name);
		flagNeedsRedraw();
	}

	void getSize(ivec2* size) override {
		return appwindow::getSize(size);
	}
	void getPos(ivec2* pos) {
		return appwindow::getPos(pos);
	}
	void setPos(ivec2 pos) {
		return appwindow::setPos(pos);
	}
	void setSize(ivec2 size) {
		return appwindow::setSize(size);
	}
	void requestRedraw() {
		flagNeedsRedraw();
	}
	void positionOnScreen(ivec2 pos, ivec2 size) {
#ifdef _WIN32
	    POINT p;
	    p.x = pos.x;
	    p.y = pos.y;
	    HMONITOR hMonitor = MonitorFromPoint(p, MONITOR_DEFAULTTONEAREST);

	    MONITORINFO mi;
	    mi.cbSize = sizeof(mi);
	    GetMonitorInfo(hMonitor, &mi);
		if (pos.x + size.x > mi.rcWork.right) {
			pos.x -= size.x;
		}
		if (pos.y + size.y > mi.rcWork.bottom) {
			pos.y -= size.y;
		}
#endif
#if __linux__
		//TODO: implement linux
#endif
        appwindow::setPos(pos);
        appwindow::setSize(size);
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
	void captureMouse() {
		appwindow::captureMouse();
	}
	void releaseMouse() {
		appwindow::releaseMouse();
	}
	void hideSystemCursor() {
//		appwindow::hideSystemCursor();
	}
	bool isMouseCaptured() {
		return appwindow::isMouseCaptured();
	}
	void onCursorEnter(int entered) {
		popupCtrl->onCursorEnter(entered);
		if (entered)
			glfwSetCursor(glfw, MouseCursors::cursors[cursorIcon]);
	}
	void updateWindowFromDlg() {
		onRefresh();
	}

	void fireMouseMoved() override {
		onMouseMoved(ivec2(0));
	}
};


void appwindow_main::onChildOverlayClose(appwindow* child) {
	appwindow::onChildOverlayClose(child);
	appwindow_overlay* wndOverlay = static_cast<appwindow_overlay*>(child);
	this->ctrl->onChildOverlayWindowClose(static_cast<window_overlay*>(wndOverlay));
}

int initDebugWindow();
class appwindow_dialog : public appwindow, public window_dialog {
public:
	void (*drawFn)(NVGcontext*,int,int,float) = NULL;
	appwindow *parent = NULL;
	appwindow_dialog(appwindow* _parent) : appwindow() {
		this->parent = _parent;
	}
	void createDialogWindow(const char* title, int w, int h, GLFWwindow* share = NULL) {
		setAppWindowHints();
		glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
		glfwWindowHint(GLFW_VISIBLE, GL_FALSE);
		glfwWindowHint(GLFW_FOCUSED, GL_TRUE);
		appwindow::createBaseWindow(title, w, h, share);
		if (parent)
		this->parent->onChildDialogCreate(this);

#ifdef _WIN32
		LONG l = GetWindowLong(hwnd, GWL_EXSTYLE);
		if (parent)
		SetWindowLong(hwnd, GWL_EXSTYLE, l & ~WS_EX_APPWINDOW);
		SetWindowLong(hwnd, GWL_STYLE, WS_CAPTION | WS_POPUP | WS_CLIPSIBLINGS | WS_SYSMENU);
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
				0, 0,          // Ignores size arguments.
				SWP_NOSIZE);
		}
#endif
#if __linux__
		//TODO: implement linux
#endif
	}
	bool isInit = false;
	void render()
	{
		glfwMakeContextCurrent(glfw);
		if (!isInit) {
			isInit = true;
			initDebugWindow();
		}
		int winwidth, winheight;
		int fbwidth, fbheight;
		glfwGetWindowSize(glfw, &winwidth, &winheight);
		glfwGetFramebufferSize(glfw, &fbwidth, &fbheight);
		float pxratio = fbwidth / (float)winwidth;
		glViewport(0, 0, fbwidth, fbheight);
		static const vec4 clearc = int32vec4(0x00000000);
		glClearColor(clearc[0], clearc[1], clearc[2], clearc[3]);
		glStencilMask(~0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

		if (drawFn) {
			drawFn(nanovgCtxt, winwidth, winheight, pxratio);
		}
		glfwSwapBuffers(glfw);
	}
	void onWindowCloseRequest() override {
		onWindowClose();
	}
	void onWindowClose() override {
#ifdef _WIN32
		if (parent)
		EnableWindow(parent->getHWND(), TRUE);
#endif
#if __linux__
		//TODO: implement linux
#endif
		my_printf("glfwSetWindowUserPointer\n", 0);
		glfwSetWindowUserPointer(glfw, NULL);
		if (parent)
		this->parent->onChildDialogClose(this);
		my_printf("END\n", 0);
	}
	void onTick() {
		flagNeedsRedraw();
	}
	void onKeyInput(int key, int scancode, int action, int mods, const char* key_name)
	{
		if (action == GLFW_PRESS) {
			if (key == GLFW_KEY_ESCAPE) {
				onWindowCloseRequest();
				return;
			}
		}
	}
	void show() {
		appwindow::showWindow();
#ifdef _WIN32
		if (parent)
			EnableWindow(parent->getHWND(), FALSE);
#endif
#if __linux__
		//TODO: implement linux
#endif
	}
	bool isShown() {
		return appwindow::isWindowNotHidden();
	}

	void getSize(ivec2* size) override {
		return appwindow::getSize(size);
	}
	void getPos(ivec2* pos) {
		return appwindow::getPos(pos);
	}
	void setPos(ivec2 pos) {
		return appwindow::setPos(pos);
	}
	void setSize(ivec2 size) {
		return appwindow::setSize(size);
	}
	void requestRedraw() {
		flagNeedsRedraw();
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
	void captureMouse() {
		appwindow::captureMouse();
	}
	void releaseMouse() {
		appwindow::releaseMouse();
	}
	void hideSystemCursor() {
//		appwindow::hideSystemCursor();
	}
	bool isMouseCaptured() {
		return appwindow::isMouseCaptured();
	}
	void updateWindowFromDlg() {
		onRefresh();
	}

	void fireMouseMoved() override {
		onMouseMoved(ivec2(0));
	}
};
void appwindow_main::updateMenu() {
#if WINDOW_HAS_MENUBAR
	ngui::MenuBar& menubar = ctrl->getMenubar();
#ifdef _WIN32
	syncMenu(hwnd, menubar);
#endif
#if __linux__
		//TODO: implement linux
#endif
#endif
}
window_overlay* appwindow_main::createOverlay() {
	std::unique_ptr<appwindow_overlay> ow = std::make_unique<appwindow_overlay>(this);
	String sName = StringFormat("%s menu", this->name);
	ow->createOverlayWindow(StringAsCStr(sName), 200, 200, nullptr);
	window_overlay* ret = ow.get();
	this->overlayWindows.push_back(std::move(ow));
	return ret;
}
void appwindow_main::destroyOverlayWindows() {
	for (std::unique_ptr<appwindow_overlay>& ow : this->overlayWindows) {
		ow->destroy();
		ow.reset();
	}
	this->overlayWindows.clear();
}
void appwindow_main::destroy() {
	if (!glfw)
		throw appexception("window null");
	glfwMakeContextCurrent(glfw);
	appwindow::destroyGL();
	appwindow::killTimer();
#ifndef DAWFRAMEWORK_PLUGIN
#ifdef _WIN32
	if (this->dropTarget)
		UnregisterDropWindow(hwnd, this->dropTarget);
	saveWindowPos(hwnd, settings.size);
#endif
#if __linux__
		//TODO: implement linux
#endif
#endif
}
void appwindow_main::createMainWindow(const char* title, int w, int h, void* parentWindowHandle) {
	setAppWindowHints();
	if (!parentWindowHandle)
		glfwWindowHint(GLFW_VISIBLE, GL_FALSE);
	appwindow::createBaseWindow(title, w, h, nullptr, parentWindowHandle);
	if (!parentWindowHandle) {
		glfwSetWindowSizeLimits(glfw, 640, 480, GLFW_DONT_CARE, GLFW_DONT_CARE);
	}
	RenderResources::initResources(nanovgCtxt);
	MouseCursors::initCursors(); //TODO: call MouseCursors::destroy() on exit of last instance

	if (!ctrl->init(this, this->nanovgCtxt)) {
		throw appexception("Couldn't start application");
	}
#ifndef DAWFRAMEWORK_PLUGIN
#ifdef _WIN32
	this->dropTarget = RegisterDropWindow(hwnd, this);
	if (!restoreWindowPos(hwnd, settings.size)) {
		this->maximize();
	}
#endif
#if __linux__
		//TODO: implement linux window pos
#endif
#endif
	glfwGetWindowSize(glfw, &w, &h);
	this->onWindowSizeChanged(w, h);
}
void appwindow_overlay::createOverlayWindow(const char* title, int w, int h, void* parentHandle) {
	setAppWindowHints();
	glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
	glfwWindowHint(GLFW_VISIBLE, GL_FALSE);
	glfwWindowHint(GLFW_FOCUSED, GL_FALSE);
	glfwWindowHint(GLFW_DECORATED, GL_FALSE);
	glfwWindowHint(GLFW_UTILITY_WINDOW, GL_TRUE);
	glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER , GL_TRUE);
	appwindow::createBaseWindow(title, w, h, nullptr, parentHandle);
	glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER , GL_FALSE);
#ifdef _WIN32
	bool tooltip = false;
	if (tooltip) {
		SetWindowLong(hwnd, GWL_EXSTYLE, WS_EX_TOPMOST|WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW);
		SetWindowLong(hwnd, GWL_STYLE, WS_CHILDWINDOW | WS_VISIBLE | WS_BORDER | WS_CLIPSIBLINGS);
	} else {

		LONG l = GetWindowLong(hwnd, GWL_EXSTYLE);
		l = l & ~WS_EX_APPWINDOW;
		l = l | WS_EX_TOOLWINDOW;
		SetWindowLong(hwnd, GWL_EXSTYLE, l);
		SetWindowLong(hwnd, GWL_STYLE, WS_CHILD | WS_CLIPSIBLINGS);
	}
	RECT rcOwner;
	RECT rcDlg;
	RECT rc;
	GetWindowRect(this->parent->getHWND(), &rcOwner);
	GetWindowRect(hwnd, &rcDlg);
	CopyRect(&rc, &rcOwner);
	OffsetRect(&rcDlg, -rcDlg.left, -rcDlg.top);
	OffsetRect(&rc, -rc.left, -rc.top);
	OffsetRect(&rc, -rcDlg.right, -rcDlg.bottom);
	if (tooltip) {
		SetWindowPos(hwnd,
			HWND_TOPMOST,
			rcOwner.left + (rc.right / 2),
			rcOwner.top + (rc.bottom / 2),
			0, 0,          // Ignores size arguments.
			SWP_NOSIZE|SWP_NOACTIVATE);
	} else {
		SetWindowPos(hwnd,
			HWND_TOP,
			rcOwner.left + (rc.right / 2),
			rcOwner.top + (rc.bottom / 2),
			0, 0,          // Ignores size arguments.
			SWP_NOSIZE);
	}
#endif
#if __linux__
	setIsTransientFor(this->parent->getGLFW(), this->getGLFW());
#endif
	RenderResources::initResources(nanovgCtxt);
	if (!popupCtrl->init(this, this->nanovgCtxt)) {
		throw appexception("Couldn't start application");
	}
}

#ifdef _WIN32
static VOID WIN32API_CALLBACK_TYPE timerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) {
	EXC_TRY
	appwindow* impl = NULL;
	GLFWwindow* glfwWindow = (GLFWwindow*) GetPropW(hwnd, L"GLFW");
	if (glfwWindow != NULL) {
		impl = (appwindow*)glfwGetWindowUserPointer(glfwWindow);
	}
	if (impl != NULL && impl->isValid()) {
		impl->onTick();
	}
	EXC_CATCH
}
LRESULT WIN32API_CALLBACK_TYPE appWndProc(HWND hwnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
	EXC_TRY
	appwindow* impl = NULL;
	GLFWwindow* glfwWindow = (GLFWwindow*) GetPropW(hwnd, L"GLFW");
	if (glfwWindow != NULL) {
		impl = (appwindow*)glfwGetWindowUserPointer(glfwWindow);
	}
	if (impl != NULL && impl->isValid()) {
		return impl->windowProc(hwnd, Msg, wParam, lParam);
	}
	return DefWindowProc(hwnd, Msg, wParam, lParam);
	EXC_CATCH
	return 0;
}
#endif
window_dialog* appwindow_main::createDialog() {
	appwindow_dialog* popupWindow = new appwindow_dialog(this);
	String sName = StringFormat("%s dialog", this->name);
	popupWindow->createDialogWindow(StringAsCStr(sName), 200, 200);
	return popupWindow;
}
static appwindow* getUserData(GLFWwindow *w) {
	appwindow* impl = (appwindow*) glfwGetWindowUserPointer(w);
	return impl;
}


static void glfw_cb_mousepos(GLFWwindow *w, double x, double y) {
	EXC_TRY
	appwindow* wu;
	if ((wu = getUserData(w)) && wu->isValid())
		wu->_onMouseMoved(x, y);
	EXC_CATCH
}
static void glfw_cb_mousebutton(GLFWwindow *w, int button, int action, int mods) {
	EXC_TRY
	appwindow* wu;
	if ((wu = getUserData(w)) && wu->isValid())
		wu->onMouseButton(button, action, mods);
	EXC_CATCH
}
static void glfw_cb_cursorenter(GLFWwindow *w, int entered) {
	EXC_TRY
	appwindow* wu;
	if ((wu = getUserData(w)) && wu->isValid())
		wu->onCursorEnter(entered);
	EXC_CATCH
}
static void glfw_cb_mousescroll(GLFWwindow *w, double xoffset, double yoffset) {
	EXC_TRY
	appwindow* wu;
	if ((wu = getUserData(w)) && wu->isValid())
		wu->onMouseScrolled(xoffset, yoffset);
	EXC_CATCH
}
static void glfw_cb_keyinput(GLFWwindow *w, int key, int scancode, int action, int mods) {
	EXC_TRY
	const char* key_name = glfwGetKeyName(key, scancode);
	appwindow* wu;
	if ((wu = getUserData(w)) && wu->isValid())
		wu->onKeyInput(key, scancode, action, mods, key_name);
	EXC_CATCH
}
static void glfw_cb_charinput(GLFWwindow *w, unsigned int codepoint) {
	EXC_TRY
	appwindow* wu;
	if ((wu = getUserData(w)) && wu->isValid())
		wu->onCharInput(codepoint);
	EXC_CATCH
}
static void glfw_cb_refresh(GLFWwindow *w) {
	EXC_TRY
	appwindow* wu;
	if ((wu = getUserData(w)) && wu->isValid())
		wu->onRefresh();
	EXC_CATCH
}
static void glfw_cb_windowclose(GLFWwindow *w) {
	EXC_TRY
	appwindow* wu;
	if ((wu = getUserData(w)) && wu->isValid())
		wu->onWindowCloseRequest();
	EXC_CATCH
}
static void glfw_cb_windowfocus(GLFWwindow *w, int focused) {
	EXC_TRY
	appwindow* wu;
	if ((wu = getUserData(w)) && wu->isValid())
		wu->onWindowFocusChanged(focused);
	EXC_CATCH
}
static void glfw_cb_windowwize(GLFWwindow *w, int width, int height) {
	EXC_TRY
	appwindow* wu;
	if ((wu = getUserData(w)) && wu->isValid())
		wu->onWindowSizeChanged(width, height);
	EXC_CATCH
}
static void glfw_cb_framebuffersize(GLFWwindow *w, int width, int height) {
	EXC_TRY
	appwindow* wu;
	if ((wu = getUserData(w)) && wu->isValid())
		wu->onFramebufferSizeChanged(width, height);
	EXC_CATCH
}

void printLeakedGuiBase();

static std::unique_ptr<appwindow_main> mainWindow;

GLFWwindow* getGlfwFromWindowBase(window_base* w) {
	return dynamic_cast<appwindow*>(w)->getGLFW();
}

#ifdef _WIN32
HWND getMainHWND() {
	return mainWindow ? mainWindow->getHWND() : NULL;
}
#endif
#ifndef TEST_PROJECT

struct data_t
{
	int id;
	int count;
};
#define MSG_LEN 10000
struct data_t messages[MSG_LEN] = {};
int maxIdx = 0;
void incrMessage(int id) {
	for (int i = 0; i < maxIdx; i++) {
		int storedId = messages[i].id;
		if (storedId == id) {
			messages[i].count++;
			return;
		}
	}
	messages[maxIdx].id = id;
	messages[maxIdx].count++;
	maxIdx++;
}
int getNumMsg() {
	return maxIdx;
}
int getMsgId(int i) {
	return messages[i].id;
}
int getMsgCnt(int i) {
	return messages[i].count;
}
std::map<String, int> hwndPaints;
int getHWNDMapSize() {
	return hwndPaints.size();
}
String getHWNDName(int i) {
	auto it = hwndPaints.begin();
	for (int j = 0; j < i; j++, it++);
	return it->first;
}
int getHWNDCnt(int i) {
	auto it = hwndPaints.begin();
	for (int j = 0; j < i; j++, it++);
	return it->second;
}
#ifndef DAWFRAMEWORK_PLUGIN
#if CREATE_DEBUG_COMPANION_WINDOW
void drawDebugWindow(NVGcontext* ctx, int winW, int winH, float pxratio);
#endif
#ifdef _WIN32
#ifndef BUILD_NO_VST
namespace vst_window_mgr {
void destroyAllVSTWindows();
bool isVstWindow(HWND hwnd);
}
#endif
#endif
std::shared_ptr<AppCtrl> makeApp();
void initColor(); // Forward declare from gui/gui.cpp
void deleteApp(); // Forward declare from host/mainctrl.cpp
void openGlobalLog(); // Forward declare from util/debug.cpp
void closeGlobalLog(); // Forward declare from util/debug.cpp

int startApplication(int argc, char* argv[]) {
	log_out("mainthread\n", 0);
	setCurrentThreadName("mainthread");
#ifndef NDEBUG
    _dup2( 1, 2 ); //workaround: redirect stderr to stdout so stderr is visible when using gdb on eclipse (bug)
#endif
#ifdef _WIN32
	OleInitialize(0);
#endif
	std::set_terminate(on_terminate);
	std::set_unexpected(on_unexpected);
#ifdef USE_WIN32_EXC_HOOKS
	setExceptionHandler();
#endif
	try {
	//bool runConsoleMode = false;
	//for (int i = 0; i < argc; i++) {
	//	if (strcmp(argv[i], "-console")) {
	//		runConsoleMode = true;
	//	}
	//}
	//if (!runConsoleMode) {
		allocConsole();
	//}
	openGlobalLog();
	setMinimumResolutionTimer();
	initColor();
	loadSettings(settings);
	glfwSetErrorCallback(glfw_startup_error_callback);
	if (!glfwInit("DAWWINDOW01")) {
		showerror("Initialization failed. Couldn't initialize glfw");
		exit(EXIT_FAILURE);
	}
	setAppWindowHints();
	std::shared_ptr<AppCtrl> ctrl = makeApp();
	ctrl->initApp(argc, argv);
	mainWindow = std::make_unique<appwindow_main>(ctrl.get());
	mainWindow->createMainWindow("main window", 1280, 720, nullptr);
	mainWindow->showWindow();
	enableGlDebugCallback();
#if CREATE_DEBUG_COMPANION_WINDOW
	{
		appwindow_dialog* w = new appwindow_dialog(NULL);
		w->drawFn=drawDebugWindow;
		int winW = 1280;
		int winH = 720;
		GLFWwindow* contextWindow = mainWindow->getGLFW();
		w->createDialogWindow("test window", winW, winH, contextWindow);
//		glfwMakeContextCurrent(w->getGLFW());
		w->centerOnScreen(0);
		w->showWindow();
//		glfwMakeContextCurrent(mainWindow->getGLFW());
	}
#endif
	glfwSetErrorCallback(glfw_runtime_error_callback);
	ctrl->postInit();
	GLFWwindow* glfwHandle = mainWindow->getGLFW();
	long start = getTimeMillis();
	while (!fataError && !glfwWindowShouldClose(glfwHandle)) {
#ifdef _WIN32
		DWORD timeout = 5;
		MsgWaitForMultipleObjects(0, NULL, FALSE, timeout, QS_ALLEVENTS);
	    MSG msg;
	    while (!fataError && PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
	    {
//	    	logEveryMsec(0, 5000, "Main msg loop");
	        if (msg.message == WM_QUIT)
	        {
	        	glfwSetWindowShouldClose(glfwHandle, 1);
	        }
	        else
	        {

	            switch (msg.message) {
#ifndef BUILD_NO_VST
	            	case WM_KEYDOWN:
					case WM_SYSKEYDOWN:
					case WM_KEYUP:
					case WM_SYSKEYUP: {
						if (vst_window_mgr::isVstWindow(msg.hwnd)) {
							msg.hwnd = mainWindow->getHWND();
						}
					}
#endif
					//no break
					default:
						TranslateMessage(&msg);
			            DispatchMessageW(&msg);
						break;
	            }


				incrMessage(msg.message);
				if (msg.message == WM_PAINT)
				{
					char clsName_v[256];
					GetClassNameA(msg.hwnd, clsName_v, 256);
					if (hwndPaints.count(clsName_v)) {
						hwndPaints[clsName_v] = hwndPaints.at(clsName_v)+1;
					} else {
						hwndPaints[clsName_v] = 1;
					}
				}
	        }
	    }
		glfwUpdateInternals();
#endif
#ifdef __linux__
		glfwWaitEventsTimeout(0.001);
		mainWindow->onRefresh();
#else
		if (getTimeMillis() - start > 0) {
			mainWindow->flagNeedsRedraw();
		}
#endif
	}
	mainWindow->setInvalid();
	ctrl->destroyControl();
	mainWindow->destroy();

	mainWindow->destroyOverlayWindows();
	if (!fataError) {
		saveSettings(settings);
	}

#ifndef BUILD_NO_VST
	vst_window_mgr::destroyAllVSTWindows();
#endif

	mainWindow.reset();
	glfwTerminate();

	} catch (std::exception& e) {
		handleStdException(e);
	}
	deleteApp();
	printLeakedGuiBase();
	if (fataError) {
		my_printf("EXIT_FAILURE\n", 0);
	} else {
		my_printf("EXIT_SUCCESS\n", 0);
	}
	closeGlobalLog();
#ifdef _WIN32
	OleUninitialize();
#endif
	return fataError ? 1 : 0;
}

#endif


#endif

#ifndef BUILD_NO_VST
#include "plugins/plugin-window.h"
#include "plugins/plugincontrol.h"
#include "plugins/handle-exceptions.h"
#include "../vstsdk-host-2.4/aeffect.h"
#include "../vstsdk-host-2.4/aeffectx.h"
#include "../vstsdk-plugin-2.4/aeffeditor.h"

class appwindow_plugin : public appwindow_main, public pluginwindow {
public:
	ERect _rect{ 0 };
	appwindow_plugin(AudioEffect *_effect, std::shared_ptr<PluginControl> _ctrl, int w, int h)
		: appwindow_main((AppCtrl*)_ctrl.get()),
		  pluginwindow(_ctrl)
	{
		this->effect = _effect;
		setRect(0, 0, w, h);
		effect->setEditor(this);
		isExternalWindow = true;
	}

	virtual ~appwindow_plugin() {
		my_printf("~pluginwindow_main()\n", 0);
	}

	void onSetParameter(int32_t index, float value) override {
		this->ctrlShared->onSetParameter(index, value);
	}
	//start aeffect AEffEditor overrides
	//-----------------------------------------------------------------------------
	void setRect(int x, int y, int width, int height)
	{
		_rect.left = x;
		_rect.top = y;
		_rect.right = x+width;
		_rect.bottom = y+height;
	}
	bool getRect(ERect **rect) override {
		*rect = &_rect;
		return true;
	}

	void createPluginWindow(const char* title, int w, int h, void* parentWindowHandle) {
		setAppWindowHints();
		appwindow::createBaseWindow(title, w, h, nullptr, parentWindowHandle);
		RenderResources::initResources(nanovgCtxt);

		if (!ctrlShared->init(this, this->nanovgCtxt)) {
			throw appexception("Couldn't start application");
		}
#ifdef _WIN32
		this->dropTarget = RegisterDropWindow(hwnd, this);
#endif
#if __linux__
		//TODO: implement linux
#endif

		glfwGetWindowSize(glfw, &w, &h);
		this->onWindowSizeChanged(w, h);
	}
	bool open(void *ptr) override {
		try {
		AEffEditor::open(ptr);
		if (ptr)
		{

			setAppWindowHints();

			createPluginWindow("plugin-window", _rect.right-_rect.left, _rect.bottom-_rect.top, ptr);
			assert(hwnd);
			assert(glfw);
			assert(nanovgCtxt);
			if (!ctrlShared->init(this, this->nanovgCtxt)) {
				throw appexception("Couldn't start application");
			}
			showWindow();
			guiOpen();
			return true;
		}
		EXC_CATCH_NO_THROW_DIALOG
		AEffEditor::close();
		destroyContextAndWindow();
		return false;
	}
	void close() override
	{
		if (this->systemWindow) {
			glfwMakeContextCurrent(glfw);
			guiClose();
			hideWindow();
			destroyContextAndWindow();
			AEffEditor::close();
		}
	}
	///< Receive key down event. Return true only if key was really used!
	virtual bool onKeyDown (VstKeyCode& keyCode) override	{
		return false;
	}
	///< Receive key up event. Return true only if key was really used!
	virtual bool onKeyUp (VstKeyCode& keyCode) override		{
		return false;
	}
	///< Handle mouse wheel event, distance is positive or negative to indicate wheel direction.
	virtual bool onWheel(float distance) override {
		return false;
	}
	///< Set knob mode (if supported by Host). See CKnobMode in VSTGUI.
	virtual bool setKnobMode (VstInt32 val) override			{ return false; }

	//end aeffect overrides

	virtual void guiOpen() {
		setValid();
		assert(hwnd);
		assert(glfw);
		assert(effect);
		assert(ctrlShared.get());
	    RECT area;
	    GetClientRect(hwnd, &area);
	    onWindowSizeChanged(area.right-area.left, area.bottom-area.top);
	    ctrlShared->onGuiOpen(effect);
	}
	virtual void guiClose() {
		setInvalid();
		ctrlShared->onGuiClose(effect);
	}
	virtual void destroyContextAndWindow() {
		destroy();
		if (glfw) {
			my_printf("glfwDestroyWindow %012X\n", (int64_t)glfw);
			glfwDestroyWindow(glfw);
			glfw = nullptr;
			hwnd = nullptr;
		}
//		wglMakeCurrent(NULL, NULL);
	}
};

AEffEditor* createPluginWindow(AudioEffect *_effect, std::shared_ptr<PluginControl> _ctrl, int w, int h) {
	appwindow_plugin* window = new appwindow_plugin(_effect, std::move(_ctrl), w, h);
	return window;
}
#endif


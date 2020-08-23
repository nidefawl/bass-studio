#include "glheaders.h"
#include <GLFW/glfw3.h>
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif
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
#include "../platform/win/platform_win.h"
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
#include "buildinfo.h"
#include "../threads/workerthread.h"
#include "window_impl.h"
#if HAS_JS_CONSOLE
#include "cli/console/console_thread.h"
#include "cli/console/commandline_rep.h"
#include "js/scripting.h"
#endif

class appwindow;
static std::vector<appwindow*> windowTimerHandleList;
void registerWindowTimer(appwindow* wnd) {
	windowTimerHandleList.push_back(wnd);
}
void unregisterWindowTimer(appwindow* wnd) {
	removeEntry(windowTimerHandleList, wnd);
}
void windowTickTimerRun();

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
	static bool reentrant = false;				\
	reentrantblocker block(reentrant); 			\
	if (!block.check()) {						\
		dbgassert(0&&reentrant_err_msg);		    \
		throw new applogicexception(reentrant_err_msg); \
	}
#define EXC_TRY try {
#define EXC_CATCH \
	} catch (std::exception& e) { 									\
		handleStdException(e);										\
	}

void handleStdException(std::exception& e) {
	getGlobalLogger()->logStr(StringFormat("std::exception: %s\n", e.what()));
	logStackTrace();
	fataError = true;
//	std::terminate();
}

void on_terminate() {
	getGlobalLogger()->logStr("on_terminate\n");
//	exit(1); // required on mingw (at least)
}
void on_unexpected() {
	getGlobalLogger()->logStr("on_unexpected\n");
	logStackTrace();
	fataError = true;
//	exit(1); // required on mingw (at least)
}

namespace RenderResources {
void initResources(NVGcontext* vg); // renderresources.cpp
}
namespace MouseCursors {
void initCursors(); // mousecursor.cpp
}

static void glfw_startup_error_callback(int error, const char* description) {
	my_printf("glfw-error %d: %s\n", error, description);
}
static void glfw_runtime_error_callback(int error, const char* description) {
	my_printf("glfw-error %d: %s\n", error, description);
}
static void setAppWindowHints() {
	glfwDefaultWindowHints();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	glfwWindowHint(GLFW_STENCIL_BITS, 8);
	glfwWindowHint(GLFW_DEPTH_BITS, 24);
	glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
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


#if HAS_APP_SETTINGS
appsettings settings;
#endif

class appwindow_dialog;
class appwindow_overlay;

#ifdef _WIN32

void syncMenu(HWND hwnd, ngui::MenuBar& menubar); // menu_win32.cpp
ngui::Menu* getUserDataFromMenu(HMENU hmenu, UINT uPos); // menu_win32.cpp
LRESULT WIN32API_CALLBACK_TYPE appWndProc(HWND hwnd, UINT Msg, WPARAM wParam, LPARAM lParam);
bool restoreWindowPos(HWND hwnd, windowsize* size);
void saveWindowPos(HWND hwnd, windowsize* size);
#define IDT_TIMER1 0
#endif

class appwindow : protected DropTargetListener {
protected:
	appwindow* const parent;
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
	bool isSharedContextSlave = false;
	bool noRawInput = false;
#ifdef _WIN32
	DropTarget* dropTarget = NULL;
	HWND hwnd = NULL;
	WNDPROC defWndProc = NULL;
#endif
	bool bCanResize = false;
	bool shown = false;
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
		if (isSharedContextSlave) {
			return;
		}
#ifdef NANOVG_GL2
		nanovgCtxt = nvgCreateGL2(NVG_ANTIALIAS | NVG_DEBUG);
#elif defined(NANOVG_GL3)
		nanovgCtxt = nvgCreateGL3(NVG_ANTIALIAS | NVG_DEBUG);
#endif
		if (!nanovgCtxt) {
			throw appexception("Couldn't initialize nanovg");
		}

		glEnable(GL_BLEND);
		glDisable(GL_CULL_FACE);
		glDisable(GL_DEPTH_TEST);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
public:
	appwindow(appwindow* _parent) : parent(_parent), tm_lastfps(getTimeMillis()) {
		name[0] = 0;
#if HAS_APP_SETTINGS
		noRawInput = settings.vmmode;
#endif
	}
	virtual ~appwindow() {
		dbgassert(std::find(windowTimerHandleList.begin(), windowTimerHandleList.end(), this) == windowTimerHandleList.end());

#ifdef _WIN32
		if (hwnd) {
			RemovePropW(hwnd, L"GLFW");
		}
#endif
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
				0, 0,          // Ignores size arguments.
				SWP_NOSIZE);
		}
#endif
	}
	virtual void onRefresh()
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
#if BUILD_VSTHOST
			daw_tls::tlsinstance& tls = daw_tls::getTls();
			tls.renderStats.fps = fps;
#endif
//			glfwSetWindowTitle(glfw, StringAsCStr(fpsStats));
			tm_lastfps = tm;
			calls = 0;
		}
		calls++;
		secondsLastDraw = getTimeHPC();
		redrawFlagged = false;
	}
	void killTimer() {
		unregisterWindowTimer(this);
	}
	void destroyGL() {
		if (nanovgCtxt) {
#ifdef NANOVG_GL2
			nvgDeleteGL2(nanovgCtxt);
#elif defined(NANOVG_GL3)
			nvgDeleteGL3(nanovgCtxt);
#endif
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
	virtual void destroy() = 0;


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

	void createBaseWindow(const char* title, int w, int h, GLFWwindow* share = nullptr, void* parentWindowHandle = nullptr);
	void showWindow() {
		if (shown)
			return;
		shown = true;
		log_printf("show window %s\n", this->name);
		glfwShowWindow(glfw);
#ifdef __linux__
		glfwFocusWindow(glfw);
#endif
	}
	void hideWindow() {
		if (!shown)
			return;
		shown = false;
		log_printf("hide window %s\n", this->name);
		glfwHideWindow(glfw);
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
    virtual void menuCommand(const menucmd_t&& command) {
    }
    virtual void onMenuOpen(ngui::Menu* menu) {
    }
#ifdef _WIN32
	virtual LRESULT windowProc(HWND _hwnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
		switch (Msg) {
		case WM_COMMAND:
			menuCommand(menucmd_t{LOWORD(wParam), ""});
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
	ivec2 getMousePos(float scale) {
		return ivec2((int)mousepos.x*scale, (int)mousepos.y*scale);
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
	std::shared_ptr<AppCtrl> sharedCtrl;
	uint64_t dblclicktimer;
	int32_t windowCreationFlags = 0;
//	WorkerThread workerThread;
	void destroyOverlayWindows();
public:
	String nameDbg;
	std::vector<std::shared_ptr<appwindow>> overlayWindows;
	appwindow_main(appwindow* _parent, std::shared_ptr<AppCtrl> _ctrl)
		: appwindow(_parent),
		  window_main(),
		  ctrl(_ctrl.get()),
		  sharedCtrl(_ctrl) {
		dblclicktimer = 0;
	}
//	WorkerThread* getWorkerThread() {
//		return &workerThread;
//	}

	AppCtrl* getCtrl() {
		return ctrl;
	}
	void postRender() override {
		glfwMakeContextCurrent(glfw);
		glfwSwapBuffers(glfw);
	}
	void preRender() override {
		glfwMakeContextCurrent(glfw);
		int winwidth, winheight;
		int fbwidth, fbheight;
		glfwGetWindowSize(glfw, &winwidth, &winheight);
		glfwGetFramebufferSize(glfw, &fbwidth, &fbheight);
//		dbgassert (winwidth>0&&winheight>0&&fbwidth>0&&fbheight>0);
		if (winwidth>0&&winheight>0&&fbwidth>0&&fbheight>0) {
//			float pxratio = fbwidth / (float)winwidth;

		}
		glViewport(0, 0, fbwidth, fbheight);
		glEnable(GL_BLEND);
		glDisable(GL_CULL_FACE);
		glDisable(GL_DEPTH_TEST);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	void createMainWindow(const char* title, int w, int h, appwindow_main* parentWindowHandle, int flags = 0);
	void initControl();
	void updateMenu() {
	#if WINDOW_HAS_MENUBAR
		ngui::MenuBar& menubar = ctrl->getMenubar();
	#ifdef _WIN32
		syncMenu(hwnd, menubar);
	#endif
	#ifdef __linux__
			//TODO: implement linux
	#endif
	#endif
	}
	void flagNeedsRedraw() override {
		appwindow::flagNeedsRedraw();
		if (cursorIcon != ctrl->cursorIcon) {
			glfwSetCursor(glfw, MouseCursors::cursors[ctrl->cursorIcon]);
			cursorIcon = ctrl->cursorIcon;
		}
	}
	window_main* createOverlay(std::shared_ptr<AppCtrl> ctrl, int flags) override;
	void closeOverlay(window_main* wnd) override {
		assert(wnd);
		wnd->hide();
		auto it = std::find_if(overlayWindows.begin(), overlayWindows.end(), [wnd](const auto& e) {
			return dynamic_cast<window_base*>(e.get()) == dynamic_cast<window_base*>(wnd);
		});
		auto handlerListSize = windowTimerHandleList.size();
		if (it != overlayWindows.end()) {
			auto& sharedPtr = *it;
			dbgassert(std::find(windowTimerHandleList.begin(), windowTimerHandleList.end(), sharedPtr.get()) != windowTimerHandleList.end());
			sharedPtr->destroy();
			dbgassert(std::find(windowTimerHandleList.begin(), windowTimerHandleList.end(), sharedPtr.get()) == windowTimerHandleList.end());
			sharedPtr.reset();
			overlayWindows.erase(it);
			dbgassert(handlerListSize != windowTimerHandleList.size());
		} else {
			dbgassert(0);
		}



	}
	bool canResize() override {
		return this->bCanResize;
	}
	void destroy();
	void onTick() {
		static bool reentrant = false;
		reentrantblocker block(reentrant);
	    if (!block.check()) {
	    	return;
	    }
//		flagNeedsRedraw();
		glfwMakeContextCurrent(glfw);
		ctrl->onAppTick();
	}
	void onRefresh() override {
		appwindow::onRefresh();

		#ifdef __APPLE__
		for (auto& w : overlayWindows)
			w->onRefresh();
		#endif
	}
	void render() {
		/*std::vector<int64_t> test;
		test.reserve(1<<31);*/
		renderMain(ctrl);
	}
	void renderMain(AppCtrl* const ctrl)
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
			hires_timer_t timer;
			timer.reset();
			float pxratio = fbwidth / (float)winwidth;
			glViewport(0, 0, fbwidth, fbheight);
			glEnable(GL_BLEND);
			glDisable(GL_CULL_FACE);
			glDisable(GL_DEPTH_TEST);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			ctrl->prerender(this->nanovgCtxt, 0, 0, winwidth, winheight, pxratio);
			glViewport(0, 0, fbwidth, fbheight);
			static const vec4 clearc = int32vec4(0xff121212);
			glClearColor(clearc[0], clearc[1], clearc[2], clearc[3]);
			glStencilMask(~0);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
			ctrl->render(this->nanovgCtxt, 0, 0, winwidth, winheight, pxratio);
#if BUILD_VSTHOST
			if (!this->parent) {
				daw_tls::tlsinstance& tls = daw_tls::getTls();
				tls.renderStats.timeRender=timer.getTime();
			}
#endif
			glfwSwapBuffers(glfw);
		}
	}
	void onMouseMoved(ivec2 deltapos) {
		if (math::abs(deltapos.x)+math::abs(deltapos.y) > 2)
			this->dblclicktimer = 0;
		ctrl->mouseMoved(getMousePos(1.0f/ctrl->m_scale), deltapos);
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
			ctrl->mouseDown(getMousePos(1.0f/ctrl->m_scale), button, dblClick);
		} else if (action == GLFW_RELEASE) {
			ctrl->mouseUp(getMousePos(1.0f/ctrl->m_scale), button);
		}
		lastclickpos = mousepos;
		flagNeedsRedraw();
	}
	void onWindowSizeChanged(int width, int height) {
		if (ctrl->isOK) {

			if (ctrl->m_size.x != width || ctrl->m_size.y != height) {
				log_printf("size change from %dx%d to %dx%d on window %08X: parent %08X\n", ctrl->m_size.x, ctrl->m_size.y, width, height, (uint64_t)(this), (uint64_t)(parent));
				ctrl->windowSizeChanged(width, height);
			} else {
				log_printf("skip window resize on window %08X: parent %08X\n", (uint64_t)(this), (uint64_t)(parent));
			}
			flagNeedsRedraw();
		}
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
		log_printf("onWindowClose ptr %s\n", StringAsCStr(nameDbg));
		ctrl->onWindowClose();
		if (parent) {
			parent->onChildOverlayClose(this);
		}
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
//		onWindowClose();
    }
    void menuCommand(const menucmd_t&& command) {
#if WINDOW_HAS_MENUBAR
    	ctrl->menuCommand(std::move(command));
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
			if (this->ctrl->window == this) {
				this->ctrl->closeAllContextMenus();
			}
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
	window_dialog* createDialog(const String& sTitle,int w, int h) override;
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
			pos.x -= (pos.x + size.x) - mi.rcWork.right;
		}
		if (pos.y + size.y > mi.rcWork.bottom) {
			pos.y -= (pos.y + size.y) - mi.rcWork.bottom;
		}
#endif
        appwindow::setPos(pos);
#ifdef __linux__
		/* calling setSize on a hidden window makes the window visible (at the wrong location!)
		 * As workaround on linux positionOnScreen is must bee called twice:
		 * Once before and once after appwindow::show() */ 
		if (shown) {
			appwindow::setSize(size);
		}
#else
		appwindow::setSize(size);
#endif
	}

	void show() {
		appwindow::showWindow();
	}

	void hide() {
		appwindow::hideWindow();
	}
};


void appwindow_main::onChildOverlayClose(appwindow* child) {

	appwindow::onChildOverlayClose(child);
	//TODO: add enum type field to appwindow
	appwindow_main* wndOverlay = dynamic_cast<appwindow_main*>(child);
	dbgassert(wndOverlay);
	if (wndOverlay) {
		log_printf("onChildOverlayClose ptr %s\n", StringAsCStr(wndOverlay->nameDbg));
		this->ctrl->onChildOverlayWindowClose(wndOverlay);
	}
}

class appwindow_dialog : public appwindow, public window_dialog {
	std::function<void(NVGcontext*,int,int,float)> drawFn;
	std::function<void()> initCallback;
	const bool disablesParent = false;
	bool init = false;
public:
	appwindow_dialog(appwindow* _parent) : appwindow(_parent) {
	}
	void setDrawFunction(const window_draw_fn& fn) override {
		this->drawFn = fn.drawCallback;
	}
	void setInitFunction(const window_init_fn& fn) override {
		this->initCallback = fn.initCallback;
	}
	void createDialogWindow(const char* title, int w, int h, GLFWwindow* share = nullptr, NVGcontext* nanovgCtxt = nullptr) {
		setAppWindowHints();
		glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
		glfwWindowHint(GLFW_VISIBLE, GL_FALSE);
		glfwWindowHint(GLFW_FOCUSED, GL_TRUE);
		this->nanovgCtxt = nanovgCtxt;
		//glfwWindowHint(GLFW_FLOATING, 1);
		appwindow::createBaseWindow(title, w, h, share);
#ifdef _WIN32
		LONG l = GetWindowLong(hwnd, GWL_EXSTYLE);
		if (parent) {
			SetWindowLong(hwnd, GWL_EXSTYLE, l & ~WS_EX_APPWINDOW);
		}
		SetWindowLong(hwnd, GWL_STYLE, WS_CAPTION | WS_POPUP | WS_CLIPSIBLINGS | WS_SYSMENU);
#endif
#ifdef __linux__
		//TODO: implement linux
#endif
		if (parent) {
			this->parent->onChildDialogCreate(this);
		//	glfwSetWindowAttrib(glfw, GLFW_FLOATING, GL_TRUE);
		}
		//glfwWindowHint(GLFW_FLOATING, 0);

	}
	void destroy() override {
		if (!glfw)
			throw appexception("window null");
		glfwMakeContextCurrent(glfw);
		appwindow::destroyGL();
		glfwSetWindowUserPointer(glfw, nullptr);
		glfwDestroyWindow(glfw);
		glfw = nullptr;
	}
	void render()
	{
		if (!init) {
			init = true;
			if (initCallback) {
				initCallback();
			}
		}
		glfwMakeContextCurrent(glfw);
		int winwidth, winheight;
		int fbwidth, fbheight;
		glfwGetWindowSize(glfw, &winwidth, &winheight);
		glfwGetFramebufferSize(glfw, &fbwidth, &fbheight);
		float pxratio = fbwidth / (float)winwidth;
		glViewport(0, 0, fbwidth, fbheight);
		static const vec4 clearc = int32vec4(0xFF000000);
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
		if (parent && disablesParent)
			EnableWindow(parent->getHWND(), TRUE);
#endif
#ifdef __linux__
		//TODO: implement linux
#endif
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
		if (parent && disablesParent)
			EnableWindow(parent->getHWND(), FALSE);
#endif
#ifdef __linux__
		//TODO: implement linux window enable/disable
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
window_main* appwindow_main::createOverlay(std::shared_ptr<AppCtrl> ctrl, int flags) {
//	std::unique_ptr<appwindow_overlay> ow = std::make_unique<appwindow_overlay>(this);
	String sName = StringFormat("%s.child", this->name);
	std::shared_ptr<appwindow_main> ow = std::make_shared<appwindow_main>(this, ctrl); //TODO: manage lifetime of control

	//pass down parent window handle if ctrl is companion ctrl of daw (signaled by WINDOW_IS_MAINWINDOW_SLAVE)
	appwindow_main* parentHandle = ((flags&WINDOW_IS_MAINWINDOW_SLAVE) != 0) ? this : nullptr;
	ivec2 windowSize;
	getSize(&windowSize);
	ow->createMainWindow(StringAsCStr(sName), windowSize.x, windowSize.y, parentHandle, flags);
	if (((flags&WINDOW_IS_MAINWINDOW_SLAVE) == 0)) {
		ow->initControl();
	}
//	ow->createOverlayWindow(StringAsCStr(sName), 200, 200, nullptr);
	auto* ret = ow.get();
	this->overlayWindows.push_back(std::move(ow));
	return ret;
}
void appwindow_main::destroyOverlayWindows() {
	for (std::shared_ptr<appwindow>& ow : this->overlayWindows) {
		ow->destroy();
		ow.reset();
	}
	this->overlayWindows.clear();
}
void appwindow_main::destroy() {
	if (!glfw)
		throw appexception("glfw null");
	destroyOverlayWindows();
	appwindow::killTimer();
#if BUILD_VSTHOST
#ifdef _WIN32
	if (this->dropTarget)
		UnregisterDropWindow(hwnd, this->dropTarget);
	if (!parent) {
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
	glfwMakeContextCurrent(glfw);
	if (!isSharedContextSlave) {
		appwindow::destroyGL();
	} else {
		nanovgCtxt = nullptr;
	}
	glfwDestroyWindow(glfw);
	glfw = nullptr;
}
void appwindow_main::initControl() {
	if (!ctrl->init(this, this->nanovgCtxt)) {
		throw appexception("Couldn't start application");
	}
#if BUILD_VSTHOST
#ifdef _WIN32
	this->dropTarget = RegisterDropWindow(hwnd, this);
	if (!parent) {
		if (windowCreationFlags & WINDOW_IS_MAINWINDOW_SLAVE) {
			if (!restoreWindowPos(hwnd, settings.wndCompanion.size.get())) {
				this->maximize();
			}
		}
		else {

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

void appwindow_main::createMainWindow(const char* title, int w, int h, appwindow_main* parentWindowHandle, int flags) {
	nameDbg=title;
	windowCreationFlags = flags;
	setAppWindowHints();
//	if (!parentWindowHandle)

		glfwWindowHint(GLFW_VISIBLE, GL_FALSE);

	if (flags&WINDOW_BORDERLESS_POPUP) {
		bCanResize = false;
		glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
		glfwWindowHint(GLFW_VISIBLE, GL_FALSE);
		glfwWindowHint(GLFW_FOCUSED, GL_FALSE);
		glfwWindowHint(GLFW_DECORATED, GL_FALSE);
//		glfwWindowHint(GLFW_UTILITY_WINDOW, GL_TRUE);
		glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER , GL_TRUE);
	} else {
		bCanResize = true;
	}

	//glfwWindowHint(GLFW_FLOATING, parent != nullptr);
	appwindow::createBaseWindow(title, w, h, parentWindowHandle ? parentWindowHandle->glfw : nullptr, nullptr);

	if (flags&WINDOW_IS_MAINWINDOW_SLAVE) {
		this->nanovgCtxt = parentWindowHandle->nanovgCtxt;
	}
	if (!parent) {
		glfwSetWindowSizeLimits(glfw, 640, 480, GLFW_DONT_CARE, GLFW_DONT_CARE);
	} else {


		//glfwSetWindowAttrib(glfw, GLFW_FLOATING, GL_TRUE);
	}
	if (flags&WINDOW_BORDERLESS_POPUP) {
		glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER , GL_FALSE); //set global state back to default
		glfwSetWindowAttrib(glfw, GLFW_FOCUS_ON_SHOW, 0);
#ifdef _WIN32
		SetWindowLongPtr(hwnd, GWLP_HWNDPARENT, (__int3264) (LONG_PTR)parent->getHWND());
		LONG l = GetWindowLong(hwnd, GWL_EXSTYLE);
		l = l & ~WS_EX_APPWINDOW;
		l = l | WS_EX_TOOLWINDOW;
		SetWindowLong(hwnd, GWL_EXSTYLE, l);
		SetWindowLong(hwnd, GWL_STYLE, WS_CHILD | WS_CLIPSIBLINGS);
#endif
	}
//	if (!(flags&WINDOW_IS_MAINWINDOW_SLAVE)) {
		RenderResources::initResources(nanovgCtxt);
		MouseCursors::initCursors(); //TODO: call MouseCursors::destroy() on exit of last instance

//	}
}
#if defined(__linux__) or defined(__APPLE__)
void AppWndProc_enableBlockReentrant() {
}
void AppWndProc_disableBlockReentrant() {
}
#endif
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

window_dialog* appwindow_main::createDialog(const String& sTitle, int w, int h) {
	appwindow_dialog* windowDialog = new appwindow_dialog(this);
	GLFWwindow* const windowOpengCtxtShare = this->glfw;
	windowDialog->createDialogWindow(StringAsCStr(sTitle), w, h, windowOpengCtxtShare, this->nanovgCtxt);
	return windowDialog;
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

void appwindow::createBaseWindow(const char* title, int w, int h, GLFWwindow* share, void* parentWindowHandle) {
	strncpy(this->name, title, 32);
	if (glfw)
		throw appexception("window not null");
	if (parentWindowHandle) {
		glfw = glfwCreateChildWindow(parentWindowHandle, w, h, title, share);
	} else {
		glfw = glfwCreateWindow(w, h, title, NULL, share);
	}
	if (share) {
		this->isSharedContextSlave = true;
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
	registerWindowTimer(this);
	last = getTimeMillis();
}


void printLeakedGuiBase();
#if BUILD_VSTHOST
void printClipAllocations();
#endif


GLFWwindow* getGlfwFromWindowBase(window_base* w) {
	return dynamic_cast<appwindow*>(w)->getGLFW();
}
void makeWindowContextCurrent(window_base* w) {
	auto glfw = getGlfwFromWindowBase(w);
	if (glfw) {
		glfwMakeContextCurrent(glfw);
	}
}
void initWindowControl(window_main* windowInitialize) {
	dynamic_cast<appwindow_main*>(windowInitialize)->initControl();
	dynamic_cast<appwindow_main*>(windowInitialize)->showWindow();
}
void destroyWindowControl(window_main* windowInitialize) {
	dynamic_cast<appwindow_main*>(windowInitialize)->setInvalid();
//	dynamic_cast<appwindow_main*>(windowInitialize)->destroy();
}
#if HAS_MAIN_LOOP
#include "platform/win/debug_msg_count.h"
win32_hwnd_msg_counter_t msgCounter;
bool msgCounterEnabled=false;

#if defined(_WIN32) && BUILD_VSTHOST
namespace vst_window_mgr {
void destroyAllVSTWindows();
bool isVstWindow(HWND hwnd);
}
#endif
std::shared_ptr<AppCtrl> makeApp();
void initColor(); // Forward declare from gui/gui.cpp
void deleteApp(); // Forward declare from host/mainctrl.cpp
void openGlobalLog(const String& logFileName); // Forward declare from util/debug.cpp
void closeGlobalLog(); // Forward declare from util/debug.cpp

int startApplication(int argc, char* argv[]) {
	setCurrentThreadName("mainthread");
#if !defined(NDEBUG) && defined(_WIN32)
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
#if HAS_JS_CONSOLE
	//lifetime of thread must exceed try/catch because deconstruction of an unjoined std::thread terminates process
	NU::CONSOLE::CommandLineREP_TCP cli;
	NU::CONSOLE::ConsoleThread threadCommandLine(cli);
#endif
	try {
	int centerScreenIdx = -1;
	for (int i = 0; i < argc; i++) {
		if (argv[i] && strcmp(argv[i], "-center") == 0 && i + 1 < argc) {
			char* a = argv[i+1];
			centerScreenIdx = atoi(a);
			argv[i] = nullptr;
			argv[i+1] = nullptr;
		}
	}
	//if (!runConsoleMode) {
	allocConsole();
	//}
	String logFileName = String(BuildInfo::BUILD_BINARY_NAME)+".log";
	openGlobalLog(logFileName);
	char* pPath;
	pPath = getenv("PATH");
	if (pPath != NULL)
		log_printf ("getenv PATH: %s\n",pPath);
	log_out("BUILD_BINARY_NAME %s\n", BuildInfo::BUILD_BINARY_NAME);
	log_out("COMPILER_ID %s\n", BuildInfo::COMPILER_ID);
	log_out("COMPILE_OPTIONS %s\n", BuildInfo::COMPILE_OPTIONS);
	log_out("COMPILE_DEFS %s\n", BuildInfo::COMPILE_DEFS);
#ifdef _ITERATOR_DEBUG_LEVEL
	log_out("_ITERATOR_DEBUG_LEVEL %d\n", (int)_ITERATOR_DEBUG_LEVEL);
#endif
//	DWORD* pTest = (DWORD*)::HeapAlloc(GetProcessHeap(), 0, 20);

	setMinimumResolutionTimer();
	initColor();
#if HAS_APP_SETTINGS
	try {
		settings = loadSettings();
	} catch (std::exception& e) {
		getGlobalLogger()->logStr(StringFormat("Exception: %s\n", e.what()));
		settings = appsettings();
		ngui::show("Couldn't read config file.\nSome settings may have been reset", "Warning", ngui::Style::Warning, ngui::Buttons::OK);
	}
#endif
	glfwSetErrorCallback(glfw_startup_error_callback);
	if (!glfwInit("DAWWINDOW01")) {
		showerror("Initialization failed. Couldn't initialize glfw");
		exit(EXIT_FAILURE);
	}
	setAppWindowHints();
	std::shared_ptr<AppCtrl> ctrl = makeApp();
	ctrl->initApp(argc, argv);

	std::unique_ptr<appwindow_main> mainWindow = std::make_unique<appwindow_main>(nullptr, ctrl);
	mainWindow->createMainWindow("main window", 1280, 720, nullptr, WINDOW_IS_MAINWINDOW_MASTER);
#ifdef _WIN32
    setMainHWND(mainWindow->getHWND());
#endif

	mainWindow->initControl();

	mainWindow->showWindow();
	if (centerScreenIdx >= 0) {
		mainWindow->centerOnScreen(centerScreenIdx);
	}

	enableGlDebugCallback();
	glfwSetErrorCallback(glfw_runtime_error_callback);
	ctrl->postInit();


#if HAS_JS_CONSOLE
	JSContext jsContext;
	String srcJS;
	int64_t ret = ReadFileText("daw_init.js", srcJS);
	if (ret > 0) {
		call_context_t ctxt;
		String response = jsContext.eval(srcJS, ctxt);
		if (response.length()) {
			fwrite(response.c_str(), response.length(), 1, stdout);
			fflush(stdout);
		}
	}

	daw_tls::tlsinstance& tls = daw_tls::getTls();
	threadCommandLine.setTls(tls);
	threadCommandLine.init();
	threadCommandLine.startThread();
#endif // HAS_JS_CONSOLE

	GLFWwindow* glfwHandle = mainWindow->getGLFW();
	int64_t lastTick = getTimeMillis();
	int64_t start = getTimeMillis();
	int64_t tmLastCheck = getTimeMillis();
	int64_t tmMsgSent = 0;
	int64_t cntMessages = 0;
	while (!fataError && !glfwWindowShouldClose(glfwHandle)) {
#ifdef _WIN32
		int64_t maxMsgProcess = 1024;
		DWORD timeout = 5;
		MsgWaitForMultipleObjects(0, NULL, FALSE, timeout, QS_ALLEVENTS);
	    MSG msg;
	    while (!fataError && PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE) && maxMsgProcess-- > 0)
	    {
	    	cntMessages++;
//	    	logEveryMsec(0, 5000, "Main msg loop");
	        if (msg.message == WM_QUIT)
	        {
	        	glfwSetWindowShouldClose(glfwHandle, 1);
	        }
	        else if (msg.message == WM_APP + 42) {
			    if (tmMsgSent != 0) {
			    	int64_t tmDuration = (getTimeMillis() - tmMsgSent);
			    	tmMsgSent = 0;
			    	if (tmDuration > 0) {
			    		log_printf("MSG took %d ms to get through, %d messages since sent\n", tmDuration, cntMessages);
			    	}
			    }
	        }
	        else
	        {

	            switch (msg.message) {
#if BUILD_VSTHOST
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

	            if (msgCounterEnabled) {
		            msgCounter.incrMessage(msg.message);
					if (msg.message == WM_PAINT)
					{
						char clsName_v[256];
						GetClassNameA(msg.hwnd, clsName_v, 256);
						msgCounter.incrPaints(clsName_v);
					}
	            }
	        }
	    }
		glfwUpdateInternals();
#endif //_WIN32
		if (getTimeMillis() - lastTick >= 20) { //TODO: figure out good tick rate
			windowTickTimerRun();
			lastTick = getTimeMillis();
		}
#if defined(__linux__) || defined(__APPLE__)
		glfwWaitEventsTimeout(0.001);
		mainWindow->onRefresh();
#else
		if (tmMsgSent > 0 && getTimeMillis() - tmMsgSent >= 1000)
		{
			tmMsgSent = 0;
		}
		if (getTimeMillis() - tmLastCheck >= 1000 && tmMsgSent == 0) {
			tmLastCheck = tmMsgSent = getTimeMillis();
			cntMessages = 0;
			PostMessage(mainWindow->getHWND(), WM_APP + 42, 0, 0);

		}
		if (getTimeMillis() - start >= 16) {
			mainWindow->flagNeedsRedraw();
			start = getTimeMillis();
		}
#endif
#if HAS_JS_CONSOLE
		cli.executeCommands();
#endif // HAS_JS_CONSOLE
	}
	mainWindow->setInvalid();
	mainWindow->destroy();

	if (!fataError) {
		try {
			saveSettings(settings);
		} catch (std::exception& e) {
			getGlobalLogger()->logStr(StringFormat("Exception: %s\n", e.what()));
			ngui::show("Couldn't write config file.", "Warning", ngui::Style::Warning, ngui::Buttons::OK);
		}
	}

#if defined(_WIN32) && BUILD_VSTHOST
	vst_window_mgr::destroyAllVSTWindows();
#endif

	mainWindow.reset();

	glfwTerminate();

	} catch (std::exception& e) {
		handleStdException(e);
	}
#if HAS_JS_CONSOLE
	threadCommandLine.stopThread();
	threadCommandLine.joinThread();
#endif // HAS_JS_CONSOLE
	deleteApp();
	printLeakedGuiBase();
#if BUILD_VSTHOST
	printClipAllocations();
#endif
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

#endif // HAS_MAIN_LOOP

void windowTickTimerRun() {
	std::vector<appwindow*> localWindowTimerHandleList = windowTimerHandleList;
	for (appwindow* window : localWindowTimerHandleList) {
		if (STL_CONTAINS(windowTimerHandleList, window))
			window->onTick();
	}
}

#if (BUILD_VSTHOST || BUILD_EXTERNAL_PLUGIN)
#include "plugins/plugin-window.h"
#include "plugins/plugincontrol.h"
#include "plugins/handle-exceptions.h"
#include "../vstsdk-host-2.4/aeffect.h"
#include "../vstsdk-host-2.4/aeffectx.h"
#include "../vstsdk-plugin-2.4/aeffeditor.h"

class appwindow_plugin : public appwindow_main, public pluginwindow {
	bool isInitialized = false;
public:
	ERect _rect{ 0 };
	appwindow_plugin(AudioEffect *_effect, std::shared_ptr<PluginControl> _ctrl, int w, int h)
		: appwindow_main(nullptr, _ctrl),
		  pluginwindow(_ctrl)
	{
		this->effect = _effect;
		setRect(0, 0, w, h);
		effect->setEditor(this);
		isExternalWindow = true;
	}

	virtual ~appwindow_plugin() {
		if (isInitialized) {
			my_printf("~pluginwindow_main()\n", 0);
			try {
				destroyContextAndWindow();
			EXC_CATCH_NO_THROW_DIALOG
		}
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
#ifdef __linux__
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
				if (!isInitialized) {
					isInitialized = true;
					setAppWindowHints();
					createPluginWindow("plugin-window", _rect.right-_rect.left, _rect.bottom-_rect.top, ptr);
				}
#ifdef _WIN32
				dbgassert(hwnd);
#endif
				dbgassert(glfw);
				dbgassert(nanovgCtxt);
				showWindow();
				guiOpen();
				return true;
			}
		EXC_CATCH_NO_THROW_DIALOG
		AEffEditor::close();
		return false;
	}
	void close() override
	{
		if (this->systemWindow) {
			glfwMakeContextCurrent(glfw);
			guiClose();
			hideWindow();
		//	destroyContextAndWindow();
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
		dbgassert(glfw);
		dbgassert(effect);
		dbgassert(ctrlShared.get());
#ifdef _WIN32
		dbgassert(hwnd);
	    RECT area;
	    GetClientRect(hwnd, &area);
	    onWindowSizeChanged(area.right-area.left, area.bottom-area.top);
#endif
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
#ifdef _WIN32
			hwnd = nullptr;
#endif
		}
//		wglMakeCurrent(NULL, NULL);
	}
};

AEffEditor* createPluginWindow(AudioEffect *_effect, std::shared_ptr<PluginControl> _ctrl, int w, int h) {
	appwindow_plugin* window = new appwindow_plugin(_effect, std::move(_ctrl), w, h);
	return window;
}
#endif


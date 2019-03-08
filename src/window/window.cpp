#include "glheaders.h"
#include <GLFW/glfw3.h>
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif
#define NANOVG_GL3_IMPLEMENTATION
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
#include <fstream>
#include <sstream>
#include <algorithm>
#include <map>
#include <glm/glm.hpp>
#include <glm/vec4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/geometric.hpp>
#include <cereal/archives/json.hpp>

using glm::vec4;
using glm::vec2;
using glm::ivec2;
using std::ifstream;
using std::ofstream;

#ifdef _WIN32
#include "../platform/win/winheaders.h"
#include "../platform/win/DropTarget.h"
#endif
#ifdef __linux__
#include "../platform/linux/x11_gtk_util.h"
#endif

#include "config.h"
#include "str_util.h"
#include "exceptions.h"
#include "color_util.h"
#include "mouse.h"
#include "keyboard.h"

#include "window.h"
#include "msgbox.h"
#include "menu.h"
// make base header (BaseCtrl)
#include "basectrl.h"
#include "droptargetlistener.h"

#include "platform.h"

#include "logging.h"
#include "settings.h"
#include "renderresources.h"
#include "mousecursor.h"
#include "fileio.h"


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
	} catch (...) {													\
		handleException();											\
	}
String excDescription;
void handleStdException(std::exception& e) {
	excDescription = StringFormat("Fatal error: %s", e.what());
	std::terminate();
}
void handleException() {
	excDescription = "Unhandled program exception";
	std::terminate();
}

namespace RenderResources {
void init(NVGcontext* vg); // renderresources.cpp
}
namespace MouseCursors {
void init(); // mousecursor.cpp
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
	char errorCodeStr[1024] = { 0 };
	_snprintf_s(errorCodeStr, 1024 - 1, _TRUNCATE, "Error %d: %s", error, description);
	ngui::show(errorCodeStr, "Error", ngui::Style::Error, ngui::Buttons::OK);
}
static void glfw_runtime_error_callback(int error, const char* description) {
	printf("Error %d: %s", error, description);
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
	printf("Error: %s\n", description);
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
#ifdef _WIN32
void syncMenu(HWND hwnd, ngui::MenuBar& menubar); // menu_win32.cpp
ngui::Menu* getUserDataFromMenu(HMENU hmenu, UINT uPos); // menu_win32.cpp
static WNDPROC glfwWndProc = NULL;

LRESULT WIN32API_CALLBACK_TYPE appWndProc(HWND hwnd, UINT Msg, WPARAM wParam, LPARAM lParam);
static VOID WIN32API_CALLBACK_TYPE timerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);

#define IDT_TIMER1 0
#endif
class appwindow_dialog;
class appwindow_overlay;
bool loadSettings(appsettings& _settings) {
	try {
		Stringstream ss;
		ifstream file(SETTINGS_NAME, ifstream::in);
		if (file) {
		    ss << file.rdbuf();
		    std::streampos length = file.tellg();
		    if (length > 10) {
			    cereal::JSONInputArchive ar(ss);
			    ar( _settings );
			    return true;
		    }
		}
	} catch (std::exception& e) {
		ngui::show("Couldn't read config file.\nSome settings may have been reset", "Warning", ngui::Style::Warning, ngui::Buttons::OK);
		std::cout << e.what();
		std::cout << std::endl;
		_settings = appsettings();
	}
	return false;
}
void saveSettings(appsettings& _settings) {
	ofstream file;
	file.exceptions(~ofstream::goodbit);
	try {
		file.open(SETTINGS_NAME, ofstream::out);
	    cereal::JSONOutputArchive ar( file );
	    ar( _settings );
	} catch (std::exception& e) {
		std::cout << "Failed writing settings\n";
		std::cout << e.what();
		std::cout << std::endl;
	}
}
appsettings settings;
class appwindow : protected DropTargetListener {
private:
	std::vector<appwindow*> children;
	uint64_t last = 0;
protected:
	char name[32];
	int cursorIcon = CURSOR_DEFAULT;
	vec2 lastclickpos;
	vec2 lastmousepos;
	vec2 mousepos;
	GLFWwindow *glfw = NULL;
	NVGcontext* nanovgCtxt = NULL;
#ifdef __linux__
	bool noRawInput = true;//disable, since Virtual Machines don't handle rawinput correctly (works on native)
#endif
#ifdef _WIN32
	bool noRawInput = false;
	UINT_PTR timer = 0;
	DropTarget* dropTarget = NULL;
	HWND hwnd = NULL;
#endif
private:
	int calls = 0;
	uint64_t tm_lastfps;
	String fpsStats;
	double secondsLastDraw = 0.0;
	double secondsLastDrawReq = 0.0;
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
#ifdef _WIN32
		if (glfwWndProc == NULL) {
			glfwWndProc = (WNDPROC)GetWindowLongPtr(hwnd, GWLP_WNDPROC);
		}
#endif
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
	}
public:
	appwindow() :
	  tm_lastfps(getTimeMillis()) {
		name[0] = 0;
	}
	virtual ~appwindow() {
		my_printf("glfwDestroyWindow\n", 0);
		glfwDestroyWindow(glfw);
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
	void destroyGL() {
		if (nanovgCtxt)
			nvgDeleteGL3(nanovgCtxt);
	}
	void _onMouseMoved(double x, double y) {
		lastmousepos = mousepos;
		mousepos.x = (float)x;
		mousepos.y = (float)y;
		vec2 delta = mousepos - lastmousepos;
		ivec2 idelta = ivec2((int)delta.x, (int)delta.y);
		onMouseMoved(idelta);
	}

	/* FROM GLFW CALLBACKS */
	virtual void onWindowCloseRequest() {
		my_printf("on window close\n", 0);
#ifdef _WIN32
		if (timer && hwnd) {
			KillTimer(hwnd, this->timer);
			my_printf("KillTimer\n", 0);
		}
#endif
	}
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


	virtual void onTick() = 0;

	virtual void hideSystemCursor() {
		glfwSetInputMode(glfw, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
	}
	virtual void captureMouse() {
		glfwSetInputMode(glfw, GLFW_CURSOR, noRawInput ? GLFW_CURSOR_HIDDEN : GLFW_CURSOR_DISABLED);
	}
	virtual void releaseMouse() {
		glfwSetInputMode(glfw, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	}
	virtual bool isMouseCaptured() {
		return glfwGetInputMode(glfw, GLFW_CURSOR) != GLFW_CURSOR_NORMAL;
	}

	virtual void createWindow(const char* title, int w, int h, GLFWwindow* share = NULL) {
		strncpy(this->name, title, 32);
		if (glfw)
			throw appexception("window not null");
		glfw = glfwCreateWindow(w, h, title, NULL, share);
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
		initOGL();
#ifdef _WIN32
		SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)appWndProc);
#endif
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
	}
	bool isWindowNotHidden() {
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
		return CallWindowProc(glfwWndProc, _hwnd, Msg, wParam, lParam);
	}
#endif

	virtual void onChildCreate(appwindow* child) {
		this->children.push_back(child);
	}
	virtual void onChildClose(appwindow* child) {
		auto it = std::find(children.begin(), children.end(), child);
		if (it != children.end())
			children.erase(it);

		delete child;
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
	void create(const char* title, int w, int h);
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
			float pxratio = fbwidth / (float)winwidth;
			ctrl->prerender(0, 0, winwidth, winheight, pxratio);
			glViewport(0, 0, fbwidth, fbheight);
			static const vec4 clearc = int32vec4(0xff121212);
			glClearColor(clearc[0], clearc[1], clearc[2], clearc[3]);
			glStencilMask(~0);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
			if (!ctrl->isOk()) {
				throw std::logic_error("invalid application state");
			}
			ctrl->render(0, 0, winwidth, winheight, pxratio);
			glfwSwapBuffers(glfw);
		}
	}
	void onMouseMoved(ivec2 deltapos) {
		if (abs(deltapos.x)+abs(deltapos.y) > 2)
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

	void onWindowCloseRequest() 
	{
		appwindow::onWindowCloseRequest();
		ctrl->onWindowCloseRequest();
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
		switch (Msg) {
		case WM_MOVING:
		case WM_MOVE:
		case WM_WINDOWPOSCHANGING:
		case WM_WINDOWPOSCHANGED:
		case WM_NCLBUTTONDOWN:
//			my_printf("WM_ closeContextMenu\n", 0);
//    		this->ctrl->closeAppMenus();
    		this->ctrl->closeContextMenu();
			break;

		}
		return appwindow::windowProc(_hwnd, Msg, wParam, lParam);
	}
#endif

	void onCharInput(unsigned int codepoint) {
		ctrl->onCharInput(codepoint);
		flagNeedsRedraw();
	}
	void onKeyInput(int key, int scancode, int action, int mods, const char* key_name)
	{
		/*if (action == GLFW_PRESS)*/
//		my_printf("keyname %s, key %d, scancode %d\n", key_name, key, scancode);
//		my_printf("mods %08X\n", mods);
		printf("onKeyInput %d (%c) %d\n", key, key, scancode);
		ctrl->onKeyInput(key, scancode, action, mods, key_name);
		flagNeedsRedraw();
	}
	void onChildClose(appwindow* child) {
		glfwFocusWindow(this->glfw);
		appwindow::onChildClose(child);
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
		ctrl->onCursorEnter(entered);
		if (entered)
			glfwSetCursor(glfw, MouseCursors::cursors[cursorIcon]);
	}
	window_dialog* createDialog();
	bool isShown() {
		return appwindow::isWindowNotHidden();
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
public:
	appwindow* const parent;
//	PopupCtrl* const ctrl;
	std::unique_ptr<PopupCtrl> ctrl;
	appwindow_overlay(appwindow* _parent)
		: appwindow(),
		  window_overlay(),
		  parent(_parent),
		  ctrl(std::make_unique<PopupCtrl>())
	{
	}
	void create(const char* title, int w, int h);
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

		if (!ctrl->isOk()) {
			throw std::logic_error("invalid application state");
		}
		ctrl->render(0, 0, winwidth, winheight, pxratio);
		glfwSwapBuffers(glfw);
	}
	PopupCtrl* getCtrl() {
		return ctrl.get();
	}
	void onWindowCloseRequest()
	{
		appwindow::onWindowCloseRequest();
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
//		ShowWindow(hwnd, SWP_NOACTIVATE);
//		if (parent)
//		SetParent(hwnd, parent->getHWND());
	}
	void hide() {
		appwindow::hideWindow();
	}
	bool isShown() {
		return appwindow::isWindowNotHidden();
	}
	void onMouseMoved(ivec2 deltapos) {
		ctrl->mouseMoved(getMousePos(), deltapos);
		flagNeedsRedraw();
	}
	void onMouseButton(int button, int action, int mods) {
		if (action == GLFW_PRESS) {
			ctrl->mouseDown(getMousePos(), button, false);
			return;
		} else if (action == GLFW_RELEASE) {
			ctrl->mouseUp(getMousePos(), button);
			return;
		}
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
	virtual void onMouseScrolled(double xoffset, double yoffset) {
		ctrl->mouseScrolled(xoffset, yoffset);
		flagNeedsRedraw();
	}
	void onWindowSizeChanged(int width, int height) {
		flagNeedsRedraw();
	}

	void onKeyInput(int key, int scancode, int action, int mods, const char* key_name)
	{
		//never fired on windows
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
		ctrl->onCursorEnter(entered);
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


int initDebugWindow();
class appwindow_dialog : public appwindow, public window_dialog {
public:
	void (*drawFn)(NVGcontext*,int,int,float) = NULL;
	appwindow *parent = NULL;
	appwindow_dialog(appwindow* _parent) : appwindow() {
		this->parent = _parent;
	}
	void create(const char* title, int w, int h, GLFWwindow* share = NULL) {
		setAppWindowHints();
		glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
		glfwWindowHint(GLFW_VISIBLE, GL_FALSE);
		glfwWindowHint(GLFW_FOCUSED, GL_TRUE);
		appwindow::createWindow(title, w, h, share);
		if (parent)
		this->parent->onChildCreate(this);

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
	void onWindowCloseRequest()
	{
		appwindow::onWindowCloseRequest();
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
		this->parent->onChildClose(this);
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
	ow->create(StringAsCStr(sName), 200, 200);
	window_overlay* ret = ow.get();
	this->overlayWindows.push_back(std::move(ow));
	return ret;
}
void appwindow_main::destroyOverlayWindows() {
	for (std::unique_ptr<appwindow_overlay>& ow : this->overlayWindows) {
		ow->destroyGL();
		ow.reset();
	}
}
void appwindow_main::destroy() {
	if (!glfw)
		throw appexception("window null");
	appwindow::destroyGL();
#ifdef _WIN32
	UnregisterDropWindow(hwnd, this->dropTarget);
	settings.size = windowsize(hwnd);
#endif
#if __linux__
		//TODO: implement linux
#endif
}
void appwindow_main::create(const char* title, int w, int h) {
	setAppWindowHints();
	glfwWindowHint(GLFW_VISIBLE, GL_FALSE);
	appwindow::createWindow(title, w, h);
	glfwSetWindowSizeLimits(glfw, 640, 480, GLFW_DONT_CARE, GLFW_DONT_CARE);
	RenderResources::init(nanovgCtxt);
	MouseCursors::init(); //TODO: call MouseCursors::destroy() on exit of last instance

	if (!ctrl->init(this, this->nanovgCtxt)) {
		throw appexception("Couldn't start application");
	}
#ifdef _WIN32
	this->dropTarget = RegisterDropWindow(hwnd, this);
#endif
#if __linux__
		//TODO: implement linux
#endif

#ifdef _WIN32
	if (settings.size.valid) {
		settings.size.apply(hwnd);
	    RECT area;
	    GetClientRect(hwnd, &area);
	    onWindowSizeChanged(area.right-area.left, area.bottom-area.top);
	} else {
		this->maximize();
	}
#endif
#if __linux__
		//TODO: implement linux
#endif
	glfwGetWindowSize(glfw, &w, &h);
	this->onWindowSizeChanged(w, h);
}
void appwindow_overlay::create(const char* title, int w, int h) {
	setAppWindowHints();
	glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
	glfwWindowHint(GLFW_VISIBLE, GL_FALSE);
	glfwWindowHint(GLFW_FOCUSED, GL_FALSE);
	glfwWindowHint(GLFW_DECORATED, GL_FALSE);
	glfwWindowHint(GLFW_UTILITY_WINDOW, GL_TRUE);
	appwindow::createWindow(title, w, h);
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
	if (!ctrl->init(this, this->nanovgCtxt)) {
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
	if (impl == NULL) {
		return;
	}
	impl->onTick();
	EXC_CATCH
}
LRESULT WIN32API_CALLBACK_TYPE appWndProc(HWND hwnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
	EXC_TRY
	appwindow* impl = NULL;
	GLFWwindow* glfwWindow = (GLFWwindow*) GetPropW(hwnd, L"GLFW");
	if (glfwWindow != NULL) {
		impl = (appwindow*)glfwGetWindowUserPointer(glfwWindow);
	}
	if (impl == NULL) {
		if (glfwWndProc)
			return CallWindowProc(glfwWndProc, hwnd, Msg, wParam, lParam);
		return 0; // Cannot throw in winproc
	}
	return impl->windowProc(hwnd, Msg, wParam, lParam);
	EXC_CATCH
	return 0;
}
#endif
window_dialog* appwindow_main::createDialog() {
	appwindow_dialog* popupWindow = new appwindow_dialog(this);
	String sName = StringFormat("%s dialog", this->name);
	popupWindow->create(StringAsCStr(sName), 200, 200);
	return popupWindow;
}
static appwindow* getUserData(GLFWwindow *w) {
	appwindow* impl = (appwindow*) glfwGetWindowUserPointer(w);
	return impl;
}
void on_terminate() {
	glfwTerminate();
	if (excDescription.length()) {
#ifdef __linux__
		printf("Error: %s\n", StringAsCStr(excDescription));
#else
		ngui::show(StringAsCStr(excDescription), "Error", ngui::Style::Error, ngui::Buttons::OK);
#endif

	}
}


static void glfw_cb_mousepos(GLFWwindow *w, double x, double y) {
	EXC_TRY
	appwindow* wu;
	if ((wu = getUserData(w)))
		wu->_onMouseMoved(x, y);
	EXC_CATCH
}
static void glfw_cb_mousebutton(GLFWwindow *w, int button, int action, int mods) {
	EXC_TRY
	appwindow* wu;
	if ((wu = getUserData(w)))
		wu->onMouseButton(button, action, mods);
	EXC_CATCH
}
static void glfw_cb_cursorenter(GLFWwindow *w, int entered) {
	EXC_TRY
	appwindow* wu;
	if ((wu = getUserData(w)))
		wu->onCursorEnter(entered);
	EXC_CATCH
}
static void glfw_cb_mousescroll(GLFWwindow *w, double xoffset, double yoffset) {
	EXC_TRY
	appwindow* wu;
	if ((wu = getUserData(w)))
		wu->onMouseScrolled(xoffset, yoffset);
	EXC_CATCH
}
static void glfw_cb_keyinput(GLFWwindow *w, int key, int scancode, int action, int mods) {
	EXC_TRY
	const char* key_name = glfwGetKeyName(key, scancode);
	appwindow* wu;
	if ((wu = getUserData(w)))
		wu->onKeyInput(key, scancode, action, mods, key_name);
	EXC_CATCH
}
static void glfw_cb_charinput(GLFWwindow *w, unsigned int codepoint) {
	EXC_TRY
	appwindow* wu;
	if ((wu = getUserData(w)))
		wu->onCharInput(codepoint);
	EXC_CATCH
}
static void glfw_cb_refresh(GLFWwindow *w) {
	EXC_TRY
	appwindow* wu;
	if ((wu = getUserData(w)))
		wu->onRefresh();
	EXC_CATCH
}
static void glfw_cb_windowclose(GLFWwindow *w) {
	EXC_TRY
	appwindow* wu;
	if ((wu = getUserData(w)))
		wu->onWindowCloseRequest();
	EXC_CATCH
}
static void glfw_cb_windowfocus(GLFWwindow *w, int focused) {
	EXC_TRY
	appwindow* wu;
	if ((wu = getUserData(w)))
		wu->onWindowFocusChanged(focused);
	EXC_CATCH
}
static void glfw_cb_windowwize(GLFWwindow *w, int width, int height) {
	EXC_TRY
	appwindow* wu;
	if ((wu = getUserData(w)))
		wu->onWindowSizeChanged(width, height);
	EXC_CATCH
}
static void glfw_cb_framebuffersize(GLFWwindow *w, int width, int height) {
	EXC_TRY
	appwindow* wu;
	if ((wu = getUserData(w)))
		wu->onFramebufferSizeChanged(width, height);
	EXC_CATCH
}

void printLeaked();

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

void drawDebugWindow(NVGcontext* ctx, int winW, int winH, float pxratio);
#ifdef _WIN32
bool isVstWindow(HWND hwnd);
#endif
std::shared_ptr<AppCtrl> makeApp();
void deleteApp();
int startApplication(int argc, char* argv[]) {
	bool test = argc > 1 && String(argv[1]) == "--test";
#ifndef NDEBUG
    _dup2( 1, 2 ); //workaround: redirect stderr to stdout so stderr is visible when using gdb on eclipse (bug)
#endif
#ifdef _WIN32
	OleInitialize(0);
#endif
	std::set_terminate(on_terminate);
	setExceptionHandler();

	EXC_TRY
	allocConsole();
	setMinimumResolutionTimer();
	loadSettings(settings);
	glfwSetErrorCallback(glfw_startup_error_callback);
	if (!glfwInit()) {
		showerror("Initialization failed. Couldn't initialize glfw");
		exit(EXIT_FAILURE);
	}
	setAppWindowHints();
	std::shared_ptr<AppCtrl> ctrl = makeApp();
	ctrl->initApp(argc, argv);
	mainWindow = std::make_unique<appwindow_main>(ctrl.get());
	mainWindow->create("main window", 1280, 720);
	mainWindow->showWindow();
	enableGlDebugCallback();
#if CREATE_DEBUG_COMPANION_WINDOW
	{
		appwindow_dialog* w = new appwindow_dialog(NULL);
		w->drawFn=drawDebugWindow;
		int winW = 1280;
		int winH = 720;
		GLFWwindow* contextWindow = mainWindow->getGLFW();
		w->create("test window", winW, winH, contextWindow);
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
	int step = 0;
	int nMsg = 0;
	while (!glfwWindowShouldClose(glfwHandle)) {
#ifdef _WIN32
		DWORD timeout = 5;
		MsgWaitForMultipleObjects(0, NULL, FALSE, timeout, QS_ALLEVENTS);
	    MSG msg;
	    int limit = 0;
	    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
	    {
	        if (msg.message == WM_QUIT)
	        {
	        	glfwSetWindowShouldClose(glfwHandle, 1);
	        }
	        else
	        {
				char clsName_v[256];

				GetClassNameA(msg.hwnd, clsName_v, 256);

	            switch (msg.message) {
					case WM_KEYDOWN:
					case WM_SYSKEYDOWN:
					case WM_KEYUP:
					case WM_SYSKEYUP: {
						if (isVstWindow(msg.hwnd)) {
							msg.hwnd = mainWindow->getHWND();
						}
					}
					//no break
					default:
						TranslateMessage(&msg);
			            DispatchMessageW(&msg);
						break;
	            }


				incrMessage(msg.message);
				if (msg.message == WM_PAINT)
				{
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
//		ctrl->numCallsWaitEvents++;
//		if (test && getTimeMillis()-start > 4) {
//			switch (step) {
//			case 0:
//				ctrl->loadFile("muuure.project");
//				break;
//			case 1:
//				ctrl->startPlaying();
//				break;
//			case 2:
//				ctrl->stopPlaying();
//				break;
//			case 3:
//				ctrl->loadFile("more.project");
//				break;
//			case 4:
//				ctrl->startPlaying();
//				break;
//			case 5:
//				ctrl->loadFile("jad.project");
//				break;
//			case 6:
//				ctrl->startPlaying();
//				break;
//			case 7:
//				ctrl->stopPlaying();
//				break;
//			case 8:
//				glfwSetWindowShouldClose(glfwHandle, 1);
//				break;
//			}
//			start = getTimeMillis();
//			step++;
//		}
	}
	ctrl->destroy();
	mainWindow->destroy();
//	PopupCtrl::get()->destroy();
	mainWindow->destroyOverlayWindows();

	saveSettings(settings);
	mainWindow.reset();
	glfwTerminate();
	EXC_CATCH
	printLeaked();
	deleteApp();
#ifdef _WIN32
	OleUninitialize();
#endif
	my_printf("EXIT_SUCCESS\n", 0);
	exit(EXIT_SUCCESS);
	return 0;
}



#endif


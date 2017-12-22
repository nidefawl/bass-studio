#include <glad/glad.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#define NANOVG_GL3_IMPLEMENTATION
#include <nanovg.h>
#include <nanovg_gl.h>

#include <windef.h>
#include <windows.h>
#include <ole2.h>

#define WIN32API_CALLBACK_TYPE __stdcall

#include <math.h>
#include <chrono>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
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

#include "../platform/win/winheaders.h"
#include "../platform/win/DropTarget.h"

#include "config.h"
#include "exceptions.h"
#include "color_util.h"
#include "mouse.h"
#include "keyboard.h"

#include "window.h"
#include "msgbox.h"
#include "menu.h"
#include "mainctrl.h"

#include "platform.h"

#include "logging.h"
#include "settings.h"
#include "renderresources.h"

#include "../host/vst_host.h"

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

namespace RenderResources{
void init(GLFWwindow *glfw, NVGcontext* vg); // renderresources.cpp
}
void syncMenu(HWND hwnd, ngui::MenuBar& menubar); // menu_win32.cpp

ngui::Menu* getUserDataFromMenu(HMENU hmenu, UINT uPos); // menu_win32.cpp

static WNDPROC glfwWndProc;

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




static LRESULT WIN32API_CALLBACK_TYPE winProc(HWND hwnd, UINT Msg, WPARAM wParam, LPARAM lParam);

static VOID WIN32API_CALLBACK_TYPE timerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);

static void glfw_startup_error_callback(int error, const char* description) {
	char errorCodeStr[1024] = { 0 };
	_snprintf_s(errorCodeStr, 1024 - 1, "Error %d: %s", error, description);
	ngui::show(errorCodeStr, "Error", ngui::Style::Error, ngui::Buttons::OK);
}
static void glfw_runtime_error_callback(int error, const char* description) {
	printf("Error %d: %s", error, description);
}
static void showerror(const char* description) {
	ngui::show(description, "Error", ngui::Style::Error, ngui::Buttons::OK);
}

#define IDT_TIMER1 0
class appwindow_dialog;
class appwindow_overlay;
#define SETTINGS_NAME "data/settings.json"
bool loadSettings(appsettings& _settings) {
	try {
		Stringstream ss;
		windowsize size;
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
	UINT_PTR timer = 0;
	std::vector<appwindow*> children;
	uint64_t last = 0;
protected:
	char name[32];
	int cursorIcon = CURSOR_DEFAULT;
	DropTarget* dropTarget = NULL;
	vec2 lastclickpos;
	vec2 lastmousepos;
	vec2 mousepos;
	GLFWwindow *glfw = NULL;
	HWND hwnd = NULL;
	NVGcontext* nanovgCtxt = NULL;
public:
	appwindow() {
		name[0] = 0;
	}
	virtual ~appwindow() {

	}
	GLFWwindow* getGLFW() {
		return glfw;
	}
	HWND getHWND() {
		return hwnd;
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
	virtual void onWindowClose() = 0;
	virtual void onRefresh() = 0;
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


	virtual void updateWindow() = 0;

	virtual void hideSystemCursor() {
		glfwSetInputMode(glfw, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
	}
	virtual void captureMouse() {
		glfwSetInputMode(glfw, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	}
	virtual void releaseMouse() {
		glfwSetInputMode(glfw, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	}
	virtual bool isMouseCaptured() {
		return glfwGetInputMode(glfw, GLFW_CURSOR) != GLFW_CURSOR_NORMAL;
	}

	virtual void initContext() {
		nanovgCtxt = nvgCreateGL3(NVG_ANTIALIAS | NVG_DEBUG);
		if (!nanovgCtxt) {
			throw appexception("Couldn't initialize nanovg");
		}
		int font = nvgCreateFont(nanovgCtxt, "sans", "res/fonts/Roboto-Regular.ttf");
		if (font == -1) {
			throw appexception("Failed loading font");
		}
	}
	virtual void preInitContext() {

	}
	virtual void create(const char* title, int w, int h) {
		strcpy_s(this->name, title);
		if (glfw)
			throw appexception("window not null");
		glfw = glfwCreateWindow(w, h, title, NULL, NULL);
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
		hwnd = glfwGetWin32Window(glfw);
		if (!hwnd)
			throw appexception("Couldn't get win32 window handle");
		preInitContext();
		SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)winProc);
		initContext();
		this->timer = SetTimer(hwnd, 0, 20, (TIMERPROC)timerProc);
		last = getTimeMillis();
	}
	void showWindow() {
		glfwShowWindow(glfw);
	}
	void hideWindow() {
		glfwHideWindow(glfw);
		//this->timer = SetTimer(hwnd, 0, 1, (TIMERPROC)NULL);
	}
	bool isWindowNotHidden() {
		if (hwnd == NULL)
			return false;
		return IsWindowVisible(hwnd) == 1;
//		WINDOWPLACEMENT wp = {};
//		wp.length = sizeof(WINDOWPLACEMENT);
//		if (!GetWindowPlacement(hwnd, &wp )) {
//			throw appexception(StringAsCStr(getLastWin32ErrorString()));
//		}
//		return wp.showCmd != SW_HIDE;
	}
	void maximize() {
		glfwMaximizeWindow(glfw);
	}
	virtual void flagNeedsRedraw() {
		InvalidateRect(hwnd, NULL, TRUE);
	}
    virtual bool filesDropBegin(std::vector<String>& files, ivec2 pos) {
    	return true;
    }
    virtual bool filesDropMove(ivec2 pos) {
    	return true;
    }
    virtual bool filesDropFinal(std::vector<String>& files, ivec2 pos) {
    	return true;
    }
    virtual void menuCommand(int cmd) {
    }
    virtual void onMenuOpen(ngui::Menu* menu) {
    }
	virtual LRESULT windowProc(HWND _hwnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
		switch (Msg) {

		case WM_COMMAND:
			menuCommand(LOWORD(wParam));
			break;
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
			break;
		default:
			break;

		}
		return CallWindowProc(glfwWndProc, _hwnd, Msg, wParam, lParam);
	}

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
};
class appwindow_main : public appwindow, public window_main  {
	MainCtrl* const ctrl;
	uint64_t dblclicktimer;
	int calls = 0;
	uint64_t tm_lastfps;
	String fpsStats;
public:
	appwindow_main(MainCtrl* _ctrl)
		: appwindow(),
		  window_main(),
		  ctrl(_ctrl),
		  tm_lastfps(getTimeMillis()) {
		dblclicktimer = 0;
	}
	void preInitContext() {
		//static code
		if (glfwWndProc == NULL) {
			glfwWndProc = (WNDPROC)GetWindowLongPtr(hwnd, GWLP_WNDPROC);
		}
		glfwMakeContextCurrent(glfw);
		// doesn't actually check availability
		if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
			throw appexception("Required OpenGL extensions not present.\nConsider updating graphics drivers");
		}
		//TODO: check actual required extensions availability
	}
	void create(const char* title, int w, int h);
	void updateMenu();
	void flagNeedsRedraw() override {
		appwindow::flagNeedsRedraw();
		if (cursorIcon != ctrl->cursorIcon) {
			glfwSetCursor(glfw, RenderResources::cursors[ctrl->cursorIcon]);
			cursorIcon = ctrl->cursorIcon;
		}
	}
	void destroy() {
		if (!glfw)
			throw appexception("window null");
		settings.size = windowsize(hwnd);
	}
	void updateWindow() {
//		flagNeedsRedraw();
		ctrl->onTick();
	}
	int getKeyMods() {
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
	void onRefresh()
	{
		PREVENT_REENTRANT("REENTRANT IN RENDER MAIN")
		glfwMakeContextCurrent(glfw);
		int winwidth, winheight;
		int fbwidth, fbheight;
		glfwGetWindowSize(glfw, &winwidth, &winheight);
		glfwGetFramebufferSize(glfw, &fbwidth, &fbheight);
		float pxratio = fbwidth / (float)winwidth;
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
		uint64_t tm = getTimeMillis();
		double since = (tm - tm_lastfps) / 1000.0;
		if (calls > 0 && since >= 1.0) {
			double fps = calls / since;
			fpsStats = StringFormat("%.2f fps\n", fps);
			glfwSetWindowTitle(glfw, StringAsCStr(fpsStats));
			tm_lastfps = tm;
			calls = 0;
		}
		calls++;
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

	void onWindowClose() 
	{
	}
	bool filesDropBegin(std::vector<String>& files, ivec2 pos) {
		flagNeedsRedraw();
		return ctrl->filesDropBegin(files, pos);
    }
	bool filesDropMove(ivec2 pos) {
		flagNeedsRedraw();
		return ctrl->filesDropMove(pos);
    }
	bool filesDropFinal(std::vector<String>& files, ivec2 pos) {
		flagNeedsRedraw();
		return ctrl->filesDropFinal(files, pos);
    }
    void requestClose() override {
		glfwSetWindowShouldClose(glfw, 1);
    }
    void menuCommand(int cmd) {
    	ctrl->menuCommand(cmd);
    }
    virtual void onMenuOpen(ngui::Menu* menu) {
    	ctrl->onMenuOpen(menu);
    }

	void onCharInput(unsigned int codepoint) {
		ctrl->onCharInput(codepoint);
		flagNeedsRedraw();
	}
	void onKeyInput(int key, int scancode, int action, int mods, const char* key_name)
	{
		/*if (action == GLFW_PRESS)
		my_printf("keyname %s, key %d, scancode %d\n", key_name, key, scancode);*/
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
	void onCursorEnter(int entered) {
		if (entered)
			glfwSetCursor(glfw, RenderResources::cursors[cursorIcon]);
	}
	void hideSystemCursor() {
//		appwindow::hideSystemCursor();
	}
	bool isMouseCaptured() {
		return appwindow::isMouseCaptured();
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
};

class appwindow_overlay : public appwindow, public window_overlay {
public:
	appwindow* const parent;
	ContextCtrl* const ctrl;
	appwindow_overlay(appwindow* _parent)
		: appwindow(),
		  window_overlay(),
		  parent(_parent),
		  ctrl(ContextCtrl::get())
	{
	}
	void create(const char* title, int w, int h);
	void onRefresh()
	{
		PREVENT_REENTRANT("REENTRANT IN RENDER OVERLAY")
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
	void onWindowClose()
	{
	}
	void updateWindow() {
	//	uint64_t tm = getTimeMillis();
	//	float f = (float)(tm / 1000.0);
	//	this->rgb[1] = 0.2f + sin(f*2.0f)*0.1f;
		//
		UpdateWindow(this->hwnd);
	}

	void show() {
		appwindow::showWindow();
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
};


class appwindow_dialog : public appwindow, public window_dialog {
public:
	appwindow *parent = NULL;
	appwindow_dialog(appwindow* _parent) : appwindow() {
		this->parent = _parent;
	}
	void create(const char* title, int w, int h) {
		glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
		glfwWindowHint(GLFW_VISIBLE, GL_FALSE);
		glfwWindowHint(GLFW_FOCUSED, GL_TRUE);
		appwindow::create(title, w, h);
		this->parent->onChildCreate(this);

		LONG l = GetWindowLong(hwnd, GWL_EXSTYLE);
		SetWindowLong(hwnd, GWL_EXSTYLE, l & ~WS_EX_APPWINDOW);
		SetWindowLong(hwnd, GWL_STYLE, WS_CAPTION | WS_POPUP | WS_CLIPSIBLINGS | WS_SYSMENU);
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
		glfwMakeContextCurrent(glfw);
		initContext();
	}
	void onRefresh()
	{
		PREVENT_REENTRANT("REENTRANT IN RENDER DIALOG")
		glfwMakeContextCurrent(glfw);
		float ratio;
		int width, height;
		glfwGetFramebufferSize(glfw, &width, &height);
		ratio = width / (float)height;
		glViewport(0, 0, width, height);
		static const vec4 clearc = int32vec4(0xff121212);
		glClearColor(clearc[0], clearc[1], clearc[2], clearc[3]);
		glClear(GL_COLOR_BUFFER_BIT);
		glfwSwapBuffers(glfw);
	}
	void onWindowClose()
	{
		EnableWindow(parent->getHWND(), TRUE);
		glfwDestroyWindow(glfw);
		this->parent->onChildClose(this);
	}
	void updateWindow() {
	//	uint64_t tm = getTimeMillis();
	//	float f = (float)(tm / 1000.0);
	//	this->rgb[1] = 0.2f + sin(f*2.0f)*0.1f;
		//flagNeedsRedraw();
		UpdateWindow(this->hwnd);
	}
	void onKeyInput(int key, int scancode, int action, int mods, const char* key_name)
	{
		if (action == GLFW_PRESS) {
			if (key == GLFW_KEY_ESCAPE) {
				onWindowClose();
				return;
			}
		}
	}
	void show() {
		appwindow::showWindow();
		EnableWindow(parent->getHWND(), FALSE);
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
};
void appwindow_main::updateMenu() {
	ngui::MenuBar& menubar = ctrl->getMenubar();
	syncMenu(hwnd, menubar);
}
void appwindow_main::create(const char* title, int w, int h) {
	glfwWindowHint(GLFW_VISIBLE, GL_FALSE);
	appwindow::create(title, w, h);
	glfwSetWindowSizeLimits(glfw, 640, 480, GLFW_DONT_CARE, GLFW_DONT_CARE);
	RenderResources::init(glfw, nanovgCtxt);
	if (!ctrl->init(this, this->nanovgCtxt)) {
		throw appexception("Couldn't start application");
	}

	this->dropTarget = RegisterDropWindow(hwnd, this);

	appwindow_overlay* overlayWindow = new appwindow_overlay(this);
	String sName = StringFormat("%s menu", this->name);
	overlayWindow->create(StringAsCStr(sName), 200, 200);
	if (settings.size.valid) {
		settings.size.apply(hwnd);
	    RECT area;
	    GetClientRect(hwnd, &area);
	    onWindowSizeChanged(area.right-area.left, area.bottom-area.top);
	} else {
		this->maximize();
	}
}
void appwindow_overlay::create(const char* title, int w, int h) {
	glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
	glfwWindowHint(GLFW_VISIBLE, GL_FALSE);
	glfwWindowHint(GLFW_FOCUSED, GL_FALSE);
	glfwWindowHint(GLFW_DECORATED, GL_FALSE);
	appwindow::create(title, w, h);
	LONG l = GetWindowLong(hwnd, GWL_EXSTYLE);
	l = l & ~WS_EX_APPWINDOW;
	l = l | WS_EX_TOOLWINDOW;
//	l = l | WS_EX_LAYERED;
	SetWindowLong(hwnd, GWL_EXSTYLE, l);
	//	SetWindowLong(hwnd, GWL_EXSTYLE, l & ~WS_EX_APPWINDOW);
	SetWindowLong(hwnd, GWL_STYLE, WS_CHILD | WS_CLIPSIBLINGS);
//		SetWindowLong(hwnd, GWL_EXSTYLE, l & ~WS_EX_APPWINDOW);
//		SetWindowLong(hwnd, GWL_STYLE, WS_CAPTION | WS_POPUP | WS_CLIPSIBLINGS | WS_SYSMENU);
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
	glfwMakeContextCurrent(glfw);
	initContext();
	if (!ctrl->init(this, this->nanovgCtxt)) {
		throw appexception("Couldn't start application");
	}
}
window_dialog* appwindow_main::createDialog() {
	appwindow_dialog* popupWindow = new appwindow_dialog(this);
	String sName = StringFormat("%s dialog", this->name);
	popupWindow->create(StringAsCStr(sName), 200, 200);
	return popupWindow;
}
static appwindow* getUserData(GLFWwindow *w) {
	appwindow* impl = (appwindow*) glfwGetWindowUserPointer(w);
	if (impl == NULL) {
		throw appexception("Invalid window handle");
	}
	return impl;
}
String excDescription;
void on_terminate() {
	glfwTerminate();
	if (excDescription.length()) {

		ngui::show(StringAsCStr(excDescription), "Error", ngui::Style::Error, ngui::Buttons::OK);
	}
}

void handleStdException(std::exception& e) {
	excDescription = StringFormat("Fatal error: %s", e.what());
	std::terminate();
}
void handleException() {
	excDescription = "Unhandled program exception";
	std::terminate();
}

#define EXC_TRY try {
#define EXC_CATCH \
	} catch (std::exception& e) { 									\
		handleStdException(e);										\
	} catch (...) {													\
		handleException();											\
	}
static void glfw_cb_mousepos(GLFWwindow *w, double x, double y) {
	EXC_TRY
	getUserData(w)->_onMouseMoved(x, y);
	EXC_CATCH
}
static void glfw_cb_mousebutton(GLFWwindow *w, int button, int action, int mods) {
	EXC_TRY
	getUserData(w)->onMouseButton(button, action, mods);
	EXC_CATCH
}
static void glfw_cb_cursorenter(GLFWwindow *w, int entered) {
	EXC_TRY
	getUserData(w)->onCursorEnter(entered);
	EXC_CATCH
}
static void glfw_cb_mousescroll(GLFWwindow *w, double xoffset, double yoffset) {
	EXC_TRY
	getUserData(w)->onMouseScrolled(xoffset, yoffset);
	EXC_CATCH
}
static void glfw_cb_keyinput(GLFWwindow *w, int key, int scancode, int action, int mods) {
	EXC_TRY
	const char* key_name = glfwGetKeyName(key, scancode);
	getUserData(w)->onKeyInput(key, scancode, action, mods, key_name);
	EXC_CATCH
}
static void glfw_cb_charinput(GLFWwindow *w, unsigned int codepoint) {
	EXC_TRY
	getUserData(w)->onCharInput(codepoint);
	EXC_CATCH
}
static void glfw_cb_refresh(GLFWwindow *w) {
	EXC_TRY
	getUserData(w)->onRefresh();
	EXC_CATCH
}
static void glfw_cb_windowclose(GLFWwindow *w) {
	EXC_TRY
	getUserData(w)->onWindowClose();
	EXC_CATCH
}
static void glfw_cb_windowfocus(GLFWwindow *w, int focused) {
	EXC_TRY
	getUserData(w)->onWindowFocusChanged(focused);
	EXC_CATCH
}
static void glfw_cb_windowwize(GLFWwindow *w, int width, int height) {
	EXC_TRY
	getUserData(w)->onWindowSizeChanged(width, height);
	EXC_CATCH
}
static void glfw_cb_framebuffersize(GLFWwindow *w, int width, int height) {
	EXC_TRY
	getUserData(w)->onFramebufferSizeChanged(width, height);
	EXC_CATCH
}
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
	impl->updateWindow();
	EXC_CATCH

}
static LRESULT WIN32API_CALLBACK_TYPE winProc(HWND hwnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
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

#define MAX_LEN_MY_PRINTF 4096
void _my_printf(const char *file, int line, const char *func, const char *fmt, ...) {
	char buf[MAX_LEN_MY_PRINTF];
	//char buf2[MAX_LEN_MY_PRINTF] = { 0 };
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, MAX_LEN_MY_PRINTF - 1, fmt, args);
	va_end(args);
	const char * pch = !file ? NULL : strrchr(file, '\\');
	pch = pch ? pch+1 : file;
	printf("%s:%d %s: %s", pch, line, func, buf);
	//sprintf_s(buf2, MAX_LEN_MY_PRINTF - 1, "%s:%d %s: %s", file, line, func, buf);
	//appendLog(buf2);
#ifdef __MINGW32__
	fflush(stdout);
#endif
}
void printLeaked();

static MainCtrl* ctrl = new MainCtrl();
static vsthost* audiohost = new vsthost(*ctrl, 44100, 128);
static appwindow_main* mainWindow = NULL;
MainCtrl* MainCtrl::get() {
	return ctrl;
}
vsthost* vsthost::getInstance()
{
	return audiohost;
}
HWND getMainHWND() {
	return mainWindow ? mainWindow->getHWND() : NULL;
}
#ifndef TEST_PROJECT
int mainHost() {
	OleInitialize(0);
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
	glfwDefaultWindowHints();

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_STENCIL_BITS, 8);
	glfwWindowHint(GLFW_DEPTH_BITS, 24);
	mainWindow = new appwindow_main(ctrl);
	mainWindow->create("main window", 1280, 720);
	mainWindow->showWindow();
	glfwSetErrorCallback(glfw_runtime_error_callback);
	ctrl->postInit();
	audiohost->postInit();
	GLFWwindow* glfwHandle = mainWindow->getGLFW();
	while (!glfwWindowShouldClose(glfwHandle)) {
		glfwWaitEvents();
	}
	audiohost->stopAudio();
	mainWindow->destroy();
	audiohost->unload();
	ctrl->destroy();
	audiohost->destroy();

	// I _want_ to use smart pointers, but eclipse cdt doesn't want me to

	glfwTerminate();
	saveSettings(settings);
	DELETE_PTR(mainWindow);
	DELETE_PTR(ctrl);
	EXC_CATCH
	printLeaked();

	exit(EXIT_SUCCESS);
	OleUninitialize();
	return 0;
}


#endif

#ifdef __linux__
#include "../../vst_window.h"
#include "../../vst_host.h"
#include "../../plugin/vst_plugin.h"
#include <vector>
#include <GLFW/glfw3.h>

struct Display;
void sendExposeEvent(GLFWwindow* glfw);
GLFWwindow* getTopLevelGlfwWindow();
extern "C" {
WINDOW_HANDLE glfwGetX11Window(GLFWwindow* window);
Display* glfwGetX11Display();
}

namespace {
	static std::vector<vst_window*> vst_window_list;
	static void addWindow (vst_window* window)
	{
		vst_window_list.push_back (window);
	}
	static void removeWindow (vst_window* window)
	{
		auto it = std::find (vst_window_list.begin (), vst_window_list.end (), window);
		if (it != vst_window_list.end ())
			vst_window_list.erase(it);
	}
	static vst_window* getWindowByHWND (WINDOW_HANDLE hwnd)
	{
		auto it = std::find_if(vst_window_list.begin (), vst_window_list.end (), [hwnd](vst_window* window) {
			return window->getHWND() == hwnd;
		});
		if (it != vst_window_list.end ())
			return *it;
		return nullptr;
	}
}


vst_window* vst_window::make (vstplugin* plugin, const String& name, Size size, bool resizeable)
{
	vst_window* vstWindow = new vst_window();
	vstWindow->init(plugin, name, size, resizeable);
	return vstWindow;
}
vst_window* vst_window::getVSTWindow(WINDOW_HANDLE handle)
{
	dbgassert(handle);
	vst_window* vstwinhandle = nullptr;
	return vstwinhandle;
}


//------------------------------------------------------------------------
std::vector<vst_window*>& vst_window::getWindows ()
{
	return vst_window_list;
}

//------------------------------------------------------------------------
bool vst_window::init(vstplugin* plugin, const String& name, Size size, bool resizeable)
{
	this->plugin = plugin;

	GLFWwindow* window = getTopLevelGlfwWindow();
	glfwDefaultWindowHints();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfw = glfwCreateWindow(size.width, size.height, StringAsCStr(name), NULL, NULL);
	WINDOW_HANDLE x11Window = glfwGetX11Window(glfw);
	hwnd = x11Window;
	//create native window
	addWindow(this);
	return hwnd != 0;
}

//------------------------------------------------------------------------
void vst_window::close()
{
	plugin->onClose();
	glfwHideWindow(glfw);
//	ShowWindow(hwnd, false);
}

//------------------------------------------------------------------------
void vst_window::destroy()
{
	plugin->onWindowDestroy();
//	SetWindowLongPtr (hwnd, GWLP_USERDATA, (__int3264) (LONG_PTR) nullptr);
//	DestroyWindow(hwnd);
	glfwDestroyWindow(glfw);
	removeWindow (this);
}
//------------------------------------------------------------------------
void vst_window::show()
{
	glfwShowWindow(glfw);
//	SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOCOPYBITS | SWP_SHOWWINDOW);
	plugin->onShow(this);
}

//------------------------------------------------------------------------
Size vst_window::getContentSize ()
{
	Size s;
	glfwGetWindowSize(glfw, &s.width, &s.height);
//	RECT r;
//	GetClientRect (hwnd, &r);
//	return {r.right - r.left, r.bottom - r.top};
	return {0, 0};
}



void vst_window::updateWindow() {
//	InvalidateRgn(hwnd, NULL, TRUE);
	sendExposeEvent(glfw);
}
//------------------------------------------------------------------------
void vst_window::resize (Size newSize)
{
	if (getContentSize () == newSize)
		return;
	glfwSetWindowSize(glfw, newSize.width, newSize.height);
//	WINDOWINFO windowInfo;
//	GetWindowInfo (hwnd, &windowInfo);
//	RECT clientRect {};
//	clientRect.right = newSize.width;
//	clientRect.bottom = newSize.height;
//	AdjustWindowRectEx (&clientRect, windowInfo.dwStyle, false, windowInfo.dwExStyle);
//	SetWindowPos (hwnd, HWND_TOP, 0, 0, clientRect.right - clientRect.left,
//	              clientRect.bottom - clientRect.top, SWP_NOMOVE | SWP_NOCOPYBITS | SWP_NOACTIVATE);
}

//------------------------------------------------------------------------
WINDOW_HANDLE vst_window::getHWND () const
{
	return hwnd;
}

#endif

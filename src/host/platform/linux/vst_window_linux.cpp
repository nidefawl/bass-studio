#if defined(__linux__) || defined(__APPLE__)
#include "../../vst_window.h"
#include "../../vst_host.h"
#include "../../plugin/vst_plugin.h"
#include <vector>
#include <GLFW/glfw3.h>
#ifdef __linux__
#define GLFW_EXPOSE_NATIVE_X11
#endif
#ifdef __APPLE__
#define GLFW_EXPOSE_NATIVE_COCOA
#endif
#include <GLFW/glfw3native.h>

GLFWwindow* getTopLevelGlfwWindow();

#ifdef __linux__
//struct Display;
void sendExposeEvent(GLFWwindow* glfw);
extern "C" {
WINDOW_HANDLE glfwGetX11Window(GLFWwindow* window);
//Display* glfwGetX11Display();
}
#endif

namespace {
	std::vector<vst_window*> vst_window_list;
	void addWindow (vst_window* window)
	{
		vst_window_list.push_back (window);
	}
	void removeWindow (vst_window* window)
	{
		auto it = std::find (vst_window_list.begin (), vst_window_list.end (), window);
		if (it != vst_window_list.end ())
			vst_window_list.erase(it);
	}
	/* vst_window* getWindowByHWND (WINDOW_HANDLE hwnd)
	{
		auto it = std::find_if(vst_window_list.begin (), vst_window_list.end (), [hwnd](vst_window* window) {
			return window->getHWND() == hwnd;
		});
		if (it != vst_window_list.end ())
			return *it;
		return nullptr;
	} */
}


vst_window* vst_window::make (vstplugin* plugin, const String& name, ivec2 size, bool resizeable)
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
bool vst_window::init(vstplugin* plugin, const String& name, ivec2 size, bool resizeable)
{
	this->plugin = plugin;

	glfwDefaultWindowHints();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfw = glfwCreateWindow(size.x, size.y, StringAsCStr(name), NULL, NULL);
	#ifdef __linux__
	WINDOW_HANDLE x11Window = glfwGetX11Window(glfw);
	hwnd = x11Window;
	#endif
	#ifdef __APPLE__
	WINDOW_HANDLE x11Window = *reinterpret_cast<int32_t*>(glfwGetCocoaWindow(glfw));
	hwnd = x11Window;
	#endif
	
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
ivec2 vst_window::getContentSize() const
{
	ivec2 s;
	glfwGetWindowSize(glfw, &s.x, &s.y);
//	RECT r;
//	GetClientRect (hwnd, &r);
//	return {r.right - r.left, r.bottom - r.top};
	return s;//{0, 0};
}



void vst_window::updateWindow() const {
//	InvalidateRgn(hwnd, NULL, TRUE);
#ifdef __linux__
	sendExposeEvent(glfw);
#endif
}
//------------------------------------------------------------------------
void vst_window::resize (ivec2 newSize) const
{
	if (getContentSize () == newSize)
		return;
	glfwSetWindowSize(glfw, newSize.x, newSize.y);
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

void vst_window::captureWindowFrame() {
	log_printf("Capture window frame not implemented\n");
}
#endif

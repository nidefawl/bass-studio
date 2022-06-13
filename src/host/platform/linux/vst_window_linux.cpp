#include "logging.h"
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
void sendExposeEvent(GLFWwindow* glfw);
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
	auto vstWindow = new vst_window();
	vstWindow->init(plugin, name, size, resizeable);
	return vstWindow;
}
vst_window* vst_window::getVSTWindow(WINDOW_HANDLE handle)
{
	dbgassert(handle);
	vst_window* vstwinhandle = nullptr;
	return vstwinhandle;
}

std::vector<vst_window*>& vst_window::getWindows ()
{
	return vst_window_list;
}


static void glfw_cb_windowclose(GLFWwindow* w) {
	auto* userpointer = static_cast<vst_window*>(glfwGetWindowUserPointer(w));
	if (userpointer) {
		userpointer->close();
	}
}
static void glfw_cb_windowsize(GLFWwindow* w, int width, int height) {
	auto* userpointer = static_cast<vst_window*>(glfwGetWindowUserPointer(w));
	if (userpointer) {
		userpointer->onResize(ivec2(width, height));
	}
}
GLFWwindow* getTopLevelGlfwWindow();
void glfw_main_cb_keyinput(GLFWwindow* w, int key, int scancode, int action, int mods);
static void glfw_cb_keyinput(GLFWwindow* w, int key, int scancode, int action, int mods) {
	GLFWwindow* mainWindow = getTopLevelGlfwWindow();
	if (mainWindow) {
		glfw_main_cb_keyinput(mainWindow, key, scancode, action, mods);	
	}
}

bool vst_window::init(vstplugin* plugin, const String& name, ivec2 size, bool resizeable)
{
	this->plugin = plugin;

	glfwDefaultWindowHints();
	glfwWindowHint(GLFW_RESIZABLE, resizeable ? GLFW_TRUE : GLFW_FALSE);
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
#ifdef __linux__
        glfwWindowHintString(GLFW_X11_INSTANCE_NAME, "DAW");
        glfwWindowHintString(GLFW_X11_CLASS_NAME, "DAW");
#endif
#ifdef __APPLE__
        glfwWindowHintString(GLFW_COCOA_FRAME_NAME, "DAW");
#endif
	glfw = glfwCreateWindow(size.x, size.y, StringAsCStr(name), NULL, NULL);
    glfwSetWindowCloseCallback(glfw, glfw_cb_windowclose);
    glfwSetKeyCallback(glfw, glfw_cb_keyinput);
    glfwSetWindowSizeCallback(glfw, glfw_cb_windowsize);
	glfwSetWindowUserPointer(glfw, this);
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

void vst_window::close()
{
	ivec4 posSize = {};
	glfwGetWindowPos(glfw, &posSize.x, &posSize.y);
	glfwGetWindowSize(glfw, &posSize.z, &posSize.w);
	plugin->storeWindowPosSize(posSize);
	plugin->onClose();
	glfwHideWindow(glfw);
}

void vst_window::destroy()
{
	plugin->onWindowDestroy();
	glfwDestroyWindow(glfw);
	removeWindow (this);
}
void vst_window::show()
{
	glfwShowWindow(glfw);
	plugin->onShow(this);
}

ivec2 vst_window::getContentSize() const
{
	ivec2 s{};
	glfwGetWindowSize(glfw, &s.x, &s.y);
	return s;;
}

void vst_window::updateWindow() const {
#ifdef __linux__
	// sendExposeEvent(glfw);
#endif
}

void vst_window::resize (ivec2 newSize) const
{
	if (getContentSize () == newSize)
		return;
	glfwSetWindowSize(glfw, newSize.x, newSize.y);
}

void vst_window::onResize (ivec2 newSize)
{
	plugin->onWindowResize(newSize);
}
void vst_window::setPosition(ivec2 newPos) {
	glfwSetWindowPos(glfw, newPos.x, newPos.y);
}


WINDOW_HANDLE vst_window::getHWND () const
{
	return hwnd;
}

void vst_window::captureWindowFrame() {
	log_printf("Capture window frame not implemented\n");
}
#endif

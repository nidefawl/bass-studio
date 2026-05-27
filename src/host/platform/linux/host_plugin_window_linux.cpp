#if defined(__linux__) || defined(__APPLE__)
#include "logging.hpp"
#include "host/host_plugin_window.hpp"
#include "host/host_pluginmanager.hpp"
#include "host/plugin/base/base-plugin.hpp"
#include "buildinfo.h"
#include <vector>
#include <GLFW/glfw3.h>
#ifdef __linux__
#define GLFW_EXPOSE_NATIVE_X11
#endif
#ifdef __APPLE__
#define GLFW_EXPOSE_NATIVE_COCOA
#endif
#include <GLFW/glfw3native.h>

#ifdef __APPLE__
static_assert(sizeof(WINDOW_HANDLE) == 8);
#endif

GLFWwindow* getTopLevelGlfwWindow();

void sendExposeEvent(GLFWwindow* glfw);

namespace {
	std::vector<host_plugin_window*> host_plugin_window_list;
	void addWindow (host_plugin_window* window)
	{
		host_plugin_window_list.push_back (window);
	}
	void removeWindow (host_plugin_window* window)
	{
		auto it = std::find (host_plugin_window_list.begin (), host_plugin_window_list.end (), window);
		if (it != host_plugin_window_list.end ())
			host_plugin_window_list.erase(it);
	}
	/* host_plugin_window* getWindowByHWND (WINDOW_HANDLE hwnd)
	{
		auto it = std::find_if(host_plugin_window_list.begin (), host_plugin_window_list.end (), [hwnd](host_plugin_window* window) {
			return window->getHWND() == hwnd;
		});
		if (it != host_plugin_window_list.end ())
			return *it;
		return nullptr;
	} */
}


host_plugin_window* host_plugin_window::make (effectbase* plugin, const String& name, ivec2 size, bool resizeable)
{
	auto plugWindow = new host_plugin_window();
	plugWindow->init(plugin, name, size, resizeable);
	return plugWindow;
}
host_plugin_window* host_plugin_window::getWindowInstance(WINDOW_HANDLE handle)
{
	dbgassert(handle);
	return nullptr;
}

std::vector<host_plugin_window*>& host_plugin_window::getWindows ()
{
	return host_plugin_window_list;
}


static void glfw_cb_windowclose(GLFWwindow* w) {
	auto* userpointer = static_cast<host_plugin_window*>(glfwGetWindowUserPointer(w));
	if (userpointer) {
		userpointer->close();
	}
}
static void glfw_cb_windowsize(GLFWwindow* w, int width, int height) {
	auto* userpointer = static_cast<host_plugin_window*>(glfwGetWindowUserPointer(w));
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

bool host_plugin_window::init(effectbase* plugin, const String& name, ivec2 size, bool resizeable)
{
	this->plugin = plugin;

	glfwDefaultWindowHints();
	glfwWindowHint(GLFW_RESIZABLE, resizeable ? GLFW_TRUE : GLFW_FALSE);
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
#ifdef __linux__
        glfwWindowHintString(GLFW_X11_INSTANCE_NAME, BuildInfo::PRODUCT_NAME_UPPER);
        glfwWindowHintString(GLFW_X11_CLASS_NAME, BuildInfo::PRODUCT_NAME_UPPER);
#endif
#ifdef __APPLE__
        glfwWindowHintString(GLFW_COCOA_FRAME_NAME, BuildInfo::PRODUCT_NAME_UPPER);
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
	hwnd = reinterpret_cast<WINDOW_HANDLE>(glfwGetCocoaWindow(glfw));
	cocoaView = reinterpret_cast<WINDOW_HANDLE>(glfwGetCocoaNSView(glfw));
	#endif
	
	//create native window
	addWindow(this);
	return hwnd != 0;
}

void host_plugin_window::close()
{
    storePosition();
	plugin->onClose();
	glfwHideWindow(glfw);
}

void host_plugin_window::storePosition() {
    ivec4 posSize = {};
    glfwGetWindowPos(glfw, &posSize.x, &posSize.y);
    glfwGetWindowSize(glfw, &posSize.z, &posSize.w);
    plugin->storeWindowPosSize(posSize);
}

void host_plugin_window::destroy()
{
	plugin->onWindowDestroy();
	glfwDestroyWindow(glfw);
	removeWindow (this);
}
void host_plugin_window::show(ivec4 posSize, bool bSetPos, bool bSetSize)
{
    if (bSetPos) {
		glfwSetWindowPos(glfw, posSize.x, posSize.y);
    }
    if (bSetSize) {
        resize(ivec2(posSize.z, posSize.w));
    }
	glfwShowWindow(glfw);
	plugin->onShow(this);
    // Toplevel LV2 UIs (e.g. Cardinal) hide this shell; do not touch GLFW/X11 after.
    if (plugin->usesExternalToplevelWindow()) {
        return;
    }
    if (bSetPos) {
		glfwSetWindowPos(glfw, posSize.x, posSize.y);
    }
    if (bSetSize) {
        resize(ivec2(posSize.z, posSize.w));
    }
}

ivec2 host_plugin_window::getContentSize() const
{
	ivec2 s{};
	glfwGetWindowSize(glfw, &s.x, &s.y);
	return s;;
}

void host_plugin_window::updateFromMainThread() const {
}

void host_plugin_window::resize (ivec2 newSize) const
{
	glfwSetWindowSize(glfw, newSize.x, newSize.y);
	plugin->onWindowResize(newSize);
}

void host_plugin_window::onResize (ivec2 newSize)
{
	plugin->onWindowResize(newSize);
}
void host_plugin_window::setPosition(ivec2 newPos) {
	glfwSetWindowPos(glfw, newPos.x, newPos.y);
}


WINDOW_HANDLE host_plugin_window::getWindowHandle() const
{
#ifdef __APPLE__
	return cocoaView;
#else
	return hwnd;
#endif
}

WINDOW_HANDLE host_plugin_window::getHWND() const
{
	return hwnd;
}

void host_plugin_window::captureWindowFrame() {
	log_printf("Capture window frame not implemented\n");
}
#endif

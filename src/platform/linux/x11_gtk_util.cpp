#ifdef __linux__
#define GLFW_EXPOSE_NATIVE_X11 1
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <gtk/gtk.h>
#include "platform.h"
void sendExposeEvent(GLFWwindow* glfw) {
//	int w, h;
//	glfwGetWindowSize(glfw, &w, &h);
	Display* x11display = glfwGetX11Display();
	Window x11Window = glfwGetX11Window(glfw);
	XExposeEvent ev = { Expose, 0, 1, x11display, x11Window, 0, 0, 1, 1, 0 };
	XSendEvent(x11display, x11Window, false, ExposureMask, (XEvent *)&ev);
//	XFlush(x11display);



//    XClearArea (x11display, x11Window, 0, 0, w, h, True);
}


#endif


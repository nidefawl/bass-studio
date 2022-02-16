#if defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11 1
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <X11/Xatom.h>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <gtk/gtk.h>
#include "platform.h"

void sendExposeEvent(GLFWwindow* glfw) {
    // int w, h;
    // glfwGetWindowSize(glfw, &w, &h);
    Display* x11display = glfwGetX11Display();
    Window x11Window    = glfwGetX11Window(glfw);
    XExposeEvent ev     = { Expose, 0, 1, x11display, x11Window, 0, 0, 1, 1, 0 };
    XSendEvent(x11display, x11Window, false, ExposureMask, (XEvent*) &ev);
    // XFlush(x11display);
    // XClearArea(x11display, x11Window, 0, 0, w, h, True);
}
class window_base;

GLFWwindow* getGlfwFromWindowBase(window_base* w);

Window getX11FromWindowBase(window_base* w) {
	GLFWwindow* glfw = getGlfwFromWindowBase(w);
	Window x11Window = glfwGetX11Window(glfw);
	return x11Window;
}

Display* getX11Display() {;
	return glfwGetX11Display();
}

struct MwmHints {
    unsigned long flags;
    unsigned long functions;
    unsigned long decorations;
    long input_mode;
    unsigned long status;
};

enum {
    MWM_HINTS_FUNCTIONS   = (1L << 0),
    MWM_HINTS_DECORATIONS = (1L << 1),

    MWM_FUNC_ALL      = (1L << 0),
    MWM_FUNC_RESIZE   = (1L << 1),
    MWM_FUNC_MOVE     = (1L << 2),
    MWM_FUNC_MINIMIZE = (1L << 3),
    MWM_FUNC_MAXIMIZE = (1L << 4),
    MWM_FUNC_CLOSE    = (1L << 5)
};

void setIsTransientFor(GLFWwindow* glfw, GLFWwindow* glfwChild) {
    XSetTransientForHint(getX11Display(), glfwGetX11Window(glfwChild), glfwGetX11Window(glfw));
    // // MwmHints hints;
    // // hints.flags           = MWM_HINTS_DECORATIONS;
    // // hints.decorations     = 0;
    // Atom mwmHintsProperty = XInternAtom(getX11Display(), "_NET_WM_STATE_SKIP_TASKBAR", 1);
    // XChangeProperty(getX11Display(), glfwGetX11Window(glfwChild), XInternAtom(getX11Display(), "_NET_WM_STATE", False), XA_ATOM, 32, PropModeReplace, (const unsigned char*) &mwmHintsProperty, 1);
}

#endif


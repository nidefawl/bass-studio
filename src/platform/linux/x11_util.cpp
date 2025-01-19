#include "host/host_plugin_window.h"
#include "logging.h"
#include <cstddef>
#if defined(__linux__)
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_X11 1
#include <GLFW/glfw3native.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <X11/Xatom.h>
#include "platform.h"

extern "C" {
    static bool IsWindowManagerStateSet(Display* display,
                                        Window window,
                                        const char* atom_name) {
        Atom net_wm_state = XInternAtom(display, "_NET_WM_STATE", False);
        if (net_wm_state == None) {
            return false;
        }

        Atom atom = XInternAtom(display, atom_name, False);
        if (atom == None) {
            return false;
        }

        bool set = false;

        Atom actual_type{};
        int actual_format{};
        unsigned long tmp_num_items{};
        unsigned long bytes_after{};
        unsigned char* prop{};
        if (Success == XGetWindowProperty(display,         /* display */
                                        window,          /* window */
                                        net_wm_state,        /* property */
                                        0,               /* long_offset */
                                        ~0L,             /* long_length */
                                        False,           /* delete */
                                        AnyPropertyType, /* req_type */
                                        &actual_type,    /* actual_type_return */
                                        &actual_format,  /* actual_format_return */
                                        &tmp_num_items,  /* nitems_return */
                                        &bytes_after,    /* bytes_after_return */
                                        &prop))          /* prop_return */
        {
            auto ws = (Atom*) prop;
            for (unsigned long ws_idx = 0; ws_idx < tmp_num_items; ++ws_idx) {
                if (ws[ws_idx] == atom) {
                    set = true;
                    break;
                }
            }
            XFree(ws);
        }
        return set;
    }
}
void sendExposeEvent(GLFWwindow* glfw) {
    int w, h;
    glfwGetWindowSize(glfw, &w, &h);
    Display* x11display = glfwGetX11Display();
    Window x11Window    = glfwGetX11Window(glfw);
    XExposeEvent ev     = { Expose, 0, 1, x11display, x11Window, 0, 0, w, h, 0 };
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
GLFWwindow* getTopLevelGlfwWindow();

int getTopLevelWindowXID() {
	GLFWwindow* glfw = getTopLevelGlfwWindow();
    if (glfw) {
	    return static_cast<int>(glfwGetX11Window(glfw));
    }
	return 0;
}

static void SetWindowMaximizedFlag(Display* display,
                                   Window window,
                                   const char* name,
                                   bool state) {
    Atom net_wm_state = XInternAtom(display, "_NET_WM_STATE", False);
    if (net_wm_state == None) {
        return;
    }

    Atom atom = XInternAtom(display, name, False);
    if (atom == None) {
        return;
    }

    XEvent e;
    memset(&e, 0, sizeof e);

    e.xany.type            = ClientMessage;
    e.xclient.message_type = net_wm_state;
    e.xclient.format       = 32;
    e.xclient.window       = window;
    e.xclient.data.l[0]    = state ? 1 : 0;
    e.xclient.data.l[1]    = (long) atom;
    // http://standards.freedesktop.org/wm-spec/1.3/ar01s07.html#sourceindication
    e.xclient.data.l[3] = 1; /* "pagers and other Clients that represent direct user actions" */

    XSendEvent(display,
               XDefaultRootWindow(display),
               True,
               SubstructureNotifyMask | SubstructureRedirectMask,
               &e);
}

bool restoreX11WindowPos(Display* display, Window window, appwindow_size_t* appwindowsize) {
    if (!display || !window) {
        return false;
    }
    if (appwindowsize->type != 2) {
        return false;
    }
    appwindow_size_linux_t* placement = (appwindow_size_linux_t*) appwindowsize->data;

    SetWindowMaximizedFlag(display, window, "_NET_WM_STATE_MAXIMIZED_HORZ", false);
    SetWindowMaximizedFlag(display, window, "_NET_WM_STATE_MAXIMIZED_VERT", false);

    if (placement->w > 0 && placement->h > 0) {
        XMoveWindow(display, window, placement->x, placement->y);
    }

    if (placement->hmax) {
        SetWindowMaximizedFlag(display, window, "_NET_WM_STATE_MAXIMIZED_HORZ", true);
    }

    if (placement->vmax) {
        SetWindowMaximizedFlag(display, window, "_NET_WM_STATE_MAXIMIZED_VERT", true);
    }

    if (!placement->vmax && !placement->hmax) {
        XResizeWindow(display, window, placement->w, placement->h);
    }
    return true;
}
bool restoreWindowPos(GLFWwindow* glfw, appwindow_size_t* appwindowsize) {
    return restoreX11WindowPos(glfwGetX11Display(), glfwGetX11Window(glfw), appwindowsize);
}
bool restoreHostWindowPos(host_plugin_window* hostWindow, appwindow_size_t* size) {
    return restoreX11WindowPos(glfwGetX11Display(), hostWindow->getHWND(), size);
}
bool saveX11WindowPos(Display* display, Window window, appwindow_size_t* appwindowsize) {
  if (!display || !window) {
      return false;
  }

  XWindowAttributes xwa;
  if (!XGetWindowAttributes(display, window, &xwa)) {
      return false;
  }

  Window child{};
  if (!XTranslateCoordinates(display, window, xwa.root, 0, 0, &xwa.x, &xwa.y, &child)) {
      return false;
  }

  if (xwa.width < 0 || xwa.height < 0) {
      return false;
  }
  appwindow_size_linux_t* placement = (appwindow_size_linux_t*) appwindowsize->data;
  placement->valid = true;
  placement->hmax  = IsWindowManagerStateSet(display, window, "_NET_WM_STATE_MAXIMIZED_HORZ");
  placement->vmax  = IsWindowManagerStateSet(display, window, "_NET_WM_STATE_MAXIMIZED_VERT");

  placement->x = xwa.x;
  placement->w = xwa.width;
  placement->y = xwa.y;
  placement->h = xwa.height;
  return true;
}

bool saveWindowPos(GLFWwindow* glfw, appwindow_size_t* appwindowsize) {
    *appwindowsize = appwindow_size_t{};
    appwindowsize->type = 2;
    return saveX11WindowPos(glfwGetX11Display(), glfwGetX11Window(glfw), appwindowsize);
}

bool saveHostWindowPos(host_plugin_window* hostWindow, appwindow_size_t* appwindowsize) {
  Window window = hostWindow->getHWND();
  *appwindowsize = appwindow_size_t{};
  appwindowsize->type = 2;
  return saveX11WindowPos(glfwGetX11Display(), window, appwindowsize);
}
#endif

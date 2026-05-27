#include "host/plugin/lv2/lv2-x11-embed.hpp"

#if defined(__linux__)

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

bool lv2_x11_embed_surface::create_from_glfw(GLFWwindow* glfw, int w, int h) {
    destroy();
    if (!glfw || w <= 0 || h <= 0) {
        return false;
    }
    Display* dpy = glfwGetX11Display();
    const Window parentWin = glfwGetX11Window(glfw);
    if (!dpy || !parentWin) {
        return false;
    }
    display = dpy;
    parent  = parentWin;
    width   = w;
    height  = h;

    const unsigned long black = BlackPixel(dpy, DefaultScreen(dpy));
    // Use black for both border and background. Plugin UIs almost always use
    // a dark theme; if the plugin briefly hasn't painted yet (e.g. just after
    // remap), a black background looks like "loading" rather than a glaring
    // white flash that masks the actual UI underneath.
    container = XCreateSimpleWindow(dpy, parentWin, 0, 0, static_cast<unsigned>(w), static_cast<unsigned>(h), 0, black, black);
    if (!container) {
        return false;
    }
    XSelectInput(dpy, container, ExposureMask | StructureNotifyMask | VisibilityChangeMask);
    XMapRaised(dpy, container);
    XFlush(dpy);
    return true;
}

void lv2_x11_embed_surface::resize(int w, int h) {
    if (!display || !container || w <= 0 || h <= 0) {
        return;
    }
    width  = w;
    height = h;
    auto* dpy = static_cast<Display*>(display);
    XResizeWindow(dpy, container, static_cast<unsigned>(w), static_cast<unsigned>(h));
    XFlush(dpy);
}

void lv2_x11_embed_surface::set_mapped(bool mapped) {
    if (!display || !container) {
        return;
    }
    // Only map/unmap our own container window — never touch the plugin's window
    // here. Children inherit parent visibility via X11, so they reappear when
    // the container maps again. Explicitly unmapping the plugin (e.g.
    // Vitalium/JUCE ParentUI) makes it tear down rendering and refuse to
    // redraw when reopened (the "black on second open" symptom).
    auto* dpy = static_cast<Display*>(display);
    if (mapped) {
        XMapRaised(dpy, container);
    } else {
        XUnmapWindow(dpy, container);
    }
    XFlush(dpy);
}

void lv2_x11_embed_surface::attach_plugin_window(unsigned long pluginXid) {
    if (!display || !container || !pluginXid) {
        return;
    }
    plugin_window = pluginXid;
    auto* dpy = static_cast<Display*>(display);
    XReparentWindow(dpy, pluginXid, container, 0, 0);
    if (width > 0 && height > 0) {
        XResizeWindow(dpy, pluginXid, static_cast<unsigned>(width), static_cast<unsigned>(height));
    }
    XMapRaised(dpy, pluginXid);
    XFlush(dpy);
}

void lv2_x11_embed_surface::destroy() {
    if (display && container) {
        auto* dpy = static_cast<Display*>(display);
        XDestroyWindow(dpy, container);
        XFlush(dpy);
    }
    display       = nullptr;
    parent        = 0;
    container     = 0;
    plugin_window = 0;
    width         = 0;
    height        = 0;
}

#else

bool lv2_x11_embed_surface::create_from_glfw(GLFWwindow*, int, int) {
    return false;
}
void lv2_x11_embed_surface::resize(int, int) {
}
void lv2_x11_embed_surface::set_mapped(bool) {
}
void lv2_x11_embed_surface::attach_plugin_window(unsigned long) {
}
void lv2_x11_embed_surface::destroy() {
}

#endif

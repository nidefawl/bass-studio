#pragma once

struct GLFWwindow;

/** Child X11 window for embedding LV2/Suil plugin UIs inside a GLFW plugin window. */
struct lv2_x11_embed_surface {
    void* display = nullptr;
    unsigned long parent = 0;
    unsigned long container = 0;
    unsigned long plugin_window = 0;
    int width  = 0;
    int height = 0;

    bool create_from_glfw(GLFWwindow* glfw, int w, int h);
    void resize(int w, int h);
    void destroy();
    /** Map or unmap the embed container and plugin child (use when hiding the host window). */
    void set_mapped(bool mapped);
    /** Reparent a plugin UI X11 window into the embed container (when Suil does not). */
    void attach_plugin_window(unsigned long pluginXid);
    /** Drop cached plugin XID so the next attach always reparents (after host hide/show). */
    void forget_plugin_window() { plugin_window = 0; }
    unsigned long xid() const { return container; }
    unsigned long plugin_xid() const { return plugin_window ? plugin_window : container; }
};

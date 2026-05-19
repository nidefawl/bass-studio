#include "host/plugin/lv2/lv2-ui-host.hpp"

#include "host/host_plugin_window.hpp"
#include "host/plugin/lv2/lv2-catalog.hpp"
#include "host/plugin/lv2/lv2-external-ui.hpp"
#include "host/plugin/lv2/lv2-plugin.hpp"
#include "str_util.hpp"
#include "host/plugin/lv2/lv2-runtime.hpp"
#include "host/plugin/lv2/lv2-x11-embed.hpp"

#include <lilv/lilv.h>
#include <lv2/data-access/data-access.h>
#include <lv2/instance-access/instance-access.h>
#include <lv2/options/options.h>
#include <lv2/ui/ui.h>
#include <lv2/urid/urid.h>

#include <algorithm>
#include <cstring>

#ifdef PROJECT_ENABLE_LV2
#include <suil/suil.h>
#endif

#if defined(__linux__)
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif

namespace {

void external_ui_closed(LV2UI_Controller controller) {
    auto* plugin = static_cast<lv2plugin*>(controller);
    if (plugin) {
        plugin->on_ui_requested_close();
    }
}

void ui_host_touch(LV2UI_Controller controller, uint32_t port_index, bool grabbed) {
    (void)controller;
    (void)port_index;
    (void)grabbed;
}

#if defined(__linux__)
uint32_t ui_port_index(LV2UI_Controller controller, const char* port_symbol) {
    auto* plugin = static_cast<lv2plugin*>(controller);
    return plugin ? plugin->port_index_for_symbol(port_symbol) : LV2UI_INVALID_PORT_INDEX;
}

int ui_host_resize(LV2UI_Feature_Handle handle, int width, int height) {
    auto* plugin = static_cast<lv2plugin*>(handle);
    if (!plugin || width <= 0 || height <= 0) {
        return -1;
    }
    plugin->try_cache_editor_size(width, height);
    // Cardinal / DPF showInterface: resize happens during instantiate, before
    // set_native_ui(). toplevel_ui_mode is set early; no embed surface exists.
    if (plugin->toplevel_ui() || !plugin->embed_surface().xid()) {
        return 0;
    }
    lv2_ui_host::resize_embed(plugin, width, height);
    if (plugin->windowHost) {
        plugin->windowHost->resize(ivec2(width, height));
    }
    return 0;
}

bool x11_window_valid(Display* dpy, Window win) {
    if (!dpy || !win) {
        return false;
    }
    XWindowAttributes attrs{};
    return XGetWindowAttributes(dpy, win, &attrs) != 0;
}

void max_window_geometry(Display* dpy, Window win, int& w, int& h) {
    if (!x11_window_valid(dpy, win)) {
        return;
    }
    XWindowAttributes attrs{};
    if (XGetWindowAttributes(dpy, win, &attrs) && attrs.width > 0 && attrs.height > 0) {
        w = std::max(w, attrs.width);
        h = std::max(h, attrs.height);
    }
    Window root = 0;
    Window parent = 0;
    Window* children = nullptr;
    unsigned nchildren = 0;
    if (XQueryTree(dpy, win, &root, &parent, &children, &nchildren)) {
        for (unsigned i = 0; i < nchildren; ++i) {
            max_window_geometry(dpy, children[i], w, h);
        }
        if (children) {
            XFree(children);
        }
    }
}

unsigned long plugin_ui_xid(const lv2plugin* plugin) {
    if (!plugin) {
        return 0;
    }
    if (plugin->embed_surface().plugin_xid()) {
        return plugin->embed_surface().plugin_xid();
    }
#ifdef PROJECT_ENABLE_LV2
    if (auto* ui = static_cast<SuilInstance*>(plugin->suil_instance_ptr())) {
        if (SuilWidget widget = suil_instance_get_widget(ui)) {
            return reinterpret_cast<unsigned long>(widget);
        }
    }
#endif
    return 0;
}
#endif

} // namespace

namespace lv2_ui_host {

bool ui_has_show_interface(LilvWorld* world, const LilvUI* ui) {
    if (!world || !ui) {
        return false;
    }
    LilvNode* show = lilv_new_uri(world, LV2_UI__showInterface);
    LilvNode* ext  = lilv_new_uri(world, LV2_CORE__extensionData);
    LilvNodes* nodes = lilv_world_find_nodes(world, lilv_ui_get_uri(ui), ext, show);
    const bool ok    = nodes && lilv_nodes_size(nodes) > 0;
    lilv_nodes_free(nodes);
    lilv_node_free(ext);
    lilv_node_free(show);
    return ok;
}

void append_ui_features(lv2plugin* plugin, std::vector<LV2_Feature*>& out, LV2_Feature* parentFeature) {
    if (!plugin) {
        return;
    }
    auto& rt = lv2_runtime::get();
    // CRITICAL: use the plugin's OWN storage. Suil / the plugin UI saves these
    // feature pointers at instantiate time and may dereference them later from
    // a worker / message thread (JUCE in Vitalium/Surge XT). If we shared one
    // static set of structs across all plugins, opening a second LV2 plugin
    // would stomp the first plugin's data_access function and any later call
    // from the first UI thread would dispatch into the WRONG plugin -> crash
    // (the JUCE_ASSERT_MESSAGE_THREAD flood + segfault on second open).
    auto& s = plugin->ui_features();

    LilvInstance* inst = plugin->lilv_instance_ptr();
    s.instanceFeature = { LV2_INSTANCE_ACCESS_URI, inst ? inst->lv2_handle : nullptr };
    s.mapFeature      = { LV2_URID__map, rt.urid_map() };
    s.unmapFeature    = { LV2_URID__unmap, rt.urid_unmap() };
    s.portMap         = LV2UI_Port_Map{ plugin, ui_port_index };
    s.portMapFeature  = { LV2_UI__portMap, &s.portMap };
    s.resizeIface     = LV2UI_Resize{ plugin, ui_host_resize };
    s.resizeFeature   = LV2_Feature{ LV2_UI__resize, &s.resizeIface };
    // Point data_access directly at THIS plugin's DSP extension_data. No
    // global indirection: the function pointer encodes which plugin to query,
    // so cross-plugin contamination is impossible.
    s.dataAccessIface.data_access = nullptr;
    if (inst && inst->lv2_descriptor && inst->lv2_descriptor->extension_data) {
        s.dataAccessIface.data_access = inst->lv2_descriptor->extension_data;
    }
    s.dataAccessFeature = LV2_Feature{ LV2_DATA_ACCESS_URI, &s.dataAccessIface };
    // Advertise that the host will call the plugin's UI idle() callback.
    // JUCE-based LV2 UIs (Vitalium, Surge XT, ...) only run their repaint /
    // event pump when the host promises to drive ui:idleInterface; without
    // it the UI never paints (white screen) and re-instantiation corrupts
    // state on close/reopen.
    s.idleInterfaceFeature = { LV2_UI__idleInterface, nullptr };
    s.touchIface.handle    = static_cast<LV2UI_Controller>(plugin);
    s.touchIface.touch     = ui_host_touch;
    s.touchFeature         = { LV2_UI__touch, &s.touchIface };
    s.externalUiHost.ui_closed         = external_ui_closed;
    s.externalUiHost.plugin_human_id = StringAsCStr(plugin->sName);
    s.externalUiHostFeature            = { LV2_EXTERNAL_UI__Host, &s.externalUiHost };
    s.externalUiHostDeprecatedFeature  = { LV2_EXTERNAL_UI_DEPRECATED_URI, &s.externalUiHost };

    if (parentFeature) {
        out.push_back(parentFeature);
    }
    out.push_back(&s.instanceFeature);
    out.push_back(&s.mapFeature);
    out.push_back(&s.unmapFeature);
    out.push_back(&s.dataAccessFeature);
    lv2_host_instance_params uiParams = plugin->host_instance_params();
    rt.build_instance_options(s.optionsStorage, uiParams, uiParams.transient_window_id != 0);
    // Carla: LV2_UI__windowTitle for showInterface / native UIs without external-ui#Widget.
    if (!s.optionsStorage.empty()) {
        s.optionsStorage.pop_back();
    }
    const char* windowTitle = StringAsCStr(plugin->sName);
    if (windowTitle && windowTitle[0] != '\0') {
        const uint32_t uridTitle  = rt.urid(LV2_UI__windowTitle);
        const uint32_t uridString = rt.urid(LV2_ATOM__String);
        s.optionsStorage.push_back({ LV2_OPTIONS_INSTANCE,
                                     0,
                                     uridTitle,
                                     static_cast<uint32_t>(std::strlen(windowTitle) + 1),
                                     uridString,
                                     windowTitle });
    }
    s.optionsStorage.push_back({});
    s.optionsFeature = LV2_Feature{ LV2_OPTIONS__options, s.optionsStorage.data() };
    out.push_back(&s.optionsFeature);
    out.push_back(&s.portMapFeature);
    out.push_back(&s.resizeFeature);
    out.push_back(&s.idleInterfaceFeature);
    out.push_back(&s.touchFeature);
    out.push_back(&s.externalUiHostFeature);
    out.push_back(&s.externalUiHostDeprecatedFeature);
}

void resize_embed(lv2plugin* plugin, int width, int height) {
    if (!plugin || width <= 0 || height <= 0 || plugin->toplevel_ui()) {
        return;
    }
    plugin->embed_surface().resize(width, height);
#if defined(__linux__)
    if (unsigned long xid = plugin_ui_xid(plugin)) {
        Display* dpy = static_cast<Display*>(plugin->embed_surface().display);
        if (!dpy && plugin->windowHost && plugin->windowHost->getGlfwWindow()) {
            dpy = glfwGetX11Display();
        }
        if (dpy && x11_window_valid(dpy, static_cast<Window>(xid))) {
            XResizeWindow(dpy, static_cast<Window>(xid), static_cast<unsigned>(width), static_cast<unsigned>(height));
            XFlush(dpy);
        }
    }
#endif
}

void fit_host_window(lv2plugin* plugin, host_plugin_window* window) {
#if !defined(__linux__)
    (void)plugin;
    (void)window;
    return;
#else
    if (!plugin || !window || plugin->toplevel_ui()) {
        return;
    }
    const unsigned long xid = plugin_ui_xid(plugin);
    if (!xid) {
        return;
    }
    Display* dpy = nullptr;
    if (plugin->embed_surface().display) {
        dpy = static_cast<Display*>(plugin->embed_surface().display);
    } else if (window->getGlfwWindow()) {
        dpy = glfwGetX11Display();
    }
    if (!dpy || !x11_window_valid(dpy, static_cast<Window>(xid))) {
        return;
    }
    int w = 0;
    int h = 0;
    max_window_geometry(dpy, static_cast<Window>(xid), w, h);
    if (w <= 0 || h <= 0) {
        return;
    }
    if (!plugin->try_cache_editor_size(w, h)) {
        return;
    }
    resize_embed(plugin, w, h);
    window->resize(ivec2(w, h));
#endif
}

void schedule_ui_fit(lv2plugin* plugin, int frames) {
    if (!plugin || frames <= 0) {
        return;
    }
    plugin->set_ui_fit_frames(frames);
}

ivec2 preferred_ui_size(const LilvUI* ui) {
    if (!ui) {
        return { 0, 0 };
    }
    LilvWorld* world = lv2_catalog::process_world();
    if (!world) {
        return { 0, 0 };
    }
    const LilvNode* subject = lilv_ui_get_uri(ui);
    LilvNode* widthPred   = lilv_new_uri(world, "http://lv2plug.in/ns/extensions/ui#width");
    LilvNode* heightPred  = lilv_new_uri(world, "http://lv2plug.in/ns/extensions/ui#height");
    LilvNode* widthNode   = lilv_world_get(world, subject, widthPred, nullptr);
    LilvNode* heightNode  = lilv_world_get(world, subject, heightPred, nullptr);
    ivec2 size{ 0, 0 };
    if (widthNode && lilv_node_is_int(widthNode)) {
        size.x = lilv_node_as_int(widthNode);
    }
    if (heightNode && lilv_node_is_int(heightNode)) {
        size.y = lilv_node_as_int(heightNode);
    }
    lilv_node_free(widthNode);
    lilv_node_free(heightNode);
    lilv_node_free(widthPred);
    lilv_node_free(heightPred);
    return size;
}

bool ui_allows_user_resize(const LilvUI* ui) {
    if (!ui) {
        return true;
    }
    LilvWorld* world = lv2_catalog::process_world();
    if (!world) {
        return true;
    }
    const LilvNode* subject = lilv_ui_get_uri(ui);
    LilvNode* opt           = lilv_new_uri(world, LV2_CORE__optionalFeature);
    LilvNode* fixed         = lilv_new_uri(world, LV2_UI__fixedSize);
    LilvNode* noResize      = lilv_new_uri(world, LV2_UI__noUserResize);
    LilvNodes* fixedMatch   = lilv_world_find_nodes(world, subject, opt, fixed);
    LilvNodes* noResizeMatch = lilv_world_find_nodes(world, subject, opt, noResize);
    const bool allowed = !fixedMatch && !noResizeMatch;
    lilv_nodes_free(noResizeMatch);
    lilv_nodes_free(fixedMatch);
    lilv_node_free(noResize);
    lilv_node_free(fixed);
    lilv_node_free(opt);
    return allowed;
}

ivec2 default_editor_size(const lv2plugin* plugin) {
    if (!plugin) {
        return { 960, 640 };
    }
    if (plugin->getModuleCategory() == 1) {
        return { 1280, 720 };
    }
    return { 900, 600 };
}

#if defined(__linux__)
bool x11_window_is_descendant(Display* dpy, Window ancestor, Window w) {
    if (!dpy || !ancestor || !w) {
        return false;
    }
    if (ancestor == w) {
        return true;
    }
    Window root = 0;
    Window parent = 0;
    Window cur    = w;
    while (cur != None && cur != root) {
        if (!XQueryTree(dpy, cur, &root, &parent, nullptr, nullptr)) {
            return false;
        }
        if (parent == ancestor) {
            return true;
        }
        cur = parent;
    }
    return false;
}

void iconify_stray_plugin_toplevels(lv2plugin* plugin, host_plugin_window* window) {
    if (!plugin || !window || plugin->toplevel_ui()) {
        return;
    }
    Display* dpy = static_cast<Display*>(plugin->embed_surface().display);
    if (!dpy && window->getGlfwWindow()) {
        dpy = glfwGetX11Display();
    }
    if (!dpy) {
        return;
    }
    const Window hostXid     = window->getGlfwWindow() ? glfwGetX11Window(window->getGlfwWindow()) : 0;
    const Window container   = plugin->embed_surface().xid();
    const Window pluginWin   = plugin->embed_surface().plugin_xid();
    XClassHint ref{};
    if (!pluginWin || !x11_window_valid(dpy, pluginWin) || !XGetClassHint(dpy, pluginWin, &ref)) {
        return;
    }

    Window root = DefaultRootWindow(dpy);
    Window parent_return = 0;
    Window* children     = nullptr;
    unsigned nchildren   = 0;
    if (XQueryTree(dpy, root, &root, &parent_return, &children, &nchildren)) {
        for (unsigned i = 0; i < nchildren; ++i) {
            const Window w = children[i];
            if (w == container || w == pluginWin || w == hostXid) {
                continue;
            }
            if (container && x11_window_is_descendant(dpy, container, w)) {
                continue;
            }
            XClassHint wh{};
            if (x11_window_valid(dpy, w) && XGetClassHint(dpy, w, &wh) && ref.res_class && wh.res_class
                && std::strcmp(ref.res_class, wh.res_class) == 0) {
                XIconifyWindow(dpy, w, DefaultScreen(dpy));
            }
            if (wh.res_name) {
                XFree(wh.res_name);
            }
            if (wh.res_class) {
                XFree(wh.res_class);
            }
        }
        XFree(children);
    }
    if (ref.res_name) {
        XFree(ref.res_name);
    }
    if (ref.res_class) {
        XFree(ref.res_class);
    }
    XFlush(dpy);
}

void apply_x11_embed_hints(lv2plugin* plugin, host_plugin_window* window) {
    if (!plugin || !window || plugin->toplevel_ui()) {
        return;
    }
    Display* dpy = static_cast<Display*>(plugin->embed_surface().display);
    if (!dpy && window->getGlfwWindow()) {
        dpy = glfwGetX11Display();
    }
    if (!dpy) {
        return;
    }
    const Window hostXid   = window->getGlfwWindow() ? glfwGetX11Window(window->getGlfwWindow()) : 0;
    const Window pluginWin = plugin->embed_surface().plugin_xid();
    if (hostXid && pluginWin && x11_window_valid(dpy, pluginWin) && x11_window_valid(dpy, hostXid)) {
        XSetTransientForHint(dpy, pluginWin, hostXid);
        XFlush(dpy);
    }
    dismiss_stray_plugin_toplevels(plugin, window);
}

void dismiss_stray_plugin_toplevels(lv2plugin* plugin, host_plugin_window* window) {
    iconify_stray_plugin_toplevels(plugin, window);
}
#endif

} // namespace lv2_ui_host

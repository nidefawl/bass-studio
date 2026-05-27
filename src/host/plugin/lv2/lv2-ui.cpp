#include "host/plugin/lv2/lv2-ui.hpp"

#include "host/host_plugin_window.hpp"
#include "host/plugin/lv2/lv2-carla-ui.hpp"
#include "host/plugin/lv2/lv2-catalog.hpp"
#include "host/plugin/lv2/lv2-native-x11-ui.hpp"
#include "host/plugin/lv2/lv2-plugin.hpp"
#include "host/plugin/lv2/lv2-runtime.hpp"
#include "host/plugin/lv2/lv2-ui-host.hpp"
#include "host/plugin/lv2/lv2-x11-embed.hpp"
#include "logging.hpp"
#if defined(__linux__)
#include "platform/linux/x11_util.hpp"
#endif

#include <lilv/lilv.h>
#include <lv2/core/lv2.h>
#include <lv2/instance-access/instance-access.h>
#include <lv2/ui/ui.h>
#include <lv2/urid/urid.h>
#include <atomic>
#include <cstring>
#include <vector>

#ifdef PROJECT_ENABLE_LV2
#include <suil/suil.h>
#endif

#if defined(__linux__)
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#include <X11/Xlib.h>
#include <gtk/gtk.h>
#endif

namespace {

std::atomic<bool> g_host_shutting_down{ false };

#if defined(__linux__)
static void wake_x11_plugin_ui(Display* dpy, Window pluginWin, int w, int h) {
    if (!dpy || !pluginWin || w <= 0 || h <= 0) {
        return;
    }
    XSync(dpy, False);
    const unsigned tw = static_cast<unsigned>(w);
    const unsigned th = static_cast<unsigned>(h);
    if (tw > 1) {
        XResizeWindow(dpy, pluginWin, tw - 1, th);
        XSync(dpy, False);
    }
    XResizeWindow(dpy, pluginWin, tw, th);
    XExposeEvent ee{};
    ee.type       = Expose;
    ee.send_event = True;
    ee.display    = dpy;
    ee.window     = pluginWin;
    ee.x          = 0;
    ee.y          = 0;
    ee.width      = static_cast<int>(tw);
    ee.height     = static_cast<int>(th);
    ee.count      = 0;
    XSendEvent(dpy, pluginWin, False, ExposureMask, reinterpret_cast<XEvent*>(&ee));
    XClearArea(dpy, pluginWin, 0, 0, 0, 0, True);
    XFlush(dpy);
}

void refresh_editor_after_show_impl(lv2plugin* plugin, host_plugin_window* window) {
    if (!plugin || !window || plugin->toplevel_ui()) {
        return;
    }
    GLFWwindow* glfw = window->getGlfwWindow();
    if (!glfw) {
        return;
    }

    ivec2 size = window->getContentSize();
    if (size.x < 64 || size.y < 64) {
        size = lv2_ui_host::default_editor_size(plugin);
    }

    lv2_x11_embed_surface& embed = plugin->embed_surface();
    if (!embed.xid()) {
        embed.create_from_glfw(glfw, size.x, size.y);
    } else {
        embed.resize(size.x, size.y);
    }
    embed.set_mapped(true);

#ifdef PROJECT_ENABLE_LV2
    if (auto* ui = static_cast<SuilInstance*>(plugin->suil_instance_ptr())) {
        if (SuilWidget widget = suil_instance_get_widget(ui)) {
            if (plugin->suil_gtk_bridge()) {
                gtk_widget_show_all(GTK_WIDGET(widget));
                for (int i = 0; i < 8 && gtk_events_pending(); ++i) {
                    gtk_main_iteration_do(false);
                }
            } else {
                const unsigned long pluginXid = reinterpret_cast<unsigned long>(widget);
                Display* dpy = static_cast<Display*>(embed.display);
                if (!dpy) {
                    dpy = glfwGetX11Display();
                }
                if (dpy && pluginXid) {
                    embed.forget_plugin_window();
                    embed.attach_plugin_window(pluginXid);
                    wake_x11_plugin_ui(dpy, static_cast<Window>(pluginXid), size.x, size.y);
                }
            }
        }
    }
#endif

    sendExposeEvent(glfw);
}
#endif

#ifdef PROJECT_ENABLE_LV2
// These must match the LV2_UI__* URIs exactly (with "UI" suffix) so that
// suil_ui_supported() finds the right Suil module.
constexpr const char* kHostContainerX11  = LV2_UI__X11UI;
constexpr const char* kHostContainerGtk  = LV2_UI__GtkUI;
constexpr const char* kHostContainerGtk3 = LV2_UI__Gtk3UI;
constexpr const char* kHostContainerQt5  = LV2_UI__Qt5UI;
constexpr const char* kExternalUiWidget  = "http://kxstudio.sf.net/ns/lv2ext/external-ui#Widget";

void suil_port_write(SuilController controller, uint32_t port_index, uint32_t buffer_size, uint32_t protocol, void const* buffer) {
    auto* plugin = static_cast<lv2plugin*>(controller);
    if (!plugin || !buffer) {
        return;
    }
    if (protocol == 0 && buffer_size == sizeof(float)) {
        plugin->apply_ui_control(port_index, *static_cast<const float*>(buffer));
    }
}

uint32_t suil_port_index(SuilController controller, const char* port_symbol) {
    auto* plugin = static_cast<lv2plugin*>(controller);
    return plugin ? plugin->port_index_for_symbol(port_symbol) : UINT32_MAX;
}

struct ui_match_t {
    const LilvUI* ui = nullptr;
    const LilvNode* uiType = nullptr;
    const char* containerUri = nullptr;
    unsigned quality = 0;
    bool nativeX11 = false;
};

bool ui_is_external_widget(LilvWorld* world, const LilvUI* ui) {
    LilvNode* external = lilv_new_uri(world, kExternalUiWidget);
    const bool isExternal = lilv_ui_is_a(ui, external);
    lilv_node_free(external);
    return isExternal;
}

bool find_suil_ui(LilvWorld* world,
                  const LilvUIs* uis,
                  const LilvNode* container,
                  const char* containerUri,
                  const LilvNode* pluginUiType,
                  ui_match_t& best) {
    if (!container) {
        return false;
    }
    LILV_FOREACH(uis, i, uis) {
        const LilvUI* ui = lilv_uis_get(uis, i);
        if (ui_is_external_widget(world, ui)) {
            continue;
        }
        if (pluginUiType && !lilv_ui_is_a(ui, pluginUiType)) {
            continue;
        }
        const LilvNode* uiType = nullptr;
        const unsigned q = lilv_ui_is_supported(ui, suil_ui_supported, container, &uiType);
        if (q > best.quality && uiType) {
            best.quality      = q;
            best.ui           = ui;
            best.uiType       = uiType;
            best.containerUri = containerUri;
            best.nativeX11    = false;
        }
    }
    return best.ui != nullptr && best.uiType != nullptr;
}

/** Same-binary X11 UI with ui:showInterface only (Cardinal / DPF toplevel windows).
 *  Vitalium / Surge also ship lv2ui_descriptor in the DSP .so but MUST use Suil —
 *  native dlopen/instantiate runs JUCE off its MessageThread and crashes. */
const LilvUI* find_native_showinterface_ui(LilvWorld* world, const LilvPlugin* plugin, const LilvUIs* uis) {
    if (!world || !plugin || !uis) {
        return nullptr;
    }
    LilvNode* x11Ui = lilv_new_uri(world, LV2_UI__X11UI);
    const LilvNode* pluginBin = lilv_plugin_get_library_uri(plugin);
    LILV_FOREACH(uis, i, uis) {
        const LilvUI* ui = lilv_uis_get(uis, i);
        if (ui_is_external_widget(world, ui)) {
            continue;
        }
        if (!lilv_ui_is_a(ui, x11Ui)) {
            continue;
        }
        if (!lv2_ui_host::ui_has_show_interface(world, ui)) {
            continue;
        }
        const LilvNode* uiBin = lilv_ui_get_binary_uri(ui);
        if (pluginBin && uiBin && lilv_node_equals(pluginBin, uiBin)) {
            lilv_node_free(x11Ui);
            return ui;
        }
    }
    lilv_node_free(x11Ui);
    return nullptr;
}

bool find_best_suil_ui(LilvWorld* world, const LilvUIs* uis, ui_match_t& best) {
    // Prefer LV2_UI__X11UI + libsuil_x11 for JUCE ParentUIs (Vitalium, Surge XT).
    // Gtk3/Qt5 bridges need a toolkit parent, not a raw X11 XID from GLFW.

    // We are an X11 host (GLFW under Linux), so the LV2_UI__parent we hand
    // to Suil is a raw X11 Window XID. The pure X11 passthrough bridge
    // (libsuil_x11.so) embeds the plugin directly via that XID and renders
    // correctly; the Gtk3 / Qt5 bridges expect a toolkit container pointer
    // as parent, so they receive bogus data from us and end up showing a
    // blank surface. Prefer X11 first, then fall back to Gtk3 / Qt5 only
    // for plugins whose UI type isn't X11.
    LilvNode* x11Ui = lilv_new_uri(world, LV2_UI__X11UI);
    LilvNode* x11Container = lilv_new_uri(world, kHostContainerX11);

    // Pass 1: prefer native X11 plugin UI directly into the X11 host.
    LILV_FOREACH(uis, i, uis) {
        const LilvUI* ui = lilv_uis_get(uis, i);
        if (ui_is_external_widget(world, ui)) {
            continue;
        }
        if (!lilv_ui_is_a(ui, x11Ui)) {
            continue;
        }
        const LilvNode* uiType = nullptr;
        const unsigned q = lilv_ui_is_supported(ui, suil_ui_supported, x11Container, &uiType);
        if (q > 0 && uiType) {
            best.quality      = q;
            best.ui           = ui;
            best.uiType       = uiType;
            best.containerUri = kHostContainerX11;
            best.nativeX11    = false;
            break;
        }
    }

    lilv_node_free(x11Container);
    lilv_node_free(x11Ui);

    if (best.ui) {
        return true;
    }

    // Fallback hosts: any UI Suil can bridge into our X11/Gtk3/Qt5 containers.
    static const char* containers[] = { kHostContainerX11, kHostContainerGtk3, kHostContainerQt5, nullptr };
    for (const char* containerUri : containers) {
        LilvNode* container = lilv_new_uri(world, containerUri);
        if (find_suil_ui(world, uis, container, containerUri, nullptr, best)) {
            lilv_node_free(container);
            return true;
        }
        lilv_node_free(container);
    }
    return best.ui != nullptr && best.uiType != nullptr;
}
#endif

int hex_digit(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return 10 + (c - 'a');
    }
    if (c >= 'A' && c <= 'F') {
        return 10 + (c - 'A');
    }
    return -1;
}

String percent_decode_path(String path) {
    String out;
    out.reserve(path.size());
    for (size_t i = 0; i < path.size(); ++i) {
        if (path[i] == '%' && i + 2 < path.size()) {
            const int hi = hex_digit(path[i + 1]);
            const int lo = hex_digit(path[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out += static_cast<char>((hi << 4) | lo);
                i += 2;
                continue;
            }
        }
        out += path[i];
    }
    return out;
}

String file_uri_to_local_path(const char* uri) {
    if (!uri) {
        return {};
    }
    String path = uri;
    if (path.find("file://") == 0) {
        path = path.substr(7);
    }
    return percent_decode_path(path);
}

String resolve_lv2_binary_path(const char* bundle_uri, const char* binary_uri) {
    String bundle = file_uri_to_local_path(bundle_uri);
    String binary = file_uri_to_local_path(binary_uri);
    if (binary.empty()) {
        return binary;
    }
    if (binary[0] != '/') {
        if (!bundle.empty() && bundle.back() != '/') {
            bundle += '/';
        }
        binary = bundle + binary;
    }
    return binary;
}

} // namespace

namespace lv2_ui {

void set_editor_visible(lv2plugin* plugin, bool visible);

#ifdef PROJECT_ENABLE_LV2
static const LV2UI_Show_Interface* suil_show_interface(SuilInstance* ui) {
    if (!ui) {
        return nullptr;
    }
    return static_cast<const LV2UI_Show_Interface*>(suil_instance_extension_data(ui, LV2_UI__showInterface));
}

static void suil_call_show_interface(lv2plugin* plugin, bool visible) {
    auto* ui = static_cast<SuilInstance*>(plugin->suil_instance_ptr());
    if (!ui) {
        return;
    }
    const LV2UI_Show_Interface* show = suil_show_interface(ui);
    LV2UI_Handle handle              = suil_instance_get_handle(ui);
    if (!show || !handle) {
        return;
    }
    if (visible) {
        if (show->show) {
            show->show(handle);
        }
    } else if (show->hide) {
        show->hide(handle);
    }
}

static bool suil_drive_idle(lv2plugin* plugin) {
    auto* ui = static_cast<SuilInstance*>(plugin->suil_instance_ptr());
    if (!ui) {
        return false;
    }
    const auto* idleIface = static_cast<const LV2UI_Idle_Interface*>(
        suil_instance_extension_data(ui, LV2_UI__idleInterface));
    if (!idleIface || !idleIface->idle) {
        return false;
    }
    return idleIface->idle(suil_instance_get_handle(ui)) != 0;
}
#endif

bool open_editor(lv2plugin* plugin, host_plugin_window* window, String& errorOut) {
#ifdef PROJECT_ENABLE_LV2
    if (!plugin || !plugin->lilv_instance_ptr()) {
        errorOut = "LV2 instance not ready";
        return false;
    }
    if (!window) {
        errorOut = "No host window";
        return false;
    }

    lv2_runtime::ensure_suil_initialized();
    const LilvPlugin* desc = plugin->lilv_descriptor_ptr();
    LilvWorld* world       = lv2_catalog::process_world();
    const LilvUIs* uis     = lilv_plugin_get_uis(desc);
    if (!uis || lilv_uis_size(uis) == 0) {
        errorOut = "Plugin has no UI";
        return false;
    }

    ui_match_t match;
    const char* pluginUri = lilv_node_as_uri(lilv_plugin_get_uri(desc));

    if (lv2_native_x11_ui::is_open(plugin)) {
        set_editor_visible(plugin, true);
        if (lv2_carla_ui::is_carla_plugin(plugin) && plugin->has_dsp_worker()) {
            lv2_carla_ui::notify_show(plugin);
        }
        if (!plugin->toplevel_ui()) {
            lv2_ui_host::fit_host_window(plugin, window);
            lv2_ui_host::schedule_ui_fit(plugin, 5);
        }
        return true;
    }

#ifdef PROJECT_ENABLE_LV2
    // Vitalium / Surge XT / JUCE: suil_instance_free on window close crashes on reopen.
    // Keep the SuilInstance; only recreate the X11 embed container when the GLFW window returns.
    if (plugin->suil_instance_ptr()) {
        plugin->windowHost = window;
#if defined(__linux__)
        GLFWwindow* glfw = window->getGlfwWindow();
        if (!glfw) {
            errorOut = "No GLFW window for LV2 UI embed";
            return false;
        }
        if (GLFWwindow* glfwParent = window->getGlfwWindow()) {
            plugin->set_transient_window_id(static_cast<int64_t>(glfwGetX11Window(glfwParent)));
        }
        suil_call_show_interface(plugin, true);
        refresh_editor_after_show(plugin, window);
        lv2_ui_host::fit_host_window(plugin, window);
        lv2_ui_host::schedule_ui_fit(plugin, 12);
        if (lv2_carla_ui::is_carla_plugin(plugin) && plugin->has_dsp_worker()) {
            lv2_carla_ui::notify_show(plugin);
        }
#endif
        return true;
    }
#endif

    // DPF / Cardinal: showInterface + same .so — Suil cannot host these; use native UI.
    if (const LilvUI* nativeUi = find_native_showinterface_ui(world, desc, uis)) {
        plugin->set_suil_gtk_bridge(false);
        if (lv2_native_x11_ui::open(plugin, window, nativeUi, errorOut)) {
            set_editor_visible(plugin, true);
            return true;
        }
        log_lf(Log::L_WARN, "LV2 native showInterface UI failed for '%s': %s\n",
               pluginUri ? pluginUri : "?",
               StringAsCStr(errorOut));
        return false;
    }

    if (!find_best_suil_ui(world, uis, match)) {
        errorOut = "No supported LV2 UI type for this host (X11/Gtk3/Qt5)";
        return false;
    }

    // JUCE LV2 UIs (Vitalium) must never keep a native dlopen from a mistaken path.
    lv2_native_x11_ui::close(plugin);

    plugin->set_suil_gtk_bridge(match.containerUri && std::strcmp(match.containerUri, kHostContainerX11) != 0);

    ivec2 size = lv2_ui_host::default_editor_size(plugin);
    const ivec2 pref = lv2_ui_host::preferred_ui_size(match.ui);
    if (pref.x >= 64 && pref.y >= 64) {
        size = pref;
    }
    // Use user's last manually-set size if available (within session or restored from snapshot)
    const ivec2 saved = plugin->saved_editor_size();
    if (saved.x >= 64 && saved.y >= 64) {
        size = saved;
    } else if (plugin->bWindowPosSizeValid && plugin->lastWindowPosSize.z > 64 && plugin->lastWindowPosSize.w > 64) {
        // Restored from saved snapshot
        size = { plugin->lastWindowPosSize.z, plugin->lastWindowPosSize.w };
    }
    plugin->bSupportsWindowResize = lv2_ui_host::ui_allows_user_resize(match.ui);

#if defined(__linux__)
    GLFWwindow* glfw = window->getGlfwWindow();
    if (!glfw) {
        errorOut = "No GLFW window for LV2 UI embed";
        return false;
    }
    if (!plugin->embed_surface().create_from_glfw(glfw, size.x, size.y)) {
        errorOut = "Failed to create X11 embed surface";
        return false;
    }
    const unsigned long parentXid = plugin->embed_surface().xid();
    if (GLFWwindow* glfwParent = window->getGlfwWindow()) {
        plugin->set_transient_window_id(static_cast<int64_t>(glfwGetX11Window(glfwParent)));
    } else {
        plugin->set_transient_window_id(static_cast<int64_t>(parentXid));
    }
#else
    const unsigned long parentXid = static_cast<unsigned long>(window->getWindowHandle());
#endif

    SuilHost* host = suil_host_new(suil_port_write, suil_port_index, nullptr, nullptr);
    if (!host) {
        errorOut = "Failed to create Suil host";
#if defined(__linux__)
        plugin->embed_surface().destroy();
#endif
        return false;
    }

    plugin->windowHost = window;

    LV2_Feature parentFeature{ LV2_UI__parent, reinterpret_cast<void*>(parentXid) };
    std::vector<LV2_Feature*> features;
    lv2_ui_host::append_ui_features(plugin, features, &parentFeature);
    features.push_back(nullptr);

    const char* pluginUri2  = pluginUri;
    const char* uiUri       = lilv_node_as_uri(lilv_ui_get_uri(match.ui));
    const char* uiTypeUri   = lilv_node_as_uri(match.uiType);
    const char* bundleUri = lilv_node_as_uri(lilv_ui_get_bundle_uri(match.ui));
    const char* binaryUri = lilv_node_as_uri(lilv_ui_get_binary_uri(match.ui));
    const String bundlePath = file_uri_to_local_path(bundleUri);
    const String binaryPath = resolve_lv2_binary_path(bundleUri, binaryUri);

    SuilInstance* ui = suil_instance_new(
        host,
        plugin,
        match.containerUri,
        pluginUri2,
        uiUri,
        uiTypeUri,
        StringAsCStr(bundlePath),
        StringAsCStr(binaryPath),
        features.data());
    if (!ui) {
        suil_host_free(host);
        plugin->embed_surface().destroy();
        // Do not fall back to raw native X11 here. JUCE LV2 UIs (Vitalium,
        // Surge XT, etc.) assert/crash if their UI is instantiated outside the
        // Suil path. A failed Suil open must be a non-fatal editor-open error,
        // not a process crash.
        errorOut = StringFormat("Suil could not instantiate UI (%s, container %s)",
                                uiTypeUri ? uiTypeUri : "?",
                                match.containerUri ? match.containerUri : "?");
        return false;
    }

    plugin->set_editor(static_cast<void*>(host), static_cast<void*>(ui));
    log_lf(Log::L_INFO, "LV2 UI open(suil): plugin=%s ui=%s type=%s container=%s\n",
           pluginUri2 ? pluginUri2 : "?",
           uiUri ? uiUri : "?",
           uiTypeUri ? uiTypeUri : "?",
           match.containerUri ? match.containerUri : "?");

#if defined(__linux__)
    if (SuilWidget widget = suil_instance_get_widget(ui)) {
        // For Suil's Gtk3 / Qt5 bridges the returned handle is a toolkit
        // widget (not an X11 XID): the bridge has already reparented the
        // plugin's X11 window into the LV2_UI__parent we supplied, but the
        // host still has to map the container so the widget hierarchy
        // becomes visible. Without gtk_widget_show_all() Vitalium / JUCE
        // UIs draw nothing and the host window stays white.
        if (match.containerUri && std::strcmp(match.containerUri, kHostContainerX11) == 0) {
            const unsigned long pluginXid = reinterpret_cast<unsigned long>(widget);
            Display* dpy = static_cast<Display*>(plugin->embed_surface().display);
            if (!dpy && window->getGlfwWindow()) {
                dpy = glfwGetX11Display();
            }
            if (dpy && pluginXid) {
                const Window pw = static_cast<Window>(pluginXid);
                XWindowAttributes attrs{};
                if (XGetWindowAttributes(dpy, pw, &attrs) != 0) {
                    plugin->embed_surface().attach_plugin_window(pluginXid);
                }
            }
        } else {
            gtk_widget_show_all(GTK_WIDGET(widget));
        }
    }
    // Do not pump GTK events here. Suil/GTK has just instantiated the plugin
    // UI; dispatching queued events synchronously can re-enter the plugin
    // before it is fully realized (Vitalium/JUCE crashes in g_main_dispatch).
    // The host's regular idle (lv2_ui::idle) pumps events safely after open.
    lv2_ui_host::fit_host_window(plugin, window);
    lv2_ui_host::schedule_ui_fit(plugin, 5);
    set_editor_visible(plugin, true);
    if (lv2_carla_ui::is_carla_plugin(plugin) && plugin->has_dsp_worker()) {
        lv2_carla_ui::notify_show(plugin);
    }
#endif
    return true;
#else
    (void)plugin;
    (void)window;
    errorOut = "LV2 UI support was not built";
    return false;
#endif
}

void refresh_editor_after_show(lv2plugin* plugin, host_plugin_window* window) {
#if defined(__linux__)
    refresh_editor_after_show_impl(plugin, window);
#else
    (void)plugin;
    (void)window;
#endif
}

void set_editor_visible(lv2plugin* plugin, bool visible) {
    if (!plugin) {
        return;
    }
#if defined(__linux__)
    if (plugin->toplevel_ui()) {
        if (const LV2UI_Show_Interface* show = plugin->native_show_interface()) {
            if (LV2UI_Handle handle = plugin->native_ui_handle()) {
                if (visible && show->show) {
                    show->show(handle);
                } else if (!visible && show->hide) {
                    show->hide(handle);
                }
            }
        }
        return;
    }

    host_plugin_window* window = plugin->windowHost;
    GLFWwindow* glfw           = window ? window->getGlfwWindow() : nullptr;

    if (!visible) {
#ifdef PROJECT_ENABLE_LV2
        if (plugin->suil_gtk_bridge()) {
            if (auto* ui = static_cast<SuilInstance*>(plugin->suil_instance_ptr())) {
                if (SuilWidget widget = suil_instance_get_widget(ui)) {
                    gtk_widget_hide(GTK_WIDGET(widget));
                }
            }
        } else if (plugin->suil_instance_ptr()) {
            // Unmap only — destroying the container also destroys the plugin's
            // child X11 window (Vitalium), which causes a black window on reopen.
            plugin->embed_surface().set_mapped(false);
        }
#endif
        if (plugin->windowHost && !plugin->suil_gtk_bridge()) {
            lv2_ui_host::dismiss_stray_plugin_toplevels(plugin, plugin->windowHost);
        }
        return;
    }

    if (glfw && !plugin->embed_surface().xid()) {
        ivec2 size = window->getContentSize();
        if (size.x < 64 || size.y < 64) {
            size = lv2_ui_host::default_editor_size(plugin);
        }
        plugin->embed_surface().create_from_glfw(glfw, size.x, size.y);
    }

#ifdef PROJECT_ENABLE_LV2
    suil_call_show_interface(plugin, true);
    if (auto* ui = static_cast<SuilInstance*>(plugin->suil_instance_ptr())) {
        if (SuilWidget widget = suil_instance_get_widget(ui)) {
            if (plugin->suil_gtk_bridge()) {
                gtk_widget_show_all(GTK_WIDGET(widget));
            } else {
                Display* dpy = static_cast<Display*>(plugin->embed_surface().display);
                if (!dpy && glfw) {
                    dpy = glfwGetX11Display();
                }
                const unsigned long pluginXid = reinterpret_cast<unsigned long>(widget);
                if (dpy && pluginXid) {
                    lv2_x11_embed_surface& embed = plugin->embed_surface();
                    embed.forget_plugin_window();
                    embed.attach_plugin_window(pluginXid);
                    int w = embed.width;
                    int h = embed.height;
                    if (window) {
                        ivec2 cs = window->getContentSize();
                        if (cs.x > 0 && cs.y > 0) {
                            w = cs.x;
                            h = cs.y;
                        }
                    }
                    wake_x11_plugin_ui(dpy, static_cast<Window>(pluginXid), w, h);
                }
            }
        }
    }
#endif

    if (glfw) {
        sendExposeEvent(glfw);
        lv2_ui_host::schedule_ui_fit(plugin, 8);
    }
    if (plugin->windowHost && !plugin->suil_gtk_bridge()) {
        lv2_ui_host::apply_x11_embed_hints(plugin, plugin->windowHost);
    }
#else
    (void)visible;
#endif
}

void hide_editor(lv2plugin* plugin) {
    if (!plugin) {
        return;
    }
    if (!plugin->suil_instance_ptr() && !lv2_native_x11_ui::is_open(plugin)) {
        return;
    }
    if (lv2_carla_ui::is_carla_plugin(plugin) && plugin->has_dsp_worker()) {
        lv2_carla_ui::notify_hide(plugin);
    }
    if (lv2_native_x11_ui::is_open(plugin)) {
        lv2_native_x11_ui::hide(plugin);
        return;
    }
#if defined(__linux__)
    if (plugin->suil_instance_ptr()) {
        if (plugin->suil_gtk_bridge()) {
            if (auto* ui = static_cast<SuilInstance*>(plugin->suil_instance_ptr())) {
                if (SuilWidget widget = suil_instance_get_widget(ui)) {
                    gtk_widget_hide(GTK_WIDGET(widget));
                }
            }
            for (int i = 0; i < 8 && gtk_events_pending(); ++i) {
                gtk_main_iteration_do(false);
            }
        } else {
            plugin->embed_surface().set_mapped(false);
        }
    }
#endif
}

void close_editor(lv2plugin* plugin) {
    if (!plugin) {
        return;
    }
    if (!plugin->suil_instance_ptr() && !lv2_native_x11_ui::is_open(plugin)) {
        return;
    }
    hide_editor(plugin);
    lv2_native_x11_ui::close(plugin);
    if (g_host_shutting_down.load()) {
        // Process exit / project unload: suil_instance_free on JUCE UIs (Vitalium)
        // can deadlock while other plugins (e.g. Cardinal) tear down GTK. Drop
        // Suil handles; the OS reclaims the process.
        plugin->set_editor(nullptr, nullptr);
        plugin->embed_surface().destroy();
        return;
    }
#if defined(__linux__)
    if (plugin->suil_gtk_bridge()) {
        for (int i = 0; i < 32 && gtk_events_pending(); ++i) {
            gtk_main_iteration_do(false);
        }
    }
#endif
#ifdef PROJECT_ENABLE_LV2
    if (auto* ui = static_cast<SuilInstance*>(plugin->suil_instance_ptr())) {
        suil_instance_free(ui);
    }
    if (auto* host = static_cast<SuilHost*>(plugin->suil_host_ptr())) {
        suil_host_free(host);
    }
#endif
    plugin->set_editor(nullptr, nullptr);
    plugin->embed_surface().destroy();
}

void resize_editor(lv2plugin* plugin, int width, int height) {
    if (!plugin || plugin->toplevel_ui()) {
        return;
    }
    lv2_ui_host::resize_embed(plugin, width, height);
}

void notify_control(lv2plugin* plugin, uint32_t lilvPortIndex, float pluginValue) {
    if (!plugin) {
        return;
    }
    if (lv2_native_x11_ui::is_open(plugin)) {
        lv2_native_x11_ui::notify_control(plugin, lilvPortIndex, pluginValue);
        return;
    }
#ifdef PROJECT_ENABLE_LV2
    auto* ui = static_cast<SuilInstance*>(plugin->suil_instance_ptr());
    if (!ui) {
        return;
    }
    suil_instance_port_event(ui, lilvPortIndex, sizeof(float), 0, &pluginValue);
#else
    (void)lilvPortIndex;
    (void)pluginValue;
#endif
}

void idle(lv2plugin* plugin) {
    if (!plugin) {
        return;
    }
    if (lv2_carla_ui::is_carla_plugin(plugin) && plugin->has_dsp_worker()) {
        lv2_carla_ui::notify_idle(plugin);
        return;
    }
    if (lv2_native_x11_ui::is_open(plugin)) {
        lv2_native_x11_ui::idle(plugin);
        return;
    }
#if defined(__linux__)
    // Gtk3/Qt5 Suil bridges run JUCE on the GLib thread — only pump GTK here.
    // Calling ui:idleInterface from the host thread triggers JUCE MessageManager
    // assertions (juce_MessageManager.cpp:310) and crashes Vitalium / Surge.
    if (plugin->suil_gtk_bridge()) {
        if (gtk_events_pending()) {
            gtk_main_iteration_do(false);
        }
        return;
    }
#endif
#ifdef PROJECT_ENABLE_LV2
    // X11 Suil (Vitalium ParentUI, OsTIrus, etc.): drive ui:idleInterface on the host UI thread.
    if (plugin->suil_instance_ptr() && suil_drive_idle(plugin)) {
        plugin->on_ui_requested_close();
    }
#endif
}

void set_host_shutting_down(bool shutting_down) {
    g_host_shutting_down.store(shutting_down);
}

} // namespace lv2_ui

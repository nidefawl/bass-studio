#include "host/plugin/lv2/lv2-native-x11-ui.hpp"

#include "host/host_plugin_window.hpp"
#include "host/plugin/lv2/lv2-carla-ui.hpp"
#include "host/plugin/lv2/lv2-catalog.hpp"
#include "host/plugin/lv2/lv2-plugin.hpp"
#include "host/plugin/lv2/lv2-runtime.hpp"
#include "host/plugin/lv2/lv2-ui-host.hpp"
#include "logging.hpp"

#include <lilv/lilv.h>
#include <lv2/core/lv2.h>
#include <lv2/instance-access/instance-access.h>
#include <lv2/ui/ui.h>
#include <lv2/urid/urid.h>
#include <cstring>
#include <dlfcn.h>
#include <vector>

#if defined(__linux__)
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#include <X11/Xlib.h>
#include "platform/linux/x11_util.hpp"

namespace {
bool x11_window_valid(Display* dpy, Window win) {
    if (!dpy || !win) {
        return false;
    }
    XWindowAttributes attrs{};
    return XGetWindowAttributes(dpy, win, &attrs) != 0;
}
} // namespace
#endif

namespace {

#if defined(__linux__)
void ui_write(LV2UI_Controller controller, uint32_t port_index, uint32_t buffer_size, uint32_t protocol, const void* buffer) {
    auto* plugin = static_cast<lv2plugin*>(controller);
    if (!plugin || !buffer || protocol != 0 || buffer_size != sizeof(float)) {
        return;
    }
    plugin->apply_ui_control(port_index, *static_cast<const float*>(buffer));
}

String bundle_dir_from_binary(const String& binaryPath) {
    String dir = binaryPath;
    const size_t slash = dir.find_last_of('/');
    if (slash != String::npos) {
        dir = dir.substr(0, slash + 1);
    }
    return dir;
}

String path_from_uri(const char* uri) {
    if (!uri) {
        return {};
    }
    String path = uri;
    if (path.find("file://") == 0) {
        path = path.substr(7);
    }
    return path;
}

using UiDescriptorFn = const LV2UI_Descriptor* (*)(uint32_t);

UiDescriptorFn load_ui_descriptor_fn(void*& libOut, const String& binaryPath) {
    void* dl = dlopen(StringAsCStr(binaryPath), RTLD_NOW | RTLD_LOCAL);
    if (!dl) {
        return nullptr;
    }
    auto* sym = reinterpret_cast<UiDescriptorFn>(dlsym(dl, "lv2ui_descriptor"));
    if (!sym) {
        dlclose(dl);
        return nullptr;
    }
    libOut = dl;
    return sym;
}

const LV2UI_Descriptor* find_ui_descriptor(UiDescriptorFn fn, const char* wantedUiUri) {
    if (!fn || !wantedUiUri) {
        return nullptr;
    }
    for (uint32_t i = 0;; ++i) {
        const LV2UI_Descriptor* desc = fn(i);
        if (!desc) {
            break;
        }
        if (desc->URI && std::strcmp(desc->URI, wantedUiUri) == 0) {
            return desc;
        }
    }
    return fn(0);
}
#endif

} // namespace

namespace lv2_native_x11_ui {

bool is_open(const lv2plugin* plugin) {
    return plugin && plugin->native_ui_handle() != nullptr;
}

bool open(lv2plugin* plugin, host_plugin_window* window, const LilvUI* ui, String& errorOut) {
#if !defined(__linux__)
    (void)plugin;
    (void)window;
    (void)ui;
    errorOut = "Native X11 LV2 UI is only supported on Linux";
    return false;
#else
    if (!plugin || !window || !ui || !plugin->lilv_instance_ptr()) {
        errorOut = "Invalid LV2 UI open request";
        return false;
    }

    close(plugin);

    const char* uiUri = lilv_node_as_uri(lilv_ui_get_uri(ui));
    if (!uiUri) {
        errorOut = "LV2 UI has no URI";
        return false;
    }

    const String binaryPath = path_from_uri(lilv_node_as_uri(lilv_ui_get_binary_uri(ui)));
    if (binaryPath.empty()) {
        errorOut = "LV2 UI has no binary";
        return false;
    }
    log_lf(Log::L_INFO, "LV2 native UI load: ui=%s bin=%s\n",
           uiUri ? uiUri : "?",
           StringAsCStr(binaryPath));

    void* dl = nullptr;
    UiDescriptorFn fn = load_ui_descriptor_fn(dl, binaryPath);
    if (!fn) {
        errorOut = StringFormat("UI binary has no lv2ui_descriptor: %s", dlerror() ? dlerror() : "?");
        return false;
    }

    const LV2UI_Descriptor* desc = find_ui_descriptor(fn, uiUri);
    if (!desc || !desc->instantiate) {
        if (dl) {
            dlclose(dl);
        }
        errorOut = StringFormat("No UI descriptor for %s", uiUri);
        return false;
    }
    log_lf(Log::L_INFO, "LV2 native UI descriptor: requested=%s resolved=%s\n",
           uiUri ? uiUri : "?",
           desc->URI ? desc->URI : "?");

    LilvWorld* world = lv2_catalog::process_world();
    bool toplevelUi =
        lv2_ui_host::ui_has_show_interface(world, ui)
        || (desc->extension_data && desc->extension_data(LV2_UI__showInterface));

    ivec2 size = lv2_ui_host::default_editor_size(plugin);
    const ivec2 pref = lv2_ui_host::preferred_ui_size(ui);
    if (pref.x >= 64 && pref.y >= 64) {
        size = pref;
    }
    const ivec2 saved = plugin->saved_editor_size();
    if (saved.x >= 64 && saved.y >= 64) {
        size = saved;
    } else if (plugin->bWindowPosSizeValid && plugin->lastWindowPosSize.z > 64 && plugin->lastWindowPosSize.w > 64) {
        size = { plugin->lastWindowPosSize.z, plugin->lastWindowPosSize.w };
    }
    plugin->bSupportsWindowResize = lv2_ui_host::ui_allows_user_resize(ui);
    if (toplevelUi) {
        plugin->set_toplevel_ui_mode(true);
    }

    LV2_Feature parentFeature{ LV2_UI__parent, nullptr };
    LV2_Feature* parentPtr = nullptr;
    if (!toplevelUi) {
        GLFWwindow* glfw = window->getGlfwWindow();
        if (!glfw) {
            if (dl) {
                dlclose(dl);
            }
            errorOut = "No GLFW window";
            return false;
        }
        if (!plugin->embed_surface().create_from_glfw(glfw, size.x, size.y)) {
            if (dl) {
                dlclose(dl);
            }
            errorOut = "Failed to create embed surface";
            return false;
        }
        parentFeature.data = reinterpret_cast<void*>(plugin->embed_surface().xid());
        parentPtr          = &parentFeature;
        if (GLFWwindow* glfwParent = window->getGlfwWindow()) {
            plugin->set_transient_window_id(static_cast<int64_t>(glfwGetX11Window(glfwParent)));
        }
    }

    std::vector<LV2_Feature*> features;
    lv2_ui_host::append_ui_features(plugin, features, parentPtr);
    features.push_back(nullptr);

    const char* pluginUri = lilv_node_as_uri(lilv_plugin_get_uri(plugin->lilv_descriptor_ptr()));
    const String bundlePath = bundle_dir_from_binary(binaryPath);
    LV2UI_Widget widget = nullptr;
    LV2UI_Handle handle = desc->instantiate(desc, pluginUri, StringAsCStr(bundlePath), ui_write, plugin, &widget, features.data());
    if (!handle) {
        if (dl) {
            dlclose(dl);
        }
        if (!toplevelUi) {
            plugin->embed_surface().destroy();
        }
        plugin->set_toplevel_ui_mode(false);
        errorOut = StringFormat("LV2 UI instantiate failed for %s", uiUri);
        return false;
    }

    const LV2UI_Idle_Interface* uiIdle = nullptr;
    const LV2UI_Show_Interface* showIface = nullptr;
    if (desc->extension_data) {
        uiIdle = static_cast<const LV2UI_Idle_Interface*>(desc->extension_data(LV2_UI__idleInterface));
        showIface = static_cast<const LV2UI_Show_Interface*>(desc->extension_data(LV2_UI__showInterface));
    }
    if (showIface) {
        toplevelUi = true;
    }

    plugin->set_native_ui(dl, desc, handle, uiIdle, showIface, toplevelUi);

    if (showIface && showIface->show) {
        log_lf(Log::L_INFO, "LV2 UI showInterface: %s\n", pluginUri ? pluginUri : "?");
        if (showIface->show(handle) != 0) {
            errorOut = StringFormat("LV2 UI show() failed for %s", uiUri);
            close(plugin);
            return false;
        }
        if (uiIdle && uiIdle->idle) {
            uiIdle->idle(handle);
        }
        if (lv2_carla_ui::is_carla_plugin(plugin) && plugin->has_dsp_worker()) {
            lv2_carla_ui::notify_show(plugin);
        }
    } else if (!toplevelUi && widget) {
        Display* dpy = static_cast<Display*>(plugin->embed_surface().display);
        if (!dpy && window->getGlfwWindow()) {
            dpy = glfwGetX11Display();
        }
        const Window pw = static_cast<Window>(reinterpret_cast<unsigned long>(widget));
        if (dpy && pw && x11_window_valid(dpy, pw)) {
            plugin->embed_surface().attach_plugin_window(reinterpret_cast<unsigned long>(widget));
            sendExposeEvent(window->getGlfwWindow());
            if (uiIdle && uiIdle->idle) {
                uiIdle->idle(handle);
            }
            lv2_ui_host::fit_host_window(plugin, window);
            lv2_ui_host::schedule_ui_fit(plugin, 5);
        }
    } else if (toplevelUi) {
        errorOut = StringFormat("LV2 UI advertises showInterface but provides no implementation for %s", uiUri);
        close(plugin);
        return false;
    }

    return true;
#endif
}

void hide(lv2plugin* plugin) {
    if (!plugin || !plugin->native_ui_handle()) {
        return;
    }
    if (plugin->native_show_interface() && plugin->native_show_interface()->hide) {
        plugin->native_show_interface()->hide(plugin->native_ui_handle());
    }
    plugin->embed_surface().destroy();
}

void close(lv2plugin* plugin) {
    if (!plugin) {
        return;
    }
    hide(plugin);
    if (plugin->native_ui_handle() && plugin->native_ui_descriptor() && plugin->native_ui_descriptor()->cleanup) {
        plugin->native_ui_descriptor()->cleanup(plugin->native_ui_handle());
    }
    if (plugin->native_ui_lib()) {
        dlclose(plugin->native_ui_lib());
    }
    plugin->clear_native_ui();
    plugin->embed_surface().destroy();
}

void notify_control(lv2plugin* plugin, uint32_t lilvPortIndex, float value) {
    if (!plugin || !plugin->native_ui_handle() || !plugin->native_ui_descriptor() || !plugin->native_ui_descriptor()->port_event) {
        return;
    }
    plugin->native_ui_descriptor()->port_event(plugin->native_ui_handle(), lilvPortIndex, sizeof(float), 0, &value);
}

void idle(lv2plugin* plugin) {
    if (!plugin || !plugin->native_ui_handle()) {
        return;
    }
    if (const LV2UI_Idle_Interface* uiIdle = plugin->native_ui_idle()) {
        if (uiIdle->idle && uiIdle->idle(plugin->native_ui_handle()) != 0) {
            plugin->on_ui_requested_close();
        }
    }
}

} // namespace lv2_native_x11_ui

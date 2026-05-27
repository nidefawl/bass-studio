#include "host/plugin/lv2/lv2-carla-ui.hpp"

#include "host/plugin/lv2/lv2-plugin.hpp"
#include "logging.hpp"

#include <lilv/lilv.h>
#include <cstring>

namespace lv2_carla_ui {

namespace {

constexpr const char* kCarlaPluginUriPrefix = "http://kxstudio.sf.net/carla/plugins/";

void run_worker_ui_message(lv2plugin* plugin, const char* msg) {
    if (!plugin || !msg || !msg[0]) {
        return;
    }
    plugin->run_worker_ui_message(msg);
}

} // namespace

bool is_carla_plugin(const lv2plugin* plugin) {
    if (!plugin || !plugin->lilv_descriptor_ptr()) {
        return false;
    }
    const char* uri = lilv_node_as_uri(lilv_plugin_get_uri(plugin->lilv_descriptor_ptr()));
    return uri && std::strncmp(uri, kCarlaPluginUriPrefix, std::strlen(kCarlaPluginUriPrefix)) == 0;
}

void notify_show(lv2plugin* plugin) {
    if (!is_carla_plugin(plugin)) {
        return;
    }
    log_lf(Log::L_INFO, "LV2 Carla UI: show\n");
    run_worker_ui_message(plugin, "show");
}

void notify_hide(lv2plugin* plugin) {
    if (!is_carla_plugin(plugin)) {
        return;
    }
    log_lf(Log::L_INFO, "LV2 Carla UI: hide\n");
    run_worker_ui_message(plugin, "hide");
}

void notify_idle(lv2plugin* plugin) {
    if (!is_carla_plugin(plugin)) {
        return;
    }
    run_worker_ui_message(plugin, "idle");
}

} // namespace lv2_carla_ui

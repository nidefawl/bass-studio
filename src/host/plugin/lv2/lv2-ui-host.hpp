#pragma once

#include "math/vec.hpp"
#include "types.hpp"

#include <lilv/lilv.h>
#include <lv2/core/lv2.h>
#include <vector>

class host_plugin_window;
class lv2plugin;

namespace lv2_ui_host {

/** @p parentFeature may be null for plugins that use ui:showInterface (toplevel window). */
void append_ui_features(lv2plugin* plugin, std::vector<LV2_Feature*>& out, LV2_Feature* parentFeature);

bool ui_has_show_interface(LilvWorld* world, const LilvUI* ui);

void resize_embed(lv2plugin* plugin, int width, int height);

void fit_host_window(lv2plugin* plugin, host_plugin_window* window);

/** Repeatedly refit while Gtk/native UIs settle (call from main thread). */
void schedule_ui_fit(lv2plugin* plugin, int frames = 5);

ivec2 default_editor_size(const lv2plugin* plugin);

/** Hint size from ui:width / ui:height in the UI TTL, if present. */
ivec2 preferred_ui_size(const LilvUI* ui);

bool ui_allows_user_resize(const LilvUI* ui);

#if defined(__linux__)
/** WM hints after Suil X11 embed (transient for host, iconify duplicate toplevels). */
void apply_x11_embed_hints(lv2plugin* plugin, host_plugin_window* window);
/** Hide duplicate plugin-owned toplevel windows (e.g. Odin2 stray window). */
void dismiss_stray_plugin_toplevels(lv2plugin* plugin, host_plugin_window* window);
#endif

} // namespace lv2_ui_host

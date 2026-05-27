#pragma once

#include "str_util.hpp"
#include "types.hpp"

class host_plugin_window;
class lv2plugin;

namespace lv2_ui {

/** Set while the DAW is unloading a project or exiting (skip blocking Suil teardown). */
void set_host_shutting_down(bool shutting_down);

bool open_editor(lv2plugin* plugin, host_plugin_window* window, String& errorOut);
/** Hide editor window; keeps Suil/native UI alive for reopen (Vitalium/JUCE). */
void hide_editor(lv2plugin* plugin);
/** Full UI teardown when the plugin instance is unloaded. */
void close_editor(lv2plugin* plugin);
void refresh_editor_after_show(lv2plugin* plugin, host_plugin_window* window);
/** Show/hide an open editor (native showInterface, or Gtk bridge widgets). */
void set_editor_visible(lv2plugin* plugin, bool visible);
void resize_editor(lv2plugin* plugin, int width, int height);
void notify_control(lv2plugin* plugin, uint32_t lilvPortIndex, float pluginValue);
void idle(lv2plugin* plugin);

} // namespace lv2_ui

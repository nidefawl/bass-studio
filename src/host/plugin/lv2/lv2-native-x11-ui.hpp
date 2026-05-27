#pragma once

#include "str_util.hpp"
#include "types.hpp"

#include <lilv/lilv.h>

class host_plugin_window;
class lv2plugin;

namespace lv2_native_x11_ui {

bool open(lv2plugin* plugin, host_plugin_window* window, const LilvUI* ui, String& errorOut);
void hide(lv2plugin* plugin);
void close(lv2plugin* plugin);
void notify_control(lv2plugin* plugin, uint32_t lilvPortIndex, float value);
void idle(lv2plugin* plugin);
bool is_open(const lv2plugin* plugin);

} // namespace lv2_native_x11_ui

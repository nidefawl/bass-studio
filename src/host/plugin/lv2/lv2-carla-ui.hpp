#pragma once

class lv2plugin;

namespace lv2_carla_ui {

/** True for KXStudio Carla native LV2 plugins (URI prefix carla/plugins/). */
bool is_carla_plugin(const lv2plugin* plugin);

/** Run Carla UI opcodes on the DSP worker thread (see carla-native-plugin.cpp lv2_work). */
void notify_show(lv2plugin* plugin);
void notify_hide(lv2plugin* plugin);
void notify_idle(lv2plugin* plugin);

} // namespace lv2_carla_ui

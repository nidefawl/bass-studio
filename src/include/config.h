#pragma once
#define USE_LOGGING 1
#ifndef TIMER_MS
#define TIMER_MS 16
#endif
#define USE_NANOVG_AA 1

#define FILE_FORMAT_VERSION 3
#define PROJECT_FILE_TYPE_DESC "Project File"
#define PROJECT_FILE_EXT "project"
#define PROJECT_BUNDLE_FILE_EXT "zip"
#ifndef USE_GUI_MENU
#define USE_GUI_MENU 1
#endif
#ifndef WINDOW_HAS_MENUBAR
#define WINDOW_HAS_MENUBAR 1
#endif
#define SETTINGS_NAME "data/settings.json"
#define THEMEFILE_NAME "theme.json"
#define KEYBIND_SETTINGS_FILENAME "data/keybinds.json"
#define CREATE_DEBUG_COMPANION_WINDOW 1
#define PARAM_COMMON_COUNT (1 << 12)
#define PARAM_OFFSET_IMPL (1 << 12)
#define PARAM_OFFSET_EXTERNAL (1 << 16)
#define PARAM_OFFSET_SEND_GAIN (1 << 12)
#define MAX_SEND_CHANNELS (1 << 4)
#define PARAM_OFFSET_SEND_PAN (PARAM_OFFSET_SEND_GAIN + MAX_SEND_CHANNELS)

#define MAX_AUDIO_IO_CHANNELS channelnum_t(255)

#ifndef BUILD_DAW_HOST
#define BUILD_DAW_HOST 1
#endif


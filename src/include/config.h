#pragma once
#define USE_LOGGING 1
#ifndef TIMER_MS
#define TIMER_MS 16
#endif
#define USE_NANOVG_AA 1

#define FILE_FORMAT_VERSION 2
#define PROJECT_FILE_EXT "project"
#ifndef USE_GUI_MENU
#define USE_GUI_MENU 1
#endif
#ifndef WINDOW_HAS_MENUBAR
#define WINDOW_HAS_MENUBAR 1
#endif
#if defined (_WIN32) || defined (__linux__)
#define WINDOW_RESTORE_POS 1
#else
#define WINDOW_RESTORE_POS 0
#endif
#define SETTINGS_NAME "data/settings.json"
#define THEMEFILE_NAME "data/theme.json"
#define CREATE_DEBUG_COMPANION_WINDOW 1
#define PARAM_COMMON_COUNT (1 << 12)
#define PARAM_OFFSET_IMPL (1 << 12)
#define PARAM_OFFSET_EXTERNAL (1 << 16)
#define PARAM_OFFSET_SEND (1 << 12)
#define MAX_SEND_CHANNELS (1 << 4)

#define MAX_AUDIO_IO_CHANNELS channelnum_t(255)

#ifndef BUILD_VSTHOST
#define BUILD_VSTHOST 1
#endif
#ifndef BUILD_EXTERNAL_PLUGIN
#define BUILD_EXTERNAL_PLUGIN 0
#endif

#if BUILD_EXTERNAL_PLUGIN && BUILD_VSTHOST
#error "BUILD_EXTERNAL_PLUGIN && BUILD_VSTHOST can't be defined at the same time"
#endif

#ifndef HAS_MAIN_LOOP
#define HAS_MAIN_LOOP (BUILD_EXTERNAL_PLUGIN == 0)
#endif

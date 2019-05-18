#pragma once
#define USE_LOGGING 1
#ifndef TIMER_MS
#define TIMER_MS 16
#endif

#define OUTPUT_CHANNELS 2
#define FILE_FORMAT_VERSION 2
#define PROJECT_FILE_EXT "project"
#ifndef USE_GUI_MENU
#define USE_GUI_MENU 1
#endif
#ifndef WINDOW_HAS_MENUBAR
#define WINDOW_HAS_MENUBAR 1
#endif
#define SETTINGS_NAME "data/settings.json"
#define THEMEFILE_NAME "data/theme.json"
#define CREATE_DEBUG_COMPANION_WINDOW 1

#define PARAM_OFFSET_EXTERNAL (1<<16)
#define PARAM_OFFSET_SEND (1<<12)
#define MAX_SEND_CHANNELS (1<<4)

#ifndef BUILD_VSTHOST
#define BUILD_VSTHOST 1
#endif
#ifndef BUILD_EXTERNAL_PLUGIN
#define BUILD_EXTERNAL_PLUGIN 0
#endif
#if BUILD_EXTERNAL_PLUGIN && BUILD_VSTHOST
#error
#endif

#if BUILD_EXTERNAL_PLUGIN && BUILD_VSTHOST
#error "cant have both"
#endif

#define HAS_MAIN_LOOP (!(TEST_PROJECT) && !BUILD_EXTERNAL_PLUGIN)

#if HAS_MAIN_LOOP
#define HAS_APP_SETTINGS 1
#else
#define HAS_APP_SETTINGS 0
#endif


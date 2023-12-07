#pragma once
#include <vector>
#include "types.h"
#include "gui/gui.h"
#include "gui/container/container.h"
#include "gui/plugin/pluginviewcontainers.h"

#if defined(_WIN32) && defined(_PLUGIN_EXPORT_DLL)
/* We are building GLFW as a Win32 DLL */
#define DAW_PLUGINAPI __declspec(dllexport)
#elif defined(_WIN32) && defined(_PLUGIN_IMPORT_DLL)
/* We are calling GLFW as a Win32 DLL */
#define DAW_PLUGINAPI __declspec(dllimport)
#elif defined(__GNUC__) && defined(_PLUGIN_EXPORT_DLL)
/* We are building GLFW as a shared / dynamic library */
#define DAW_PLUGINAPI __attribute__((visibility("default")))
#else
/* We are building or calling GLFW as a static library */
#define DAW_PLUGINAPI
#endif

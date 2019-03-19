#pragma once
#include <vector>
#include <stdint.h>
#include "gui/gui.h"
#include "gui/guicontainer.h"
#include "gui/pluginviewcontainers.h"

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

class AudioEffect;
//class PluginViewContainers {
//public:
//	virtual ~PluginViewContainers() {
//	}
//	virtual void onGuiOpen(AudioEffect* eff) {}
//	virtual void onGuiClose(AudioEffect* eff) {}
//	virtual void addTo(std::vector<guictr_base*>& v) = 0;
//	virtual void layout(int32_t winW, int32_t winH) = 0;
//	virtual void onSetParameter(int32_t index, float value) {}
//
//};
class PluginViewContainersImpl : public PluginViewContainers {
public:
	const int width;
	const int height;
	PluginViewContainersImpl(int _width, int _height) : width(_width), height(_height) {
	}
	virtual ~PluginViewContainersImpl() {
	}
};

extern "C" {
DAW_PLUGINAPI PluginViewContainers* createView();
}


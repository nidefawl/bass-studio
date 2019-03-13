#pragma once
#include <vector>
#include <stdint.h>
#include "gui.h"
#include "guicontainer.h"
class AudioEffect;
class vstplugin;
class PluginViewContainers {
public:
	virtual ~PluginViewContainers() {
	}
	 //all are called from host side
	virtual void setVSTPlugin(vstplugin* hostsideplugin) = 0;
	virtual void onGuiOpen(AudioEffect* eff) = 0;
	virtual void onGuiClose(AudioEffect* eff) = 0;
	virtual void addTo(std::vector<guictr_base*>& v) = 0;
	virtual void layout(int32_t winW, int32_t winH) = 0;
	virtual void onSetParameter(int32_t index, float value) = 0;
	virtual void getFixedSize(int32_t* w, int32_t* h) = 0;

};

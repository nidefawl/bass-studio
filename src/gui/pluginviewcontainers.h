#pragma once
#include <vector>
#include <stdint.h>
#include "gui.h"
#include "guicontainer.h"
#include "logging.h"
class AudioEffect;
class vstplugin;
class PluginViewContainers {
    bool inUse = true;

public:
    virtual ~PluginViewContainers() {
    }
    virtual void setVSTPlugin(vstplugin* hostsideplugin)    = 0;
    virtual void onGuiOpen(AudioEffect* eff)                = 0;
    virtual void onGuiClose(AudioEffect* eff)               = 0;
    virtual void addTo(std::vector<guictr_base*>& v)        = 0;
    virtual void layout(int32_t winW, int32_t winH)         = 0;
    virtual void onSetParameter(int32_t index, float value) = 0;
    virtual void getFixedSize(int32_t* w, int32_t* h)       = 0;
    void setFree() {
        inUse = false;
    }
    void setUsed() {
        inUse = true;
    }
    bool isInUse() const {
        return inUse;
    }
};

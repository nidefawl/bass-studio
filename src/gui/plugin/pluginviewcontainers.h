#pragma once
#include <vector>
#include "types.h"
#include "gui/gui.h"
#include "gui/container/container.h"
#include "logging.h"

#define UID_VIEW_CTR_WINDOW 1
#define UID_VIEW_CTR_PLUGIN_CTR 2
#define UID_VIEW_CTR_NODES 3

class PluginViewContainer {
    bool inUse = true;
    int32_t uiId = 0;
public:
    virtual ~PluginViewContainer() = default;
    virtual void addTo(std::vector<guictr_base*>& v)        = 0;
    virtual void layout(int32_t winW, int32_t winH)         = 0;
    virtual void onSetParameter(int32_t index, float value) = 0;
    virtual void getFixedSize(int32_t* w, int32_t* h)       = 0;
    virtual void onGuiOpen()              = 0;
    /* may be called multiple times */
    virtual void onGuiClose()                               = 0;
    virtual void onPresetLoaded() { }
    virtual void setFree() {
        onGuiClose();
        inUse = false;
    }
    virtual void setUsed() {
        inUse = true;
    }
    bool isInUse() const {
        return inUse;
    }
    void setUiId(int32_t id) {
        uiId = id;
    }
    int32_t getUiId() const {
        return uiId;
    }
    virtual bool isViewSupported(int32_t id) const {
        return true;
    }
};
template<typename PluginGUI, typename Plugin>
class PluginViewContainerBasic final : public PluginViewContainer {
protected:
    uint32_t width;
    uint32_t height;

public:
    PluginGUI ctr_main;
    explicit PluginViewContainerBasic(Plugin* eff, uint32_t _width = 320, uint32_t _height = 320)
        : width(_width), height(_height), ctr_main(eff) {
    }
    ~PluginViewContainerBasic() override = default;
    PluginGUI& getPluginUI() {
        return ctr_main;
    }
    const PluginGUI& getPluginUI() const {
        return ctr_main;
    }
    void layout(int32_t winW, int32_t winH) override {
        ctr_main.pos  = { 0, 0 };
        ctr_main.size = { winW, winH };
    }
    void addTo(std::vector<guictr_base*>& v) override {
        v.push_back(&ctr_main);
    }
    void onGuiOpen() override {
        ctr_main.onGuiOpen();
    }
    void onGuiClose() override {
        ctr_main.onGuiClose();
    }
    void onSetParameter(int32_t index, float value) override {
        ctr_main.onSetParameter(index, value);
    }
    void getFixedSize(int32_t* w, int32_t* h) override {
        ctr_main.getSizeScale(*w, *h);
    }
};

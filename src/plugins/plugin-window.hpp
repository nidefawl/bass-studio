#pragma once
#include "types.hpp"
#include "plugincontrol.hpp"
#include <vstsdk-plugin-2.4/aeffeditor.h>
#include <utility>

class pluginwindow : public AEffEditor {
protected:
    std::shared_ptr<PluginControl> const ctrlShared;

public:
    explicit pluginwindow(std::shared_ptr<PluginControl> _ctrl) : ctrlShared(std::move(_ctrl)) {
    }
    ~pluginwindow() override = default;

    virtual void onSetParameter(int32_t index, float value) = 0;
    virtual void destroyContextAndWindow()                  = 0;
    virtual void onHostWindowResize(int32_t w, int32_t h)   = 0;
    void idle() override {
    }
};

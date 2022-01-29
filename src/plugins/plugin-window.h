#pragma once
#include <stdint.h>
#include "../vstsdk-plugin-2.4/aeffeditor.h"
#include "plugincontrol.h"

class pluginwindow : public AEffEditor {
protected:
    std::shared_ptr<PluginControl> const ctrlShared;

public:
    pluginwindow(std::shared_ptr<PluginControl> _ctrl) : ctrlShared(_ctrl) {
    }
    virtual ~pluginwindow() {
    }

    virtual void onSetParameter(int32_t index, float value) = 0;
    virtual void destroyContextAndWindow()                  = 0;
    void idle() override {
    }
};

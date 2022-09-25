#pragma once
#include <vector>
#include "internal_plugin.h"
#include "modules.h"
#include "str_util.h"

class module_empty : public internalplugin {
    struct internal_handles_t;
    internal_handles_t* handle;

public:
    explicit module_empty(int32_t _projectGlobalId, IHostCallback* _hostCallback);
    ~module_empty() override;

public:
    int getModuleType() override { return PLUGIN_TYPE_EMPTY; };
    guiplugin* makeGui() override;
    guiplugin* getGui() override;
    void process(AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) override;
    bool isBypass() override {
        return true;
    }
    bool hasWindowEditor() override {
        return false;
    }
};

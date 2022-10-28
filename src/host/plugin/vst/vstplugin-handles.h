#pragma once
#include <vstsdk-host-2.4/aeffectx.h>
#include <memory>
#include <vector>
#include <cstdio>

struct audio_stage_t;
struct VstEvent_t;
class guiplugin;
class BasePluginVST2;
class PluginViewContainers;
struct handles_t {
    struct param_editing_t {
        int32_t paramIdx = -1;
        float   valBefore = 0;
    };
    param_editing_t paramEditing;
    uint32_t localCurrentUniqueId = 0;
    VstTimeInfo localTimeInfo{};
    VstEvent_t* midiEventsBuf = nullptr;
    std::vector<int32_t> heldNotes;
    BasePluginVST2* axEffect = nullptr;// Optional/Internal plugin only: handle to plugin implementation instance
    AEffect* aeffect       = nullptr;// hmodule owns if axEffect == null
    void* hmodule          = nullptr;// we dont own
    std::shared_ptr<guiplugin> gui;
    std::vector<uint8_t> dataChunkLocalMemory;
    handles_t(BasePluginVST2* ex, AEffect* e, void* m) {
        axEffect = ex;
        aeffect  = e;
        hmodule  = m;
    }
    ~handles_t();
};

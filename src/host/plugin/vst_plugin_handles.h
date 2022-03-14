#pragma once
#include <vstsdk-host-2.4/aeffectx.h>
#include <memory>
#include <vector>
#include <cstdio>

struct audio_stage_t;
class guiplugin;
class AudioEffectX;
class PluginViewContainers;
struct handles_t {
    uint32_t localCurrentUniqueId = 0;
    VstTimeInfo localTimeInfo{};
    AudioEffectX* axEffect = nullptr;// Optional/Internal plugin only: handle to plugin implementation instance
    AEffect* aeffect       = nullptr;// hmodule owns if axEffect == null
    void* hmodule          = nullptr;// we dont own
    std::shared_ptr<guiplugin> gui;
    std::vector<uint8_t> dataChunkLocalMemory;
    handles_t(AudioEffectX* ex, AEffect* e, void* m) {
        axEffect = ex;
        aeffect  = e;
        hmodule  = m;
    }
    ~handles_t();
};

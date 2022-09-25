#pragma once
#include "samplerate.h"
#include "types.h"
#include "assert_dbg.h"
#include <vstsdk-host-2.4/aeffectx.h>

#define PLUGIN_TYPE_VST 1
#define PLUGIN_TYPE_EMPTY 2
#define PLUGIN_TYPE_GROUP 3
#define PLUGIN_TYPE_INTERNAL_EFFECT 4
#define PLUGIN_TYPE_DEFERRED 5
#define PLUGIN_TYPE_AU 6
#define PLUGIN_TYPE_GAIN 7
#define PLUGIN_TYPE_LATENCY 8
#define PLUGIN_TYPE_SAMPLE_DELAY 9
#define PLUGIN_TYPE_SAMPLE_CRUSH 10
#define PLUGIN_TYPE_STEREO_WIDTH 11
#define PLUGIN_TYPE_SYNTH 12

#define PLUG_INT_HOSTINFO 1004
#define PLUG_INT_SYNTH 1005

class effectbase;

class IHostCallback {
    public:
    sampleformat_t m_sampleFormatInternal{};
    int32_t vstShellCurrentUniqueId = 0;
    VstTimeInfo m_vstTimeInfo{};
    bool isOfflineRendering = false;
    virtual ~IHostCallback() = default;
    virtual void onLatencyChanged(effectbase* effect) = 0;
    virtual void onParametersChanged(effectbase* effect, int32_t idx, float val, int flags, int stage) = 0;
    virtual void onIOConfigChanged(effectbase* effect) = 0;
    virtual void onUiChanged(effectbase* effect) = 0;
};

template<typename T>
effectbase* makeInstance(int32_t _projectGlobalId, IHostCallback* _hostCallback);

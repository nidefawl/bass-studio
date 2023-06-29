#pragma once
#include "samplerate.h"
#include "types.h"
#include "assert_dbg.h"
#include <vstsdk-host-2.4/aeffectx.h>

enum PluginType : int32_t {
    PLUGIN_TYPE_VST = 1,
    PLUGIN_TYPE_CLAP = 2,
    PLUGIN_TYPE_GROUP,
    PLUGIN_TYPE_INTERNAL_EFFECT,
    PLUGIN_TYPE_DEFERRED,
    PLUGIN_TYPE_AU,
    PLUGIN_TYPE_GAIN,
    PLUGIN_TYPE_LATENCY,
    PLUGIN_TYPE_SAMPLE_DELAY,
    PLUGIN_TYPE_SAMPLE_CRUSH,
    PLUGIN_TYPE_STEREO_WIDTH,
    PLUGIN_TYPE_SYNTH,
    PLUGIN_TYPE_MACROS,
    PLUGIN_TYPE_LFO,
    PLUGIN_TYPE_EMPTY,
    PLUGIN_TYPE_EQ,
    PLUGIN_TYPE_VISUALIZER,
    NUM_INTERNAL_PLUGINS,
    PLUG_INT_HOSTINFO = 1004,
    PLUG_INT_SYNTH = 1005
};

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

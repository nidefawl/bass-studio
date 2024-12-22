#pragma once
#include "host/project/project.h"
#include "samplerate.h"
#include "seq_time.h"
#include "types.h"
#include "assert_dbg.h"
#include <vstsdk-host-2.4/aeffectx.h>

enum ModuleType : int32_t {
    MODULE_TYPE_VST2 = 1,
    MODULE_TYPE_CLAP = 2,
    MODULE_TYPE_VST3 = 3,
    MODULE_TYPE_INTERNAL_EFFECT = 4,
    MODULE_TYPE_DEFERRED = 5,
    MODULE_TYPE_AU = 6,
};
enum PluginType : int32_t {
    PLUGIN_TYPE_GROUP = 3,
    PLUGIN_TYPE_GAIN = 7,
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
    PLUGIN_TYPE_SYNTH_MONO,
    PLUGIN_TYPE_SYNTH_SHAPER,
    PLUGIN_TYPE_SYNTH_GPU,
    PLUGIN_TYPE_SYNTH_KICKXP,
    PLUGIN_TYPE_HOSTINFO = 1004,
};

class effectbase;

class IHostCallback {
    public:
    sampleformat_t m_sampleFormatInternal{};
    int32_t vstShellCurrentUniqueId = 0;
    VstTimeInfo m_vstTimeInfo{};
    bool isOfflineRendering = false;
    playback_state m_playbackState = playback_state::status_no_process;
    export_settings_t m_exportSettings{};

    virtual ~IHostCallback() = default;
    virtual void onLatencyChanged(effectbase* effect) = 0;
    virtual void onParametersChanged(effectbase* effect, int32_t idx, float val, int flags, int stage) = 0;
    virtual void onIOConfigChanged(effectbase* effect) = 0;
    virtual void onUiChanged(effectbase* effect) = 0;
};

template<typename T>
effectbase* makeInstance(int32_t _projectGlobalId, IHostCallback* _hostCallback);

#pragma once
#include "types.h"
#include "assert_dbg.h"

#define PLUGIN_TYPE_VST 1
#define PLUGIN_TYPE_EMPTY 2
#define PLUGIN_TYPE_GROUP 3
#define PLUGIN_TYPE_INTERNAL_EFFECT 4
#define PLUGIN_TYPE_DEFERRED 5
#define PLUGIN_TYPE_AU 6
#define PLUGIN_TYPE_GAIN 7
#define PLUGIN_TYPE_LATENCY 8
#define PLUGIN_TYPE_SAMPLE_DELAY 9

#define PLUG_INT_STEREOWIDTH 1000
#define PLUG_INT_TEST 1001
#define PLUG_INT_CRASHVST 1002
#define PLUG_INT_HOSTINFO 1004
#define PLUG_INT_SYNTH 1005
#define PLUG_INT_BITCRUSH 1006

class effectbase;

class i_host_callback {
    public:
    virtual ~i_host_callback() = default;
    virtual void onLatencyChanged(effectbase* effect) = 0;
    virtual void onParametersChanged(effectbase* effect, int32_t idx, float val, int flags, int stage) = 0;
    virtual void onIOConfigChanged(effectbase* effect) = 0;
};

template<typename T>
effectbase* makeInstance(int32_t _projectGlobalId, i_host_callback* _hostCallback);

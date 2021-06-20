#pragma once
#include <stdint.h>
#include "assert_dbg.h"

#define PLUGIN_TYPE_VST 1
#define PLUGIN_TYPE_EMPTY 2
#define PLUGIN_TYPE_GROUP 3
#define PLUGIN_TYPE_INTERNAL_EFFECT 4
#define PLUGIN_TYPE_DEFERRED 5
#define PLUGIN_TYPE_AU 6
#define PLUGIN_TYPE_GAIN 7

#define PLUG_INT_STEREOWIDTH 1000
#define PLUG_INT_TEST 1001
#define PLUG_INT_CRASHVST 1002
#define PLUG_INT_LATENCY 1003

class effectbase;
template<typename T>
effectbase* makeInstance(int32_t _projectGlobalId);

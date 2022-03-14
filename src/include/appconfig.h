#pragma once
#include <cstdint>
#include <map>
#include "str_util.h"

struct runtime_info_t {
    String glRenderer;
    String glVendor;
    String glVersion;
};
struct app_config_t {
    runtime_info_t runtime;
    bool enableCache                  = true;
    bool disableWaveformUpdates       = false;
    bool enableClipRendererDebugLayer = false;
};

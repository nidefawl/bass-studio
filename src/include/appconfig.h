#pragma once
#include <cstdint>
#include <map>
#include "str_util.h"

struct app_config_t {
    bool enableCache                  = true;
    bool disableWaveformUpdates       = false;
    bool enableClipRendererDebugLayer = false;
};

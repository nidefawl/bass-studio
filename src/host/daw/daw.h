
#pragma once
#include "types.h"

namespace DAW {
    enum PluginLoadFlags : int32_t {
        FLAG_DEFER_LOAD = 1 << 0,
        FLAG_INVOKE_USER_CB_DEFERLOAD = 1 << 1,
    };
}

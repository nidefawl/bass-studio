#pragma once
#include "types.hpp"

struct tracksnapshot_store_opts_t {
    bool storePluginPreset = true;
    bool storeAutomation = true;
    bool storeClips = true;
    bool storeLayouts = true;
    static inline tracksnapshot_store_opts_t All() {
        return tracksnapshot_store_opts_t{true, true, true, true};
    }
    static inline tracksnapshot_store_opts_t NoPluginPresets() {
        return tracksnapshot_store_opts_t{false, true, true, true};
    }
    static inline tracksnapshot_store_opts_t AutomationOnly() {
        return tracksnapshot_store_opts_t{false, true, false, false};
    }
};

struct param_snapshot_t {
    int32_t idx = 0;
    float val   = 0;
    int flags   = 0;
};

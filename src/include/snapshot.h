#pragma once
#include <cstdint>
#include <vector>
#include "str_util.h"

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
struct automation_view_t;
struct plugin_snapshot_t {
    int32_t projectGlobalId = 0;
    bool present            = false;
    bool enabled            = false;
    int32_t slot            = 0;
    int32_t pluginType      = 0;
    int32_t localDbId       = 0;
    int32_t vendorVersion   = 0;
    int32_t uId             = 0;
    String name;
    int32_t currentProgram = -1;
    String currentProgramName;
    std::vector<uint8_t> dataChunk;
    std::vector<uint8_t> dataChunk2;
    std::vector<param_snapshot_t> params;
    std::vector<automation_view_t> automatedParams;
    std::vector<plugin_snapshot_t> pluginSnapshots;
};

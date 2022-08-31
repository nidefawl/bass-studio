#pragma once
#include "types.h"
#include "math/vec.h"
#include <cstdint>
#include <vector>
#include "host/daw_channel.h"
#include "str_util.h"
#include "track_routing_snapshot.h"

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
struct plugin_iodesc_snapshot_t {
    std::vector<DAW::channel_desc> input;
    std::vector<DAW::channel_desc> output;
};
struct automation_view_t;
struct track_effect_routing_snapshot_t;
struct plugin_ui_snapshot_t {
    ivec4 windowPosSize{};
    bool windowPosSizeValid = false;
    bool isWindowOpen = false;
    bool parameterListVisible = true;
    int32_t layoutMode = -1;
};
struct plugin_snapshot_t {
    uint32_t version = 0;
    int32_t projectGlobalId = 0;
    bool enabled            = false;
    int32_t slot            = 0;
    int32_t pluginType      = 0;
    int32_t localDbId       = 0;
    int32_t vendorVersion   = 0;
    uint32_t uId            = 0;
    String name;
    int32_t currentProgram = -1;
    String currentProgramName;
    plugin_iodesc_snapshot_t ioChannels;
    track_effect_routing_snapshot_t effectRouting;
    track_id_snapshot_t stageIds;
    plugin_ui_snapshot_t uiSnapshot;
    std::vector<uint8_t> dataChunk;
    std::vector<uint8_t> dataChunk2;
    std::vector<param_snapshot_t> params;
    std::vector<automation_view_t> automatedParams;
    std::vector<plugin_snapshot_t> pluginSnapshots;
};

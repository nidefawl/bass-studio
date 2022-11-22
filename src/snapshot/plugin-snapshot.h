#pragma once
#include "fileio.h"
#include "snapshot/snapshot.h"
#include "snapshot/trackrouting-snapshot.h"
#include "snapshot/track-snapshot.h"
#include "host/daw_channel.h"
#include "host/automation/automation.h"
#include "str_util.h"
#include "logging.h"
#include <vector>
#include <map>

class track_t;
struct track_impl_t;

struct plugin_iodesc_snapshot_t {
    std::vector<DAW::channel_desc> input;
    std::vector<DAW::channel_desc> output;
};
struct automation_view_t;
struct track_effect_routing_snapshot_t;
struct plugin_ui_snapshot_t {
    bool isValidSnapshot = false;
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
    String clapId;
    String name;
    int32_t currentProgram = -1;
    String currentProgramName;
    plugin_iodesc_snapshot_t ioChannels;
    track_effect_routing_snapshot_t effectRouting;
    track_modulation_routing_snapshot_t modulationRouting;
    track_id_snapshot_t stageIds;
    std::map<int32_t, plugin_ui_snapshot_t> uiSnapshots;
    std::vector<uint8_t> dataChunk;
    std::vector<uint8_t> dataChunk2;
    std::vector<param_snapshot_t> params;
    std::vector<automation_view_t> automatedParams;
    std::vector<plugin_snapshot_t> pluginSnapshots;
};


extern const SupportedFileTypes FILE_TYPES_PLUGINSNAPSHOT;
bool savePluginSnapshot(const plugin_snapshot_t& snapshot, const String& path);
std::shared_ptr<plugin_snapshot_t> loadPluginSnapshot(const String& path);
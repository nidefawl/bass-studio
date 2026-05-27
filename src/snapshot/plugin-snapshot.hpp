#pragma once
#include "fileio.hpp"
#include "platform/win/windowsize.hpp"
#include "snapshot/snapshot.hpp"
#include "snapshot/trackrouting-snapshot.hpp"
#include "snapshot/track-snapshot.hpp"
#include "host/daw_channel.hpp"
#include "host/automation/automation.hpp"
#include "str_util.hpp"
#include "logging.hpp"
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
struct plugin_windowlayout_snapshot_t {
    bool isValidSnapshot = false;
    ivec4 windowPosSize{};
    bool windowPosSizeValid = false;
    bool isWindowOpen = false;
    appwindow_size_t windowSize{};
};
struct plugin_ui_snapshot_t {
    bool isValidSnapshot = false;
    bool parameterListVisible = true;
    int32_t layoutMode = -1;
};
struct plugin_snapshot_t {
    uint32_t version = 0;
    int32_t projectGlobalId = 0;
    bool enabled            = false;
    int32_t slot            = 0;
    int32_t moduleType      = 0;
    int32_t localDbId       = 0;
    int32_t vendorVersion   = 0;
    uint32_t uId            = 0;
    String clapId;
    /** LV2 plugin instance URI (http://…); not used for other module types. */
    String instanceUri;
    String name;
    int32_t currentProgram = -1;
    String currentProgramName;
    plugin_iodesc_snapshot_t ioChannels;
    track_effect_routing_snapshot_t effectRouting;
    track_modulation_routing_snapshot_t modulationRouting;
    track_id_snapshot_t stageIds;
    std::map<int32_t, plugin_ui_snapshot_t> uiSnapshots;
    plugin_windowlayout_snapshot_t windowLayout;
    std::vector<uint8_t> dataChunk;
    std::vector<uint8_t> dataChunk2;
    std::vector<param_snapshot_t> params;
    std::vector<automation_view_t> automatedParams;
    std::vector<plugin_snapshot_t> pluginSnapshots;
};


extern const SupportedFileTypes FILE_TYPES_PLUGINSNAPSHOT;

namespace DAW::ProjectFileV1 {
    std::optional<String> savePluginSnapshot(const plugin_snapshot_t& snapshot, const String& path);
    std::variant<std::shared_ptr<plugin_snapshot_t>, String> loadPluginSnapshot(const String& path);
} // namespace DAW::ProjectFileV1

namespace DAW::ProjectFileV2 {
    std::optional<String> savePluginSnapshot(const plugin_snapshot_t& snapshot, const String& path);
    std::variant<std::shared_ptr<plugin_snapshot_t>, String> loadPluginSnapshot(const String& path);
} // namespace DAW::ProjectFileV2
            
#pragma once
#include <vector>
#include <map>
#include "assert_dbg.h"
#include "cursor.h"
#include "clip.h"
#include "str_util.h"
#include "logging.h"
#include "automation.h"
#include "snapshot.h"
#include "track.h"
#include "track_routing_snapshot.h"

class track_t;
struct track_impl_t;


struct track_params_snapshot_t {
    std::vector<param_snapshot_t> params;
    std::vector<automation_view_t> automatedParams;
};
struct arp_snapshot {
    std::vector<param_snapshot_t> params;
    std::vector<automation_view_t> automatedParams;
};
struct audio_stage_t;
struct plugin_snapshot_t;
struct track_impl_snapshot_t {
    arp_snapshot trackArp;
    track_io_configuration_snapshot_t trackIO;
    track_params_snapshot_t trackParams;
    std::vector<plugin_snapshot_t> pluginSnapshots;
    track_effect_routing_snapshot_t effectRouting;
    track_impl_snapshot_t() = default;
    track_impl_snapshot_t(track_impl_t* p, const tracksnapshot_store_opts_t& opts);
};
struct track_layout_snapshot_t {
    tracklayout_settings_t layout;
    std::vector<automatable_param_ref_t> automationLanes;
};
struct track_snapshot_t {
    tracksnapshot_store_opts_t storeOpts;
    tracksettings_t trackSettings;
    track_id_snapshot_t stageIds;
    int32_t localIdx     = -1;
    track_t* trackLoaded = nullptr;// ref set in first phase of, cleared in second of 2-phase loading
    track_impl_snapshot_t data;
    std::vector<clip_t> clips;
    std::map<int32_t, track_layout_snapshot_t> layouts;
    track_snapshot_t() = default;
    track_snapshot_t(const track_t* track, const tracksnapshot_store_opts_t& opts);
};

struct trackcontainer_snapshot_t {
    std::vector<track_snapshot_t> tracks;
    std::vector<int32_t> hierachy;
};

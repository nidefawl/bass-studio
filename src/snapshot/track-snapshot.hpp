#pragma once
#include "snapshot/snapshot.hpp"
#include "host/automation/automation.hpp"
#include "host/clip/clip.hpp"
#include "host/track/track_types.hpp"
#include "host/daw_channel.hpp"
#include "snapshot/trackrouting-snapshot.hpp"
#include <vector>
#include <map>

class track_t;
struct track_impl_t;
struct automation_view_t;
struct audio_stage_t;
struct plugin_snapshot_t;
struct track_effect_routing_snapshot_t;

struct track_params_snapshot_t {
    std::vector<param_snapshot_t> params;
    std::vector<automation_view_t> automatedParams;
};

struct arp_snapshot {
    std::vector<param_snapshot_t> params;
    std::vector<automation_view_t> automatedParams;
};

struct track_modulation_routing_snapshot_t {
    std::vector<DAW::modulation_channel_ref> mixer;
    std::vector<DAW::modulation_channel_ref> arp;
    std::map<int32_t, std::vector<DAW::modulation_channel_ref>> effectMods;
};

struct track_impl_snapshot_t {
    arp_snapshot trackArp;
    track_io_configuration_snapshot_t trackIO;
    track_params_snapshot_t trackParams;
    std::vector<plugin_snapshot_t> pluginSnapshots;
    track_effect_routing_snapshot_t effectRouting;
    track_modulation_routing_snapshot_t modulationRouting;
    track_impl_snapshot_t() = default;
    track_impl_snapshot_t(track_impl_t* p, const tracksnapshot_store_opts_t& opts);
};

struct subtrack_snapshot_t {
    subtracksettings_t settings;
    subtracklayout_settings_t layoutSettings;
    automatable_param_ref_t atlRef;
};

struct track_layout_snapshot_t {
    tracklayout_settings_t layout;
    std::vector<subtrack_snapshot_t> subtracks;
};

struct mixer_layout_snapshot_t {
    mixerlayout_settings_t layout;
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
    std::map<int32_t, mixer_layout_snapshot_t> layoutsMixer;
    track_snapshot_t() = default;
    track_snapshot_t(const track_t* track, const tracksnapshot_store_opts_t& opts);
};

struct trackcontainer_snapshot_t {
    std::vector<track_snapshot_t> tracks;
    std::vector<int32_t> hierachy;
};

#pragma once
#include <vector>
#include <utility>
#include "types.h"
#include "seq_time.h"
#include "host/automation/automation.h"
#include "host/track/track.h"

class clip_t;
class track_t;
struct track_clipboard_t {
    std::vector<std::shared_ptr<clip_t>> clips;
    std::vector<automation_clipboard_t> automations;
};

struct clip_clipboard {
    enum clipboard_type_e {
        ClipboardFull,
        ClipboardAutomation
    };
    std::vector<std::shared_ptr<track_clipboard_t>> tracks;
    std::vector<automation_clipboard_t> automationLanes;
    tick_t srcPos         = 0;
    tick_t srcTrack       = 0;
    int32_t selRange      = 0;
    int32_t selTrackRange = 0;
    clipboard_type_e type = ClipboardFull;
};
using clipboard_track_view_t = std::pair<track_t*, std::vector<clip_t*>>;
struct clipboard_view_t {
    std::vector<clipboard_track_view_t> tracks{};
};


struct plugin_snapshot_t;
struct plugin_clipboard_t {
    std::vector<plugin_snapshot_t> plugins;
    int32_t range = 0;
};

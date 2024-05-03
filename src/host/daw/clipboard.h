#pragma once
#include <vector>
#include <utility>
#include "host/clip/clip.h"
#include "note.h"
#include "types.h"
#include "seq_time.h"
#include "host/automation/automation.h"
#include "host/track/track.h"
#include "host/track/trackctr_types.h"

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
    int32_t srcTrack      = 0;
    int32_t selRange      = 0;
    int32_t selTrackRange = 0;
    clipboard_type_e type = ClipboardFull;
};
using track_view_selection_t = std::pair<track_gui_entry_t, std::vector<clip_t*>>;

struct editor_view_selection_t {
    std::vector<track_view_selection_t> tracks{};
    size_t totalClipCount = 0;
    tick_t viewBegin = 0;
    tick_t viewEnd   = 0;
    clip_editor_layout_t editorLayout;
};

struct plugin_snapshot_t;
struct plugin_clipboard_t {
    std::vector<plugin_snapshot_t> plugins;
    int32_t range = 0;
};

struct notes_clipboard {
    clip_notes_t notes;
    tick_t cursorRange = 0;
    bool empty() const {
        return notes.isEmpty();
    }
};

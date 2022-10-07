#pragma once
#include "types.h"
#include <vector>
#include <memory>
#include <map>
#include "config.h"
#include "math/seq_math.h"
#include "math/vec.h"
#include "seq_util.h"
#include "track.h"
#include "snapshot/track-snapshot.h"
#include "grid.h"

class gui_track;
class guictr_tracks;
class gui_track_subtrack;
class gui_track_controls;
class DawCtrl;

struct tracklayout_state_t {
    automatable_t* selectedAutomationCtr = nullptr;
    int32_t selectedAutomationParam      = -1;
    track_layout_snapshot_t layoutSaved;
    bool wasInHide = false;
};

struct track_gui_entry_t {
    DawCtrl* parentCtrl = nullptr;
    track_t* track = nullptr;
    gui_track* content = nullptr;
    guictr_tracks* parent = nullptr;
    gui_track_controls* mixer = nullptr;
    int32_t idx = -1;
    tracklayout_settings_t layout;
    tracklayout_state_t state;
    std::vector<gui_track_subtrack*> subtracks;
    std::map<clip_t*, gui_clip*> clipsGuis;
    bool validSubtrack(int32_t subtrackIdx) const {
        return subtrackIdx >= 0 && subtrackIdx < (int32_t) subtracks.size();
    }
    bool isHidden() const {
        return layout.hideTrack && track->children.empty();
    }
};

void getTrackGuiYBounds(const track_gui_entry_t* track, ivec2& topBottom);
track_gui_entry_t* getParentOf(track_gui_entry_t* t);
using track_gui_vector_td       = std::vector<track_gui_entry_t*>;
using const_track_gui_vector_td = std::vector<const track_gui_entry_t*>;

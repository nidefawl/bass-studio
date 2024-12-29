#pragma once
#include "gui/container/container.h"
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

class gui_track_content;
class guictr_tracks;
class gui_track_subtrack;
class gui_track_control;
class effect_deferred;
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
    gui_track_content* trackContent = nullptr;
    guictr_tracks* parent = nullptr;
    gui_track_control* trackControls = nullptr;
    guictr_base* trackMixers = nullptr;
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
    int32_t getHeight() const;
};

void getTrackGuiYBounds(const track_gui_entry_t* track, ivec2& topBottom);
track_gui_entry_t* getParentOf(track_t* t);
using track_gui_vector_td       = std::vector<track_gui_entry_t*>;
using const_track_gui_vector_td = std::vector<const track_gui_entry_t*>;


namespace DAW {
    class gui_track_drop_position_t {
    public:
        enum drop_type {
            none,
            track_on,
            track_before,
            track_after
        };
        int slot = 0;
        track_t* droppedTrack;
        drop_type droptype = none;
        ivec2 pos{};
    };
    gui_track_drop_position_t GetTrackSlotFromCoord(guictr_tracks* parent, const ivec2 pos, bool bIncludeBeforeAfter = true);
    void SetDragDropTrackInidicatorFromMousePos(guictr_tracks* parent, ivec2 mousepos, const String& trackName, bool bIncludeBeforeAfter = true);
    void MoveTrackToSlot(DawInstance* daw, track_t* track, gui_track_drop_position_t slot);
    void InsertTrackContainerOnTrack(DawInstance* daw, trackcontainer_snapshot_t* ctr, const gui_track_drop_position_t& slot);
    void InsertEffectDeferredOnStage(DawInstance* daw, audio_stage_t* stage, effect_deferred* effect, int32_t slot, bool activate, bool scrollTo);
}

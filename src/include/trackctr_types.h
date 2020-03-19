#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <vector>
#include <memory>
#include "config.h"
#include "math/vec.h"
#include "seq_util.h"
#include "track.h"
#include "track_snapshot.h"
#include "grid.h"

class gui_track;
class guictr_tracks;
class gui_track_subtrack;
class gui_track_controls;
struct tracklayout_state_t {
	automatable_t* selectedAutomationCtr = nullptr;
	int32_t selectedAutomationParam = -1;
	track_layout_snapshot_t layoutSaved;
	bool wasInHide = false;
};
struct track_gui_entry_t {
	int32_t idx = -1;
	track_t* track;
	gui_track* content;
	guictr_tracks* parent;
	std::vector<gui_track_subtrack*> subtracks;
	gui_track_controls* mixer = nullptr;
	tracklayout_settings_t layout;
	tracklayout_state_t state;
};
track_gui_entry_t* getParentOf(track_gui_entry_t* t);
using track_gui_vector_td = std::vector<track_gui_entry_t*>;


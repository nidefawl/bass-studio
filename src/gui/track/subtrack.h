#pragma once
class track_t;
struct track_gui_entry_t;
class gui_track_subtrack;
class scaled_grid;
gui_track_subtrack* makeGuiSubtrack(track_gui_entry_t* entry, scaled_grid& grid, int type);

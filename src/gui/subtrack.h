#pragma once
class track_t;
class MainCtrl;
class gui_track_subtrack;

gui_track_subtrack* makeGuiSubtrack(track_gui_entry_t* entry, DawCtrl* ctrl, int type);

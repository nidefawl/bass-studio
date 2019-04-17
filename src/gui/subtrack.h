#pragma once
class track_t;
class MainCtrl;
class gui_track_subtrack;

gui_track_subtrack* makeGuiSubtrack(MainCtrl* ctrl, track_t* track, int type);

#pragma once
#include <vector>
#include <stdint.h>
#include "seq_time.h"
#include "track.h"

struct track_clipboard_t {
	std::vector<std::shared_ptr<clip_t>> clips;
};
class clip_clipboard {
public:
	std::vector<std::shared_ptr<track_clipboard_t>> tracks;
	tick_t srcPos = 0;
	tick_t srcTrack = 0;
	int32_t selRange = 0;
	int32_t selTrackRange = 0;
};

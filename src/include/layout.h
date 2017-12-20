#pragma once
#include "grid_constants.h"

struct layout_pianoroll_t {
	float yscale = 10.0f;
	float yoffset = 0.0f;
};
struct layout_grid_t {
	int offset = 0.0f;
	float zoom = DEFAULT_ZOOM;
};
struct clip_editor_layout_t {
	layout_grid_t layoutGrid;
	layout_pianoroll_t layoutPianoRoll;
};

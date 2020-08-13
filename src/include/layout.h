#pragma once
#include "grid_constants.h"

struct layout_pianoroll_t {
	float yscale = 10.0f;
	float yoffset = 0.0f;
	bool fold = false;
	float yscalefold = 10.0f;
	float yoffsetfold = 0.0f;
	float& offset() {
		return fold ? this->yoffsetfold : this->yoffset;
	}
	float& scale() {
		return fold ? this->yscalefold : this->yscale;
	}
};
struct layout_grid_t {
	int offset = 0;
	double zoom = DEFAULT_ZOOM;
};
struct clip_editor_layout_t {
	layout_grid_t layoutGrid;
	layout_pianoroll_t layoutPianoRoll;
};

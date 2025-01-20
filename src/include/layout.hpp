#pragma once
#include "types.hpp"
#include "grid_constants.hpp"

struct layout_pianoroll_t {
    float yscale      = 6.0f;
    float yoffset     = 280.0f;
    bool bFoldNotes   = false;
    float yscalefold  = 10.0f;
    float yoffsetfold = 0.0f;
    float& offset() {
        return bFoldNotes ? this->yoffsetfold : this->yoffset;
    }
    float& scale() {
        return bFoldNotes ? this->yscalefold : this->yscale;
    }
    float offset() const {
        return bFoldNotes ? this->yoffsetfold : this->yoffset;
    }
    float scale() const {
        return bFoldNotes ? this->yscalefold : this->yscale;
    }
};
struct layout_grid_t {
    double offset  = 0;
    double zoom = DEFAULT_ZOOM;
};
struct clip_editor_layout_t {
    layout_grid_t layoutGrid;
    layout_pianoroll_t layoutPianoRoll;
    bool noLayout = true;
};

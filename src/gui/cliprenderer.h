#pragma once
#include "math/vec.h"
#include "math/seq_math.h"
#include "track.h"
#include "clip.h"
#include "grid.h"
#include "theme.h"
#include "wave/waveform_render.h"

struct gui_waveform_texture_ref;
void renderAudioClip(NVGcontext* vg, waveformrender* wfrenderer, const guitheme_t* theme, const track_t* tr, const clip_t* cl, const gui_waveform_texture_ref* guiaudioclip, ivec2 pos, ivec2 size, ivec2 posClipped, ivec2 sizeClipped);
void renderMidiClip(NVGcontext* vg, const guitheme_t* theme, const track_gui_entry_t* entry, const clip_t* cl, ivec2 pos, ivec2 size);
audioclip_texture_t makeWaveformFromClip(const int32_t tempo100, const samplerate_t samplerate, scaled_grid& grid, ivec2& trackSize, const clip_t* m_clip, const ivec2& pos, const ivec2& size, ivec2& posClipped, ivec2& sizeClipped);


inline float noteToScreen(float note, float scale, float offset, float sizeY) {
    float offsetKey = note * scale;
    float rel       = offsetKey - offset;
    return (sizeY) -rel;
}
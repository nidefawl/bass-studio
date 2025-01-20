#pragma once
#include "math/vec.hpp"
#include "math/seq_math.hpp"
#include "host/track/track.hpp"
#include "host/clip/clip.hpp"
#include "grid.hpp"
#include "theme.hpp"
#include "wave/waveform_render.hpp"

struct gui_waveform_texture_ref;
void renderAudioClip(NVGcontext* vg, waveformrender* wfrenderer, const guitheme_t* theme, const clip_t* cl, const audiofile_t* file, const gui_waveform_texture_ref* guiaudioclip, ivec2 pos, ivec2 size, ivec2 posClipped, ivec2 sizeClipped, GuiColor::constant_t colOutline = GuiColor::COL_CLIP_OUTLINE);
void renderMidiClip(NVGcontext* vg, const guitheme_t* theme, const clip_t* cl, ivec2 pos, ivec2 size, GuiColor::constant_t colOutline = GuiColor::COL_CLIP_OUTLINE);
audioclip_texture_t makeWaveformFromClip(const int32_t tempo100, const samplerate_t samplerate, scaled_grid& grid, ivec2& trackSize, const clip_t* m_clip, const ivec2& pos, const ivec2& size, ivec2& posClipped, ivec2& sizeClipped);

namespace DAW {
    void RenderClipAt(NVGcontext* vg, guitheme_t* theme, DawCtrl* dawCtrl, scaled_grid& grid, clip_t* cl, tick_t offset, ivec2 clipPos, ivec2 clipSize);
} // namespace DAW


inline float noteToScreen(float note, float scale, float offset, float sizeY) {
    float offsetKey = note * scale;
    float rel       = offsetKey - offset;
    return (sizeY) -rel;
}
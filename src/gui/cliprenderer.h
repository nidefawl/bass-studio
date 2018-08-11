#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <vector>
#include "seq_math.h"
#include "seq_util.h"
#include "color_util.h"
#include "track.h"
#include "clip.h"
#include "grid.h"
#include "guicontainer.h"
#include "trackautomation.h"
#include "audiowaveform.h"
#include "leak_detect.h"
#include <glm/glm.hpp>
#include <glm/vec2.hpp>
using glm::vec2;
using glm::ivec2;

void renderAudioClip(NVGcontext* vg, const track_t* tr, const clip_t* cl, const gui_waveform_texture_ref* guiaudioclip, ivec2 pos, ivec2 size, ivec2 sizeClipped);
void renderMidiClip(NVGcontext* vg, const track_t* tr, const clip_t* cl, ivec2 pos, ivec2 size);
bool getClipPosition(scaled_grid& grid, const ivec2& trackSize, const clip_t* cl, ivec2& pos, ivec2& size, tick_t offset);
audioclip_texture_t makeWaveformFromClip(project_t& project, scaled_grid& grid, ivec2& trackSize, clip_t* m_clip, ivec2& pos, ivec2& size, ivec2& posClipped, ivec2& sizeClipped);

#pragma once
#include <vector>
#include "math/vec.h"
#include "audiosample.h"
#include "waveform_render.h"

void tesselateWaveform(audiosample_t* sample, float x, float y, audioclip_texture_t* waveform, SampleMethod method, std::vector<std::vector<vec2>>& channels);

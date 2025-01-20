#pragma once
#include <vector>
#include "math/vec.hpp"
#include "host/audiosample.hpp"
#include "waveform_render.hpp"

void tesselateWaveform(audiosample_t* sample, float x, float y, audioclip_texture_t* waveform, SampleMethod method, std::vector<std::vector<vec2>>& channels);

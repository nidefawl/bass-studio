#pragma once
#include <vector>
#include <stdint.h>
#include <vector>
#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include "audiosample.h"
using glm::ivec2;
using glm::ivec4;

enum SampleMethod {
	sample_straight, sample_sum, sample_minmax, sample_interp, sample_minmax2
};


struct NVGLUframebuffer;
struct audioclip_texture_t {
	ivec2 pos;
	ivec2 startOffset;
	ivec2 size;
	double sampleBegin;
	double sampleBeginOffset;
	double sampleEnd;
	double samplesPerPx = 0;
	int quality = 1;
	int scale = 1;
	float scaleX = 1.0f;
	float linewidth = 1.0f;
	SampleMethod method = SampleMethod::sample_straight;
	int audioId = -1;
};
inline bool operator==(const audioclip_texture_t& lhs, const audioclip_texture_t& rhs){
	return lhs.pos == rhs.pos && lhs.startOffset == rhs.startOffset &&
			lhs.size == rhs.size && lhs.sampleBegin == rhs.sampleBegin &&
			lhs.sampleBeginOffset == rhs.sampleBeginOffset && lhs.sampleEnd == rhs.sampleEnd &&
			lhs.samplesPerPx == rhs.samplesPerPx &&
			lhs.audioId == rhs.audioId && lhs.quality == rhs.quality && lhs.method == rhs.method;
}
inline bool operator!=(const audioclip_texture_t& lhs, const audioclip_texture_t& rhs){return !operator==(lhs,rhs);}

void downsample(float sampleRate, float* samplesIn, int len, std::vector<float>& samplesOut, int downSampleFactor);
void tesselateWaveform(audiosample_t* sample, float x, float y, audioclip_texture_t* waveform, SampleMethod method, std::vector<std::vector<glm::vec2>>& channels);

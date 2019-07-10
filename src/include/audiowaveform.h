#pragma once
#include <vector>
#include <stdint.h>
#include <vector>
#include "math/vec.h"
#include "audiosample.h"

enum SampleMethod {
	sample_straight, sample_sum, sample_minmax, sample_interp, sample_minmax2
};


struct NVGLUframebuffer;
struct audioclip_texture_t {
	ivec2 pos{0};
//	ivec2 startOffset{0};
	ivec2 size{0};
	int64_t sampleBegin{0};
	int64_t sampleBeginOffset{0};
	int64_t sampleEnd{0};
	double samplesPerPx = 0;
	int quality = 1;
//	int scale = 1;
	float scaleX = 1.0f;
	float scaleY = 1.0f;
	float linewidth = 1.0f;
	SampleMethod method = SampleMethod::sample_straight;
	int audioId = -1;
	bool clipped = false;
};
inline bool isEqualWaveform(const audioclip_texture_t& lhs, const audioclip_texture_t& rhs){
	return (lhs.sampleBeginOffset - lhs.sampleBegin) == (rhs.sampleBeginOffset - rhs.sampleBegin) &&
			(lhs.sampleEnd - lhs.sampleBegin) == (rhs.sampleEnd - rhs.sampleBegin) &&
//			lhs.startOffset == rhs.startOffset &&
			lhs.size == rhs.size &&
			lhs.samplesPerPx == rhs.samplesPerPx &&
//			lhs.scale == rhs.scale &&
			lhs.scaleX == rhs.scaleX &&
			lhs.scaleY == rhs.scaleY &&
			lhs.audioId == rhs.audioId && lhs.quality == rhs.quality && lhs.method == rhs.method;
}
bool isEqualWaveform3(const audioclip_texture_t& lhs, const audioclip_texture_t& rhs);
bool isEqualWaveform2(const audioclip_texture_t& lhs, const audioclip_texture_t& rhs);


inline bool operator==(const audioclip_texture_t& lhs, const audioclip_texture_t& rhs){
	return lhs.pos == rhs.pos &&
//			lhs.startOffset == rhs.startOffset &&
			lhs.size == rhs.size && lhs.sampleBegin == rhs.sampleBegin &&
			lhs.sampleBeginOffset == rhs.sampleBeginOffset && lhs.sampleEnd == rhs.sampleEnd &&
			lhs.samplesPerPx == rhs.samplesPerPx &&
			lhs.audioId == rhs.audioId && lhs.quality == rhs.quality && lhs.method == rhs.method && lhs.clipped == rhs.clipped;
}
inline bool operator!=(const audioclip_texture_t& lhs, const audioclip_texture_t& rhs){return !operator==(lhs,rhs);}

void downsample(float sampleRate, float* samplesIn, int len, std::vector<float>& samplesOut, int downSampleFactor);
void tesselateWaveform(audiosample_t* sample, float x, float y, audioclip_texture_t* waveform, SampleMethod method, std::vector<std::vector<vec2>>& channels);

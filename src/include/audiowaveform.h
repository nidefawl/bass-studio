#pragma once
#include <vector>
#include <stdint.h>
#include <vector>
#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include "audiosample.h"
#include "audiowaveform.h"
using glm::ivec2;
using glm::ivec4;

enum SampleMethod {
	sample_peakdetect, sample_sum, sample_minmax, sample_interp, sample_minmax2
};


struct NVGLUframebuffer;
struct audiowaveform_t {
	ivec2 pos;
	ivec2 startOffset;
	ivec2 size;
	double sampleBegin;
	double sampleBeginOffset;
	double sampleEnd;
	double res = 0;
	int quality = 1;
	SampleMethod method = SampleMethod::sample_peakdetect;

	bool rendered = false;
	ivec2 renderedSize;

	int image = 0;
	int glTexture = 0;
	NVGLUframebuffer* fb = NULL;
};

void downsample(float sampleRate, float* samplesIn, int len, std::vector<float>& samplesOut, int downSampleFactor);
void renderWaveProcessed(audiosample_t* sample, float x, float y, audiowaveform_t* waveform, SampleMethod method, std::vector<std::vector<glm::vec2>>& channels);

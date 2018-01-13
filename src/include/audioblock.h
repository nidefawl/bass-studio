#pragma once
#include <stdint.h>
#include <memory.h>
#include <stdlib.h>
#include <assert.h>
#include "seq_math.h"

struct AudioBlock {
	const uint32_t channels;
	uint32_t samples;
	float** buf;
	AudioBlock(uint32_t _channels, uint32_t _samples)
		: channels(_channels), samples(0)
	{
		buf = new float*[_channels];
		for (uint32_t i = 0; i < _channels; i++) {
			buf[i] = NULL;
		}
		realloc(_samples);
	};
	~AudioBlock() {
		for (uint32_t i = 0; i < channels; i++) {
			if (buf[i]) {
				free(buf[i]);
			}
		}
		delete[] buf;
	};
	void clear() {
		for (uint32_t i = 0; i < channels; i++) {
			memset(buf[i], 0, samples * sizeof(float));
		}
	}
	void copyTo(float **outputs) {
		for (uint32_t i = 0; i < channels; i++) {
			memcpy(outputs[i], buf[i], samples * sizeof(float));
		}
	}
	void copyFrom(AudioBlock* src) {
		copyFrom(src->buf, src->samples, src->channels);
	}
	void copyFrom(float **srcBuf, uint32_t srcSamples, uint32_t srcChannels) {
		assert(srcSamples == samples);
		uint32_t nChannels = max(srcChannels, channels);
		uint32_t nSamples = min(srcSamples, samples);
		for (uint32_t i = 0; i < nChannels; i++) {
			uint32_t srcChannelIdx = min(srcChannels-1, i);
			uint32_t dstChannelIdx = min(channels-1, i);
			float* srcBufChannel = srcBuf[srcChannelIdx];
			float* dstBufChannel = buf[dstChannelIdx];
			//TODO: this does 2 copys to the same destination when going from stereo to mono (MIX FIRST)
			memcpy(dstBufChannel, srcBufChannel, nSamples * sizeof(float));
		}
	}
	void copyFromPosToPos(float **srcBuf, uint32_t offsetIn, uint32_t offsetOut, uint32_t srcSamples, uint32_t srcChannels) {
//		assert(srcSamples == samples);
		uint32_t nChannels = max(srcChannels, channels);
		uint32_t nSamples = min(srcSamples, samples);
		for (uint32_t i = 0; i < nChannels; i++) {
			uint32_t srcChannelIdx = min(srcChannels-1, i);
			uint32_t dstChannelIdx = min(channels-1, i);
			float* srcBufChannel = srcBuf[srcChannelIdx];
			float* dstBufChannel = buf[dstChannelIdx];
			//TODO: this does 2 copys to the same destination when going from stereo to mono (MIX FIRST)
			memcpy(dstBufChannel+offsetOut, srcBufChannel+offsetIn, nSamples * sizeof(float));
		}
	}
	void addFrom(AudioBlock* src) {
		addFrom(src->buf, src->samples, src->channels);
	}
	void addFrom(float **srcBuf, uint32_t srcSamples, uint32_t srcChannels) {
		assert(srcSamples == samples);
		assert(srcChannels == channels); //remove when adding sub-track mixers (between plugins)
		uint32_t nChannels = max(srcChannels, channels);
		for (uint32_t i = 0; i < nChannels; i++) {
			uint32_t srcChannelIdx = min(srcChannels-1, i);
			uint32_t dstChannelIdx = min(channels-1, i);
			float* srcBufChannel = srcBuf[srcChannelIdx];
			float* dstBufChannel = buf[dstChannelIdx];
			//TODO: this does 2 additions to the same destination when going from stereo to mono (MIX FIRST)
			for (int j = 0; j < samples; j++) {
				dstBufChannel[j] += srcBufChannel[j];
			}
		}
	}
	void realloc(uint32_t _samples) {
		if (samples < _samples) {
			for (uint32_t i = 0; i < channels; i++) {
				float* newBuf = (float*)calloc(_samples,sizeof(float));
				if (buf[i]) {
					memcpy(newBuf, buf[i], samples * sizeof(float));
					free(buf[i]);
				}
				buf[i] = newBuf;
			}
			samples = _samples;
		}
	}
};


struct DelayLine {
	AudioBlock block;
	int32_t blockOffset = 0;
	DelayLine(uint32_t _channels, uint32_t _samples)
	  : block(_channels, _samples)
	{ }
};

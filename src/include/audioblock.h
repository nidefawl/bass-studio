#pragma once
#include <stdint.h>
#include <memory.h>
#include <stdlib.h>
#include "assert_dbg.h"
#include "math/seq_math.h"
#include "mem.h"
#include "samplerate.h"

enum alloc_type {
	internal, external_channels_only, external_array
};
struct AudioBlock {
	const uint32_t channels{ 0 };
	uint32_t samples{ 0 };
	float** buf{ 0 };
	alloc_type allocType;
	AudioBlock(uint32_t _channels, uint32_t _samples)
		: channels(_channels), samples(0), buf(new float*[_channels]), allocType(alloc_type::internal)
	{
		for (uint32_t i = 0; i < _channels; i++) {
			buf[i] = NULL;
		}
		realloc(_samples);
	};
	AudioBlock(float** buf, uint32_t _channels, uint32_t _samples)
		: channels(_channels), samples(_samples), buf(buf), allocType(alloc_type::external_array)
	{
	};
	AudioBlock(const std::vector<float*>& channels, uint32_t _samples)
		: channels(channels.size()),  samples(_samples), buf(new float*[channels.size()]), allocType(alloc_type::external_channels_only)
	{
		memcpy(buf, channels.data(), channels.size()*sizeof(decltype(channels[0])));
		float** pBuf = buf;
		for (float* channel : channels) {
			*pBuf++ = channel;
		}
	};
	~AudioBlock() {
		if (allocType != alloc_type::external_array) {
			if (allocType == alloc_type::internal) {
				for (uint32_t i = 0; i < channels; i++) {
					if (buf[i]) {
						free(buf[i]);
					}
				}
			}
			delete[] buf;
		}
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
	void copyFrom(float **srcBuf, uint32_t srcSamples, uint32_t srcChannels, uint32_t channelOffset = 0) {
		dbgassert(srcSamples == samples);
		uint32_t nChannels = math::min(srcChannels, channels);
		uint32_t nSamples = math::min(srcSamples, samples);
		for (uint32_t i = 0; i < nChannels; i++) {
			uint32_t srcChannelIdx = math::min(srcChannels-1, i+channelOffset);
			uint32_t dstChannelIdx = math::min(channels-1, i);
			float* srcBufChannel = srcBuf[srcChannelIdx];
			float* dstBufChannel = buf[dstChannelIdx];
			memcpy(dstBufChannel, srcBufChannel, nSamples * sizeof(float));
		}
	}
	template<class T>
	void copyFrom(float **srcBuf, uint32_t srcSamples, uint32_t srcChannels, T getMappedSrcChannel) {
		dbgassert(srcSamples == samples);
		uint32_t nChannels = math::min(srcChannels, channels);
		uint32_t nSamples = math::min(srcSamples, samples);
		for (uint32_t i = 0; i < nChannels; i++) {
			uint32_t dstChannelIdx = math::min(channels-1, i);
			uint32_t srcChannelIdx = math::min(srcChannels-1, getMappedSrcChannel(dstChannelIdx, i));
			float* srcBufChannel = srcBuf[srcChannelIdx];
			float* dstBufChannel = buf[dstChannelIdx];
			memcpy(dstBufChannel, srcBufChannel, nSamples * sizeof(float));
		}
	}
	template<class T>
	void copyFrom(AudioBlock* src, T getMappedSrcChannel) {
		copyFrom(src->buf, src->samples, src->channels, getMappedSrcChannel);
	}
	void copyFromPosToPos(float **srcBuf, uint32_t offsetIn, uint32_t offsetOut, uint32_t srcSamples, uint32_t srcChannels) {
		assert(offsetIn >= 0);
		assert(offsetOut >= 0);
//		dbgassert(srcSamples == samples);
		uint32_t nChannels = math::max(srcChannels, channels);
		uint32_t nSamples = math::min(srcSamples, samples);
		for (uint32_t i = 0; i < nChannels; i++) {
			uint32_t srcChannelIdx = math::min(srcChannels-1, i);
			uint32_t dstChannelIdx = math::min(channels-1, i);
			const float* srcBufChannel = srcBuf[srcChannelIdx];
			float* dstBufChannel = buf[dstChannelIdx];
			//TODO: this does 2 copys to the same destination when going from stereo to mono (MIX FIRST)
			memcpy(dstBufChannel+offsetOut, srcBufChannel+offsetIn, nSamples * sizeof(float));
		}
	}
	void addFrom(AudioBlock* src, float gain) {
		addFrom(src->buf, src->samples, src->channels, gain);
	}
	void addFrom(float **srcBuf, uint32_t srcSamples, uint32_t srcChannels, float gain) {
		dbgassert(srcSamples == samples);
		dbgassert(srcChannels == channels); //remove when adding sub-track mixers (between plugins)
		uint32_t nChannels = math::max(srcChannels, channels);
		for (uint32_t i = 0; i < nChannels; i++) {
			uint32_t srcChannelIdx = math::min(srcChannels-1, i);
			uint32_t dstChannelIdx = math::min(channels-1, i);
			float* srcBufChannel = srcBuf[srcChannelIdx];
			float* dstBufChannel = buf[dstChannelIdx];
			//TODO: this does 2 additions to the same destination when going from stereo to mono (MIX FIRST)
			for (uint32_t j = 0; j < samples; j++) {
				dstBufChannel[j] += srcBufChannel[j] * gain;
			}
		}
	}
	void realloc(uint32_t _samples) {
		if (samples != _samples) {
			if (allocType == alloc_type::internal) {
				for (uint32_t i = 0; i < channels; i++) {
					float* newBuf = (float*)calloc(_samples,sizeof(float));
					if (!newBuf) {
						handleFailedAllocation(0x1000, _samples*sizeof(float));
					} else {
						memset(newBuf, 0, sizeof(float)*_samples);
						if (buf[i]) {
							memcpy(newBuf, buf[i], math::min(_samples, samples) * sizeof(float));
							free(buf[i]);
						}
					}
					buf[i] = newBuf;
				}
				samples = _samples;
			}
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
void delayAudio(DelayLine* delayLine, AudioBlock* input, AudioBlock* output, samplerate_t delay);

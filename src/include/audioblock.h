#pragma once
#include <stdint.h>
#include <memory.h>
#include <stdlib.h>
#include <atomic>
#include "assert_dbg.h"
#include "math/seq_math.h"
#include "mem.h"
#include "samplerate.h"

enum alloc_type {
	internal, external_channels_only, external_array
};
struct AudioBlock {
	enum mix_op : int32_t {
		MIX, ADD
	};
	static std::atomic<int32_t> instanceCount;
    uint32_t channels{ 0 };
	uint32_t samples{ 0 };
	float** buf{ 0 };
	alloc_type allocType;
	bool debug = false;
	AudioBlock() = delete;
	AudioBlock(const AudioBlock&) = delete;
	AudioBlock(AudioBlock&& other) {
		instanceCount++;
		std::swap(allocType, other.allocType);
		std::swap(channels, other.channels);
		std::swap(samples, other.samples);
		std::swap(buf, other.buf);
		std::swap(debug, other.debug);
	}
	AudioBlock& operator=(const AudioBlock&) = delete;
	AudioBlock& operator=(AudioBlock&& other) {
		//if (buf) {
		//	if (allocType != alloc_type::external_array) {
		//		if (allocType == alloc_type::internal) {
		//			for (uint32_t i = 0; i < channels; i++) {
		//				if (buf[i]) {
		//					delete[] buf[i];
		//					buf[i] = nullptr;
		//				}
		//			}
		//		}
		//		delete[] buf;
		//		buf = nullptr;
		//	}
		//}
		std::swap(allocType, other.allocType);
		std::swap(channels, other.channels);
		std::swap(samples, other.samples);
		std::swap(buf, other.buf);
		std::swap(debug, other.debug);
		return *this;
	};
	explicit AudioBlock(uint32_t _channels, uint32_t _samples, bool _bIsDebug = false)
		: channels(_channels), samples(0), buf(new float*[_channels]), allocType(alloc_type::internal), debug(_bIsDebug)
	{
		instanceCount++;
		dbgassert(channels);
		for (uint32_t i = 0; i < _channels; i++) {
			buf[i] = NULL;
		}
		realloc(_samples);
	};
	explicit AudioBlock(float** buf, uint32_t _channels, uint32_t _samples)
		: channels(_channels), samples(_samples), buf(buf), allocType(alloc_type::external_array)
	{
		instanceCount++;
		dbgassert(channels);
	};
	explicit AudioBlock(const std::vector<float*>& vecChannels, uint32_t _samples)
		: channels(vecChannels.size()),  samples(_samples), buf(new float*[vecChannels.size()]), allocType(alloc_type::external_channels_only)
	{
		instanceCount++;
		dbgassert(channels);
		memcpy(buf, vecChannels.data(), vecChannels.size()*sizeof(decltype(vecChannels[0])));
		float** pBuf = buf;
		for (float* channel : vecChannels) {
			dbgassert(*pBuf++ == channel);
		}
	};
	explicit AudioBlock(const AudioBlock& src, const int32_t channelOffset, const int32_t numChannels, const int32_t sampleOffset, const int32_t numSamples)
		: channels(numChannels), samples(numSamples), buf(new float*[numChannels]), allocType(alloc_type::external_channels_only)
	{
		instanceCount++;
		dbgassert(channels);
		dbgassert(samples);
		for (uint32_t i = 0; i < channels; i++) {
			dbgassert(src.buf[i]);
			buf[i] = src.buf[channelOffset + i] + sampleOffset;
		}
	};
	~AudioBlock() {
		instanceCount--;
		if (allocType != alloc_type::external_array) {
			if (allocType == alloc_type::internal) {
				for (uint32_t i = 0; i < channels; i++) {
					if (buf[i]) {
						delete[] buf[i];
					}
				}
			}
			delete[] buf;
		}
	};
	AudioBlock getOffsetBlock(const int32_t sampleOffset) const
	{
		return AudioBlock(*this, 0, this->channels, sampleOffset, this->samples - sampleOffset);
	};
	AudioBlock SubBlock(const int32_t channelOffset, const int32_t numChannels, const int32_t sampleOffset, const int32_t numSamples) const
	{
		return AudioBlock(*this, channelOffset, numChannels, sampleOffset, numSamples);
	};
	void clear() {
		for (uint32_t i = 0; i < channels; i++) {
			memset(buf[i], 0, samples * sizeof(float));
		}
	}
	void shiftBegin(int32_t numSamples) {
		dbgassert(numSamples < 0 || samples > static_cast<uint32_t>(numSamples));
		for (uint32_t i = 0; i < channels; i++) {
			buf[i] += numSamples;
		}
		samples -= numSamples;
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
	void addFromOp(AudioBlock* src, const mix_op op, float gain) {
		addFromOp(src->buf, src->samples, src->channels, op, gain);
	}
	void addFromOp(float **srcBuf, const uint32_t srcSamples, const uint32_t srcChannels, const mix_op op, float gain) {
		dbgassert(srcSamples <= samples);
//		dbgassert(srcChannels == channels); //remove when adding sub-track mixers (between plugins)
		uint32_t nChannels = math::max(srcChannels, channels);
		uint32_t nSamples = math::min(srcSamples, samples);
		float srcGain = 1.0f;
		if (srcChannels > channels) {
			if (srcChannels == 0 && channels == 1) {
				srcGain = 0.5f; // mix the same way as multiple stereo channels
			} else if (srcChannels == 2 && channels == 1) {
				srcGain = 0.5f;
			} else {
				dbgassert(0&&"conversion not implemented");
			}
		}
		bool bdbgProcessed = false;
		for (uint32_t i = 0; i < nChannels; i++) {
			uint32_t srcChannelIdx = math::min(srcChannels-1, i);
			uint32_t dstChannelIdx = math::min(channels-1, i);
			float* srcBufChannel = srcBuf[srcChannelIdx];
			float* dstBufChannel = buf[dstChannelIdx];
			for (uint32_t j = 0; j < nSamples; j++) {
				const float fSrc = (srcBufChannel[j] * srcGain * gain);
				const float fDst = dstBufChannel[j] * (op == MIX ? 1.0f-gain : 1.0f);
				dstBufChannel[j] = fSrc + fDst;
				bdbgProcessed = true;
			}
		}
		dbgassert(bdbgProcessed);
	}
	void fillNoise(uint32_t seed);
	void realloc(uint32_t _samples);
};


struct DelayLine {
	AudioBlock block;
	int32_t blockOffset = 0;
	static std::atomic<int32_t> instanceCount;
	DelayLine(uint32_t _channels, uint32_t _samples)
	  : block(_channels, _samples)
	{
		block.debug=true;
		instanceCount++;
	}
	DelayLine(const DelayLine& other)
	  : block(other.block.channels, other.block.samples)
	{
		block.debug=true;
		instanceCount++;
	}
	DelayLine() = delete;
	DelayLine(DelayLine&& other) = delete;
	DelayLine& operator=(const DelayLine& other) = delete;
	DelayLine& operator=(DelayLine&& other) = delete;
	~DelayLine() {
		instanceCount--;
	}
};
void delayAudio(DelayLine* delayLine, AudioBlock* input, AudioBlock* output, samplerate_t delay);

#pragma once
#include "math/seq_math.h"
#include "str_util.h"
#include "seq_util.h"
#include "seq_time.h"
#include "dsp_util.h"
#include "samplerate.h"
#include "audioblock.h"
#include "audiobuffer.h"

#include "audio_host.h"
#include "midi_host.h"
#include "midi-defs.h"
#include "midi-msg.h"

#include <stdint.h>
#include "assert_dbg.h"

#include <deque>
#include <soxr.h>
//#define RESAMPLER_H_ENABLE_BUFFER_CHECKS


struct oversample_config_t {
	uint32_t inputSampleRate = 0;
	uint32_t outputSampleRate = 0;
	uint32_t numChannels = 0;
	uint32_t numSamplesInput = 0;
	uint32_t numSamplesResampled = 0;
	void setInputLength(uint32_t numSamples) {
		numSamplesInput = numSamples;
		dbgassert(FitsTypeRange<uint32_t>((int64_t)numSamplesInput * (int64_t)outputSampleRate / (double)inputSampleRate + .5));
		numSamplesResampled = (uint32_t) ((int64_t)numSamplesInput * (int64_t)outputSampleRate / (double)inputSampleRate + .5);
	}
};
struct oversampler_t : public oversample_config_t {
	std::vector<float*> channelPtrsOut;
	std::vector<float*> channelPtrsIn;
	soxr_t soxr = 0;
	soxr_error_t soxrError = 0;
	oversampler_t(oversample_config_t cfg) {
		*static_cast<oversample_config_t*>(this) = cfg;
		channelPtrsIn.resize(numChannels);
		channelPtrsOut.resize(numChannels);
		for (uint32_t i = 0; i < numChannels; i++) {
			channelPtrsIn[i] = nullptr;
			channelPtrsOut[i] = nullptr;
		}

		soxr_quality_spec_t q_spec = soxr_quality_spec(0, 0);
		soxr_io_spec_t io_spec = soxr_io_spec(SOXR_FLOAT32_S, SOXR_FLOAT32_S);
		soxr_runtime_spec_t const runtime_spec = soxr_runtime_spec(0);

		soxr = soxr_create((double)inputSampleRate, (double)outputSampleRate, numChannels, &soxrError, &io_spec, &q_spec, &runtime_spec);
		if (!!soxrError) {
			log_printf("soxr_create failed: %d %s\n", soxrError, soxr_strerror(soxrError));
		}


	}
	bool runResample(AudioBlock& srcBlock, AudioBlock& dstBlock, uint32_t& nOutputProcessed) {
		dbgassert(srcBlock.samples == this->numSamplesInput);
		dbgassert(srcBlock.channels >= this->numChannels);
		dbgassert(dstBlock.samples >= this->numSamplesResampled);
		dbgassert(dstBlock.channels >= this->numChannels);

		for (uint32_t i = 0; i < numChannels; i++) {
			if (i < srcBlock.channels) {
				channelPtrsIn[i] = srcBlock.buf[i];
			} else {
				channelPtrsIn[i] = nullptr;
			}
			if (i < dstBlock.channels) {
				channelPtrsOut[i] = dstBlock.buf[i];
			} else {
				channelPtrsOut[i] = nullptr;
			}
		}
		if (soxr) {
			size_t inputProcessed = 0;
			size_t outputProcessed = 0;
			soxrError = soxr_process(soxr, channelPtrsIn.data(), numSamplesInput, NULL, channelPtrsOut.data(), numSamplesResampled, &outputProcessed);
			if (!soxrError) {
				nOutputProcessed = static_cast<uint32_t>(outputProcessed);
				return true;
			} else {

				log_printf("soxr_process failed: %d %s\n", soxrError, soxr_strerror(soxrError));
			}
		}
		return false;
	}
	~oversampler_t() {
		soxr_delete(soxr);
	}
};
#define resampler_buffer_block_cnt 32
struct resampler_t {
	const uint32_t idx;
	const uint32_t numChannels;
	oversampler_t resampler;
	sampleformat_t in;
	sampleformat_t out;
	AudioBlock bufScratch;
	struct buf_t {
		AudioBlock* block{ nullptr };
		uint32_t samplesAvail{ 0 };
		uint32_t readOffset{ 0 };
		bool inUse{ false };
	};
	uint32_t numSamplesQueued = 0;
	std::vector<buf_t*> outputBuffers;
	std::deque<buf_t*> outputQueue;
	resampler_t(const uint32_t _idx, sampleformat_t _in, sampleformat_t _out, oversample_config_t config) :
		idx(_idx), numChannels(config.numChannels), resampler(config), in(_in), out(_out), bufScratch(config.numChannels, _in.blockSize) {
	}
	~resampler_t() {
		for (auto& b : outputBuffers) {
			delete b->block;
			delete b;
		}
	}
	buf_t* getFreeOutputBuffer() {
		for (buf_t* b : outputBuffers) {
			if (!b->inUse) {
				return b;
			}
		}
		outputBuffers.push_back(new buf_t{ new AudioBlock(numChannels, resampler.numSamplesResampled), false });
		log_printf("Allocate new output buffer, total %d buffers\n", outputBuffers.size());
		return outputBuffers.back();
	}

	bool push(AudioBlock& block) {
		if (outputQueue.size() > 32) {
			log_printf("Output queue is not processed, flushing %d output buffers\n", outputBuffers.size());
			releaseBuffers();
		}
		buf_t* buf = getFreeOutputBuffer();
		buf->inUse = true;
		//TODO: avoid this copy step by setting the resamplers channel count equal to the external input/output channel count
		bufScratch.copyFrom(&block);
		uint32_t nOutputProcessed = 0;
		if (!resampler.runResample(bufScratch, *buf->block, nOutputProcessed)) {
			return false;
		}
		buf->samplesAvail = nOutputProcessed;
		numSamplesQueued += nOutputProcessed;
		outputQueue.push_back(buf);
		return true;
	}
	AudioBlock pop() {

#ifdef RESAMPLER_H_ENABLE_BUFFER_CHECKS
		dbgassert(outputQueue.size() > 0);
		uint32_t numSamplesBegin = getNumSamplesOutputBuffer();
		dbgassert(numSamplesBegin >= out.blockSize);
		dbgassert(numSamplesBegin == numSamplesQueued);
#endif

		AudioBlock blockOut(numChannels, out.blockSize);
		uint32_t writeOffset = 0;

		while (writeOffset < out.blockSize) {
			buf_t* b = outputQueue.front();
			auto* ptrBlockResampled = b->block;
			auto maxCopy = math::min<uint32_t>(b->samplesAvail - b->readOffset, blockOut.samples - writeOffset);

			auto srcBlock = ptrBlockResampled->SubChannelsSamplesBlock(0, ptrBlockResampled->channels, b->readOffset, maxCopy);
			blockOut.SubChannelsSamplesBlock(0, numChannels, writeOffset, maxCopy).addFromOp(&srcBlock, AudioBlock::mix_op::MIX, 1.0f);

			b->readOffset += maxCopy;
			writeOffset += maxCopy;
			numSamplesQueued-= maxCopy;
			if (b->samplesAvail - b->readOffset <= 0) {
				b->inUse = false;
				b->samplesAvail = 0;
				b->readOffset = 0;
				outputQueue.pop_front();
				// assertion that we filled blockOut fully or there is more readable in queue
				dbgassert(writeOffset == out.blockSize || outputQueue.size());
			} else {
				// assert that we filled blockOut fully
				dbgassert(writeOffset == out.blockSize);
			}

		}

#ifdef RESAMPLER_H_ENABLE_BUFFER_CHECKS
		uint32_t numSamplesEnd = getNumSamplesOutputBuffer();
		dbgassert(numSamplesEnd < numSamplesBegin);
		dbgassert(numSamplesEnd == numSamplesQueued);
#endif

		return blockOut;
	}
	uint32_t getNumSamplesOutputBuffer() {
#ifdef RESAMPLER_H_ENABLE_BUFFER_CHECKS
		uint32_t numSamples = 0;

		for (buf_t* b : outputQueue) {
			numSamples += b->samplesAvail - b->readOffset;
		}
		dbgassert(numSamples == numSamplesQueued);
#endif
		return numSamplesQueued;
	}
	uint32_t numBlocksToPop() {
		uint32_t numSamples = getNumSamplesOutputBuffer();
		uint32_t numBlocks = numSamples / out.blockSize;
#ifdef RESAMPLER_H_ENABLE_BUFFER_CHECKS
		dbgassert(numBlocks == 0 || outputQueue.size() > 0);
#endif
		return numBlocks;
	}
	void releaseBuffers() {
		for (buf_t* b : outputQueue) {
			b->readOffset = 0;
			b->samplesAvail = 0;
			b->inUse = false;
		}
		numSamplesQueued = 0;
		outputQueue.clear();
	}
};

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
//		std::vector<std::vector<float>> dataIn;
//		std::vector<std::vector<float>> dataOut;
	std::vector<float*> channelPtrsOut;
	std::vector<float*> channelPtrsIn;
	soxr_t soxr = 0;
	soxr_error_t soxrError = 0;
	oversampler_t(oversample_config_t cfg) {
		*static_cast<oversample_config_t*>(this) = cfg;
//			dataIn.resize(numChannels);
//			dataOut.resize(numChannels);
		channelPtrsIn.resize(numChannels);
		channelPtrsOut.resize(numChannels);
//			float* dataInput = new float[numSamplesInput];
//			float* dataOutput = new float[numSamplesResampled];
//			for (int i = 0; i < numChannels; i++) {
//				dataIn[i].clear();
//				dataIn[i].insert(dataIn[i].begin(), dataInput, dataInput+numSamplesInput);
//				dataOut[i].clear();
//				dataOut[i].insert(dataOut[i].begin(), dataOutput, dataOutput+numSamplesResampled);
//			}
//			for (int i = 0; i < numChannels; i++) {
//				channelPtrsIn[i] = dataIn[i].data();
//				channelPtrsOut[i] = dataOut[i].data();
//			}
//			delete [] dataInput;
//			delete [] dataOutput;
		for (uint32_t i = 0; i < numChannels; i++) {
			channelPtrsIn[i] = nullptr;
			channelPtrsOut[i] = nullptr;
		}

		soxr_quality_spec_t q_spec = soxr_quality_spec(0, 0);
		soxr_io_spec_t io_spec = soxr_io_spec(SOXR_FLOAT32_S, SOXR_FLOAT32_S);
		soxr_runtime_spec_t const runtime_spec = soxr_runtime_spec(0);

//			my_printf("soxr_oneshot from %d to %d, samples %d -> %d, channels %d\n", wav.sampleRate, this->samplerate, wav.totalSampleCount, olen, wav.channels);
//			my_printf("pSamples.size %d\n", pSamples.size());
//			my_printf("pSamples2.size %d\n", pSamples2.size());

		soxr = soxr_create((double)inputSampleRate, (double)outputSampleRate, numChannels, &soxrError, &io_spec, &q_spec, &runtime_spec);


	}
	void runResample(AudioBlock& srcBlock, AudioBlock& dstBlock) {
		dbgassert(srcBlock.samples == this->numSamplesInput);
		dbgassert(srcBlock.channels <= this->numChannels);
		dbgassert(dstBlock.samples >= this->numSamplesResampled);
		dbgassert(dstBlock.channels <= this->numChannels);

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
		if (!!soxrError) {
			my_printf("soxr_create failed: %d %s\n", soxrError, soxr_strerror(soxrError));
		} else {
			size_t offset = 0;
			soxrError = soxr_process(soxr, channelPtrsIn.data(), numSamplesInput, NULL, channelPtrsOut.data(), numSamplesResampled, &offset);
//				my_printf("offset %d, numSamplesInput: %d\n", offset, numSamplesInput);


			if (!!soxrError) {
				my_printf("soxr_process failed: %d %s\n", soxrError, soxr_strerror(soxrError));
			} else {
//					my_printf("soxr_process success %d\n", error);
			}
		}
	}
	~oversampler_t() {
//			my_printf("%-26s\n", soxr_strerror(error));
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
		bool inUse{ false };
	};
	uint32_t readOffset = 0;
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
		return outputBuffers.back();
	}

	bool push(AudioBlock& block) {
		if (outputQueue.size() > 16) {
			releaseBuffers();
		}
		buf_t* buf = getFreeOutputBuffer();
		buf->inUse = true;
		bufScratch.copyFrom(&block);
		resampler.runResample(bufScratch, *buf->block);
		outputQueue.push_back(buf);
		return true;
	}
	AudioBlock pop() {
		dbgassert(outputQueue.size() > 0);
		AudioBlock blockOut(numChannels, out.blockSize);
		uint32_t writeOffset = 0;
		uint32_t numSamplesBegin = getNumSamplesOutputBuffer();
		dbgassert(numSamplesBegin >= out.blockSize);
		while (writeOffset < out.blockSize) {
			buf_t* b = outputQueue.front();
			auto* ptrBlockResampled = b->block;
			auto maxCopy = math::min<uint32_t>(resampler.numSamplesResampled-readOffset, blockOut.samples-writeOffset);
			//b.block->fillNoise(nNoise++);
			auto srcBlock = ptrBlockResampled->SubBlock(0, ptrBlockResampled->channels, readOffset, maxCopy);
			blockOut.SubBlock(0, numChannels, writeOffset, maxCopy)
					.addFromOp(&srcBlock, AudioBlock::mix_op::MIX, 1.0f);
			readOffset = (readOffset + maxCopy) % ptrBlockResampled->samples;
			writeOffset += maxCopy;
			if (readOffset > 0) {
				dbgassert(writeOffset == out.blockSize);
				break;
			}
			b->inUse = false;
			outputQueue.pop_front();
		}
		uint32_t numSamplesEnd = getNumSamplesOutputBuffer();
		dbgassert(numSamplesEnd < numSamplesBegin);

		return blockOut;
	}
	uint32_t getNumSamplesOutputBuffer() {
		uint32_t numSamples;
		if (readOffset) {
			dbgassert(outputQueue.size() > 0);
			buf_t* b = outputQueue.front();
			numSamples = b->block->samples - readOffset;
			numSamples += (static_cast<uint32_t>(outputQueue.size()) - 1) * resampler.numSamplesResampled;
		}
		else {
			numSamples = static_cast<uint32_t>(outputQueue.size()) * resampler.numSamplesResampled;
		}
		return numSamples;
	}
	uint32_t numBlocksToPop() {
		uint32_t numSamples = getNumSamplesOutputBuffer();
		uint32_t numBlocks = numSamples / out.blockSize;
		dbgassert(numBlocks == 0 || outputQueue.size() > 0);
		return numBlocks;
	}
	void releaseBuffers() {
		readOffset = 0;
		for (buf_t* b : outputQueue) {
			b->inUse = false;
		}
		outputQueue.clear();
	}
};

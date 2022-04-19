#pragma once
#include "logging.h"
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

#include "types.h"
#include "assert_dbg.h"

#include <deque>
#include <soxr.h>


//#define RESAMPLER_H_ENABLE_BUFFER_CHECKS

struct oversample_config_t {
    samplerate_t inputSampleRate     = 0;
    samplerate_t outputSampleRate    = 0;
    channelnum_t numChannels         = 0;
    samplecount_t numSamplesInput     = 0;
    samplecount_t numSamplesResampled = 0;
    void setInputLength(uint32_t numSamples) {
        numSamplesInput = numSamples;
        numSamplesResampled = (samplecount_t) ((int64_t) numSamplesInput * (int64_t) outputSampleRate / (double) inputSampleRate + .5);
        dbgassert(numSamplesResampled > 0);
    }
};

struct oversampler_t : public oversample_config_t {
    std::vector<float*> channelPtrsOut;
    std::vector<float*> channelPtrsIn;
    soxr_t soxr            = nullptr;
    soxr_error_t soxrError = nullptr;
    explicit oversampler_t(oversample_config_t cfg) {
        *static_cast<oversample_config_t*>(this) = cfg;
        channelPtrsIn.resize(numChannels);
        channelPtrsOut.resize(numChannels);
        for (uint32_t i = 0; i < numChannels; i++) {
            channelPtrsIn[i]  = nullptr;
            channelPtrsOut[i] = nullptr;
        }

        soxr_quality_spec_t q_spec             = soxr_quality_spec(0, 0);
        soxr_io_spec_t io_spec                 = soxr_io_spec(SOXR_FLOAT32_S, SOXR_FLOAT32_S);
        soxr_runtime_spec_t const runtime_spec = soxr_runtime_spec(0);

        soxr = soxr_create((double) inputSampleRate, (double) outputSampleRate, numChannels, &soxrError, &io_spec, &q_spec, &runtime_spec);
        if (!!soxrError) {
            log_lf(Log::L_ERROR, "soxr_create failed: %s\n", soxr_strerror(soxrError));
        }
    }
    bool runResample(AudioBlock& srcBlock, AudioBlock& dstBlock, uint32_t& nOutputProcessed) {
        dbgassert(srcBlock.samples == this->numSamplesInput);
        dbgassert(srcBlock.channels >= this->numChannels);
        dbgassert(dstBlock.samples >= this->numSamplesResampled);
        dbgassert(dstBlock.channels >= this->numChannels);

        for (channelnum_t i = 0; i < numChannels; i++) {
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
            size_t outputProcessed = 0;
            soxrError = soxr_process(soxr, channelPtrsIn.data(), numSamplesInput, nullptr, channelPtrsOut.data(), numSamplesResampled, &outputProcessed);
            if (!soxrError) {
                nOutputProcessed = static_cast<uint32_t>(outputProcessed);
                return outputProcessed > 0;
            } 
            log_lf(Log::L_ERROR, "soxr_process failed: %s\n", soxr_strerror(soxrError));
        }
        return false;
    }
    ~oversampler_t() {
        soxr_delete(soxr);
    }
};

struct resampler_t {
    AudioBlock bufScratch;
    oversampler_t resampler;
    sampleformat_t in;
    sampleformat_t out;
    const uint32_t idx;
    const channelnum_t numChannels;
    struct buf_t {
        AudioBlock* block{ nullptr };
        samplecount_t samplesAvail{ 0 };
        samplecount_t readOffset{ 0 };
        bool inUse{ false };
    };
    samplecount_t numSamplesQueued = 0;
    std::vector<buf_t*> outputBuffers;
    std::deque<buf_t*> outputQueue;
    resampler_t(const uint32_t _idx, sampleformat_t _in, sampleformat_t _out, oversample_config_t config)
        : bufScratch(config.numChannels, _in.blockSize),
          resampler(config),
          in(_in),
          out(_out),
          idx(_idx),
          numChannels(config.numChannels)
    {
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
        outputBuffers.push_back(new buf_t{ new AudioBlock(numChannels, resampler.numSamplesResampled), 0, 0, false });
        if (outputBuffers.size() % 128 == 0)
            log_printf("Allocate new output buffer, total %zu buffers\n", outputBuffers.size());
        return outputBuffers.back();
    }

    bool push(AudioBlock& block) {
        if (numSamplesQueued > out.blockSize * 32) {
            log_lf(Log::L_WARN, "Output queue is not processed, flushing %zd samples. %zu output buffers\n", numSamplesQueued, outputBuffers.size());
            releaseBuffers();
        }
        buf_t* buf = getFreeOutputBuffer();
        //TODO: avoid this copy step by setting the resamplers channel count equal to the external input/output channel count

        uint32_t nOutputProcessed = 0;
#ifdef RESAMPLER_H_ENABLE_BUFFER_CHECKS
        for (buf_t* b : outputBuffers) {
            dbgassert(b->inUse == (b->samplesAvail > 0));
        }
#endif

        if (numChannels == 0) {
            nOutputProcessed = resampler.numSamplesResampled;
        } else {
            bufScratch.copyFrom(&block);
            if (!resampler.runResample(bufScratch, *buf->block, nOutputProcessed)) {
                return false;
            }
        }
        dbgassert(nOutputProcessed);
        buf->inUse        = true;
        buf->samplesAvail = nOutputProcessed;
        numSamplesQueued += nOutputProcessed;
        outputQueue.push_back(buf);
#ifdef RESAMPLER_H_ENABLE_BUFFER_CHECKS
        for (buf_t* b : outputBuffers) {
            dbgassert(b->inUse == (b->samplesAvail > 0));
        }
#endif
        return true;
    }
    AudioBlock pop() {

#ifdef RESAMPLER_H_ENABLE_BUFFER_CHECKS
        dbgassert(outputQueue.size() > 0);
        uint32_t numSamplesBegin = getNumSamplesOutputBuffer();
        dbgassert(numSamplesBegin >= out.blockSize);
        dbgassert(numSamplesBegin == numSamplesQueued);
        for (buf_t* b : outputBuffers) {
            dbgassert(b->inUse == (b->samplesAvail > 0));
        }
#endif

        AudioBlock blockOut(numChannels, out.blockSize);
        uint32_t writeOffset = 0;

        while (writeOffset < out.blockSize) {
            buf_t* b                = outputQueue.front();
            auto* ptrBlockResampled = b->block;
            auto maxCopy            = math::min<samplecount_t>(b->samplesAvail - b->readOffset, blockOut.samples - writeOffset);

            auto srcBlock = ptrBlockResampled->SubChannelsSamplesBlock(0, ptrBlockResampled->channels, b->readOffset, maxCopy);
            blockOut.SubChannelsSamplesBlock(0, numChannels, writeOffset, maxCopy).addFromOp(&srcBlock, AudioBlock::mix_op::MIX, 1.0f);

            b->readOffset += maxCopy;
            writeOffset += maxCopy;
            numSamplesQueued -= maxCopy;
            if (b->samplesAvail - b->readOffset <= 0) {
                b->inUse        = false;
                b->samplesAvail = 0;
                b->readOffset   = 0;
                outputQueue.pop_front();
                // assertion that we filled blockOut fully or there is more readable in queue
                dbgassert(writeOffset == out.blockSize || !outputQueue.empty());
            } else {
                // assert that we filled blockOut fully
                dbgassert(writeOffset == out.blockSize);
            }
        }

#ifdef RESAMPLER_H_ENABLE_BUFFER_CHECKS
        uint32_t numSamplesEnd = getNumSamplesOutputBuffer();
        dbgassert(numSamplesEnd < numSamplesBegin);
        dbgassert(numSamplesEnd == numSamplesQueued);
        for (buf_t* b : outputBuffers) {
            dbgassert(b->inUse == (b->samplesAvail > 0));
        }
#endif

        return blockOut;
    }
    samplecount_t getNumSamplesOutputBuffer() const {
#ifdef RESAMPLER_H_ENABLE_BUFFER_CHECKS
        uint32_t numSamples = 0;

        for (buf_t* b : outputQueue) {
            numSamples += b->samplesAvail - b->readOffset;
        }
        dbgassert(numSamples == numSamplesQueued);
#endif
        return numSamplesQueued;
    }
    uint32_t numBlocksToPop() const {
        auto numSamples = getNumSamplesOutputBuffer();
        auto numBlocks  = numSamples / out.blockSize;
#ifdef RESAMPLER_H_ENABLE_BUFFER_CHECKS
        dbgassert(numBlocks == 0 || outputQueue.size() > 0);
#endif
        return numBlocks;
    }
    void releaseBuffers() {
        for (buf_t* b : outputQueue) {
            b->readOffset   = 0;
            b->samplesAvail = 0;
            b->inUse        = false;
        }
        numSamplesQueued = 0;
        outputQueue.clear();
    }
};

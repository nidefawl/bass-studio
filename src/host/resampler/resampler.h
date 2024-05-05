#pragma once
#include "logging.h"
#include "math/seq_math.h"
#include "str_util.h"
#include "seq_util.h"
#include "seq_time.h"
#include "dsp_util.h"
#include "samplerate.h"
#include "host/audiobuffer/audioblock.h"
#include "host/audiobuffer/audiobuffer.h"

#include "host/audiohost/audio_host.h"
#include "host/midihost/midi_host.h"
#include "midi-defs.h"
#include "midi-event.h"

#include "types.h"
#include "assert_dbg.h"

#include <deque>
#include <soxr.h>


// #define RESAMPLER_H_ENABLE_BUFFER_CHECKS

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

struct oversampler_t final : public oversample_config_t {
    std::vector<float*> channelPtrsOut;
    std::vector<float*> channelPtrsIn;
    soxr_t soxr            = nullptr;
    soxr_error_t soxrError = nullptr;
    double sampleDelay = 0.0;
    explicit oversampler_t(oversample_config_t cfg) {
        *static_cast<oversample_config_t*>(this) = cfg;
        openResampler();
    }

    void openResampler() {
        closeResampler();
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
    void closeResampler() {
        if (soxr) {
            soxr_delete(soxr);
            soxr = nullptr;
        }
    }
    void resetResampler() {
        if (soxr) {
            soxr_clear(soxr);
        }
    }
    double getResamplerDelay() {
        return sampleDelay;
    }
    bool runResample(AudioBlock& srcBlock, AudioBlock& dstBlock, uint32_t& nOutputProcessed);
    ~oversampler_t() {
        closeResampler();
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
        AudioBufferTimeInfo timeInfo;
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
    samplecount_t getResamplerDelay() {
        return math::ceildS64(resampler.getResamplerDelay());
    }
    void resetResampler() {
        resampler.resetResampler();
        log_lf(Log::L_WARN, "Flushing %zd samples. %zu output buffers\n", numSamplesQueued, outputBuffers.size());
        releaseBuffers();
    }
    buf_t* getFreeOutputBuffer() {
        for (buf_t* b : outputBuffers) {
            if (!b->inUse) {
                return b;
            }
        }
        auto len = resampler.numSamplesResampled + 32;
        outputBuffers.push_back(new buf_t{ new AudioBlock(numChannels, len), {}, 0, 0, false });
        if (outputBuffers.size() % 128 == 0)
            log_printf("Allocate new output buffer, total %zu buffers\n", outputBuffers.size());
        return outputBuffers.back();
    }

    bool push(AudioBlock& block, AudioBufferTimeInfo& timeinfo) {
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
        buf->timeInfo = timeinfo;
        numSamplesQueued += nOutputProcessed;
        outputQueue.push_back(buf);
#ifdef RESAMPLER_H_ENABLE_BUFFER_CHECKS
        for (buf_t* b : outputBuffers) {
            dbgassert(b->inUse == (b->samplesAvail > 0));
        }
#endif
        return true;
    }
    void pop(AudioBufferTimeInfo& timeinfo, AudioBlock& blockOut) {

#ifdef RESAMPLER_H_ENABLE_BUFFER_CHECKS
        dbgassert(outputQueue.size() > 0);
        uint32_t numSamplesBegin = getNumSamplesOutputBuffer();
        dbgassert(numSamplesBegin >= out.blockSize);
        dbgassert(numSamplesBegin == numSamplesQueued);
        for (buf_t* b : outputBuffers) {
            dbgassert(b->inUse == (b->samplesAvail > 0));
        }
#endif
        if (blockOut.channels != numChannels || blockOut.samples != out.blockSize) {
            blockOut = AudioBlock(numChannels, out.blockSize);
        }
        uint32_t writeOffset = 0;

        while (writeOffset < out.blockSize) {
            buf_t* b                = outputQueue.front();
            auto* ptrBlockResampled = b->block;
            auto maxCopy            = math::min<samplecount_t>(b->samplesAvail - b->readOffset, blockOut.samples - writeOffset);

            auto srcBlock = ptrBlockResampled->SubChannelsSamplesBlock(0, ptrBlockResampled->channels, b->readOffset, maxCopy);
            blockOut.SubChannelsSamplesBlock(0, numChannels, writeOffset, maxCopy).addFromOp(&srcBlock, mix_op::MIX, 1.0f);
            timeinfo = b->timeInfo;

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

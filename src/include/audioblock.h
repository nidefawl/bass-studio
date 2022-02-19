#pragma once
#include <cstdint>
#include <memory.h>
#include <atomic>
#include "assert_dbg.h"
#include "math/seq_math.h"
#include "mem.h"
#include "samplerate.h"

enum alloc_type {
    internal,
    external_channels_only,
    external_array
};
struct DelayLine;
struct AudioBlock {
    enum mix_op : int32_t {
        MIX,
        ADD
    };
    static std::atomic<int32_t> instanceCstrd;
    static std::atomic<int32_t> numAllocs;
    static std::atomic<int32_t> instanceCount;
    static volatile bool recordAllocs;
    static void BeginTrace();
    static void EndTrace();

    uint32_t channels{};
    uint32_t samples{};
    float** buf{};
    alloc_type allocType = internal;
    bool debug           = false;


    AudioBlock()                  = delete;
    AudioBlock(const AudioBlock&) = delete;
    AudioBlock& operator=(const AudioBlock&) = delete;

    AudioBlock(AudioBlock&& other) noexcept {
        //instanceCount++;
        std::swap(allocType, other.allocType);
        std::swap(channels, other.channels);
        std::swap(samples, other.samples);
        std::swap(buf, other.buf);
        std::swap(debug, other.debug);
    }

    AudioBlock& operator=(AudioBlock&& other)  noexcept {
        std::swap(allocType, other.allocType);
        std::swap(channels, other.channels);
        std::swap(samples, other.samples);
        std::swap(buf, other.buf);
        std::swap(debug, other.debug);
        return *this;
    }

    explicit AudioBlock(uint32_t _channels, uint32_t _samples, bool _bIsDebug = false)
        : channels(_channels), samples(0), buf(new float*[_channels]), allocType(alloc_type::internal), debug(_bIsDebug) {
        instanceCount++;
        instanceCstrd++;
        for (uint32_t i = 0; i < _channels; i++) {
            buf[i] = nullptr;
        }
        realloc(_samples);
    }

    explicit AudioBlock(float** buf, uint32_t _channels, uint32_t _samples)
        : channels(_channels), samples(_samples), buf(buf), allocType(alloc_type::external_array) {
        instanceCount++;
        instanceCstrd++;
    }

    explicit AudioBlock(const std::vector<float*>& vecChannels, uint32_t _samples)
        : channels(static_cast<uint32_t>(vecChannels.size())), samples(_samples), buf(new float*[vecChannels.size()]), allocType(alloc_type::external_channels_only) {
        instanceCount++;
        instanceCstrd++;
        memcpy(buf, vecChannels.data(), vecChannels.size() * sizeof(decltype(vecChannels[0])));
        float** pBuf = buf;
        for (float* channel : vecChannels) {
            dbgassert(*pBuf++ == channel);
        }
    }

    explicit AudioBlock(const AudioBlock& src, const uint32_t channelOffset, const uint32_t numChannels, const uint32_t sampleOffset, const uint32_t numSamples)
        : channels(numChannels), samples(numSamples), buf(new float*[numChannels]), allocType(alloc_type::external_channels_only) {
        instanceCount++;
        instanceCstrd++;
        dbgassert(samples);
        for (uint32_t i = 0; i < channels; i++) {
            dbgassert(src.buf[i]);
            buf[i] = src.buf[channelOffset + i] + sampleOffset;
        }
    }

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
    }

    AudioBlock getOffsetBlock(const uint32_t sampleOffset) const {
        dbgassert(sampleOffset < this->samples);
        return AudioBlock(*this, 0, this->channels, sampleOffset, this->samples - sampleOffset);
    }

    AudioBlock SubChannelsSamplesBlock(const uint32_t channelOffset, const uint32_t numChannels, const uint32_t sampleOffset, const uint32_t numSamples) const {
        dbgassert(channelOffset + numChannels <= this->channels);
        dbgassert(sampleOffset + numSamples <= this->samples);
        return AudioBlock(*this, channelOffset, numChannels, sampleOffset, numSamples);
    }

    AudioBlock SubChannelsBlock(const uint32_t channelOffset, const uint32_t numChannels) const {
        dbgassert(channelOffset + numChannels <= this->channels);
        return AudioBlock(*this, channelOffset, numChannels, 0, this->samples);
    }

    void clear() {
        for (uint32_t i = 0; i < channels; i++) {
            memset(buf[i], 0, samples * sizeof(float));
        }
    }

    void shiftBegin(uint32_t numSamples) {
        dbgassert(samples > numSamples);
        for (uint32_t i = 0; i < channels; i++) {
            buf[i] += numSamples;
        }
        samples -= numSamples;
    }

    void copyTo(float** outputs) {
        for (uint32_t i = 0; i < channels; i++) {
            memcpy(outputs[i], buf[i], samples * sizeof(float));
        }
    }

    void copyFrom(AudioBlock* src) {
        copyFrom(src->buf, src->samples, src->channels);
    }

    void copyFrom(const float * const * const srcBuf, uint32_t srcSamples, uint32_t srcChannels, uint32_t channelOffset = 0) {
        dbgassert(srcSamples == samples);
        uint32_t nChannels = math::min(srcChannels, channels);
        uint32_t nSamples  = math::min(srcSamples, samples);
        for (uint32_t i = 0; i < nChannels; i++) {
            uint32_t srcChannelIdx = math::min(srcChannels - 1, i + channelOffset);
            uint32_t dstChannelIdx = math::min(channels - 1, i);
            auto   srcBufChannel   = srcBuf[srcChannelIdx];
            float* dstBufChannel   = buf[dstChannelIdx];
            memcpy(dstBufChannel, srcBufChannel, nSamples * sizeof(float));
        }
    }

    template<class T>
    void copyFrom(const float * const * const srcBuf, uint32_t srcSamples, uint32_t srcChannels, T getMappedSrcChannel) {
        dbgassert(srcSamples == samples);
        uint32_t nChannels = math::min(srcChannels, channels);
        uint32_t nSamples  = math::min(srcSamples, samples);
        for (uint32_t i = 0; i < nChannels; i++) {
            uint32_t dstChannelIdx = math::min(channels - 1, i);
            uint32_t srcChannelIdx = math::min(srcChannels - 1, getMappedSrcChannel(dstChannelIdx, i));
            auto   srcBufChannel   = srcBuf[srcChannelIdx];
            float* dstBufChannel   = buf[dstChannelIdx];
            memcpy(dstBufChannel, srcBufChannel, nSamples * sizeof(float));
        }
    }

    template<class T>
    void copyFrom(const AudioBlock* const src, T getMappedSrcChannel) {
        copyFrom(src->buf, src->samples, src->channels, getMappedSrcChannel);
    }

    void copyFromPosToPos(const float * const * const srcBuf, uint32_t offsetIn, uint32_t offsetOut, uint32_t len, uint32_t srcChannels) {
        dbgassert(srcChannels > 0);
        uint32_t nChannels = math::max(srcChannels, channels);
        uint32_t nSamples  = math::min(len, samples);
        dbgassert(offsetOut + nSamples <= samples);
        for (uint32_t i = 0; i < nChannels; i++) {
            uint32_t srcChannelIdx = math::min(srcChannels - 1, i);
            uint32_t dstChannelIdx = math::min(channels - 1, i);
            auto srcBufChannel     = srcBuf[srcChannelIdx];
            float* dstBufChannel   = buf[dstChannelIdx];
            //TODO: this does 2 copys to the same destination when going from stereo to mono (MIX FIRST)
            memcpy(dstBufChannel + offsetOut, srcBufChannel + offsetIn, nSamples * sizeof(float));
        }
    }

    void addFrom(AudioBlock* src, float gain) {
        addFrom(src->buf, src->samples, src->channels, gain);
    }

    void addFrom(const float * const * const srcBuf, uint32_t srcSamples, uint32_t srcChannels, float gain) {
        dbgassert(srcSamples == samples);
        dbgassert(srcChannels == channels);//remove when adding sub-track mixers (between plugins)
        uint32_t nChannels = math::max(srcChannels, channels);
        for (uint32_t i = 0; i < nChannels; i++) {
            uint32_t srcChannelIdx = math::min(srcChannels - 1, i);
            uint32_t dstChannelIdx = math::min(channels - 1, i);
            auto   srcBufChannel   = srcBuf[srcChannelIdx];
            float* dstBufChannel   = buf[dstChannelIdx];
            //TODO: this does 2 additions to the same destination when going from stereo to mono (MIX FIRST)
            for (uint32_t j = 0; j < samples; j++) {
                dstBufChannel[j] += srcBufChannel[j] * gain;
            }
        }
    }

    void addFromOp(AudioBlock* src, const mix_op op, float gain) {
        addFromOp(src->buf, src->samples, src->channels, op, gain);
    }

    void addFromOp(const float * const * const srcBuf, const uint32_t srcSamples, const uint32_t srcChannels, const mix_op op, float gain) {
        dbgassert(srcSamples <= samples);
        //dbgassert(srcChannels == channels);//remove when adding sub-track mixers (between plugins)
        uint32_t nChannels = math::min(srcChannels, channels);
        uint32_t nSamples  = math::min(srcSamples, samples);
        float srcGain      = 1.0f;
        //if (srcChannels > channels) {
        //    if (srcChannels == 2 && channels == 1) {
        //        srcGain = 0.5f;
        //    } else {
        //        dbgassert(0 && "conversion not implemented");
        //    }
        //}
        if (srcChannels == 2 && channels == 1) {
            srcGain   = 0.5f;
            nChannels = 2;
        }
        if (srcChannels == 1 && channels == 2) {
            nChannels = 2;
        }
        bool bdbgProcessed = false;
        for (uint32_t i = 0; i < nChannels; i++) {
            uint32_t srcChannelIdx = i % srcChannels;
            uint32_t dstChannelIdx = i % channels;
            auto   srcBufChannel   = srcBuf[srcChannelIdx];
            float* dstBufChannel   = buf[dstChannelIdx];
            for (uint32_t j = 0; j < nSamples; j++) {
                const float fSrc = (srcBufChannel[j] * srcGain * gain);
                const float fDst = dstBufChannel[j] * (op == MIX ? 1.0f - gain : 1.0f);
                dstBufChannel[j] = fSrc + fDst;
                bdbgProcessed    = true;
            }
        }
        dbgassert(bdbgProcessed);
    }

    void addFromDelayLineOp(DelayLine* delayLine, const samplerate_t delay, const mix_op op, float gain);

    void fillNoise(uint32_t seed);

    void realloc(uint32_t _samples);

};


struct DelayLine {
    AudioBlock block;
    int32_t writeOffset = 0;
    uint16_t blockSize = 0;
    static std::atomic<int32_t> instanceCount;
    DelayLine(uint32_t _channels, uint32_t _samples)
        : block(_channels, _samples) {
        block.debug = true;
        instanceCount++;
    }
    DelayLine(const DelayLine& other)
        : block(other.block.channels, other.block.samples) {
        block.debug = true;
        instanceCount++;
    }
    DelayLine()                  = delete;
    DelayLine(DelayLine&& other) = delete;
    DelayLine& operator=(const DelayLine& other) = delete;
    DelayLine& operator=(DelayLine&& other) = delete;
    void updateSize(uint16_t _blockSize, uint8_t _numChannels, samplerate_t _delay);
    ~DelayLine() {
        instanceCount--;
    }
};
void delayAudio(DelayLine* delayLine, AudioBlock* input, AudioBlock* output, samplerate_t delay);
void delayLineWrite(DelayLine* delayLine, AudioBlock* input, samplerate_t delay);

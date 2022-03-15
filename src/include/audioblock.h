#pragma once
#include "types.h"
#include <cwchar>
#include <memory.h>
#include <atomic>
#include <vector>
#include <array>
#include "assert_dbg.h"
#include "math/seq_math.h"
#include "mem.h"
#include "samplerate.h"
#include "types.h"

enum alloc_type {
    empty = 0,
    stack = 1,
    heap = 2,
    external = 4
};
class DelayLine;
struct alignas(16) AudioBlock {
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
    std::array<float*, 8> heapBuf{};

    float** buf{};
    samplecount_t samples{};
    channelnum_t channels{};
    alloc_type channelsAlloc = empty;
    alloc_type dataAlloc = empty;


    AudioBlock()                  = default;
    AudioBlock(const AudioBlock&) = delete;
    AudioBlock& operator=(const AudioBlock&) = delete;

    AudioBlock(AudioBlock&& other) noexcept {
        instanceCount++;
        *this = std::move(other);
    }

    AudioBlock& operator=(AudioBlock&& other) noexcept {
        channels = other.channels;
        samples = other.samples;
        if (other.channelsAlloc == stack) {
            memcpy(heapBuf.data(), other.heapBuf.data(), sizeof(float*) * heapBuf.size());
            buf = heapBuf.data();
        } else {
            buf = other.buf;
        }
        channelsAlloc = other.channelsAlloc;
        dataAlloc = other.dataAlloc;
        other.channelsAlloc = empty;
        other.dataAlloc = empty;
        other.buf = nullptr;
        return *this;
    }
    
    void allocChannelsArray() {
        if (channels <= heapBuf.size()) {
            channelsAlloc = alloc_type::stack; 
            buf = heapBuf.data();
        } else {
            channelsAlloc = alloc_type::heap; 
            buf = new float*[channels];
            memset(buf, 0, sizeof(float*) * channels);
        } 
    }

    explicit AudioBlock(channelnum_t _channels, samplecount_t _samples, bool _bIsDebug = false)
        : channels(_channels), dataAlloc(heap) {
        allocChannelsArray();
        instanceCount++;
        instanceCstrd++;
        realloc(_samples);
    }

    explicit AudioBlock(float** _buf, channelnum_t _channels, samplecount_t _samples)
        : buf(_buf),
        samples(_samples),
        channels(_channels),
        channelsAlloc(external),
        dataAlloc(external) {
        instanceCount++;
        instanceCstrd++;
    }

    explicit AudioBlock(const std::vector<float*>& vecChannels, samplecount_t _samples)
        : samples(_samples),
        channels(static_cast<channelnum_t>(vecChannels.size())),
        dataAlloc(external) 
    {
        allocChannelsArray();
        instanceCount++;
        instanceCstrd++;
        memcpy(buf, vecChannels.data(), vecChannels.size() * sizeof(decltype(vecChannels[0])));
        float** pBuf = buf;
        for (float* channel : vecChannels) {
            dbgassert(*pBuf++ == channel);
        }
    }

    explicit AudioBlock(const AudioBlock& src, const channelnum_t channelOffset, const channelnum_t numChannels, const samplecount_t sampleOffset, const samplecount_t numSamples)
        : samples(numSamples),
        channels(numChannels),
        dataAlloc(external) {
        allocChannelsArray();
        instanceCount++;
        instanceCstrd++;
        dbgassert(samples);
        for (channelnum_t i = 0; i < channels; i++) {
            dbgassert(src.buf[i]);
            buf[i] = src.buf[channelOffset + i] + sampleOffset;
        }
    }

    ~AudioBlock() {
        instanceCount--;
        if (dataAlloc == alloc_type::heap) {
            for (channelnum_t i = 0; i < channels; i++) {
                if (buf[i]) {
                    // delete[] buf[i];
                    aligned_free(buf[i]);
                }
            }
        }
        if (channelsAlloc == alloc_type::heap) {
            delete[] buf;
        }
    }

    AudioBlock getOffsetBlock(const samplecount_t sampleOffset) const {
        dbgassert(sampleOffset < this->samples);
        return AudioBlock(*this, 0, this->channels, sampleOffset, this->samples - sampleOffset);
    }

    AudioBlock SubChannelsSamplesBlock(const channelnum_t channelOffset, const channelnum_t numChannels, const samplecount_t sampleOffset, const samplecount_t numSamples) const {
        dbgassert(sampleOffset + numSamples <= this->samples);
        if (this->channels < numChannels && channelOffset == 0) {
            return AudioBlock(*this, 0, this->channels, sampleOffset, numSamples);
        }
        dbgassert(channelOffset + numChannels <= this->channels);
        return AudioBlock(*this, channelOffset, numChannels, sampleOffset, numSamples);
    }

    AudioBlock SubChannelsBlock(const channelnum_t channelOffset, const channelnum_t numChannels) const {
        if (this->channels < numChannels && channelOffset == 0) {
            return AudioBlock(*this, 0, this->channels, 0, this->samples);
        }
        dbgassert(channelOffset + numChannels <= this->channels);
        return AudioBlock(*this, channelOffset, numChannels, 0, this->samples);
    }

    void clear() {
        for (channelnum_t i = 0; i < channels; i++) {
            memset(buf[i], 0, samples * sizeof(float));
        }
    }

    void copyTo(float** outputs) {
        for (channelnum_t i = 0; i < channels; i++) {
            memcpy(outputs[i], buf[i], samples * sizeof(float));
        }
    }

    void copyFrom(AudioBlock* src) {
        copyFrom(src->buf, src->samples, src->channels);
    }

    void copyFrom(const float * const * const srcBuf, samplecount_t srcSamples, channelnum_t srcChannels, channelnum_t channelOffset = 0) {
        dbgassert(srcSamples == samples);
        const channelnum_t nChannels = math::min(srcChannels, channels);
        const samplecount_t nSamples  = math::min(srcSamples, samples);
        for (channelnum_t i = 0; i < nChannels; i++) {
            auto srcChannelIdx = math::min<channelnum_t>(srcChannels - 1, i + channelOffset);
            auto dstChannelIdx = math::min<channelnum_t>(channels - 1, i);
            auto   srcBufChannel   = srcBuf[srcChannelIdx];
            float* dstBufChannel   = buf[dstChannelIdx];
            memcpy(dstBufChannel, srcBufChannel, nSamples * sizeof(float));
        }
    }

    template<class T>
    void copyFrom(const float * const * const srcBuf, samplecount_t srcSamples, channelnum_t srcChannels, T getMappedSrcChannel) {
        dbgassert(srcSamples == samples);
        const channelnum_t nChannels = math::min(srcChannels, channels);
        const samplecount_t nSamples  = math::min(srcSamples, samples);
        for (channelnum_t i = 0; i < nChannels; i++) {
            auto dstChannelIdx = math::min<channelnum_t>(channels - 1, i);
            auto srcChannelIdx = math::min<channelnum_t>(srcChannels - 1, getMappedSrcChannel(dstChannelIdx, i));
            auto   srcBufChannel   = srcBuf[srcChannelIdx];
            float* dstBufChannel   = buf[dstChannelIdx];
            memcpy(dstBufChannel, srcBufChannel, nSamples * sizeof(float));
        }
    }

    template<class T>
    void copyFrom(const AudioBlock* const src, T getMappedSrcChannel) {
        copyFrom(src->buf, src->samples, src->channels, getMappedSrcChannel);
    }

    void copyFromPosToPos(const float * const * const srcBuf, samplecount_t offsetIn, samplecount_t offsetOut, samplecount_t len, channelnum_t srcChannels) {
        dbgassert(srcChannels > 0);
        const channelnum_t nChannels = math::max(srcChannels, channels);
        const samplecount_t nSamples  = math::min(len, samples);
        dbgassert(offsetOut + nSamples <= samples);
        for (channelnum_t i = 0; i < nChannels; i++) {
            auto srcChannelIdx = srcChannels < 1 ? 0 : math::min<channelnum_t>(srcChannels - 1, i);
            auto dstChannelIdx = channels < 1 ? 0 : math::min<channelnum_t>(channels - 1, i);
            auto srcBufChannel     = srcBuf[srcChannelIdx];
            float* dstBufChannel   = buf[dstChannelIdx];
            //TODO: this does 2 copys to the same destination when going from stereo to mono (MIX FIRST)
            memcpy(dstBufChannel + offsetOut, srcBufChannel + offsetIn, nSamples * sizeof(float));
        }
    }

    void addFrom(AudioBlock* src, float gain) {
        addFrom(src->buf, src->samples, src->channels, gain);
    }

    void addFrom(const float * const * const srcBuf, samplecount_t srcSamples, channelnum_t srcChannels, float gain) {
        dbgassert(srcSamples == samples);
        dbgassert(srcChannels == channels);//remove when adding sub-track mixers (between plugins)
        const channelnum_t nChannels = math::max(srcChannels, channels);
        for (channelnum_t i = 0; i < nChannels; i++) {
            auto srcChannelIdx = srcChannels < 1 ? 0 : math::min<channelnum_t>(srcChannels - 1, i);
            auto dstChannelIdx = channels < 1 ? 0 : math::min<channelnum_t>(channels - 1, i);
            auto   srcBufChannel   = srcBuf[srcChannelIdx];
            float* dstBufChannel   = buf[dstChannelIdx];
            //TODO: this does 2 additions to the same destination when going from stereo to mono (MIX FIRST)
            for (samplecount_t j = 0; j < samples; j++) {
                dstBufChannel[j] += srcBufChannel[j] * gain;
            }
        }
    }

    void addFromOp(AudioBlock* src, const mix_op op, float gain) {
        addFromOp(src->buf, src->samples, src->channels, op, gain);
    }

    void addFromOp(const float * const * const srcBuf, const samplecount_t srcSamples, const channelnum_t srcChannels, const mix_op op, float gain) {
        dbgassert(srcSamples <= samples);
        //dbgassert(srcChannels == channels);//remove when adding sub-track mixers (between plugins)
        const auto nSamples  = math::min<samplecount_t>(srcSamples, samples);
        auto nChannels = math::min<channelnum_t>(srcChannels, channels);
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
        // bool bdbgProcessed = false;
        for (channelnum_t i = 0; i < nChannels; i++) {
            channelnum_t srcChannelIdx = srcChannels < 1 ? 0 : i % srcChannels;
            channelnum_t dstChannelIdx = channels < 1 ? 0 : i % channels;
            auto   srcBufChannel   = srcBuf[srcChannelIdx];
            float* dstBufChannel   = buf[dstChannelIdx];
            for (samplecount_t j = 0; j < nSamples; j++) {
                const float fSrc = (srcBufChannel[j] * srcGain * gain);
                const float fDst = dstBufChannel[j] * (op == MIX ? 1.0f - gain : 1.0f);
                dstBufChannel[j] = fSrc + fDst;
                // bdbgProcessed    = true;
            }
        }
        // dbgassert(bdbgProcessed);
    }

    void addFromDelayLineOp(DelayLine* delayLine, const samplecount_t delay, const mix_op op, float gain);

    void fillNoise(uint32_t seed);

    void realloc(samplecount_t _samples);

};


class DelayLine {
    AudioBlock block;
    samplecount_t writeOffset = 0;
public:
    static std::atomic<int32_t> instanceCount;
    DelayLine() {
        instanceCount++;
    }
    ~DelayLine() {
        instanceCount--;
    }
    DelayLine(const DelayLine& other) = delete;
    DelayLine(DelayLine&& other) = delete;
    DelayLine& operator=(const DelayLine& other) = delete;
    DelayLine& operator=(DelayLine&& other) = delete;
    void updateSize(blocksize_t _blockSize, channelnum_t _numChannels, samplecount_t _delay);
    void write(AudioBlock* input, samplecount_t delay);
    samplecount_t getWriteOffset() const {
        return writeOffset;
    }
    AudioBlock& getBlock() {
        return block;
    }
};

void delayAudio(DelayLine* delayLine, AudioBlock* input, AudioBlock* output, samplecount_t delay);

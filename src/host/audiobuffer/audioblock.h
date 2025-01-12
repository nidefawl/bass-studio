#pragma once
#include "types.h"
#include <memory.h>
#include <atomic>
#include <vector>
#include <array>
#include <type_traits>
#include "assert_dbg.h"
#include "math/seq_math.h"
#include "mem.h"
#include "samplerate.h"
#include "types.h"

#define TRACK_ALLOCATIONS_AUDIOBLOCK 0

enum alloc_type {
    empty = 0,
    stack = 1,
    heap = 2,
    external = 3
};
class DelayLine;
class seq_rand;
struct AudioBlockBaseAllocStats {
    std::atomic<int32_t> instanceCstrd;
    std::atomic<int32_t> numAllocs;
    std::atomic<int32_t> instanceCount;
    volatile bool recordAllocs;
};
enum mix_op : int32_t {
    MIX,
    ADD
};
template<typename FPType>
struct alignas(16) AudioBlockBase {
    using DataType = FPType;
#if TRACK_ALLOCATIONS_AUDIOBLOCK
    static AudioBlockBaseAllocStats& allocStats() {
        static AudioBlockBaseAllocStats stats;
        return stats;
    }
#endif
    static void BeginTrace();
    static void EndTrace();
    std::array<FPType*, 8> heapBuf{};

    FPType** buf{};
    samplecount_t samples{};
    channelnum_t channels{};
    alloc_type channelsAlloc = empty;
    alloc_type dataAlloc = empty;


    AudioBlockBase(const AudioBlockBase&) = delete;
    AudioBlockBase& operator=(const AudioBlockBase&) = delete;

    AudioBlockBase(AudioBlockBase&& other) noexcept {
#if TRACK_ALLOCATIONS_AUDIOBLOCK
        allocStats().instanceCount++;
#endif
        *this = std::move(other);
    }

    AudioBlockBase& operator=(AudioBlockBase&& other) noexcept;

    void allocChannelsArray() {
        if (channels <= heapBuf.size()) {
            channelsAlloc = alloc_type::stack; 
            buf = heapBuf.data();
        } else {
            channelsAlloc = alloc_type::heap; 
            buf = new FPType*[channels];
            memset(buf, 0, sizeof(FPType*) * channels);
        } 
    }

    AudioBlockBase() {
#if TRACK_ALLOCATIONS_AUDIOBLOCK
        allocStats().instanceCount++;
        allocStats().instanceCstrd++;
#endif
    }

    explicit AudioBlockBase(channelnum_t _channels, samplecount_t _samples, bool _bIsDebug = false)
        : channels(_channels), dataAlloc(heap) {
        allocChannelsArray();
#if TRACK_ALLOCATIONS_AUDIOBLOCK
        allocStats().instanceCount++;
        allocStats().instanceCstrd++;
#endif
        realloc(_samples);
    }

    explicit AudioBlockBase(FPType** _buf, channelnum_t _channels, samplecount_t _samples)
        : buf(_buf),
        samples(_samples),
        channels(_channels),
        channelsAlloc(external),
        dataAlloc(external) {
#if TRACK_ALLOCATIONS_AUDIOBLOCK
        allocStats().instanceCount++;
        allocStats().instanceCstrd++;
#endif
    }

    explicit AudioBlockBase(std::vector<std::vector<FPType>>& vecChannels)
        : samples(samplecount_t(vecChannels[0].size())),
        channels(static_cast<channelnum_t>(vecChannels.size())),
        dataAlloc(external) 
    {
        dbgassert(vecChannels.size());
        allocChannelsArray();
#if TRACK_ALLOCATIONS_AUDIOBLOCK
        allocStats().instanceCount++;
        allocStats().instanceCstrd++;
#endif
        for (channelnum_t i = 0; i < channels; i++) {
            buf[i] = vecChannels[i].data();
        }
    }
    explicit AudioBlockBase(const std::vector<FPType*>& vecChannels, samplecount_t _samples)
        : samples(_samples),
        channels(static_cast<channelnum_t>(vecChannels.size())),
        dataAlloc(external) 
    {
        allocChannelsArray();
#if TRACK_ALLOCATIONS_AUDIOBLOCK
        allocStats().instanceCount++;
        allocStats().instanceCstrd++;
#endif
        memcpy(buf, vecChannels.data(), vecChannels.size() * sizeof(decltype(vecChannels[0])));
#ifndef NDEBUG
        FPType** pBuf = buf;
        for (FPType* channel : vecChannels) {
            dbgassert(*pBuf++ == channel);
        }
#endif
    }

    explicit AudioBlockBase(const AudioBlockBase& src, const channelnum_t channelOffset, const channelnum_t numChannels, const samplecount_t sampleOffset, const samplecount_t numSamples)
        : samples(numSamples),
        channels(numChannels),
        dataAlloc(external) {
        allocChannelsArray();
#if TRACK_ALLOCATIONS_AUDIOBLOCK
        allocStats().instanceCount++;
        allocStats().instanceCstrd++;
#endif
        dbgassert(samples);
        for (channelnum_t i = 0; i < channels; i++) {
            dbgassert(src.buf[i]);
            buf[i] = src.buf[channelOffset + i] + sampleOffset;
        }
    }

    ~AudioBlockBase() {
#if TRACK_ALLOCATIONS_AUDIOBLOCK
        allocStats().instanceCount--;
#endif
        if (channelsAlloc != alloc_type::empty && dataAlloc == alloc_type::heap) {
            for (channelnum_t i = 0; i < channels; i++) {
                if (buf[i]) {
                    // delete[] buf[i];
                    DAW::aligned_free(buf[i]);
                }
            }
        }
        if (channelsAlloc == alloc_type::heap) {
            delete[] buf;
        }
    }

    AudioBlockBase getOffsetBlock(const samplecount_t sampleOffset) const {
        dbgassert(sampleOffset < this->samples);
        return AudioBlockBase(*this, 0, this->channels, sampleOffset, this->samples - sampleOffset);
    }

    AudioBlockBase SubChannelsSamplesBlock(const channelnum_t channelOffset, const channelnum_t numChannels, const samplecount_t sampleOffset, const samplecount_t numSamples) const {
        dbgassert(sampleOffset + numSamples <= this->samples);
        if (this->channels < numChannels && channelOffset == 0) {
            return AudioBlockBase(*this, 0, this->channels, sampleOffset, numSamples);
        }
        dbgassert(channelOffset + numChannels <= this->channels);
        return AudioBlockBase(*this, channelOffset, numChannels, sampleOffset, numSamples);
    }

    AudioBlockBase SubSamplesBlock(const samplecount_t sampleOffset, const samplecount_t numSamples) const {
        dbgassert(sampleOffset + numSamples <= this->samples);
        return AudioBlockBase(*this, 0, this->channels, sampleOffset, numSamples);
    }

    AudioBlockBase SubChannelsBlock(const channelnum_t channelOffset, const channelnum_t numChannels) const {
        if (this->channels < numChannels && channelOffset == 0) {
            return AudioBlockBase(*this, 0, this->channels, 0, this->samples);
        }
        if (!assert_expr(channelOffset + numChannels <= this->channels)){
            return AudioBlockBase(*this, 0, this->channels, 0, this->samples);
        }

        return AudioBlockBase(*this, channelOffset, numChannels, 0, this->samples);
    }

    void clear() {
        for (channelnum_t i = 0; i < channels; i++) {
            memset(buf[i], 0, samples * sizeof(FPType));
        }
    }

    void fill(double f) {
        for (channelnum_t i = 0; i < channels; i++) {
            std::fill(buf[i], buf[i] + samples, FPType(f));
        }
    }

    void copyTo(FPType** outputs) {
        for (channelnum_t i = 0; i < channels; i++) {
            memcpy(outputs[i], buf[i], samples * sizeof(FPType));
        }
    }

    template<typename FPTypeSrc>
    void copyFrom(AudioBlockBase<FPTypeSrc>* src) {
        copyFrom(src->buf, src->samples, src->channels);
    }

    template<typename FPTypeSrc>
    void copyFrom(const FPTypeSrc * const * const srcBuf, samplecount_t srcSamples, channelnum_t srcChannels, channelnum_t channelOffset = 0) {
        // dbgassert(srcSamples == samples);
        const channelnum_t nChannels = math::min(srcChannels, channels);
        const samplecount_t nSamples  = math::min(srcSamples, samples);
        for (channelnum_t i = 0; i < nChannels; i++) {
            auto srcChannelIdx = math::min<channelnum_t>(srcChannels - 1, i + channelOffset);
            auto dstChannelIdx = math::min<channelnum_t>(channels - 1, i);
            auto   srcBufChannel   = srcBuf[srcChannelIdx];
            FPType* dstBufChannel  = buf[dstChannelIdx];
            if constexpr(std::is_same<FPType, FPTypeSrc>::value) {
                // detect if we are copying to overlapping memory, otherwise use memcpy
                if (srcBufChannel + nSamples <= dstBufChannel || dstBufChannel + nSamples <= srcBufChannel) {
                    memcpy(dstBufChannel, srcBufChannel, nSamples * sizeof(FPType));
                } else {
                    for (samplecount_t j = 0; j < nSamples; j++) {
                        dstBufChannel[j] = srcBufChannel[j];
                    }
                }
            } else {
                for (samplecount_t j = 0; j < nSamples; j++) {
                    dstBufChannel[j] = srcBufChannel[j];
                }
            }
        }
    }

    template<typename FPTypeSrc, class T>
    void copyFrom(const FPTypeSrc * const * const srcBuf, samplecount_t srcSamples, channelnum_t srcChannels, T getMappedSrcChannel) {
        dbgassert(srcSamples == samples);
        const channelnum_t nChannels = math::min(srcChannels, channels);
        const samplecount_t nSamples  = math::min(srcSamples, samples);
        for (channelnum_t i = 0; i < nChannels; i++) {
            auto dstChannelIdx = math::min<channelnum_t>(channels - 1, i);
            auto srcChannelIdx = math::min<channelnum_t>(srcChannels - 1, getMappedSrcChannel(dstChannelIdx, i));
            auto   srcBufChannel   = srcBuf[srcChannelIdx];
            FPType* dstBufChannel   = buf[dstChannelIdx];
            if (srcBufChannel + nSamples <= dstBufChannel || dstBufChannel + nSamples <= srcBufChannel) {
                memcpy(dstBufChannel, srcBufChannel, nSamples * sizeof(FPType));
            } else {
                for (samplecount_t j = 0; j < nSamples; j++) {
                    dstBufChannel[j] = srcBufChannel[j];
                }
            }
        }
    }

    template<typename FPTypeSrc, class T>
    void copyFrom(const AudioBlockBase<FPTypeSrc>* const src, T getMappedSrcChannel) {
        copyFrom(src->buf, src->samples, src->channels, getMappedSrcChannel);
    }

    template<typename FPTypeSrc>
    void copyFromPosToPos(const FPTypeSrc* const* const srcBuf, samplecount_t offsetIn, samplecount_t offsetOut, samplecount_t len, channelnum_t srcChannels) {
        dbgassert(srcChannels > 0);
        const channelnum_t nChannels = channels;
        const samplecount_t nSamples = math::min(len, samples);
        dbgassert(offsetOut + nSamples <= samples);
        for (channelnum_t i = 0; i < nChannels; i++) {
            auto srcChannelIdx   = srcChannels < 1 ? 0 : math::min<channelnum_t>(srcChannels - 1, i);
            auto dstChannelIdx   = channels < 1 ? 0 : math::min<channelnum_t>(channels - 1, i);
            auto srcBufChannel   = srcBuf[srcChannelIdx];
            FPType* dstBufChannel = buf[dstChannelIdx];
            //TODO: this does 2 copys to the same destination when going from stereo to mono (MIX FIRST)

            // this memcpy would do overlapping copies, which is undefined behavior
            // memcpy(dstBufChannel + offsetOut, srcBufChannel + offsetIn, nSamples * sizeof(FPType));

            // detect if we are copying to overlapping memory, otherwise use memcpy
            if (dstBufChannel + offsetOut >= srcBufChannel + offsetIn + nSamples || dstBufChannel + offsetOut + nSamples <= srcBufChannel + offsetIn) {
                memcpy(dstBufChannel + offsetOut, srcBufChannel + offsetIn, nSamples * sizeof(FPType));
            } else {
                // copy the samples one by one
                for (samplecount_t j = 0; j < nSamples; j++) {
                    dstBufChannel[offsetOut + j] = srcBufChannel[offsetIn + j];
                }
            }
        }
    }
    template<typename FPTypeSrc>
    void addFrom(AudioBlockBase<FPTypeSrc>* src, double gain) {
        addFrom(src->buf, src->samples, src->channels, gain);
    }

    template<typename FPTypeSrc>
    void addFrom(const FPTypeSrc * const * const srcBuf, samplecount_t srcSamples, channelnum_t srcChannels, double gain) {
        dbgassert(srcSamples == samples);
        dbgassert(srcChannels == channels);//remove when adding sub-track mixers (between plugins)
        const channelnum_t nChannels = math::max(srcChannels, channels);
        for (channelnum_t i = 0; i < nChannels; i++) {
            auto srcChannelIdx = srcChannels < 1 ? 0 : math::min<channelnum_t>(srcChannels - 1, i);
            auto dstChannelIdx = channels < 1 ? 0 : math::min<channelnum_t>(channels - 1, i);
            auto   srcBufChannel  = srcBuf[srcChannelIdx];
            FPType* dstBufChannel = buf[dstChannelIdx];
            //TODO: this does 2 additions to the same destination when going from stereo to mono (MIX FIRST)
            for (samplecount_t j = 0; j < samples; j++) {
                dstBufChannel[j] += srcBufChannel[j] * gain;
            }
        }
    }

    template<typename FPTypeSrc>
    void addFromOp(AudioBlockBase<FPTypeSrc>* src, const mix_op op, double gain) {
        addFromOp(src->buf, src->samples, src->channels, op, gain);
    }

    template<typename FPTypeSrc>
    void addFromOp(const FPTypeSrc * const * const srcBuf, const samplecount_t srcSamples, const channelnum_t srcChannels, const mix_op op, double gain) {
        dbgassert(srcSamples <= samples);
        const auto nSamples  = math::min<samplecount_t>(srcSamples, samples);
        auto nChannels = math::min<channelnum_t>(srcChannels, channels);
        FPType srcGain(1.0f);
        if (srcChannels == 2 && channels == 1) {
            srcGain   = 0.5f;
            nChannels = 2;
        }
        if (srcChannels == 1 && channels == 2) {
            nChannels = 2;
        }
        const auto fpGain = FPType(gain);
        for (channelnum_t i = 0; i < nChannels; i++) {
            channelnum_t srcChannelIdx = srcChannels < 1 ? 0 : i % srcChannels;
            channelnum_t dstChannelIdx = channels < 1 ? 0 : i % channels;
            auto    srcBufChannel = srcBuf[srcChannelIdx];
            FPType* dstBufChannel = buf[dstChannelIdx];
            for (samplecount_t j = 0; j < nSamples; j++) {
                const FPType fSrc = (srcBufChannel[j] * srcGain * fpGain);
                const FPType fDst = dstBufChannel[j] * FPType(op == MIX ? 1.0f - fpGain : 1.0f);
                dstBufChannel[j] = fSrc + fDst;
            }
        }
    }

    void addFromDelayLineOp(DelayLine* delayLine, const samplecount_t delay, const mix_op op, double gain);

    void fillNoise(seq_rand& rnd, double gain);

    void realloc(samplecount_t _samples);

};

using AudioBlock = AudioBlockBase<float>;

class DelayLine {
    AudioBlock block;
    samplecount_t writeOffset = 0;
public:
#if TRACK_ALLOCATIONS_AUDIOBLOCK
    static std::atomic<int32_t> instanceCount;
    DelayLine() {
        instanceCount++;
    }
    ~DelayLine() {
        instanceCount--;
    }
#else
    DelayLine() = default;
#endif
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

struct IDelayLineStorage {
    public:
    virtual ~IDelayLineStorage() = default;
    virtual DelayLine* getProcessingDelayLine(uint32_t id) = 0;
};
void delayAudio(DelayLine* delayLine, AudioBlock* input, AudioBlock* output, samplecount_t delay);

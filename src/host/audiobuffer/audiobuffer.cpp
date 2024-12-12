#include <algorithm>
#include <atomic>
#include <memory.h>
#include <cstdlib>
#include <cstring>
#include "types.h"
#include "audiobuffer.h"
#include "config.h"
#include "seq_util.h"
#include "mem.h"
#include "audioblock.h"
#include "rand.h"
#include "math/seq_math.h"
#include "types.h"
#include "assert_dbg.h"
#include "logging.h"

template<typename FPType>
void AudioBlockBase<FPType>::BeginTrace() {
#if TRACK_ALLOCATIONS_AUDIOBLOCK
    auto& stats = allocStats();
    stats.instanceCstrd = 0;
    stats.numAllocs = 0;
    stats.recordAllocs = true;
#endif
}

template<typename FPType>
void AudioBlockBase<FPType>::EndTrace() {
#if TRACK_ALLOCATIONS_AUDIOBLOCK
    auto& stats = allocStats();
    stats.recordAllocs = false;
    log_printf("AudioBlock stats: %d blocks, %d allocs\n", stats.instanceCstrd.load(), stats.numAllocs.load());
#endif
}

AudioBuffer* allocateBuffer(channelnum_t nChannels) {
    auto* buffer = new AudioBuffer{};
    buffer->output = new AudioBlock(nChannels, 512);
    return buffer;
}

void allocRingBuffer(audiothread_ringbuffer_t& ringbuffer, channelnum_t nChannels) {
    for (auto& buffer : ringbuffer.buffers) {
        buffer = allocateBuffer(nChannels);
    }
}
void freeRingBuffer(audiothread_ringbuffer_t& ringbuffer) {
    for (auto& buffer : ringbuffer.buffers) {
        if (buffer) {
            delete buffer->output;
            delete buffer;
            buffer = nullptr;
        }
    }
}

/* fillNoise: -6dB white noise */
template<typename FPType>
void AudioBlockBase<FPType>::fillNoise(seq_rand& rnd, double gain) {
    for (channelnum_t i = 0; i < channels; i++) {
        for (samplecount_t s = 0; s < samples; s++) {
            buf[i][s] = FPType((rnd.rng_rand(1 << 16) / FPType(1 << 16)) * gain);
        }
    }
}

#define DEBUG_PRINT_AUDIOBUFFER_ALLOC 0

template<typename FPType>
void AudioBlockBase<FPType>::realloc(samplecount_t _samples) {

    if (samples != _samples) {
        if (channelsAlloc != alloc_type::empty && dataAlloc == alloc_type::heap) {
#if TRACK_ALLOCATIONS_AUDIOBLOCK
            if (allocStats().recordAllocs)
                allocStats().numAllocs++;
#endif
            for (channelnum_t i = 0; i < channels; i++) {
                FPType* const newBuf = static_cast<FPType*>(DAW::aligned_malloc(sizeof(FPType) * _samples, 512));
#if DEBUG_PRINT_AUDIOBUFFER_ALLOC
                log_lf(Log::L_TRACE, "AudioBlock buffer[%d] allocate 0x%08X\n", i, reinterpret_cast<int64_t>(newBuf));
#endif
                if (!newBuf) {
                    DAW::handleFailedAllocation(0x1000, _samples * sizeof(FPType));
                } else {
                    memset(newBuf, 0, sizeof(FPType) * _samples);
                    if (buf[i]) {
                        if (math::min(_samples, samples) > 0) {
                            memcpy(newBuf, buf[i], math::min(_samples, samples) * sizeof(FPType));
                        }
#if DEBUG_PRINT_AUDIOBUFFER_ALLOC
                        log_lf(Log::L_TRACE, "AudioBlock buffer[%d] release 0x%08X\n", i, reinterpret_cast<int64_t>(newBuf));
#endif
                        DAW::aligned_free(buf[i]);
                    }
                }
                buf[i] = newBuf;
            }
            samples = _samples;
        } else if (channels > 0) {
            dbgassert(0 && "Cannot reallocate externally allocated audiobuffer");
        }
    }
}

template<typename FPType>
AudioBlockBase<FPType>& AudioBlockBase<FPType>::operator=(AudioBlockBase<FPType>&& other) noexcept {
    // more trivial implementation: deallocates *this and does leaves other as empty
    // this could possibly be implemented as full swap, but it's not needed
    if (channelsAlloc != alloc_type::empty && dataAlloc == alloc_type::heap) {
        for (channelnum_t i = 0; i < channels; i++) {
            if (buf[i]) {
                DAW::aligned_free(buf[i]);
                buf[i] = nullptr;
            }
        }
        dataAlloc = alloc_type::empty;
        samples = 0;
    }
    if (channelsAlloc == alloc_type::heap) {
        delete[] buf;
        buf = nullptr;
        channelsAlloc = alloc_type::empty;
        channels = 0;
    }
    std::swap(channels, other.channels);
    std::swap(samples, other.samples);
    if (other.channelsAlloc <= stack) {
        memcpy(heapBuf.data(), other.heapBuf.data(), sizeof(FPType*) * heapBuf.size());
        buf = heapBuf.data();
    } else {
        std::swap(buf, other.buf);
    }
    std::swap(channelsAlloc, other.channelsAlloc);
    std::swap(dataAlloc, other.dataAlloc);
    return *this;
}

template<typename FPType>
void AudioBlockBase<FPType>::addFromDelayLineOp(DelayLine* delayLine, const samplecount_t delay, const mix_op op, double gain) {
    auto& delayBlock = delayLine->getBlock();
    const auto readSamples = this->samples;
    const auto delayLineSize = delayBlock.samples;
    const auto writeOffset = delayLine->getWriteOffset();
    auto readPos = writeOffset - delay;
    if (delay > writeOffset) {
        readPos = writeOffset + delayLineSize - delay;
    }
    dbgassert(readPos >= 0);
    if (readPos + readSamples > delayLineSize) {
        const auto read1Len = delayLineSize - readPos;
        const auto read2Len = readSamples - read1Len;
        auto subBlock1 = delayBlock.SubChannelsSamplesBlock(0, this->channels, readPos, read1Len);
        this->addFromOp(&subBlock1, op, gain);
        auto subBlock2 = delayBlock.SubChannelsSamplesBlock(0, this->channels, 0, read2Len);
        this->SubChannelsSamplesBlock(0, this->channels, read1Len, read2Len).addFromOp(&subBlock2, op, gain);
    } else {
        auto subBlock = delayBlock.SubChannelsSamplesBlock(0, this->channels, readPos, this->samples);
        this->addFromOp(&subBlock, op, gain);
    }
}

void DelayLine::updateSize(blocksize_t _blockSize, channelnum_t _numChannels, samplecount_t _delay) {
    dbgassert(_delay < (1 << 20));
    dbgassert(_blockSize);
    const auto delayBlocks = (_delay + _blockSize + _blockSize - 1) / _blockSize;
    const auto newBufferSize = delayBlocks * _blockSize;
    if (this->block.samples != newBufferSize
        || this->block.channels != _numChannels) {
        this->block = AudioBlock(_numChannels, newBufferSize);
        if (this->writeOffset % _blockSize != 0) {
            this->writeOffset = (this->writeOffset / _blockSize) * _blockSize;
            dbgassert(this->writeOffset <= newBufferSize);
        }
        if (this->writeOffset >= newBufferSize) {
            this->writeOffset = 0;
        }
    }
}
void DelayLine::write(AudioBlock* input, samplecount_t delay) {
    dbgassert(input->channels < std::numeric_limits<channelnum_t>::max());
    dbgassert(input->samples < std::numeric_limits<blocksize_t>::max());
    updateSize(static_cast<blocksize_t>(input->samples), static_cast<channelnum_t>(input->channels), delay);
    writeOffset += input->samples;
    if (writeOffset >= block.samples) {
        writeOffset = 0;
    }
    dbgassert(writeOffset + input->samples <= block.samples);
    block.copyFromPosToPos(input->buf, 0, writeOffset, input->samples, input->channels);
}

void delayAudio(DelayLine* delayLine, AudioBlock* input, AudioBlock* output, samplecount_t delay) {
    delayLine->write(input, delay);
    output->addFromDelayLineOp(delayLine, delay, mix_op::MIX, 1.0f);
}


#if TRACK_ALLOCATIONS_AUDIOBLOCK
std::atomic<int32_t> DelayLine::instanceCount{ 0 };
#endif

void printLeakedAudioBuffers() {
#if TRACK_ALLOCATIONS_AUDIOBLOCK
    auto& stats = AudioBlock::allocStats();
    log_printf("AudioBlock::instanceCstrd: %d\n", stats.instanceCstrd.load());
    log_printf("AudioBlock::instanceCount: %d\n", stats.instanceCount.load());
    log_printf("AudioBlock::numAllocs: %d\n", stats.numAllocs.load());
    log_printf("DelayLine::instanceCount: %d\n", DelayLine::instanceCount.load());
#endif
}

namespace DAW {
#define TRACE_ALLOCATIONS 0
#if TRACE_ALLOCATIONS == 0

void* aligned_malloc(size_t size, size_t align) {
    void* result;
#if defined(_MSC_VER) || defined(__MINGW32__)
    result = _aligned_malloc(size, align);
#else
    if (posix_memalign(&result, align, size)) result = 0;
#endif
    return result;
}

void aligned_free(void* ptr) {
#if defined(_MSC_VER) || defined(__MINGW32__)
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}
#else
struct aligned_alloc_t {
    void* ptr;
    size_t size;
    size_t align;
};

std::vector<aligned_alloc_t> allocations;
std::mutex m_mtx;
void* aligned_malloc(size_t size, size_t align) {
    std::unique_lock<std::mutex> lock(m_mtx);
    void* result;
#if defined(_MSC_VER) || defined(__MINGW32__)
    result = _aligned_malloc(size, align);
#else
    if (posix_memalign(&result, align, size)) result = 0;
#endif
    allocations.push_back({ result, size, align });
    return result;
}
void aligned_free(void* ptr) {
    std::unique_lock<std::mutex> lock(m_mtx);
    int32_t index = -1;
    aligned_alloc_t alloc;
    for (int32_t i = 0; i < allocations.size(); i++) {
        if (allocations[i].ptr == ptr) {
            alloc = allocations[i];
            index = i;
            break;
        }
    }
    if (index >= 0) {
        allocations.erase(allocations.begin() + index);
    } else {
        log_lf(Log::L_ERROR, "aligned_free: ptr not found\n");
        dbgassert(0);
    }

#if defined(_MSC_VER) || defined(__MINGW32__)
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

#endif
void handleFailedAllocation(int allocId, size_t allocSize) {
    log_printf("Failed allocation of size %zu at %d\n", allocSize, allocId);
    dbgassert(0);
}
}
template struct AudioBlockBase<float>;
template struct AudioBlockBase<double>;

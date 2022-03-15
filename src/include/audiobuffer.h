#pragma once
#include "types.h"
#include <atomic>
#include "assert_dbg.h"

struct AudioBlock;
struct alignas(64) AudioBuffer {
    AudioBlock* output{};
    int32_t writeOffset{};
    std::atomic<bool> inUse{};
    bool submitted{};
    int32_t nonce{};
    double blockPosSample{};
    double blockPosTick{};
};
//static_assert(std::is_pod<AudioBuffer>::value, "AudioBuffer is not POD type.");

#define RING_BUF_SIZE (1 << 4)
#define RING_BUF_MASK (RING_BUF_SIZE - 1)
struct audiothread_ringbuffer_t {
    int32_t writePos = 0;
    AudioBuffer* buffers[RING_BUF_SIZE] = { 0 };
};
AudioBuffer* allocateBuffer(channelnum_t nChannels);
void allocRingBuffer(audiothread_ringbuffer_t&, channelnum_t nChannels);
void freeRingBuffer(audiothread_ringbuffer_t&);

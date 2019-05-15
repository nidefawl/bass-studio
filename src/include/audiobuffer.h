#pragma once
#include <stdint.h>
#include <memory.h>
#include <atomic>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include "assert_dbg.h"

struct AudioBlock;
struct AudioBuffer {
	AudioBlock* output;
	std::atomic<bool> inUse;
	bool submitted;
	int32_t nonce;
	double blockPosSample;
	double blockPosTick;
};
//static_assert(std::is_pod<AudioBuffer>::value, "AudioBuffer is not POD type.");

#define RING_BUF_SIZE (1<<4)
#define RING_BUF_MASK (RING_BUF_SIZE-1)
struct audiothread_ringbuffer_t {
	int32_t readPos = 0;
	int32_t writePos = 0;
	AudioBuffer* buffers[RING_BUF_SIZE] = { 0 };
};
AudioBuffer* allocateBuffer();
void allocRingBuffer(audiothread_ringbuffer_t&);
void freeRingBuffer(audiothread_ringbuffer_t&);

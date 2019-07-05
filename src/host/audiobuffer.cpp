#include <atomic>
#include <memory.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "audiobuffer.h"
#include "config.h"
#include "seq_util.h"
#include "mem.h"
#include "audioblock.h"

AudioBuffer* allocateBuffer(int32_t nChannels) {
	AudioBuffer* buffer = (AudioBuffer*) aligned_malloc(sizeof(AudioBuffer), 128);
	memset(buffer, 0, sizeof(AudioBuffer));
	buffer->output = new AudioBlock(nChannels, 1);
	buffer->submitted = false;
	std::atomic_init(&buffer->inUse, false);
	return buffer;
}

void allocRingBuffer(audiothread_ringbuffer_t& ringbuffer, int32_t nChannels) {
	for (int i = 0; i < RING_BUF_SIZE; i++) {
		ringbuffer.buffers[i] = allocateBuffer(nChannels);
	}
}
void freeRingBuffer(audiothread_ringbuffer_t& ringbuffer) {
	for (int i = 0; i < RING_BUF_SIZE; i++) {
		if (ringbuffer.buffers[i]) {
			delete ringbuffer.buffers[i]->output;
			aligned_free(ringbuffer.buffers[i]);
			ringbuffer.buffers[i] = nullptr;
		}
	}
}

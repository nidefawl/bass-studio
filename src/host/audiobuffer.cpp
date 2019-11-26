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

void AudioBlock::realloc(uint32_t _samples) {

	if (samples != _samples) {
		if (allocType == alloc_type::internal) {
			for (uint32_t i = 0; i < channels; i++) {
	//					float* newBuf = (float*)calloc(_samples,sizeof(float));
				float* const newBuf = new float[_samples];
				if (debug) {
					log_printf("AudioBlock buffer[%d] calloc 0x%08X\n", i, reinterpret_cast<int64_t>(newBuf));
				}
				if (!newBuf) {
					handleFailedAllocation(0x1000, _samples*sizeof(float));
				} else {
					memset(newBuf, 0, sizeof(float)*_samples);
					if (buf[i]) {
						memcpy(newBuf, buf[i], math::min(_samples, samples) * sizeof(float));
						if (debug) {
							log_printf("AudioBlock buffer[%d] free 0x%08X\n", i, reinterpret_cast<int64_t>(newBuf));
						}
	//							free(buf[i]);
						delete[] buf[i];
					}
				}
				buf[i] = newBuf;
			}
			samples = _samples;
		}
	}
}

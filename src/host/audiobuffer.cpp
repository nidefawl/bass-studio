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
#include "rand.h"

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
void AudioBlock::fillNoise(uint32_t seed) {
	seq_rand rnd;
	rnd.rng_seed(seed);
	for (uint32_t i = 0; i < channels; i++) {
		for (int s = 0; s < samples; s++) {
			float f = 0.0f;
			uint32_t rngBits = rnd.rng_rand();
			uint32_t* floatAsU32 = reinterpret_cast<uint32_t*>(&f);
			*floatAsU32 |= (rngBits&0x3F) << 24;
			*floatAsU32 |= ((rngBits&(~0x3F))>>6) & 0x3FFFFF;
			buf[i][s] = f;
		}
	}
}

void AudioBlock::realloc(uint32_t _samples) {

	if (samples != _samples) {
		if (allocType == alloc_type::internal) {
			bool allN = true;
			for (uint32_t i = 0; i < channels; i++) {
	//					float* newBuf = (float*)calloc(_samples,sizeof(float));
				float* const newBuf = new float[_samples];
				if (debug) {
					log_printf("AudioBlock buffer[%d] allocate 0x%08X\n", i, reinterpret_cast<int64_t>(newBuf));
				}
				if (!newBuf) {
					handleFailedAllocation(0x1000, _samples*sizeof(float));
				} else {
					memset(newBuf, 0, sizeof(float)*_samples);
					if (buf[i]) {
						if (math::min(_samples, samples) > 0) {
							memcpy(newBuf, buf[i], math::min(_samples, samples) * sizeof(float));
						}

						if (debug) {
							log_printf("AudioBlock buffer[%d] release 0x%08X\n", i, reinterpret_cast<int64_t>(newBuf));
						}
	//							free(buf[i]);
						delete[] buf[i];
					}
				}
				buf[i] = newBuf;
			}
			samples = _samples;
			static uint32_t nextSeed = 0;
			fillNoise(nextSeed++);
		}
		else {
			dbgassert(0 && "Cannot reallocate externally allocated audiobuffer");
		}
	}
}

std::atomic<int32_t> DelayLine::instanceCount{0};
std::atomic<int32_t> AudioBlock::instanceCount{0};

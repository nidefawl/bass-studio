#include <atomic>
#include <memory.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include "audiobuffer.h"
#include "config.h"
#include "seq_util.h"
#include "mem.h"
#include "audioblock.h"
#include "rand.h"
#include "math/seq_math.h"

void AudioBlock::BeginTrace() {
    AudioBlock::instanceCstrd = 0;
    AudioBlock::numAllocs = 0;
    AudioBlock::recordAllocs = true;

}
void AudioBlock::EndTrace() {
    AudioBlock::recordAllocs = false;
    log_printf("AudioBlock stats: %d blocks, %d allocs\n", AudioBlock::instanceCstrd.load(), AudioBlock::numAllocs.load());
}

AudioBuffer* allocateBuffer(int32_t nChannels) {
    auto* buffer = new AudioBuffer{};
    buffer->output = new AudioBlock(nChannels, 512);
    return buffer;
}

void allocRingBuffer(audiothread_ringbuffer_t& ringbuffer, int32_t nChannels) {
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
void AudioBlock::fillNoise(uint32_t seed) {
    seq_rand rnd;
    rnd.rng_seed(seed);
    for (uint32_t i = 0; i < channels; i++) {
        for (uint32_t s = 0; s < samples; s++) {
            buf[i][s] = (rnd.rng_rand(1<<16)/(float)(1<<16))*0.4f;
        }
    }
}

void AudioBlock::realloc(uint32_t _samples) {

    if (samples != _samples) {
        if (allocType == alloc_type::internal) {
            if (recordAllocs)
                numAllocs++;
            for (uint32_t i = 0; i < channels; i++) {
                float* const newBuf = static_cast<float*>(aligned_malloc(sizeof(float) * _samples, 512));
                if (debug) {
                    log_lf(Log::L_TRACE, "AudioBlock buffer[%d] allocate 0x%08X\n", i, reinterpret_cast<int64_t>(newBuf));
                }
                if (!newBuf) {
                    handleFailedAllocation(0x1000, _samples * sizeof(float));
                } else {
                    memset(newBuf, 0, sizeof(float) * _samples);
                    if (buf[i]) {
                        if (math::min(_samples, samples) > 0) {
                            memcpy(newBuf, buf[i], math::min(_samples, samples) * sizeof(float));
                        }

                        if (debug) {
                            log_lf(Log::L_TRACE, "AudioBlock buffer[%d] release 0x%08X\n", i, reinterpret_cast<int64_t>(newBuf));
                        }

                        aligned_free(buf[i]);
                    }
                }
                buf[i] = newBuf;
            }
            samples = _samples;
        } else {
            dbgassert(0 && "Cannot reallocate externally allocated audiobuffer");
        }
    }
}

void AudioBlock::addFromDelayLineOp(DelayLine* delayLine, const samplerate_t delay, const mix_op op, float gain) {
    auto& delayBlock = delayLine->block;
    const auto readSamples = this->samples;
    const auto delayLineSize = delayBlock.samples;
    auto readPos = delayLine->writeOffset - delay;
    if (delay > delayLine->writeOffset) {
        readPos = delayLine->writeOffset + delayLineSize - delay;
    }
    if (readPos + readSamples > delayLineSize) {
        const auto read1Len = delayLineSize - readPos;
        const auto read2Len = readSamples - read1Len;
        AudioBlock subBlock1 = delayBlock.SubChannelsSamplesBlock(0, this->channels, readPos, read1Len);
        this->addFromOp(&subBlock1, op, gain);
        AudioBlock subBlock2 = delayBlock.SubChannelsSamplesBlock(0, this->channels, 0, read2Len);
        this->SubChannelsSamplesBlock(0, this->channels, read1Len, read2Len).addFromOp(&subBlock2, op, gain);
    } else {
        AudioBlock subBlock = delayBlock.SubChannelsSamplesBlock(0, this->channels, readPos, this->samples);
        this->addFromOp(&subBlock, op, gain);
    }
}

void DelayLine::updateSize(uint16_t _blockSize, uint8_t _numChannels, samplerate_t _delay) {
    dbgassert(_delay < (1 << 20));
    dbgassert(_blockSize);
    auto delayBlocks = (_delay + _blockSize - 1) / _blockSize;
    auto newBufferSize = delayBlocks * _blockSize;
    if (this->blockSize != _blockSize
        || this->block.samples != newBufferSize
        || this->block.channels != _numChannels) {
        this->block = AudioBlock(_numChannels, newBufferSize);
        this->blockSize = _blockSize;
        if (this->writeOffset % _blockSize != 0) {
            this->writeOffset = (this->writeOffset / _blockSize) * _blockSize;
            dbgassert(this->writeOffset <= newBufferSize);
        }
        if (this->writeOffset >= newBufferSize) {
            this->writeOffset = 0;
        }
    }
}
void delayLineWrite(DelayLine* delayLine, AudioBlock* input, samplerate_t delay) {
    dbgassert(delayLine);
    dbgassert(input->channels < 256);
    dbgassert(input->samples < std::numeric_limits<uint16_t>::max());
    delayLine->updateSize(input->samples, static_cast<uint8_t>(input->channels), delay);
    delayLine->writeOffset += delayLine->blockSize;
    if (delayLine->writeOffset >= delayLine->block.samples) {
        delayLine->writeOffset = 0;
    }
    dbgassert(delayLine->writeOffset + input->samples <= delayLine->block.samples);
    delayLine->block.copyFromPosToPos(input->buf, 0, delayLine->writeOffset, input->samples, input->channels);
}

void delayAudio(DelayLine* delayLine, AudioBlock* input, AudioBlock* output, samplerate_t delay) {
    delayLineWrite(delayLine, input, delay);
    output->addFromDelayLineOp(delayLine, delay, AudioBlock::MIX, 1.0f);
}


std::atomic<int32_t> DelayLine::instanceCount{ 0 };
std::atomic<int32_t> AudioBlock::instanceCstrd{ 0 };
std::atomic<int32_t> AudioBlock::numAllocs{ 0 };
std::atomic<int32_t> AudioBlock::instanceCount{ 0 };
volatile bool AudioBlock::recordAllocs{ 0 };


void printLeakedAudioBuffers() {
    log_printf("AudioBlock::instanceCount: %d\n", AudioBlock::instanceCount.load());
    log_printf("DelayLine::instanceCount: %d\n", DelayLine::instanceCount.load());
}

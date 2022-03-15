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

void AudioBlock::BeginTrace() {
    AudioBlock::instanceCstrd = 0;
    AudioBlock::numAllocs = 0;
    AudioBlock::recordAllocs = true;

}
void AudioBlock::EndTrace() {
    AudioBlock::recordAllocs = false;
    log_printf("AudioBlock stats: %d blocks, %d allocs\n", AudioBlock::instanceCstrd.load(), AudioBlock::numAllocs.load());
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
void AudioBlock::fillNoise(uint32_t seed) {
    seq_rand rnd;
    rnd.rng_seed(seed);
    for (channelnum_t i = 0; i < channels; i++) {
        for (samplecount_t s = 0; s < samples; s++) {
            buf[i][s] = (rnd.rng_rand(1<<16)/(float)(1<<16))*0.4f;
        }
    }
}
#define DEBUG_PRINT_AUDIOBUFFER_ALLOC 0
void AudioBlock::realloc(samplecount_t _samples) {

    if (samples != _samples) {
        if (channelsAlloc != alloc_type::empty && dataAlloc == alloc_type::heap) {
            if (recordAllocs)
                numAllocs++;
            for (channelnum_t i = 0; i < channels; i++) {
                float* const newBuf = static_cast<float*>(aligned_malloc(sizeof(float) * _samples, 512));
#if DEBUG_PRINT_AUDIOBUFFER_ALLOC
                log_lf(Log::L_TRACE, "AudioBlock buffer[%d] allocate 0x%08X\n", i, reinterpret_cast<int64_t>(newBuf));
#endif
                if (!newBuf) {
                    handleFailedAllocation(0x1000, _samples * sizeof(float));
                } else {
                    memset(newBuf, 0, sizeof(float) * _samples);
                    if (buf[i]) {
                        if (math::min(_samples, samples) > 0) {
                            memcpy(newBuf, buf[i], math::min(_samples, samples) * sizeof(float));
                        }
#if DEBUG_PRINT_AUDIOBUFFER_ALLOC
                        log_lf(Log::L_TRACE, "AudioBlock buffer[%d] release 0x%08X\n", i, reinterpret_cast<int64_t>(newBuf));
#endif
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

void AudioBlock::addFromDelayLineOp(DelayLine* delayLine, const samplecount_t delay, const mix_op op, float gain) {
    auto& delayBlock = delayLine->getBlock();
    const auto readSamples = this->samples;
    const auto delayLineSize = delayBlock.samples;
    const auto writeOffset = delayLine->getWriteOffset();
    auto readPos = writeOffset - delay;
    if (delay > writeOffset) {
        readPos = writeOffset + delayLineSize - delay;
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
    output->addFromDelayLineOp(delayLine, delay, AudioBlock::MIX, 1.0f);
}


std::atomic<int32_t> DelayLine::instanceCount{ 0 };
std::atomic<int32_t> AudioBlock::instanceCstrd{ 0 };
std::atomic<int32_t> AudioBlock::numAllocs{ 0 };
std::atomic<int32_t> AudioBlock::instanceCount{ 0 };
volatile bool AudioBlock::recordAllocs{ false };


void printLeakedAudioBuffers() {
    log_printf("AudioBlock::instanceCount: %d\n", AudioBlock::instanceCount.load());
    log_printf("DelayLine::instanceCount: %d\n", DelayLine::instanceCount.load());
}

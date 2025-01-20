#pragma once
#include "dsp_util.hpp"
#include "host/audiobuffer/audioblock.hpp"
#include "host/audiobuffer/audiobuffer.hpp"
#include "host/project/project.hpp"
#include "rand.hpp"
#include "seq_time.hpp"
#include <readerwriterqueue/readerwriterqueue.hpp>

namespace DAW {

class AuxOutputSource {
    struct ProcessedAudioBlock {
        AudioBlock block;
        sampleformat_t sampleFormat;
        int32_t samplePos;
        double posDouble;
    };

public:
    virtual ~AuxOutputSource()                                                                                                                                 = default;
    virtual bool feedTo(AudioBlock& block)                                                                                                                     = 0;
    virtual void processBlock(const sampleformat_t& sampleFormat, int32_t sample, double posDouble, playback_state state, const project_globals_t& prjGlobals) = 0;
};


// dummy class that implements AuxOutputSource providing a -24dB noise source
class AuxOutputNoiseSource : public AuxOutputSource {
    static constexpr channelnum_t numChannels = 2;
    audiothread_ringbuffer_t ringbuffer;
    moodycamel::ReaderWriterQueue<AudioBuffer*> audioQueue;
    seq_rand rnd;
public:
    AuxOutputNoiseSource() {
        allocRingBuffer(ringbuffer, numChannels);
    }
    ~AuxOutputNoiseSource() override {
        freeRingBuffer(ringbuffer);
    }
    bool feedTo(AudioBlock& block) override { 
        AudioBuffer* buf = nullptr;
        if (audioQueue.try_dequeue(buf)) {
            buf->inUse = false;
            block.addFromOp(buf->output, mix_op::ADD, 0.5);
            return true;
        }
        return false;
    }
    void processBlock(const sampleformat_t& sampleFormat, int32_t sample, double posDouble, playback_state state, const project_globals_t& prjGlobals) override {
        auto& writePos = ringbuffer.writePos;
        AudioBuffer* bufferWrite = ringbuffer.buffers[writePos];
        if (!bufferWrite->inUse) {
            bufferWrite->output->realloc(sampleFormat.blockSize);
            bufferWrite->output->fillNoise(rnd, dsp_util::fromdBFS(-24.0 + 6.0));
            bufferWrite->inUse = true;
            bufferWrite->time.inputTimeSeconds = posDouble;
            audioQueue.enqueue(bufferWrite);
            writePos++;
            writePos &= RING_BUF_MASK;
        }
    }
};

} // namespace DAW

#pragma once
#include <memory>
#include <array>
#include "types.h"
#include "config.h"
#include "samplerate.h"
#include "str_util.h"
#include "seq_time.h"
#include "dsp_util.h"
#include "audiobuffer.h"
#include <readerwriterqueue/readerwriterqueue.hpp>
#include "meter.h"
#include "audio_config.h"
#include "appsettings.h"

using PaStream = void;
class audiohost_callback;
class audiohost {
    friend class HostIOStream;
    friend class audiohost_callback;
public:
    class HostIOStream : public DAW::AudioIO::AudioStream {
        public:
        struct IOChannel {
            AudioBlock buf;
            DAW::rmsmeter meter;
            int32_t index;
            channelnum_t channelOffset;
            DAW::channel_pairing type;

            IOChannel(int32_t _index, DAW::channel_pairing _type, channelnum_t _channelOffset, DAW::rmsmeter&& _meter)
                : buf(DAW::AudioIO::getNumChannelsFromTrackType(_type), 0),
                meter(_meter),
                index(_index),
                channelOffset(_channelOffset),
                type(_type)
            {
            }
            ~IOChannel() = default;
        };

        audiothread_ringbuffer_t ringbuffer;
        moodycamel::ReaderWriterQueue<AudioBuffer*> audioQueue;
        moodycamel::ReaderWriterQueue<AudioBuffer*> audioQueueInput;

        //TODO: heap alloc to reduce allocated memory
        std::array<DAW::meter_runningsum, MAX_AUDIO_IO_CHANNELS> meterDataInput;
        std::array<DAW::meter_runningsum, MAX_AUDIO_IO_CHANNELS> meterDataOutput;
        DAW::rmsmeter metersInput;
        DAW::rmsmeter metersOutput;

        std::vector<std::shared_ptr<IOChannel>> channelsInput;
        std::vector<std::shared_ptr<IOChannel>> channelsOutput;

        audiohost* const host;
        const int streamId;
        const size_t streamIdx;
        const channelnum_t nInputChannels;
        const channelnum_t nOutputChannels;

        PaStream* stream{ nullptr };
    
        String inputName;
        String outputName;
        String device_api;

        std::atomic<bool> streamShouldEnd{ false };
        std::atomic<bool> streamFinished{ false };

        int64_t lastAudioCallbackInvocationTime_i64 = 0;

        HostIOStream(audiohost* const _host, int32_t _streamId, int32_t _streamIdx, DAW::AudioIO::io_cfg_tracks& cfg, channelnum_t nOutputChannels, channelnum_t nInputChannels);
        ~HostIOStream() override;
        audiothread_ringbuffer_t& getRingbuffer() {
            return ringbuffer;
        }
        static inline String getTrackName(IOChannel* track, bool isInput) {
            return DAW::AudioIO::getTrackName(track->type, track->index, isInput);
        }
        void enqueue(AudioBuffer*) override;
        bool try_dequeue(AudioBuffer*&) override;
        void enqueueInput(AudioBuffer*) override;
        bool try_dequeueInput(AudioBuffer*&) override;
        int32_t getOutputQueueSize() const override {
            return static_cast<int32_t>(audioQueue.size_approx());
        }
        int32_t getInputQueueSize() const override {
            return static_cast<int32_t>(audioQueueInput.size_approx());
        }
        samplerate_t getSampleRate() const override {
            return this->host->lSampleRate;
        }
        blocksize_t getBlockSize() const override {
                return this->host->lBlockSize;
        }
        bool isActive() const override {
            return !streamShouldEnd && !streamFinished;
        }
        channelnum_t getNumInputChannels() const override {
            return this->nInputChannels;
        }
        channelnum_t getNumOutputChannels() const override {
            return this->nOutputChannels;
        }
    };

private:
    std::vector<std::shared_ptr<HostIOStream>> streams;
    bool paIsInitalized = false;
    int32_t nextStreamId = 0;
    samplerate_t lSampleRate = 0;
    blocksize_t lBlockSize      = 0;

public:
    uint32_t blockReads          = 0;
    uint32_t bufferUnderuns      = 0;
    uint32_t inputBufferUnderuns = 0;

    int32_t audioCallbackInvocationDelay_usec = 0;
public:
    audiohost()  = default;
    ~audiohost() = default;
    static audiohost* getInstance();
    HostIOStream* getStream(size_t idx);
    std::shared_ptr<audiohost::HostIOStream> getStreamSharedPtr(size_t idx);
    bool initPa();
    void deinitPa();
    void removeStream(HostIOStream* stream);
    bool startAudio(app_iosettings& settings);
    bool stopAudio();
    bool isStreaming() {
        return !streams.empty();
    }
};

#pragma once
#include <cstdint>
#include <memory>
#include <array>
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
class audiohost {
public:
    class HostIOStream : public DAW::AudioIO::AudioStream {
        public:
        struct IOChannel {
            DAW::rmsmeter meter;
            AudioBlock buf;
            int32_t index         = 0;
            int32_t channelOffset = 0;
            DAW::channelcount type;

            IOChannel(int32_t _index, DAW::channelcount _type, int32_t _channelOffset, DAW::rmsmeter&& _meter)
                : meter(_meter),
                buf((uint32_t) DAW::AudioIO::getNumChannelsFromTrackType(_type), 0),
                index(_index),
                channelOffset(_channelOffset),
                type(_type)
            {
            }
            ~IOChannel() = default;
        };

        enum StreamDirection {
            DIR_IN  = 1,
            DIR_OUT = 2,
        };

        int streamId = 1;
        audiohost* host{ nullptr };
        PaStream* stream{ nullptr };

        int flagsIO = DIR_IN | DIR_OUT;
        int idx     = 0;

        int32_t nInputChannels  = 0;
        int32_t nOutputChannels = 0;

        String inputName;
        String outputName;
        String device_api;

        std::atomic<bool> streamShouldEnd{ false };
        std::atomic<bool> streamFinished{ false };

        moodycamel::ReaderWriterQueue<AudioBuffer*> audioQueue;
        moodycamel::ReaderWriterQueue<AudioBuffer*> audioQueueInput;
        audiothread_ringbuffer_t ringbuffer;

        std::array<DAW::meter_runningsum, 32> meterDataInput;
        std::array<DAW::meter_runningsum, 32> meterDataOutput;
        DAW::rmsmeter metersInput;
        DAW::rmsmeter metersOutput;

        std::vector<std::shared_ptr<IOChannel>> channelsInput;
        std::vector<std::shared_ptr<IOChannel>> channelsOutput;

        int64_t lastAudioCallbackInvocationTime_i64 = 0;

        HostIOStream(int32_t streamId, DAW::AudioIO::io_cfg_tracks cfg, int32_t nOutputChannels = 0, int32_t nInputChannels = 0);
        ~HostIOStream();
        audiothread_ringbuffer_t& getRingbuffer() {
            return ringbuffer;
        }
        static inline String getTrackName(IOChannel* track, bool isInput) {
            return DAW::AudioIO::getTrackName(track->type, track->index, isInput);
        }
        void enqueue(AudioBuffer*);
        bool try_dequeue(AudioBuffer*&);
        void enqueueInput(AudioBuffer*);
        bool try_dequeueInput(AudioBuffer*&);
        int32_t getOutputQueueSize() const {
            return static_cast<int32_t>(audioQueue.size_approx());
        }
        int32_t getInputQueueSize() const {
            return static_cast<int32_t>(audioQueueInput.size_approx());
        }
        samplerate_t getSampleRate() const {
            return this->host->lSampleRate;
        }
        uint16_t getBlockSize() const {
            return this->host->lBlockSize;
        }
        bool isActive() const {
            return !streamShouldEnd && !streamFinished;
        }
    };

private:
    std::vector<std::shared_ptr<HostIOStream>> streams;
    bool paIsInitalized = false;

public:
    int32_t nextStreamId{ 0 };
    samplerate_t lSampleRate = 0;
    uint16_t lBlockSize      = 0;

    uint32_t blockReads          = 0;
    uint32_t bufferUnderuns      = 0;
    uint32_t inputBufferUnderuns = 0;

    int32_t audioCallbackInvocationDelay_usec = 0;

private:
public:
    audiohost()  = default;
    ~audiohost() = default;
    static audiohost* getInstance();
    HostIOStream* getStream(int idx);
    std::shared_ptr<audiohost::HostIOStream> getStreamSharedPtr(int idx);
    bool initPa();
    void deinitPa();
    void removeStream(HostIOStream* stream);

public:
    bool startAudio(app_iosettings& settings);
    bool stopAudio();
    bool isStreaming() {
        return !streams.empty();
    }
};

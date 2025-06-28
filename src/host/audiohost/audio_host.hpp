#pragma once
#include <memory>
#include <array>
#include "platform.hpp"
#include "types.hpp"
#include "config.hpp"
#include "samplerate.hpp"
#include "str_util.hpp"
#include "seq_time.hpp"
#include "dsp_util.hpp"
#include "host/audiobuffer/audiobuffer.hpp"
#include <readerwriterqueue/readerwritercircularbuffer.hpp>
#include "host/meter/meter.hpp"
#include "host/audio_config.hpp"
#include "appsettings.hpp"

using PaStream = void;
class audiohost_callback;
class audiohost {
    friend class HostIOStream;
    friend class audiohost_callback;
public:
    class HostIOStream final : public DAW::AudioIO::AudioStream {
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
        moodycamel::BlockingReaderWriterCircularBuffer<AudioBuffer*> audioQueue;
        moodycamel::BlockingReaderWriterCircularBuffer<AudioBuffer*> audioQueueInput;

        std::vector<DAW::meter_runningsum> meterDataInput;
        std::vector<DAW::meter_runningsum> meterDataOutput;
        DAW::rmsmeter metersInput;
        DAW::rmsmeter metersOutput;
        std::vector<DAW::meter_runningsum> meterDataCBInput;
        std::vector<DAW::meter_runningsum> meterDataCBOutput;
        std::shared_ptr<DAW::rmsmeter> meterCallbackInput;
        std::shared_ptr<DAW::rmsmeter> meterCallbackOutput;

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

        double streamInputLatency = 0.0;
        double streamOutputLatency = 0.0;

        double inputTimeSeconds = 0.0;
        double outputTimeSeconds = 0.0;
        double outputTickPos = 0.0;

        double firstInputTimeSeconds = -1.0;
        double playbackBeginTimeSeconds = 0.0;
        double playbackBeginTickPos = 0.0;
    
        samplecount_t inputSamplePos = 0;
        samplecount_t outputSamplePos = 0;

        uint32_t numInvocations = 0;
        DAW::AudioIO::AudioStream::stream_timings_t audioCallbackInvocationDelay;
        int64_t lastAudioCallbackInvocationTime_i64 = 0;
        uint32_t bufferUnderuns      = 0;
        uint32_t inputBufferUnderuns = 0;

        HostIOStream(audiohost* const _host, int32_t _streamId, int32_t _streamIdx, DAW::AudioIO::io_cfg_tracks& cfg, channelnum_t nOutputChannels, channelnum_t nInputChannels);
        ~HostIOStream() override;
        audiothread_ringbuffer_t& getRingbuffer() {
            return ringbuffer;
        }
        double getInputTimeSeconds() const override {
            return inputTimeSeconds;
        }
        double getPlaybackTimeSeconds() const override {
            return getTimeSecondsD() - playbackBeginTimeSeconds;
        }
        double getOutputTickPos() const override {
            return outputTickPos;
        }
        double getPlaybackBeginTickPos() const override {
            return playbackBeginTickPos;
        }
        double getStreamInputLatency() const override {
            return streamInputLatency;
        }
        double getStreamOutputLatency() const override {
            return streamOutputLatency;
        }
        void onPlayback() override {
            playbackBeginTimeSeconds = getTimeSecondsD();
        }

        DAW::rmsmeter& getMeterInput() {
            return metersInput;
        }
        DAW::rmsmeter& getMeterOutput() {
            return metersOutput;
        }
        DAW::rmsmeter* getMeterCallbackInput() {
            return meterCallbackInput.get();
        }
        DAW::rmsmeter* getMeterCallbackOutput() {
            return meterCallbackOutput.get();
        }
        DAW::AudioIO::AudioStream::stream_timings_t getStreamTimings() const override {
            return audioCallbackInvocationDelay;
        }
        uint32_t getBufferUnderuns() const override {
            return bufferUnderuns;
        }
        uint32_t getInputBufferUnderuns() const override {
            return inputBufferUnderuns;
        }
        uint32_t getNumCallbacks() const override {
            return numInvocations;
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
        void requestReset() {
            host->requestReset();
        }
    };

private:
    std::vector<std::shared_ptr<HostIOStream>> streams;
    bool paIsInitalized = false;
    int32_t nextStreamId = 0;
    samplerate_t lSampleRate = 0;
    blocksize_t lBlockSize      = 0;
    String lastErrorMessage;
    std::atomic<bool> bRequestReset{ false };


    bool onError(const char* msg, int err);

public:
    audiohost()  = default;
    ~audiohost() = default;
    HostIOStream* getStream(size_t idx);
    std::shared_ptr<audiohost::HostIOStream> getStreamSharedPtr(size_t idx);
    bool initPa();
    void deinitPa();
    void removeStream(HostIOStream* stream);
    bool startAudio(app_iosettings& settings);
    const String& getLastErrorMessage() const {
        return lastErrorMessage;
    }
    bool stopAudio();
    bool isStreaming() {
        return !streams.empty();
    }
    void requestReset() {
        bRequestReset = true;
    }
    bool isResetRequested() {
        return bRequestReset;
    }
};

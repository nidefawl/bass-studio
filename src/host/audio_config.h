#pragma once
#include "str_util.h"
#include "samplerate.h"
#include "types.h"
#include <vector>
#include <array>

struct AudioBuffer;

namespace DAW {

enum class channel_pairing {
    MONO,
    STEREO,
};

enum class stage_bufferpoint {
    INPUT,
    OUTPUT,
    OUTPUT_POST
};

enum class stage_type {
    INPUT_DEFAULT,
    INPUT_EMPTY,
    INPUT_EXTERNAL_AUDIO,
    INPUT_AUDIOSTAGE,
    INPUT_AUDIOSTAGE_EFFECT
};
enum class midistage_type {
    INPUT_DEFAULT,
    INPUT_EMPTY,
    INPUT_EXTERNAL_MIDI,
    INPUT_AUDIOSTAGE
};

inline bool isStageBufferPointInput(const stage_bufferpoint stBufPt) {
    return stBufPt == stage_bufferpoint::INPUT;
}

namespace AudioIO {

    extern const std::array<samplerate_t, 4> ExtSamplerates;
    extern const std::array<samplerate_t, 4> IntSamplerates;

    struct io_cfg_channel {
        String name;
        int32_t idx          = 0;
        channelnum_t offset  = 0;
        channel_pairing type = channel_pairing::MONO;
    };

    struct io_cfg_tracks {
        bool isInit = false;
        std::vector<io_cfg_channel> input;
        std::vector<io_cfg_channel> output;
    };

    channelnum_t getNumChannelsFromTrackType(channel_pairing t);
    channel_pairing getTrackTypeFromNumChannels(channelnum_t t);
    channelnum_t getNumChannelsInConfig(const std::vector<io_cfg_channel>& cfg);

    String getExternalIOName(channel_pairing type, channelnum_t index, stage_bufferpoint isInput);
    String getTrackName(channel_pairing type, channelnum_t index, bool isInput);
    String getTrackTypeStr(channel_pairing type);
    channel_pairing getNextTrackType(channel_pairing type);

    class AudioStream {
        public:
        struct stream_timings_t {
            samplecount_t samplePos  = 0;
            samplecount_t samplePosProcIn  = 0;
            samplecount_t samplePosProcOut  = 0;
            int64_t tmDeltaCbMin = 0;
            int64_t tmDeltaCbMax = 0;
            int64_t tmDeltaCbAvg = -1;
        };
        virtual ~AudioStream() = default;

        virtual void enqueue(AudioBuffer*) = 0;
        virtual bool try_dequeue(AudioBuffer*&) = 0;
        virtual void enqueueInput(AudioBuffer*) = 0;
        virtual bool try_dequeueInput(AudioBuffer*&) = 0;
        virtual int32_t getOutputQueueSize() const = 0;
        virtual int32_t getInputQueueSize() const = 0;
        virtual samplerate_t getSampleRate() const = 0;
        virtual blocksize_t getBlockSize() const = 0;
        virtual bool isActive() const = 0;
        virtual channelnum_t getNumInputChannels() const = 0;
        virtual channelnum_t getNumOutputChannels() const = 0;
        virtual DAW::AudioIO::AudioStream::stream_timings_t getStreamTimings() const = 0;
        virtual uint32_t getBufferUnderuns() const = 0;
        virtual uint32_t getInputBufferUnderuns() const = 0;
        virtual uint32_t getNumCallbacks() const = 0;
        virtual double getInputTimeSeconds() const = 0;
        virtual double getPlaybackTimeSeconds() const = 0;
        virtual double getOutputTickPos() const = 0;
        virtual double getPlaybackBeginTickPos() const = 0;
        virtual double getStreamInputLatency() const = 0;
        virtual double getStreamOutputLatency() const = 0;
        virtual void onPlayback() = 0;
    };
} // namespace AudioIO

} // namespace DAWmidistage_type
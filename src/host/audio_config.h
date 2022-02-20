#pragma once
#include "str_util.h"
#include "samplerate.h"
#include <cstdint>
#include <vector>

struct AudioBuffer;

enum class stagebuffer_point {
    INPUT,
    OUTPUT,
    OUTPUT_POST
};
inline bool isStageBufferPointInput(const stagebuffer_point stBufPt) {
    return stBufPt == stagebuffer_point::INPUT;
}
namespace AudioIO {

    extern const std::array<uint32_t, 4> ExtSamplerates;
    extern const std::array<uint32_t, 4> IntSamplerates;

    enum tracktype {
        MONO,
        STEREO,
        MULTI_CHANNEL_4,
        MULTI_CHANNEL_6
    };

    struct io_cfg_channel {
        String name;
        int32_t idx           = -1;
        int32_t channelOffset = -1;
        tracktype type        = MONO;
    };

    struct io_cfg_tracks {
        bool isInit = false;
        std::vector<io_cfg_channel> input;
        std::vector<io_cfg_channel> output;
    };

    int32_t getNumChannelsFromTrackType(tracktype t);
    tracktype getTrackTypeFromNumChannels(int32_t t);
    int32_t getNumChannelsInConfig(const std::vector<io_cfg_channel>& cfg);

    String getTrackNameShort(AudioIO::tracktype type, int32_t index, stagebuffer_point isInput);
    String getTrackName(AudioIO::tracktype type, int32_t index, bool isInput);
    String getTrackTypeStr(tracktype type);
    tracktype getNextTrackType(tracktype type);

    class AudioStream {
        public:
        virtual ~AudioStream() = default;

        virtual void enqueue(AudioBuffer*) = 0;
        virtual bool try_dequeue(AudioBuffer*&) = 0;
        virtual void enqueueInput(AudioBuffer*) = 0;
        virtual bool try_dequeueInput(AudioBuffer*&) = 0;
        virtual int32_t getOutputQueueSize() const = 0;
        virtual int32_t getInputQueueSize() const = 0;
        virtual samplerate_t getSampleRate() const = 0;
        virtual uint16_t getBlockSize() const = 0;
        virtual bool isActive() const = 0;
    };
}// namespace AudioIO

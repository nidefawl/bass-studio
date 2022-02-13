#pragma once
#include "str_util.h"
#include <cstdint>
#include <vector>

enum class stagebuffer_point {
    INPUT,
    OUTPUT,
    OUTPUT_POST
};
inline bool isStageBufferPointInput(const stagebuffer_point stBufPt) {
    return stBufPt == stagebuffer_point::INPUT;
}
namespace AudioIO {
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
}// namespace AudioIO

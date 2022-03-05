#pragma once
#include <cstdint>
#include "audio_config.h"

class vsthost;
class project_t;
struct track_impl_t;
struct AudioBlock;
struct track_audio_src;
struct audio_stage_t;
struct io_configuration_snapshot_t;

enum class audiostageid_i32 : int32_t {};

#define TRACKID_INVALID_I32 ((audiostageid_i32) - 1)
#define TRACKID_DEFAULT_I32 (audiostageid_i32) 0

struct audio_stage_ref_t {
    audiostageid_i32 stageId = TRACKID_INVALID_I32;
};
inline audio_stage_ref_t AudioStageRefNULL() {
    return { TRACKID_INVALID_I32 };
}
inline audio_stage_ref_t AudioStageRefFromId(int32_t id) {
    return { static_cast<audiostageid_i32>(id) };
}
struct audio_channel_ref_t {
    audio_stage_ref_t stageRef;
    stagebuffer_point buffer;
};
inline audio_channel_ref_t AudioChannelRefNULL() {
    return { { TRACKID_INVALID_I32 }, stagebuffer_point::OUTPUT_POST };
}

struct audio_stage_id_t {
    audiostageid_i32 stageId           = TRACKID_INVALID_I32;
    audiostageid_i32 inputStageId      = TRACKID_INVALID_I32;
    audiostageid_i32 outputStageId     = TRACKID_INVALID_I32;
    audiostageid_i32 outputPostStageId = TRACKID_INVALID_I32;
};

inline bool audioStageIdMatches(const audio_stage_id_t& stageIds, const audiostageid_i32 stageId) {
    return stageIds.stageId == stageId || stageIds.inputStageId == stageId || stageIds.outputStageId == stageId || stageIds.outputPostStageId == stageId;
}

namespace DAW {

    enum channel_input_type {
        INPUT_DEFAULT,
        INPUT_EMPTY,
        INPUT_EXTERNAL_AUDIO,
        INPUT_AUDIOSTAGE,
        INPUT_AUDIOSTAGE_EFFECT
    };

    struct channel_ref_t {
        channel_input_type type = INPUT_EMPTY;
        AudioIO::tracktype externalInputType;
        int32_t externalInputIdx   = -1;
        int32_t inputChannelOffset = 0;
        audio_channel_ref_t stage{ { TRACKID_INVALID_I32 }, stagebuffer_point::OUTPUT_POST };
        int32_t projectGlobalId = 0;
        String name             = "None";
        channel_input_type getType() const {
            return type;
        }
    };

    enum bus_type {
        external,
        internal
    };

    bool resolveDefaultConnection(const vsthost* host, const project_t* project, track_impl_t* trImpl, bool isInput, channel_ref_t& out);
    bool resolveAudioChannel(const vsthost* host, int32_t numChannelsTrack, const channel_ref_t& inputChannel, const AudioBlock* ptrExternalInputs, track_audio_src& out);

    struct channel_desc {
        int offset = -1;
        int count = -1;
        String name;
    };
}// namespace DAW

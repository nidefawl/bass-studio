#pragma once
#include "types.h"
#include "audio_config.h"

namespace DAW::Host {
    class Host;
    class PluginManager;
}

class project_t;
struct track_impl_t;
struct AudioBlock;
struct track_audio_src;
struct audio_stage_t;
struct io_configuration_snapshot_t;

enum audiostageid_i32 : int32_t {};

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
    DAW::stage_bufferpoint buffer;
};
inline audio_channel_ref_t AudioChannelRefNULL() {
    return { { TRACKID_INVALID_I32 }, DAW::stage_bufferpoint::OUTPUT_POST };
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
    struct midichannel_ref_t {
        midistage_type type = midistage_type::INPUT_EMPTY;
        audio_channel_ref_t stage{ { TRACKID_INVALID_I32 }, stage_bufferpoint::OUTPUT_POST };
        channelnum_t externalInputIdx = 0;
        int32_t srcChannel = -1;
        int32_t dstChannel = -1;
        String name = "None";
        midistage_type getType() const {
            return type;
        }
    };
    struct channel_ref_t {
        stage_type type = stage_type::INPUT_EMPTY;
        channel_pairing externalInputType = channel_pairing::STEREO;
        audio_channel_ref_t stage{ { TRACKID_INVALID_I32 }, stage_bufferpoint::OUTPUT_POST };
        int32_t projectGlobalId = 0;
        channelnum_t externalInputIdx = 0;
        channelnum_t srcChannelOffset = 0;
        channelnum_t dstChannelOffset = 0;
        String name = "None";
        stage_type getType() const {
            return type;
        }
    };
    struct channel_desc {
        channelnum_t offset = 0;
        channelnum_t count = 2;
        String name;
    };

    enum bus_type {
        external,
        internal
    };

    bool resolveDefaultConnection(const Host::PluginManager* host, const project_t* project, track_impl_t* trImpl, bool isInput, channel_ref_t& out);
    bool resolveAudioChannel(const Host::Host* host, channelnum_t numChannelsTrack, const channel_ref_t& inputChannel, const AudioBlock* ptrExternalInputs, track_audio_src& out);

}// namespace DAW

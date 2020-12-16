#pragma once
#include <stdint.h>
#include "audio_config.h"

class vsthost;
class project_t;
struct track_impl_t;
struct AudioBlock;
struct track_audio_src;
struct audio_stage_t;
struct io_configuration_snapshot_t;

//using trackid_i32 = int32_t;
enum class audiostageid_i32 : int32_t {};
#define TRACKID_INVALID_I32 (audiostageid_i32)-1
#define TRACKID_DEFAULT_I32 (audiostageid_i32)0
struct audio_stage_ref_t {
	audiostageid_i32 stageId = TRACKID_INVALID_I32;
};
inline const audio_stage_ref_t AudioStageRefNULL() {
	return {TRACKID_INVALID_I32};
}
struct audio_channel_ref_t {
	audio_stage_ref_t stageRef;
	stagebuffer_point buffer;
};
inline const struct audio_channel_ref_t AudioChannelRefNULL() {
	return {{TRACKID_INVALID_I32}, stagebuffer_point::OUTPUT_POST};
}

namespace DAW {

enum channel_input_type {
	INPUT_DEFAULT, INPUT_EMPTY, INPUT_EXTERNAL_AUDIO, INPUT_AUDIOSTAGE, INPUT_AUDIOSTAGE_EFFECT
};

struct channel_ref_t {
	channel_input_type type = INPUT_EMPTY;
	AudioIO::tracktype externalInputType;
	int32_t externalInputIdx = -1;
	int32_t inputChannelOffset = 0;
	audio_channel_ref_t stage{{TRACKID_INVALID_I32}, stagebuffer_point::OUTPUT_POST};
	int32_t projectGlobalId = 0;
	String name = "None";
//	channel_input_type getType() const {
//		if (externalInputIdx != -1)
//			return channel_input_type::INPUT_EXTERNAL_AUDIO;
//		if (stage.stageRef.stageId != TRACKID_INVALID_I32)
//			return channel_input_type::INPUT_AUDIOSTAGE;
//		return channel_input_type::INPUT_EMPTY;
//	}
	channel_input_type getType() const {
		return type;
	}
};
enum bus_type {
	external, internal
};
bool resolveDefaultConnection(const vsthost* const host, const project_t* const project, track_impl_t* const trImpl, const bool isInput, channel_ref_t& out);
bool resolveAudioChannel(const vsthost* const host, int32_t numChannelsTrack, const channel_ref_t& inputChannel, const AudioBlock* const ptrExternalInputs, track_audio_src& out);

}

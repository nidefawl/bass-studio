#pragma once
#include "str_util.h"
#include <stdint.h>
#include <vector>

enum class stagebuffer_point {
	INPUT, OUTPUT, OUTPUT_POST
};
namespace AudioIO {
	enum tracktype {
		MONO, STEREO, MULTI_CHANNEL_4, MULTI_CHANNEL_6
	};
	struct io_cfg_channel {
		String name;
		int32_t idx = -1;
		int32_t channelOffset = -1;
		tracktype type = MONO;
	};
	struct io_cfg_tracks {
		bool isInit = false;
		std::vector<io_cfg_channel> input;
		std::vector<io_cfg_channel> output;
	};
	int32_t getNumChannelsTrackType(tracktype t);
	tracktype getTrackTypeNumChannels(int32_t t);

	String getTrackNameShort(AudioIO::tracktype type, int32_t index, stagebuffer_point isInput);
	String getTrackName(AudioIO::tracktype type, int32_t index, bool isInput);
}

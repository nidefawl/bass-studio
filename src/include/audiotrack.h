#pragma once
#include "audiosample.h"
#include "audioblock.h"
#include <stdint.h>
#include <memory>

class vsthost;
struct audiotrack_split_t : public samplesource_t {
	int64_t sampleId = 0;
	int64_t version = 0;
	int32_t samplePos;
	audiosample_t sample;
	audiosample_t* getSample() override {
		return &sample;
	}
};
struct audiotrack_block_t {
	int64_t version = 0;
	AudioBlock data;
	audiotrack_block_t(uint32_t _channels, uint32_t _samples) : data(_channels, _samples) {

	}
};
struct audiotrack_t {
	std::vector<std::shared_ptr<audiotrack_block_t>> data;
	std::vector<std::shared_ptr<audiotrack_split_t>> samples;
	std::shared_ptr<audiotrack_split_t> getSample(int32_t samplePos);
	int32_t convertToSamples(vsthost* host);
	void store(AudioBlock* input, int32_t samplePos);
	~audiotrack_t() {
	}
};

#include "TestBase.hpp"
#include <vector>
#include <stdint.h>
#include <memory>
#include "seq_time.h"
#include "clip.h"
#include "project.h"
#include "../host/mainctrl.h"
#include "../host/resampler.h"
#include "test_common.h"

namespace {


class test_impl {
public:
	std::vector<std::shared_ptr<resampler_t>> resamplers;
	std::shared_ptr<oversampler_t> oversampler;

	std::shared_ptr<resampler_t> getResampler(sampleformat_t in, sampleformat_t out, int32_t idx) {
		auto it = std::find_if(resamplers.begin(), resamplers.end(), [&in,&out](std::shared_ptr<resampler_t>& ptr){
			return ptr->in == in && ptr->out == out;
		});
		if (it == resamplers.end()) {

			oversample_config_t config;
			config.inputSampleRate = in.sampleRate;
			config.outputSampleRate = out.sampleRate;
			config.numChannels = 32;
			config.setInputLength(in.blockSize);
			std::shared_ptr<resampler_t> resampler = std::make_shared<resampler_t>(idx, in, out, config);
			resamplers.push_back(resampler);
			return resampler;

		}
		return *it;
	}
	test_impl() {

	}
	~test_impl() {

	}
};
void testAudioBlock() {
	ALEPH_TEST_BEGIN("testAudioBlock");
	test_impl impl;
	const uint32_t numChannels = 2;
	const uint32_t blockSize = 512;
	sampleformat_t sfIn = sampleformat_t{48000, blockSize, sampleformat_bits_t::FLOAT_32};
	sampleformat_t sfOut = sampleformat_t{44100, blockSize, sampleformat_bits_t::FLOAT_32};
	std::shared_ptr<resampler_t> ptr = impl.getResampler(sfIn, sfOut, 0);
	ALEPH_ASSERT_THROW(ptr.get() != nullptr);
	AudioBlock block(numChannels, blockSize);

	int32_t noiseSeed = 13;
	int32_t popped = 0;
	int32_t pushed = 0;
	while (ptr->numBlocksToPop() < 4) {
		block.fillNoise(noiseSeed++);
		ptr->push(block);
		pushed++;
	}
	ALEPH_ASSERT_THROW(pushed);
	while (ptr->numBlocksToPop() > 0) {
		AudioBlock blockOut = ptr->pop();
		blockOut.fillNoise(noiseSeed++);
		popped++;
	}
	ALEPH_ASSERT_THROW(popped);
	ALEPH_TEST_END();
}

}
int main() {
	testAudioBlock();
	return 0;
}

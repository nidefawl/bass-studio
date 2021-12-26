#include "TestBase.hpp"
#include <vector>
#include <memory>
#include "seq_time.h"
#include "../host/resampler.h"
#include "test_common.h"

namespace {


    class test_impl {
    public:
        std::vector<std::shared_ptr<resampler_t>> resamplers;

        std::shared_ptr<resampler_t> getResampler(sampleformat_t in, sampleformat_t out, int32_t idx) {
            auto it = std::find_if(resamplers.begin(), resamplers.end(), [&in, &out](std::shared_ptr<resampler_t>& ptr) {
                return ptr->in == in && ptr->out == out;
            });
            if (it == resamplers.end()) {

                oversample_config_t config;
                config.inputSampleRate  = in.sampleRate;
                config.outputSampleRate = out.sampleRate;
                config.numChannels      = 32;
                config.setInputLength(in.blockSize);
                std::shared_ptr<resampler_t> resampler = std::make_shared<resampler_t>(idx, in, out, config);
                resamplers.push_back(resampler);
                return resampler;
            }
            return *it;
        }
    };
    void testAudioBlock() {
        TEST_BEGIN("testAudioBlock");
        test_impl impl;
        const uint32_t numChannels = 2;
        const uint32_t blockSize   = 512;
        sampleformat_t sfIn        = sampleformat_t{ 48000, blockSize, sampleformat_bits_t::FLOAT_32 };
        sampleformat_t sfOut       = sampleformat_t{ 44100, blockSize, sampleformat_bits_t::FLOAT_32 };

        std::shared_ptr<resampler_t> ptr = impl.getResampler(sfIn, sfOut, 0);

        TEST_ASSERT_THROW(ptr.get());

        AudioBlock block(numChannels, blockSize);

        uint32_t noiseSeed = 13;
        int32_t popped     = 0;
        int32_t pushed     = 0;
        while (ptr->numBlocksToPop() < 4) {
            block.fillNoise(noiseSeed++);
            ptr->push(block);
            pushed++;
        }
        TEST_ASSERT_THROW(pushed);
        while (ptr->numBlocksToPop() > 0) {
            AudioBlock blockOut = ptr->pop();
            blockOut.fillNoise(noiseSeed++);
            popped++;
        }
        TEST_ASSERT_THROW(popped);
        TEST_END();
    }

}// namespace
int main() {
    testAudioBlock();
    return 0;
}

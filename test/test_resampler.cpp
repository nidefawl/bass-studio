#include "TestBase.hpp"
#include <vector>
#include <memory>
#include "seq_time.h"
#include "host/resampler.h"
#include "audioblock.h"
#include "common/test_common.h"

namespace {


    class test_impl {
    public:
        std::vector<std::shared_ptr<resampler_t>> resamplers;

        std::shared_ptr<resampler_t> getResampler(sampleformat_t in, sampleformat_t out, channelnum_t numChannels) {
            auto it = std::find_if(resamplers.begin(), resamplers.end(), [&](std::shared_ptr<resampler_t>& ptr) {
                return ptr->in == in && ptr->out == out && ptr->numChannels == numChannels;
            });
            if (it == resamplers.end()) {

                oversample_config_t config;
                config.inputSampleRate  = in.sampleRate;
                config.outputSampleRate = out.sampleRate;
                config.numChannels      = numChannels;
                config.setInputLength(in.blockSize);
                auto idx = static_cast<int32_t>(resamplers.size());
                std::shared_ptr<resampler_t> resampler = std::make_shared<resampler_t>(idx, in, out, config);
                resamplers.push_back(resampler);
                return resampler;
            }
            return *it;
        }
        void testResamplerConfig(sampleformat_t sfIn, sampleformat_t sfOut, channelnum_t numChannels) {
            auto strTestConfig = StringFormat("%u channels %u Hz@%u Samples -> %u Hz@%u Samples",
                                              numChannels, sfIn.sampleRate, sfIn.blockSize, sfOut.sampleRate, sfOut.blockSize);
            TEST_BEGIN("testResampler with "+strTestConfig);

            std::shared_ptr<resampler_t> ptr = getResampler(sfIn, sfOut, numChannels);

            TEST_ASSERT_THROW(ptr.get());

            AudioBlock block(numChannels, sfIn.blockSize);

            uint32_t noiseSeed = 13;
            int32_t popped     = 0;
            int32_t pushed     = 0;
            int32_t maxIterations = 10000;
            while (true) {
                if (maxIterations > 0) {
                    maxIterations--;
                    if (ptr->numBlocksToPop() < 4) {
                        block.fillNoise(noiseSeed++);
                        ptr->push(block);
                        pushed++;
                    }
                }
                while (ptr->numBlocksToPop() > 0) {
                    AudioBlock blockOut = ptr->pop();
                    TEST_ASSERT_THROW(blockOut.samples == sfOut.blockSize);
                    TEST_ASSERT_THROW(blockOut.channels == numChannels);
                    popped++;
                }
                if (maxIterations <= 0 || popped > 10){
                    break;
                }
            }
            TEST_ASSERT_THROW(pushed);
            TEST_ASSERT_THROW(popped);
            TEST_END();
        }
    };
    void testResampler() {
        test_impl impl;
        std::vector<blocksize_t> blockSizes;
        for (blocksize_t i = 16; i <= 1024*8; i<<=2) {
            blockSizes.push_back(i);
        }
        for (blocksize_t i = 16; i <= 1024*8; i<<=2) {
            blockSizes.push_back(i + 13);
        }

        sampleformat_t sfIn        = sampleformat_t{ 0, 0, sampleformat_bits_t::FLOAT_32 };
        sampleformat_t sfOut       = sampleformat_t{ 0, 0, sampleformat_bits_t::FLOAT_32 };

        sfIn.blockSize = sfOut.blockSize = 1024;
        sfIn.sampleRate = 44100; sfOut.sampleRate = 48000;
        impl.testResamplerConfig(sfIn, sfOut, 2);
        impl.testResamplerConfig(sfIn, sfOut, 1);
        impl.testResamplerConfig(sfIn, sfOut, 3);
        impl.testResamplerConfig(sfIn, sfOut, 4);
        impl.testResamplerConfig(sfIn, sfOut, 5);

        const samplerate_t sampleRates[] = {44100, 48000, 96000, 192000};
        for (auto blockSizeIn : blockSizes) {
            sfIn.blockSize = blockSizeIn;
            for (auto blockSizeOut : blockSizes) {
                sfOut.blockSize = blockSizeOut;
                for (auto srIn : sampleRates) {
                    sfIn.sampleRate = srIn;
                    for (auto srOut : sampleRates) {
                        sfOut.sampleRate = srOut;
                        impl.testResamplerConfig(sfIn, sfOut, 2);
                    }
                }
            }
        }
    }

}// namespace
int main() {
    testResampler();
    return 0;
}

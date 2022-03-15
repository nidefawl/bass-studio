#include "TestBase.hpp"
#include "audioblock.h"
#include "common/test_common.h"
#include "types.h"
#include "util/testing_environment.h"
#include <vector>

namespace test_audioblock {
    void testExternAllocation() {
        TEST_BEGIN("testExternAllocation");
        const channelnum_t numChannels = 2;
        const blocksize_t  blockSize   = 512;
        std::vector<float> vecFloatSamples;
        vecFloatSamples.resize(blockSize * numChannels);
        std::vector<float*> vecFloatPtrs(numChannels);
        vecFloatPtrs[0] = vecFloatSamples.data();
        vecFloatPtrs[1] = vecFloatPtrs[0] + blockSize;

        {
            AudioBlock block(vecFloatPtrs, blockSize);
            AudioBlock block2(block.channels, block.samples);
            block.copyFrom(&block2);
            block2.copyFrom(&block);
        }

        {

            AudioBlock block(vecFloatPtrs, blockSize);
            float** pBuf = block.buf;
            AudioBlock block2(pBuf, block.channels, block.samples);
            AudioBlock block3(block2.channels, block2.samples);
            block2.copyFrom(&block3);
            block3.copyFrom(&block2);
        }

        {

            AudioBlock block(vecFloatPtrs, blockSize);

            AudioBlock block2 = block.SubChannelsBlock(0, 1);
            AudioBlock block3(block2.channels, block2.samples);
            block2.copyFrom(&block3);
            block3.copyFrom(&block2);
        }

        {

            AudioBlock block(vecFloatPtrs, blockSize);

            AudioBlock block2 = block.SubChannelsSamplesBlock(0, 1, 32, 32);
            AudioBlock block3(block2.channels, block2.samples);
            block2.copyFrom(&block3);
            block3.copyFrom(&block2);
        }

        TEST_END();
    }
    void testChannelCount() {
        TEST_BEGIN("testChannelCount");
        const blocksize_t  blockSize   = 512;
        std::vector<float> vecFloatSamples;
        for (int64_t numChannels = 0; numChannels <= 16; numChannels++) {
            AudioBlock blockA(numChannels, blockSize);
            AudioBlock blockB(numChannels, blockSize);
            blockA.copyFrom(&blockB);
        }
        for (int64_t numChannels = 16; numChannels < 16*8; numChannels+=8) {
            AudioBlock blockA(numChannels, blockSize);
            AudioBlock blockB(numChannels, blockSize);
            blockA.copyFrom(&blockB);
        }
        for (int64_t numChannels = 250; numChannels < 256; numChannels++) {
            AudioBlock blockA(numChannels, blockSize);
            AudioBlock blockB(numChannels, blockSize);
            blockA.copyFrom(&blockB);
        }
        for (int64_t numChannels = 2; numChannels <= 255; numChannels+=7) {
            AudioBlock blockA(numChannels, blockSize);
            AudioBlock blockB(numChannels, blockSize);
            AudioBlock blockASub = blockA.SubChannelsBlock(numChannels>>1, numChannels>>1);
            blockASub.copyFrom(&blockB);
            blockB.copyFrom(&blockASub);
        }
        TEST_END();
    }
    void testCopy() {
        TEST_BEGIN("testCopy");
        const channelnum_t numChannels = 2;
        const blocksize_t  blockSize   = 512;
        AudioBlock block(numChannels, blockSize);
        AudioBlock block2(numChannels, blockSize);
        AudioBlock blockHalf(numChannels, blockSize>>1);
        AudioBlock block1Channel(1, blockSize);
        AudioBlock block3Channel(3, blockSize);
        float** pBuf = block2.buf;
        const float * const * pConst = block2.buf;
        block.copyFromPosToPos(pConst, 10, 20, 30, numChannels);
        block.copyFromPosToPos(pBuf, 10, 20, 30, numChannels);
        block2.copyFromPosToPos(pBuf, 10, 20, 30, numChannels);
        block2.copyFrom(&block1Channel);
        block3Channel.SubChannelsBlock(1, 1).copyFrom(&block1Channel);
        block3Channel.SubChannelsBlock(1, 1).copyFrom(&block1Channel);

        TEST_EXPECT_EXCEPTION(blockHalf.copyFrom(&block1Channel), daw_test::failed_assert_exception);
        TEST_EXPECT_EXCEPTION(blockHalf.copyFrom(&block3Channel), daw_test::failed_assert_exception);
        TEST_EXPECT_EXCEPTION(block2.copyFrom(&blockHalf), daw_test::failed_assert_exception);
        TEST_END();
    }
}// namespace

int main() {
    daw_test::testThrowAssertEnabled = true;
    test_audioblock::testCopy();
    test_audioblock::testExternAllocation();
    test_audioblock::testChannelCount();
    return 0;
}

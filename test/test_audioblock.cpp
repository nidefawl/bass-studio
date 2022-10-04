#include "TestBase.hpp"
#include "audioblock.h"
#include "common/test_common.h"
#include "types.h"
#include "util/testing_environment.h"
#include <utility>
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
        const AudioBlock blockInputData(numChannels, blockSize);
        const float* const* const ppInputData = blockInputData.buf;

        AudioBlock block(numChannels, blockSize);
        AudioBlock block3(numChannels, blockSize);
        AudioBlock blockHalf(numChannels, blockSize>>1);
        AudioBlock block1Channel(1, blockSize);
        AudioBlock block3Channel(3, blockSize);
        const float * const * pConst = blockInputData.buf;
        block.copyFromPosToPos(pConst, 10, 20, 30, numChannels);
        block.copyFromPosToPos(ppInputData, 10, 20, 30, numChannels);
        block3.copyFromPosToPos(ppInputData, 10, 20, 30, numChannels);
        block3.copyFrom(&block1Channel);
        block3Channel.SubChannelsBlock(1, 1).copyFrom(&block1Channel);
        block3Channel.SubChannelsBlock(1, 1).copyFrom(&block1Channel);

        TEST_EXPECT_EXCEPTION(blockHalf.copyFrom(&block1Channel), daw_test::failed_assert_exception);
        TEST_EXPECT_EXCEPTION(blockHalf.copyFrom(&block3Channel), daw_test::failed_assert_exception);
        TEST_EXPECT_EXCEPTION(block3.copyFrom(&blockHalf), daw_test::failed_assert_exception);
        TEST_END();
    }
    void testSelfOverlappingCopy() {
        TEST_BEGIN("testSelfOverlappingCopy");
        const channelnum_t numChannels = 2;
        const blocksize_t  blockSize   = 512;
        AudioBlock block(numChannels, blockSize);
        AudioBlock block2(numChannels, blockSize);
        const float* const* const pBuf = block2.buf;
        const float * const * pConst = block2.buf;
        samplecount_t copyLen = 30;
        float testInputBuf[copyLen];
        for (samplecount_t i = 0; i < copyLen; i++) {
            testInputBuf[i] = (float)i;
        }
        const samplecount_t copyDstPos = 10;
        const samplecount_t copySrcPos = 10;
        const float const* testInputArrays[] = {&testInputBuf[0], &testInputBuf[0]};
        for (channelnum_t j = 0; j < numChannels; j++) {
            block2.SubChannelsBlock(j, 1).copyFromPosToPos(testInputArrays, 0, copySrcPos, copyLen, 1);
            for (samplecount_t i = 0; i < copyLen; i++) {
                TEST_ASSERT_EQUAL(block2.buf[j][copySrcPos + i], (float)i);
            }
        }
        block2.copyFromPosToPos(pBuf, copySrcPos, copyDstPos, copyLen, numChannels);
        for (channelnum_t j = 0; j < numChannels; j++) {
            for (samplecount_t i = 0; i < copyLen; i++) {
                TEST_ASSERT_EQUAL(block2.buf[j][copyDstPos + i], (float)i);
            }
        }
        auto srcBlock = block2.getOffsetBlock(copySrcPos);
        auto dstBlock = block2.getOffsetBlock(copyDstPos);
        srcBlock.samples = copyLen;
        dstBlock.copyFrom(&srcBlock);
        // block2.copyFrom(&subBlock);
        TEST_END();
    }
    void testType() {
        struct Temp {
            AudioBlock block;
        } tmp;
        auto test = AudioBlock{2, 512};
        tmp.block = AudioBlock{4, 1024};
        tmp.block.realloc(512);
        tmp.block = std::move(test);
    }
}// namespace

int main() {
    daw_test::testThrowAssertEnabled = true;
    test_audioblock::testCopy();
    test_audioblock::testExternAllocation();
    test_audioblock::testChannelCount();
    test_audioblock::testSelfOverlappingCopy();
    test_audioblock::testType();
    return 0;
}

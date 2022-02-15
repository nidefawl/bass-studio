#include "TestBase.hpp"
#include "audioblock.h"
#include "common/test_common.h"
#include "util/testing_environment.h"

namespace {
    void testCopy() {
        const uint32_t numChannels = 2;
        const uint32_t blockSize   = 512;
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
    }
}// namespace

int main() {
    daw_test::testThrowAssertEnabled = true;
    testCopy();
    return 0;
}

#include "TestBase.hpp"
#include <vector>
#include <stdint.h>
#include "seq_time.h"
#include "clip.h"
#include "project.h"
#include "test_common.h"

namespace {
	void testTickConversions() {
		ALEPH_TEST_BEGIN("testTickConversions");
		project_globals_t project;
		samplerate_t samplerate = 44100;
		int32_t blocksize = 512;
		int32_t tempo100 = 12800;
		int32_t blockPos = 0;
		for (blockPos = 0; blockPos < 160000; blockPos++) {
			double blockStartTick = blockToTickPrecise(blockPos, tempo100, samplerate, blocksize);
	//		printf("BLOCK %d blockToTickPrecise %f\n", blockPos, blockStartTick);
			double block = tickToBlockPrecise(blockStartTick, tempo100, samplerate, blocksize);
	//		double rounded = round(block);
	//		int32_t blockPosI = (int32_t) rounded;
	//		printf("blockStartTick %f tickToBlockPrecise %f, to int %d\n", blockStartTick, block, blockPosI);
			ALEPH_ASSERT_THROW(almost_equal(static_cast<double>(blockPos), block, 2));
	//		dbgassert((double)blockPos == block);
		}
		ALEPH_TEST_END();
	}
}
int main() {
	testTickConversions();
	return 0;
}

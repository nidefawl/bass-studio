#include "TestBase.hpp"
#include <vector>
#include <stdint.h>
#include "seq_time.h"
#include "math/seq_math.h"
#include "clip.h"
#include "project.h"
#include "track.h"
#include "host/projectcontroller.h"
#include "tests/common/test_common.h"

namespace {
    void testTickConversions() {
        TEST_BEGIN("testTickConversions");
        project_globals_t project;
        samplerate_t samplerate = 44100;
        int32_t blocksize       = 512;
        int32_t tempo100        = 12800;
        int32_t blockPos        = 0;
        for (blockPos = 0; blockPos < 160000; blockPos++) {
        }
        TEST_END();
    }
    void testTimeSignatureConversion() {
        TEST_BEGIN("testTimeSignatureConversion");
        project_t project;
        project_globals_t projectGlobals;
        project_controller_t projectController{ &project, &projectGlobals };
        projectGlobals.signatureNum   = 4;
        projectGlobals.signatureDenom = 2;
        {
            beatbar16th_t timeSig = projectController.toBeatBar16th(0);
            TEST_ASSERT_THROW(timeSig.bar == 0);
            TEST_ASSERT_THROW(timeSig.beat == 0);
            TEST_ASSERT_THROW(timeSig.th == 0);
        }
        {
            beatbar16th_t timeSig = projectController.toBeatBar16th(TICKS_16TH);
            TEST_ASSERT_THROW(timeSig.bar == 0);
            TEST_ASSERT_THROW(timeSig.beat == 0);
            TEST_ASSERT_THROW(timeSig.th == 1);
        }
        {
            beatbar16th_t timeSig = projectController.toBeatBar16th(TICKS_QUARTER);
            TEST_ASSERT_THROW(timeSig.bar == 0);
            TEST_ASSERT_THROW(timeSig.beat == 1);
            TEST_ASSERT_THROW(timeSig.th == 0);
        }
        {
            beatbar16th_t timeSig = projectController.toBeatBar16th(TICKS_BAR);
            TEST_ASSERT_THROW(timeSig.bar == 1);
            TEST_ASSERT_THROW(timeSig.beat == 0);
            TEST_ASSERT_THROW(timeSig.th == 0);
        }
        {
            beatbar16th_t timeSig = projectController.toBeatBar16th(TICKS_BAR * 200);
            TEST_ASSERT_THROW(timeSig.bar == 200);
            TEST_ASSERT_THROW(timeSig.beat == 0);
            TEST_ASSERT_THROW(timeSig.th == 0);
        }
        {
            beatbar16th_t timeSig = projectController.toBeatBar16th(-1);
            TEST_ASSERT_THROW(timeSig.bar == -1);
            TEST_ASSERT_THROW(timeSig.beat == 3);
            TEST_ASSERT_THROW(timeSig.th == 3);
        }
        {
            beatbar16th_t timeSig = projectController.toBeatBar16th(-(TICKS_BAR));
            TEST_ASSERT_THROW(timeSig.bar == -1);
            TEST_ASSERT_THROW(timeSig.beat == 0);
            TEST_ASSERT_THROW(timeSig.th == 0);
        }
        {
            beatbar16th_t timeSig = projectController.toBeatBar16th(-(TICKS_BAR + 1));
            TEST_ASSERT_THROW(timeSig.bar == -2);
            TEST_ASSERT_THROW(timeSig.beat == 3);
            TEST_ASSERT_THROW(timeSig.th == 3);
        }
        TEST_END();
    }
}// namespace
int main() {
    testTickConversions();
    testTimeSignatureConversion();
    return 0;
}

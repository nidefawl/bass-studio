#include "TestBase.hpp"
#include <vector>
#include "types.h"
#include "seq_time.h"
#include "math/seq_math.h"
#include "host/clip/clip.h"
#include "host/project/project.h"
#include "host/track/track.h"
#include "host/project/projectcontroller.h"
#include "common/test_common.h"

namespace {
    void test_TickConversions() {
        TEST_BEGIN("testTickConversions");
        project_globals_t project;
        samplerate_t samplerate = 44100;
        int32_t blocksize       = 512;
        int32_t tempo100        = 12800;
        tick_t tick = 0;
        tickToSampleConvert<double, roundmode::none>(0, tempo100, samplerate);
        tickToSampleConvert<double, roundmode::floor>(0, tempo100, samplerate);
        tickToSampleConvert<double, roundmode::floorclamp>(0, tempo100, samplerate);
        tickToSampleConvert<double, roundmode::round>(0, tempo100, samplerate);
        tickToSampleConvert<double, roundmode::ceil>(0, tempo100, samplerate);
        sampleToTickConvert<int32_t, roundmode::none>(0, tempo100, samplerate);
        sampleToTickConvert<int32_t, roundmode::floor>(0, tempo100, samplerate);
        sampleToTickConvert<int32_t, roundmode::floorclamp>(0, tempo100, samplerate);
        sampleToTickConvert<int32_t, roundmode::round>(0, tempo100, samplerate);
        sampleToTickConvert<int32_t, roundmode::ceil>(0, tempo100, samplerate);
        TEST_END();
    }
    void test_tickToBarBeat16thAbsolute() {
        TEST_BEGIN("test_tickToBarBeat16thAbsolute");
        uint32_t signatureNum   = 4;
        uint32_t signatureDenomBits = 2; // exponent of 2
        const bool isRelative = false;
        {
            tick_t tick = 0;
            auto timeBeat = tickToBarBeat16th(tick, signatureNum, signatureDenomBits, isRelative);
            TEST_ASSERT_THROW(timeBeat.bar == 0);
            TEST_ASSERT_THROW(timeBeat.beat == 0);
            TEST_ASSERT_THROW(timeBeat.th == 0);
            TEST_ASSERT_THROW(timeBeat.subticks == 0);
        }
        {
            tick_t tick = 1;
            auto timeBeat = tickToBarBeat16th(tick, signatureNum, signatureDenomBits, isRelative);
            TEST_ASSERT_THROW(timeBeat.bar == 0);
            TEST_ASSERT_THROW(timeBeat.beat == 0);
            TEST_ASSERT_THROW(timeBeat.th == 0);
            TEST_ASSERT_THROW(timeBeat.subticks == 1);
        }
        {
            tick_t tick = TICKS_16TH;
            auto timeBeat = tickToBarBeat16th(tick, signatureNum, signatureDenomBits, isRelative);
            TEST_ASSERT_THROW(timeBeat.bar == 0);
            TEST_ASSERT_THROW(timeBeat.beat == 0);
            TEST_ASSERT_THROW(timeBeat.th == 1);
            TEST_ASSERT_THROW(timeBeat.subticks == 0);
        }
        {
            tick_t tick = TICKS_QUARTER;
            auto timeBeat = tickToBarBeat16th(tick, signatureNum, signatureDenomBits, isRelative);
            TEST_ASSERT_THROW(timeBeat.bar == 0);
            TEST_ASSERT_THROW(timeBeat.beat == 1);
            TEST_ASSERT_THROW(timeBeat.th == 0);
            TEST_ASSERT_THROW(timeBeat.subticks == 0);
        }
        {
            tick_t tick = TICKS_BAR;
            auto timeBeat = tickToBarBeat16th(tick, signatureNum, signatureDenomBits, isRelative);
            TEST_ASSERT_THROW(timeBeat.bar == 1);
            TEST_ASSERT_THROW(timeBeat.beat == 0);
            TEST_ASSERT_THROW(timeBeat.th == 0);
            TEST_ASSERT_THROW(timeBeat.subticks == 0);
        }
        {
            tick_t tick = TICKS_BAR * 200;
            auto timeBeat = tickToBarBeat16th(tick, signatureNum, signatureDenomBits, isRelative);
            TEST_ASSERT_THROW(timeBeat.bar == 200);
            TEST_ASSERT_THROW(timeBeat.beat == 0);
            TEST_ASSERT_THROW(timeBeat.th == 0);
            TEST_ASSERT_THROW(timeBeat.subticks == 0);
        }
        {
            tick_t tick = -1;
            auto timeBeat = tickToBarBeat16th(tick, signatureNum, signatureDenomBits, isRelative);
            TEST_ASSERT_THROW(timeBeat.bar == -1);
            TEST_ASSERT_THROW(timeBeat.beat == 3);
            TEST_ASSERT_THROW(timeBeat.th == 3);
            TEST_ASSERT_THROW(timeBeat.subticks == 1023);
        }
        {
            tick_t tick = -(TICKS_BAR);
            auto timeBeat = tickToBarBeat16th(tick, signatureNum, signatureDenomBits, isRelative);
            TEST_ASSERT_THROW(timeBeat.bar == -1);
            TEST_ASSERT_THROW(timeBeat.beat == 0);
            TEST_ASSERT_THROW(timeBeat.th == 0);
            TEST_ASSERT_THROW(timeBeat.subticks == 0);
        }
        {
            tick_t tick = -(TICKS_BAR + 1);
            auto timeBeat = tickToBarBeat16th(tick, signatureNum, signatureDenomBits, isRelative);
            TEST_ASSERT_THROW(timeBeat.bar == -2);
            TEST_ASSERT_THROW(timeBeat.beat == 3);
            TEST_ASSERT_THROW(timeBeat.th == 3);
            TEST_ASSERT_THROW(timeBeat.subticks == 1023);
        }
        TEST_END();
    }

    void test_tickToBarBeat16thRelative() {
        TEST_BEGIN("test_tickToBarBeat16thRelative");
        uint32_t signatureNum   = 4;
        uint32_t signatureDenomBits = 2; // exponent of 2
        const bool isRelative = true;
        {
            tick_t tick = 0;
            auto timeBeat = tickToBarBeat16th(tick, signatureNum, signatureDenomBits, isRelative);
            TEST_ASSERT_THROW(timeBeat.bar == 0);
            TEST_ASSERT_THROW(timeBeat.beat == 0);
            TEST_ASSERT_THROW(timeBeat.th == 0);
            TEST_ASSERT_THROW(timeBeat.subticks == 0);
        }
        {
            tick_t tick = 1;
            auto timeBeat = tickToBarBeat16th(tick, signatureNum, signatureDenomBits, isRelative);
            TEST_ASSERT_THROW(timeBeat.bar == 0);
            TEST_ASSERT_THROW(timeBeat.beat == 0);
            TEST_ASSERT_THROW(timeBeat.th == 0);
            TEST_ASSERT_THROW(timeBeat.subticks == 1);
        }
        {
            tick_t tick = TICKS_16TH;
            auto timeBeat = tickToBarBeat16th(tick, signatureNum, signatureDenomBits, isRelative);
            TEST_ASSERT_THROW(timeBeat.bar == 0);
            TEST_ASSERT_THROW(timeBeat.beat == 0);
            TEST_ASSERT_THROW(timeBeat.th == 1);
            TEST_ASSERT_THROW(timeBeat.subticks == 0);
        }
        {
            tick_t tick = TICKS_QUARTER;
            auto timeBeat = tickToBarBeat16th(tick, signatureNum, signatureDenomBits, isRelative);
            TEST_ASSERT_THROW(timeBeat.bar == 0);
            TEST_ASSERT_THROW(timeBeat.beat == 1);
            TEST_ASSERT_THROW(timeBeat.th == 0);
            TEST_ASSERT_THROW(timeBeat.subticks == 0);
        }
        {
            tick_t tick = TICKS_BAR;
            auto timeBeat = tickToBarBeat16th(tick, signatureNum, signatureDenomBits, isRelative);
            TEST_ASSERT_THROW(timeBeat.bar == 1);
            TEST_ASSERT_THROW(timeBeat.beat == 0);
            TEST_ASSERT_THROW(timeBeat.th == 0);
            TEST_ASSERT_THROW(timeBeat.subticks == 0);
        }
        {
            tick_t tick = TICKS_BAR * 200;
            auto timeBeat = tickToBarBeat16th(tick, signatureNum, signatureDenomBits, isRelative);
            TEST_ASSERT_THROW(timeBeat.bar == 200);
            TEST_ASSERT_THROW(timeBeat.beat == 0);
            TEST_ASSERT_THROW(timeBeat.th == 0);
            TEST_ASSERT_THROW(timeBeat.subticks == 0);
        }
        {
            tick_t tick = -1;
            auto timeBeat = tickToBarBeat16th(tick, signatureNum, signatureDenomBits, isRelative);
            TEST_ASSERT_THROW(timeBeat.bar == -1);
            TEST_ASSERT_THROW(timeBeat.beat == 0);
            TEST_ASSERT_THROW(timeBeat.th == 0);
            TEST_ASSERT_THROW(timeBeat.subticks == 1);
        }
        {
            tick_t tick = -TICKS_QUARTER;
            auto timeBeat = tickToBarBeat16th(tick, signatureNum, signatureDenomBits, isRelative);
            TEST_ASSERT_THROW(timeBeat.bar == -1);
            TEST_ASSERT_THROW(timeBeat.beat == 1);
            TEST_ASSERT_THROW(timeBeat.th == 0);
            TEST_ASSERT_THROW(timeBeat.subticks == 0);
        }
        {
            tick_t tick = -TICKS_16TH;
            auto timeBeat = tickToBarBeat16th(tick, signatureNum, signatureDenomBits, isRelative);
            TEST_ASSERT_THROW(timeBeat.bar == -1);
            TEST_ASSERT_THROW(timeBeat.beat == 0);
            TEST_ASSERT_THROW(timeBeat.th == 1);
            TEST_ASSERT_THROW(timeBeat.subticks == 0);
        }
        {
            tick_t tick = -1 - TICKS_BAR;
            auto timeBeat = tickToBarBeat16th(tick, signatureNum, signatureDenomBits, isRelative);
            auto timeBeat3 = tickToBarBeat16th(-1, signatureNum, signatureDenomBits, isRelative);
            auto timeBeat2 = tickToBarBeat16th(-1 - TICKS_BAR*2, signatureNum, signatureDenomBits, isRelative);
            TEST_ASSERT_THROW(timeBeat.bar == -2);
            TEST_ASSERT_THROW(timeBeat.beat == 0);
            TEST_ASSERT_THROW(timeBeat.th == 0);
            TEST_ASSERT_THROW(timeBeat.subticks == 1);
        }
        TEST_END();
    }
    void test_beatBarNthToTickAbsolute() {
        TEST_BEGIN("test_beatBarNthToTickAbsolute");
        uint32_t signatureNum   = 4;
        uint32_t signatureDenomBits = 2; // exponent of 2
        const bool isRelative = false;
        {
            beatbar16th_t beat{0, 0, 0, 0};
            auto timeBeat = beatBarNthToTick(beat, signatureNum, signatureDenomBits, isRelative);
            TEST_ASSERT_EQUAL(timeBeat, 0);
        }
        {
            beatbar16th_t beat{0, 1, 0, 0};
            auto timeBeat = beatBarNthToTick(beat, signatureNum, signatureDenomBits, isRelative);
            TEST_ASSERT_EQUAL(timeBeat, TICKS_QUARTER);
        }
        {
            beatbar16th_t beat{0, 0, 1, 0};
            auto timeBeat = beatBarNthToTick(beat, signatureNum, signatureDenomBits, isRelative);
            TEST_ASSERT_EQUAL(timeBeat, TICKS_16TH);
        }
        {
            beatbar16th_t beat{0, 0, 0, 1};
            auto timeBeat = beatBarNthToTick(beat, signatureNum, signatureDenomBits, isRelative);
            TEST_ASSERT_EQUAL(timeBeat, 1);
        }
        {
            beatbar16th_t beat{2, 2, 2, 2};
            auto timeBeat = beatBarNthToTick(beat, signatureNum, signatureDenomBits, isRelative);
            TEST_ASSERT_EQUAL(timeBeat, 2*TICKS_BAR + 2*TICKS_QUARTER + 2*TICKS_16TH + 2);
        }
        {
            beatbar16th_t beat{-1, 0, 0, 0};
            auto timeBeat = beatBarNthToTick(beat, signatureNum, signatureDenomBits, isRelative);
            TEST_ASSERT_EQUAL(timeBeat, -1*TICKS_BAR);
        }
        {
            beatbar16th_t beat{-2, 0, 0, 0};
            auto timeBeat = beatBarNthToTick(beat, signatureNum, signatureDenomBits, isRelative);
            TEST_ASSERT_EQUAL(timeBeat, -2*TICKS_BAR);
        }
        {
            beatbar16th_t beat{-1, 3, 3, 0};
            auto timeBeat = beatBarNthToTick(beat, signatureNum, signatureDenomBits, isRelative);
            TEST_ASSERT_EQUAL(timeBeat, (-TICKS_16TH));
        }
        {
            beatbar16th_t beat{-2, 3, 3, 0};
            auto timeBeat = beatBarNthToTick(beat, signatureNum, signatureDenomBits, isRelative);
            TEST_ASSERT_EQUAL(timeBeat, -(TICKS_16TH+TICKS_BAR));
        }
        TEST_END();
    }

    void test_beatBarNthToTickRelative() {
        TEST_BEGIN("test_beatBarNthToTickRelative");
        uint32_t signatureNum   = 4;
        uint32_t signatureDenomBits = 2; // exponent of 2
        const bool isRelative = true;
        {
            beatbar16th_t beat{0, 0, 0, 0};
            auto timeBeat = beatBarNthToTick(beat, signatureNum, signatureDenomBits, isRelative);
            TEST_ASSERT_EQUAL(timeBeat, 0);
        }
        {
            beatbar16th_t beat{0, 1, 0, 0};
            auto timeBeat = beatBarNthToTick(beat, signatureNum, signatureDenomBits, isRelative);
            TEST_ASSERT_EQUAL(timeBeat, TICKS_QUARTER);
        }
        {
            beatbar16th_t beat{0, 0, 1, 0};
            auto timeBeat = beatBarNthToTick(beat, signatureNum, signatureDenomBits, isRelative);
            TEST_ASSERT_EQUAL(timeBeat, TICKS_16TH);
        }
        {
            beatbar16th_t beat{0, 0, 0, 1};
            auto timeBeat = beatBarNthToTick(beat, signatureNum, signatureDenomBits, isRelative);
            TEST_ASSERT_EQUAL(timeBeat, 1);
        }
        {
            beatbar16th_t beat{2, 2, 2, 2};
            auto timeBeat = beatBarNthToTick(beat, signatureNum, signatureDenomBits, isRelative);
            TEST_ASSERT_EQUAL(timeBeat, 2*TICKS_BAR + 2*TICKS_QUARTER + 2*TICKS_16TH + 2);
        }
        {
            beatbar16th_t beat{-1, 0, 0, 0};
            auto timeBeat = beatBarNthToTick(beat, signatureNum, signatureDenomBits, isRelative);
            TEST_ASSERT_EQUAL(timeBeat, 0); // equals -0
        }
        {
            beatbar16th_t beat{-1, 1, 1, 0};
            auto timeBeat = beatBarNthToTick(beat, signatureNum, signatureDenomBits, isRelative);
            TEST_ASSERT_EQUAL(timeBeat, -(TICKS_QUARTER+TICKS_16TH));
        }
        {
            beatbar16th_t beat{-2, 1, 1, 1};
            auto timeBeat = beatBarNthToTick(beat, signatureNum, signatureDenomBits, isRelative);
            TEST_ASSERT_EQUAL(timeBeat, -(TICKS_BAR+TICKS_QUARTER+TICKS_16TH + 1));
        }
        TEST_END();
    }

    void test_beatBarNthToString() {
        TEST_BEGIN("test_beatBarNthToString");
        uint32_t signatureNum   = 4;
        uint32_t signatureDenomBits = 2; // exponent of 2
        {
            const bool isRelative = true;
            TEST_ASSERT_EQUAL(beatBarNthToString(beatbar16th_t{-2, 1, 1, 1}, isRelative), "-1.1.1.1");
        }
        {
            const bool isRelative = false;
            TEST_ASSERT_EQUAL(beatBarNthToString(beatbar16th_t{-2, 1, 1, 1}, isRelative), "-2.2.2.1");
        }
        {
            const bool isRelative = true;
            TEST_ASSERT_EQUAL(beatBarNthToString(beatbar16th_t{-3, 2, 2, 1}, isRelative), "-2.2.2.1");
        }
        {
            const bool isRelative = true;
            TEST_ASSERT_EQUAL(beatBarNthToString(beatbar16th_t{-2, 0, 0, 0}, isRelative), "-1.0.0.0");
        }
        {
            const bool isRelative = false;
            TEST_ASSERT_EQUAL(beatBarNthToString(beatbar16th_t{-3, 1, 1, 1}, isRelative), "-3.2.2.1");
        }
        {
            const bool isRelative = true;
            TEST_ASSERT_EQUAL(beatBarNthToString(beatbar16th_t{0, 0, 0, 0}, isRelative), "0.0.0.0");
        }
        {
            const bool isRelative = false;
            TEST_ASSERT_EQUAL(beatBarNthToString(beatbar16th_t{0, 0, 0, 0}, isRelative), "1.1.1.0");
        }
        {
            const bool isRelative = true;
            TEST_ASSERT_EQUAL(beatBarNthToString(beatbar16th_t{1, 0, 0, 0}, isRelative), "1.0.0.0");
        }
        {
            const bool isRelative = true;
            TEST_ASSERT_EQUAL(beatBarNthToString(beatbar16th_t{2, 0, 0, 0}, isRelative), "2.0.0.0");
        }
        {
            const bool isRelative = false;
            TEST_ASSERT_EQUAL(beatBarNthToString(beatbar16th_t{2, 0, 0, 0}, isRelative), "3.1.1.0");
        }
        TEST_END();
    }

    void test_stringToBeatBarNth() {
        TEST_BEGIN("test_beatBarNthToString");
        uint32_t signatureNum   = 4;
        uint32_t signatureDenomBits = 2; // exponent of 2
        {
            const bool isRelative = false;
            auto timeBeat = stringToBeatBarNth("1.1.1.0", isRelative, signatureNum, signatureDenomBits);
            TEST_ASSERT_EQUAL(timeBeat.bar, 0);
            TEST_ASSERT_EQUAL(timeBeat.beat, 0);
            TEST_ASSERT_EQUAL(timeBeat.th, 0);
            TEST_ASSERT_EQUAL(timeBeat.subticks, 0);
        }
        {
            const bool isRelative = false;
            auto timeBeat = stringToBeatBarNth("2.3.4.5", isRelative, signatureNum, signatureDenomBits);
            TEST_ASSERT_EQUAL(timeBeat.bar, 1);
            TEST_ASSERT_EQUAL(timeBeat.beat, 2);
            TEST_ASSERT_EQUAL(timeBeat.th, 3);
            TEST_ASSERT_EQUAL(timeBeat.subticks, 5);
        }
        {
            const bool isRelative = false;
            auto timeBeat = stringToBeatBarNth("1.1.1.1", isRelative, signatureNum, signatureDenomBits);
            TEST_ASSERT_EQUAL(timeBeat.bar, 0);
            TEST_ASSERT_EQUAL(timeBeat.beat, 0);
            TEST_ASSERT_EQUAL(timeBeat.th, 0);
            TEST_ASSERT_EQUAL(timeBeat.subticks, 1);
        }
        {
            const bool isRelative = false;
            auto timeBeat = stringToBeatBarNth("-2.3.4.5", isRelative, signatureNum, signatureDenomBits);
            TEST_ASSERT_EQUAL(timeBeat.bar, -2);
            TEST_ASSERT_EQUAL(timeBeat.beat, 2);
            TEST_ASSERT_EQUAL(timeBeat.th, 3);
            TEST_ASSERT_EQUAL(timeBeat.subticks, 5);
        }
        {
            const bool isRelative = true;
            auto timeBeat = stringToBeatBarNth("0.0.0.0", isRelative, signatureNum, signatureDenomBits);
            TEST_ASSERT_EQUAL(timeBeat.bar, 0);
            TEST_ASSERT_EQUAL(timeBeat.beat, 0);
            TEST_ASSERT_EQUAL(timeBeat.th, 0);
            TEST_ASSERT_EQUAL(timeBeat.subticks, 0);
        }
        {
            const bool isRelative = true;
            auto timeBeat = stringToBeatBarNth("0.1.0.2", isRelative, signatureNum, signatureDenomBits);
            TEST_ASSERT_EQUAL(timeBeat.bar, 0);
            TEST_ASSERT_EQUAL(timeBeat.beat, 1);
            TEST_ASSERT_EQUAL(timeBeat.th, 0);
            TEST_ASSERT_EQUAL(timeBeat.subticks, 2);
        }
        {
            const bool isRelative = true;
            auto timeBeat = stringToBeatBarNth("0.0.1.0", isRelative, signatureNum, signatureDenomBits);
            TEST_ASSERT_EQUAL(timeBeat.bar, 0);
            TEST_ASSERT_EQUAL(timeBeat.beat, 0);
            TEST_ASSERT_EQUAL(timeBeat.th, 1);
            TEST_ASSERT_EQUAL(timeBeat.subticks, 0);
        }
        {
            const bool isRelative = true;
            auto timeBeat = stringToBeatBarNth("1.0.0.0", isRelative, signatureNum, signatureDenomBits);
            TEST_ASSERT_EQUAL(timeBeat.bar, 1);
            TEST_ASSERT_EQUAL(timeBeat.beat, 0);
            TEST_ASSERT_EQUAL(timeBeat.th, 0);
            TEST_ASSERT_EQUAL(timeBeat.subticks, 0);
        }
        {
            const bool isRelative = true;
            auto timeBeat = stringToBeatBarNth("-0.0.0.1", isRelative, signatureNum, signatureDenomBits);
            TEST_ASSERT_EQUAL(timeBeat.bar, -1);
            TEST_ASSERT_EQUAL(timeBeat.beat, 0);
            TEST_ASSERT_EQUAL(timeBeat.th, 0);
            TEST_ASSERT_EQUAL(timeBeat.subticks, 1);
        }
        {
            const bool isRelative = true;
            auto timeBeat = stringToBeatBarNth("-0.0.1.0", isRelative, signatureNum, signatureDenomBits);
            TEST_ASSERT_EQUAL(timeBeat.bar, -1);
            TEST_ASSERT_EQUAL(timeBeat.beat, 0);
            TEST_ASSERT_EQUAL(timeBeat.th, 1);
            TEST_ASSERT_EQUAL(timeBeat.subticks, 0);
        }
        {
            const bool isRelative = true;
            auto timeBeat = stringToBeatBarNth("-0.1.0.0", isRelative, signatureNum, signatureDenomBits);
            TEST_ASSERT_EQUAL(timeBeat.bar, -1);
            TEST_ASSERT_EQUAL(timeBeat.beat, 1);
            TEST_ASSERT_EQUAL(timeBeat.th, 0);
            TEST_ASSERT_EQUAL(timeBeat.subticks, 0);
        }
        {
            const bool isRelative = true;
            auto timeBeat = stringToBeatBarNth("-1.0.0.0", isRelative, signatureNum, signatureDenomBits);
            TEST_ASSERT_EQUAL(timeBeat.bar, -2);
            TEST_ASSERT_EQUAL(timeBeat.beat, 0);
            TEST_ASSERT_EQUAL(timeBeat.th, 0);
            TEST_ASSERT_EQUAL(timeBeat.subticks, 0);
        }
        {
            const bool isRelative = true;
            auto timeBeat = stringToBeatBarNth("-0.0.0.0", isRelative, signatureNum, signatureDenomBits);
            TEST_ASSERT_EQUAL(timeBeat.bar, 0);
            TEST_ASSERT_EQUAL(timeBeat.beat, 0);
            TEST_ASSERT_EQUAL(timeBeat.th, 0);
            TEST_ASSERT_EQUAL(timeBeat.subticks, 0);
        }
        TEST_END();
    }
        
}// namespace
int main() {
    test_TickConversions();
    test_tickToBarBeat16thAbsolute();
    test_tickToBarBeat16thRelative();
    test_beatBarNthToTickAbsolute();
    test_beatBarNthToTickRelative();
    test_beatBarNthToString();
    test_stringToBeatBarNth();
    return 0;
}

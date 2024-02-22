#include "TestBase.hpp"
#include "math/seq_math.h"
#include "logging.h"
#include "rand.h"
#include <limits>
#include <vector>

#ifdef NDEBUG
#define TEST_FASTMATH
#endif

namespace test_math {
    constexpr int64_t LIMIT_OFFSET = 129;
    template<typename R, typename T>
    R getAbove() {
        return static_cast<R>(std::numeric_limits<T>::max()) + LIMIT_OFFSET;
    }
    template<typename R, typename T>
    R getBelow() {
        return static_cast<R>(std::numeric_limits<T>::min()) - LIMIT_OFFSET;
    }
    float getFloatAboveS64() {
        return getAbove<float, int64_t>();
    }
    float getFloatBelowS64() {
        return getBelow<float, int64_t>();
    }
    float getFloatAboveS32() {
        return static_cast<float>(getAbove<int64_t, int32_t>());
    }
    float getFloatBelowS32() {
        return static_cast<float>(getBelow<int64_t, int32_t>());
    }
    float getFloatAboveU32() {
        return static_cast<float>(getAbove<int64_t, uint32_t>());
    }
    int64_t getS64AboveS32() {
        return getAbove<int64_t, int32_t>();
    }
    int64_t getS64AboveU32() {
        return getAbove<int64_t, uint32_t>();
    }
    int64_t getS64BelowS32() {
        return getBelow<int64_t, int32_t>();
    }

    template<typename Func>
    void testTemplateFunctionDeduction(Func func) {
        func(float{3.0f});
    }
    void testRoundS32(int32_t(*funcRoundF)(float)) {
        TEST_ASSERT_EQUAL(funcRoundF(0.0f), 0);
        TEST_ASSERT_EQUAL(funcRoundF(0.5f), 1);
        TEST_ASSERT_EQUAL(funcRoundF(1.0f), 1);
        TEST_ASSERT_EQUAL(funcRoundF(1.5f), 2);
        TEST_ASSERT_EQUAL(funcRoundF(2.0f), 2);
        TEST_ASSERT_EQUAL(funcRoundF(-0.0f), 0);
        TEST_ASSERT_EQUAL(funcRoundF(-0.25f), 0);
        TEST_ASSERT_EQUAL(funcRoundF(-0.5f), -1);
        TEST_ASSERT_EQUAL(funcRoundF(-1.0f), -1);
        TEST_ASSERT_EQUAL(funcRoundF(-1.5f), -2);
        TEST_ASSERT_EQUAL(funcRoundF(-2.0f), -2);
        TEST_ASSERT_EQUAL(funcRoundF(std::numeric_limits<float>::max()), std::numeric_limits<int32_t>::max());
        TEST_ASSERT_EQUAL(funcRoundF(std::numeric_limits<float>::lowest()), std::numeric_limits<int32_t>::min());
        TEST_ASSERT_EQUAL(funcRoundF(std::numeric_limits<float>::min()), 0);
        TEST_ASSERT_EQUAL(funcRoundF(std::numeric_limits<float>::denorm_min()), 0);
        TEST_ASSERT_EQUAL(funcRoundF(std::numeric_limits<float>::infinity()), std::numeric_limits<int32_t>::max());
        // this test currently fails with -ffast-math. See https://godbolt.org/z/q9q9PzG7E
        // TEST_ASSERT_EQUAL(funcRoundF(-std::numeric_limits<float>::infinity()), std::numeric_limits<int32_t>::min());
        TEST_ASSERT_EQUAL(funcRoundF(INFINITY), std::numeric_limits<int32_t>::max());
        TEST_ASSERT_EQUAL(funcRoundF(-INFINITY), std::numeric_limits<int32_t>::min());
        TEST_ASSERT_EQUAL(funcRoundF(std::numeric_limits<float>::quiet_NaN()), 0);
        TEST_ASSERT_EQUAL(funcRoundF(getFloatAboveS32()), std::numeric_limits<int32_t>::max());
        TEST_ASSERT_EQUAL(funcRoundF(getFloatBelowS32()), std::numeric_limits<int32_t>::min());

        float f32;
        f32 = 0.5f;
        (*reinterpret_cast<uint32_t*>(&f32))-=2; // mingw clang requires 2
        TEST_ASSERT_EQUAL(funcRoundF(f32), 0);

        f32 = 1.5f;
        (*reinterpret_cast<uint32_t*>(&f32))--;
        TEST_ASSERT_EQUAL(funcRoundF(f32), 1);

        f32 = -0.5f;
        (*reinterpret_cast<uint32_t*>(&f32))-=2; // mingw clang requires 2
        TEST_ASSERT_EQUAL(funcRoundF(f32), 0);

        f32 = -0.5f;
        (*reinterpret_cast<uint32_t*>(&f32))++;
        TEST_ASSERT_EQUAL(funcRoundF(f32), -1);

        f32 = -1.5f;
        (*reinterpret_cast<uint32_t*>(&f32))--;
        TEST_ASSERT_EQUAL(funcRoundF(f32), -1);

        f32 = -1.5f;
        (*reinterpret_cast<uint32_t*>(&f32))++;
        TEST_ASSERT_EQUAL(funcRoundF(f32), -2);

        int32_t signed32 = 2;
        for (; signed32 < 8388608; signed32 += (signed32 / 2)) {
            f32 = static_cast<float>(signed32);
            TEST_ASSERT_EQUAL(funcRoundF(f32), signed32);
        }

        f32 = 8388609.0f;
        TEST_ASSERT_EQUAL(funcRoundF(f32), 8388609);
        f32 = 8388609.0f;
        TEST_ASSERT_EQUAL(funcRoundF(f32), 8388609);
        f32 = 8388609.5f;
        TEST_ASSERT_EQUAL(funcRoundF(f32), 8388610);
        f32 = 8388609.9f;
        TEST_ASSERT_EQUAL(funcRoundF(f32), 8388610);
        f32 = 8388610.0f;
        TEST_ASSERT_EQUAL(funcRoundF(f32), 8388610);
        f32 = 8388610.5f;
        TEST_ASSERT_EQUAL(funcRoundF(f32), 8388610);
        f32 = 8388611.0f;
        (*reinterpret_cast<uint32_t*>(&f32))--;
        TEST_ASSERT_EQUAL(funcRoundF(f32), 8388610);
        f32 = 8388611.0f;
        TEST_ASSERT_EQUAL(funcRoundF(f32), 8388611);
        f32 = 8388612.0f;
        (*reinterpret_cast<uint32_t*>(&f32))--;
        TEST_ASSERT_EQUAL(funcRoundF(f32), 8388611);
        f32 = 8388612.0f;
        TEST_ASSERT_EQUAL(funcRoundF(f32), 8388612);
        f32 = 8388613.0f;
        TEST_ASSERT_EQUAL(funcRoundF(f32), 8388613);
        f32 = 8388613.49f;
        TEST_ASSERT_EQUAL(funcRoundF(f32), 8388613);
        f32 = 8388613.5f;
        TEST_ASSERT_EQUAL(funcRoundF(f32), 8388614);
        f32 = 16777216.5f;
        TEST_ASSERT_EQUAL(funcRoundF(f32), 16777216);
        f32 = 16777217.5f;
        TEST_ASSERT_EQUAL(funcRoundF(f32), 16777218);
        f32 = 16777219.0f;
        TEST_ASSERT_EQUAL(funcRoundF(f32), 16777220);
        f32 = 2147483647.0f;
        (*reinterpret_cast<uint32_t*>(&f32))--;
        TEST_ASSERT_EQUAL(funcRoundF(f32), 2147483520);
        f32 = 2147483647.0f;
        TEST_ASSERT_EQUAL(funcRoundF(f32), std::numeric_limits<int32_t>::max());

        /* Test if 32 bit floats are clamped to integer range and no overflow occurs */
        f32 = 2147483647.0f;
        (*reinterpret_cast<uint32_t*>(&f32)) += 20;
        TEST_ASSERT_EQUAL(funcRoundF(f32), std::numeric_limits<int32_t>::max());

        f32 = -2147483647.0f;
        (*reinterpret_cast<uint32_t*>(&f32))--;
        TEST_ASSERT_EQUAL(funcRoundF(f32), -2147483520);
        f32 = -2147483647.0f;
        TEST_ASSERT_EQUAL(funcRoundF(f32), std::numeric_limits<int32_t>::min());
        f32 = -2147483647.0f;
        (*reinterpret_cast<uint32_t*>(&f32)) += 20;
        TEST_ASSERT_EQUAL(funcRoundF(f32), std::numeric_limits<int32_t>::min());

        /* Compare with traditional rounding: rounded = (int) (f + 0.5) */
        std::vector<float> fValues = {
            -1.501f,
            -1.5f,
            -1.499f,
            -0.51f,
            -0.5f,
            0.0f,
            0.1f,
            0.499f,
            0.5f,
            0.501f,
            0.999f,
            1.1f,
            1.499f,
            1.5f,
            1.501f,
        };

        auto traditionalIntTrunc = [](float f) -> int32_t {
            /* Negative rounding is off by 1 when using to int truncation */
            return static_cast<int32_t>(f + 0.5f) - (std::signbit(f) ? 1 : 0);
        };
        log_printf("Value  int truncation           funcRoundF\n");
        for (auto fValue : fValues) {
            auto iRounded  = traditionalIntTrunc(fValue);
            auto iRounded2 = funcRoundF(fValue);
            TEST_ASSERT_EQUAL(iRounded, iRounded2);
            log_printf("%+.1f %16d     %16d\n", fValue, iRounded, iRounded2);
        }
        
        /* Some of the following tests may fail with fastmath enabled */
        fValues = {
#ifndef TEST_FASTMATH
            -0.0f,
            std::numeric_limits<float>::quiet_NaN(),
#endif
            std::numeric_limits<float>::infinity(),
            -std::numeric_limits<float>::infinity(),
        };
        for (auto fValue : fValues) {
            auto iRounded  = traditionalIntTrunc(fValue);
            auto iRounded2 = funcRoundF(fValue);
            TEST_ASSERT_NOT_EQUAL(iRounded, iRounded2);
            log_printf("%+.1f %16d     %16d\n", fValue, iRounded, iRounded2);
        }
    }

    void testRoundS64(int64_t(*funcRoundF)(float)) {
        TEST_ASSERT_EQUAL(funcRoundF(0.0f), 0);
        TEST_ASSERT_EQUAL(funcRoundF(0.5f), 1);
        TEST_ASSERT_EQUAL(funcRoundF(1.0f), 1);
        TEST_ASSERT_EQUAL(funcRoundF(1.5f), 2);
        TEST_ASSERT_EQUAL(funcRoundF(2.0f), 2);
        TEST_ASSERT_EQUAL(funcRoundF(-0.0f), 0);
        TEST_ASSERT_EQUAL(funcRoundF(-0.25f), 0);
        TEST_ASSERT_EQUAL(funcRoundF(-0.5f), -1);
        TEST_ASSERT_EQUAL(funcRoundF(-1.0f), -1);
        TEST_ASSERT_EQUAL(funcRoundF(-1.5f), -2);
        TEST_ASSERT_EQUAL(funcRoundF(-2.0f), -2);
        TEST_ASSERT_EQUAL(funcRoundF(std::numeric_limits<float>::min()), 0);
        TEST_ASSERT_EQUAL(funcRoundF(std::numeric_limits<float>::denorm_min()), 0);
        TEST_ASSERT_EQUAL(funcRoundF(std::numeric_limits<float>::max()), std::numeric_limits<int64_t>::max());
        TEST_ASSERT_EQUAL(funcRoundF(std::numeric_limits<float>::lowest()), std::numeric_limits<int64_t>::min());
        TEST_ASSERT_EQUAL(funcRoundF(std::numeric_limits<float>::min()), 0);
        TEST_ASSERT_EQUAL(funcRoundF(std::numeric_limits<float>::denorm_min()), 0);
        TEST_ASSERT_EQUAL(funcRoundF(std::numeric_limits<float>::infinity()), std::numeric_limits<int64_t>::max());
        // TEST_ASSERT_EQUAL(funcRoundF(-std::numeric_limits<float>::infinity()), std::numeric_limits<int64_t>::min());
        TEST_ASSERT_EQUAL(funcRoundF(INFINITY), std::numeric_limits<int64_t>::max());
        TEST_ASSERT_EQUAL(funcRoundF(-INFINITY), std::numeric_limits<int64_t>::min());
        TEST_ASSERT_EQUAL(funcRoundF(getFloatAboveS64()), std::numeric_limits<int64_t>::max());
        TEST_ASSERT_EQUAL(funcRoundF(getFloatBelowS64()), std::numeric_limits<int64_t>::min());

        TEST_ASSERT_EQUAL(funcRoundF(std::numeric_limits<float>::quiet_NaN()), 0);


        /* Compare with traditional rounding: rounded = (int) (f + 0.5) */
        std::vector<float> fValues = {
            -1.501f,
            -1.5f,
            -1.499f,
            -0.51f,
            -0.5f,
            0.0f,
            0.1f,
            0.499f,
            0.5f,
            0.501f,
            0.999f,
            1.1f,
            1.499f,
            1.5f,
            1.501f,
        };

        auto traditionalIntTrunc = [](float f) -> int64_t {
            /* Negative rounding is off by 1 when using to int truncation */
            return static_cast<int64_t>(f + 0.5f) - (std::signbit(f) ? 1 : 0);
        };
        log_printf("Value  int truncation           funcRoundF\n");
        for (auto fValue : fValues) {
            auto iRounded  = traditionalIntTrunc(fValue);
            auto iRounded2 = funcRoundF(fValue);
            TEST_ASSERT_EQUAL(iRounded, iRounded2);
            log_printf("%+.1f %16zd     %16zd\n", fValue, iRounded, iRounded2);
        }
        fValues = {
#ifndef TEST_FASTMATH
            -0.0f,
            std::numeric_limits<float>::quiet_NaN(),
#endif
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::infinity(),
            -std::numeric_limits<float>::infinity(),
        };
        for (auto fValue : fValues) {
            auto iRounded  = traditionalIntTrunc(fValue);
            auto iRounded2 = funcRoundF(fValue);
            TEST_ASSERT_NOT_EQUAL(iRounded, iRounded2);
            log_printf("%+.1f %16zd     %16zd\n", fValue, iRounded, iRounded2);
        }
    }
    void testFloorS32() {
        using ::math::floorfS32;
        TEST_ASSERT_EQUAL(floorfS32(0.0f), 0);
        TEST_ASSERT_EQUAL(floorfS32(0.5f), 0);
        TEST_ASSERT_EQUAL(floorfS32(1.0f), 1);
        TEST_ASSERT_EQUAL(floorfS32(1.5f), 1);
        TEST_ASSERT_EQUAL(floorfS32(2.0f), 2);
        TEST_ASSERT_EQUAL(floorfS32(-0.0f), 0);
        TEST_ASSERT_EQUAL(floorfS32(-0.25f), -1);
        TEST_ASSERT_EQUAL(floorfS32(-0.5f), -1);
        TEST_ASSERT_EQUAL(floorfS32(-1.0f), -1);
        TEST_ASSERT_EQUAL(floorfS32(-1.5f), -2);
        TEST_ASSERT_EQUAL(floorfS32(-2.0f), -2);

        TEST_ASSERT_EQUAL(floorfS32(std::numeric_limits<float>::max()), std::numeric_limits<int32_t>::max());
        TEST_ASSERT_EQUAL(floorfS32(std::numeric_limits<float>::lowest()), std::numeric_limits<int32_t>::min());
        TEST_ASSERT_EQUAL(floorfS32(std::numeric_limits<float>::min()), 0);
        TEST_ASSERT_EQUAL(floorfS32(std::numeric_limits<float>::denorm_min()), 0);
        TEST_ASSERT_EQUAL(floorfS32(std::numeric_limits<float>::infinity()), std::numeric_limits<int32_t>::max());
        // TEST_ASSERT_EQUAL(floorfS32(-std::numeric_limits<float>::infinity()), std::numeric_limits<int32_t>::min());
        TEST_ASSERT_EQUAL(floorfS32(INFINITY), std::numeric_limits<int32_t>::max());
        TEST_ASSERT_EQUAL(floorfS32(-INFINITY), std::numeric_limits<int32_t>::min());
        TEST_ASSERT_EQUAL(floorfS32(getFloatAboveS32()), std::numeric_limits<int32_t>::max());
        TEST_ASSERT_EQUAL(floorfS32(getFloatBelowS32()), std::numeric_limits<int32_t>::min());

#ifdef TEST_FASTMATH
#if defined(__clang__)
        /* we can't test for result of floored NaN */
        /* as this is undefined behavior on clang with -ffast-math */
#else
        TEST_ASSERT_EQUAL(floorfS32(std::numeric_limits<float>::quiet_NaN()), 0);
#endif
#else
        TEST_ASSERT_EQUAL(floorfS32(std::numeric_limits<float>::quiet_NaN()), 0);
#endif
    }

    void testFloorU32() {
        using ::math::floorfU32;
        TEST_ASSERT_EQUAL(floorfU32(0.0f), 0);
        TEST_ASSERT_EQUAL(floorfU32(0.5f), 0);
        TEST_ASSERT_EQUAL(floorfU32(1.0f), 1);
        TEST_ASSERT_EQUAL(floorfU32(1.5f), 1);
        TEST_ASSERT_EQUAL(floorfU32(2.0f), 2);
        TEST_ASSERT_EQUAL(floorfU32(-0.0f), 0);
        TEST_ASSERT_EQUAL(floorfU32(-0.25f), 0);
        TEST_ASSERT_EQUAL(floorfU32(-0.5f), 0);
        TEST_ASSERT_EQUAL(floorfU32(-1.0f), 0);
        TEST_ASSERT_EQUAL(floorfU32(-1.5f), 0);
        TEST_ASSERT_EQUAL(floorfU32(-2.0f), 0);

        TEST_ASSERT_EQUAL(floorfU32(std::numeric_limits<float>::max()), std::numeric_limits<uint32_t>::max());
        TEST_ASSERT_EQUAL(floorfU32(std::numeric_limits<float>::lowest()), std::numeric_limits<uint32_t>::min());
        TEST_ASSERT_EQUAL(floorfU32(std::numeric_limits<float>::min()), 0);
        TEST_ASSERT_EQUAL(floorfU32(std::numeric_limits<float>::denorm_min()), 0);
        TEST_ASSERT_EQUAL(floorfU32(std::numeric_limits<float>::infinity()), std::numeric_limits<uint32_t>::max());
        TEST_ASSERT_EQUAL(floorfU32(-std::numeric_limits<float>::infinity()), std::numeric_limits<uint32_t>::min());
        TEST_ASSERT_EQUAL(floorfU32(INFINITY), std::numeric_limits<uint32_t>::max());
        TEST_ASSERT_EQUAL(floorfU32(-INFINITY), std::numeric_limits<uint32_t>::min());
        TEST_ASSERT_EQUAL(floorfU32(getFloatAboveU32()), std::numeric_limits<uint32_t>::max());

#ifdef TEST_FASTMATH
#if defined(__clang__)
        /* we can't test for result of floored NaN */
        /* as this is undefined behavior on clang with -ffast-math */
#else
        TEST_ASSERT_EQUAL(floorfU32(std::numeric_limits<float>::quiet_NaN()), 0);
#endif
#else
        TEST_ASSERT_EQUAL(floorfU32(std::numeric_limits<float>::quiet_NaN()), 0);
#endif
    }


    void testFloorS64F() {
        using ::math::floorfS64;
        TEST_ASSERT_EQUAL(floorfS64(0.0f), 0);
        TEST_ASSERT_EQUAL(floorfS64(0.5f), 0);
        TEST_ASSERT_EQUAL(floorfS64(1.0f), 1);
        TEST_ASSERT_EQUAL(floorfS64(1.5f), 1);
        TEST_ASSERT_EQUAL(floorfS64(2.0f), 2);
        TEST_ASSERT_EQUAL(floorfS64(-0.0f), 0);
        TEST_ASSERT_EQUAL(floorfS64(-0.25f), -1);
        TEST_ASSERT_EQUAL(floorfS64(-0.5f), -1);
        TEST_ASSERT_EQUAL(floorfS64(-1.0f), -1);
        TEST_ASSERT_EQUAL(floorfS64(-1.5f), -2);
        TEST_ASSERT_EQUAL(floorfS64(-2.0f), -2);

        /**
         * The sint64 will overflow and return a negative value for a positive
         * float that doesn't fit its range.
         * This is not intuitive, but will not be fixed, as it introduces a runtime overhead.
         */
#ifndef TEST_FASTMATH
        // float max will be negative
        TEST_ASSERT_THROW(floorfS64(std::numeric_limits<float>::max()) < 0);
#else
#if defined(__clang__)
        /* TODO: Clang: test result of floored float maximum */
        // TEST_ASSERT_THROW(floorfS64(std::numeric_limits<float>::max()) < 0);
        // TEST_ASSERT_EQUAL(floorfS64(std::numeric_limits<float>::max()), 9223372036854775807LL);
#else
        TEST_ASSERT_EQUAL(floorfS64(std::numeric_limits<float>::max()), 9223372036854775807LL);
#endif
#endif
        float maxS64PosValue = 0.0f;
        // This is the last positive float value we can store in a sint64
        (*reinterpret_cast<uint32_t*>(&maxS64PosValue)) = 0x5EFFFFFF;
        int64_t maxS64PosValueFloord = floorfS64(maxS64PosValue);
#ifndef TEST_FASTMATH
        TEST_ASSERT_THROW(maxS64PosValueFloord > 0);
#else
        TEST_ASSERT_EQUAL(maxS64PosValueFloord, 9223371487098961920LL);
#endif
        // At this point we overflow
        (*reinterpret_cast<uint32_t*>(&maxS64PosValue)) = 0x5F000000;
        int64_t maxS64PosValuePlus1Floord = floorfS64(maxS64PosValue);
#ifndef TEST_FASTMATH
        TEST_ASSERT_THROW(maxS64PosValuePlus1Floord < 0);
#else
#if defined(__clang__)
        /* TODO: Clang: test result of floored float maximum */
        // TEST_ASSERT_EQUAL(maxS64PosValuePlus1Floord, std::numeric_limits<int64_t>::max());
#else
        TEST_ASSERT_EQUAL(maxS64PosValuePlus1Floord, std::numeric_limits<int64_t>::max());
#endif
#endif

        float onebeforemax = std::numeric_limits<float>::max();
        (*reinterpret_cast<uint32_t*>(&onebeforemax))--;
#ifndef TEST_FASTMATH
        TEST_ASSERT_EQUAL(floorfS64(std::numeric_limits<float>::lowest()), std::numeric_limits<int64_t>::min());
        TEST_ASSERT_NOT_EQUAL(floorfS64(onebeforemax), std::numeric_limits<int64_t>::max());
#else
#if defined(__clang__)
        /* TODO: Clang: test result of floored float minimum */
        // TEST_ASSERT_EQUAL(floorfS64(std::numeric_limits<float>::lowest()), std::numeric_limits<int64_t>::min());
        /* TODO: Clang: test result of floored float maximum */
        // TEST_ASSERT_EQUAL(floorfS64(onebeforemax), std::numeric_limits<int64_t>::max());
#else
        TEST_ASSERT_EQUAL(floorfS64(std::numeric_limits<float>::lowest()), std::numeric_limits<int64_t>::min());
        TEST_ASSERT_EQUAL(floorfS64(onebeforemax), std::numeric_limits<int64_t>::max());
#endif
#endif



        TEST_ASSERT_EQUAL(floorfS64(std::numeric_limits<float>::min()), 0);
        TEST_ASSERT_EQUAL(floorfS64(std::numeric_limits<float>::denorm_min()), 0);
        // TEST_ASSERT_EQUAL(floorfS64(std::numeric_limits<float>::infinity()), 0);
        // TEST_ASSERT_EQUAL(floorfS64(-std::numeric_limits<float>::infinity()), 0);
        // TEST_ASSERT_EQUAL(floorfS64(INFINITY), 0);
        // TEST_ASSERT_EQUAL(floorfS64(-INFINITY), 0);
        // TEST_ASSERT_EQUAL(floorfS64(std::numeric_limits<float>::quiet_NaN()), 0);

        TEST_ASSERT_NOT_EQUAL(floorfS64(getFloatAboveS32()), std::numeric_limits<int64_t>::max());
        TEST_ASSERT_NOT_EQUAL(floorfS64(getFloatBelowS32()), std::numeric_limits<int64_t>::min());
    }

    void testFloorS64D() {
        using ::math::floordS64;
        TEST_ASSERT_EQUAL(floordS64(0.0), 0);
        TEST_ASSERT_EQUAL(floordS64(0.5), 0);
        TEST_ASSERT_EQUAL(floordS64(1.0), 1);
        TEST_ASSERT_EQUAL(floordS64(1.5), 1);
        TEST_ASSERT_EQUAL(floordS64(2.0), 2);
        TEST_ASSERT_EQUAL(floordS64(-0.0), 0);
        TEST_ASSERT_EQUAL(floordS64(-0.25), -1);
        TEST_ASSERT_EQUAL(floordS64(-0.5), -1);
        TEST_ASSERT_EQUAL(floordS64(-1.0), -1);
        TEST_ASSERT_EQUAL(floordS64(-1.5), -2);
        TEST_ASSERT_EQUAL(floordS64(-2.0), -2);

        TEST_ASSERT_EQUAL(floordS64(std::numeric_limits<double>::min()), 0);
        TEST_ASSERT_EQUAL(floordS64(std::numeric_limits<double>::denorm_min()), 0);
    
        double onebeforemax = std::numeric_limits<double>::max();
        (*reinterpret_cast<uint64_t*>(&onebeforemax))--;
        /**
         * The sint64 will overflow and return a negative value for a positive
         * float that doesn't fit its range.
         * This is not intuitive, but will not be fixed, as it introduces a runtime overhead.
         */
#ifndef TEST_FASTMATH
        // double max will be negative
        TEST_ASSERT_THROW(floordS64(std::numeric_limits<double>::max()) < 0);
        TEST_ASSERT_NOT_EQUAL(floordS64(onebeforemax), std::numeric_limits<int64_t>::max());
        TEST_ASSERT_EQUAL(floordS64(std::numeric_limits<double>::lowest()), std::numeric_limits<int64_t>::min());
        TEST_ASSERT_EQUAL(floordS64(std::numeric_limits<double>::quiet_NaN()), 0);
        TEST_ASSERT_EQUAL(floordS64(std::numeric_limits<double>::infinity()), 0);
        TEST_ASSERT_EQUAL(floordS64(-std::numeric_limits<double>::infinity()), 0);
        TEST_ASSERT_EQUAL(floordS64(INFINITY), 0);
        TEST_ASSERT_EQUAL(floordS64(-INFINITY), 0);
#else
#if !defined(__clang__)
        TEST_ASSERT_EQUAL(floordS64(std::numeric_limits<double>::max()), 9223372036854775807LL);
        TEST_ASSERT_EQUAL(floordS64(onebeforemax), std::numeric_limits<int64_t>::max());
        TEST_ASSERT_EQUAL(floordS64(std::numeric_limits<double>::lowest()), std::numeric_limits<int64_t>::min());
        TEST_ASSERT_EQUAL(floordS64(std::numeric_limits<double>::quiet_NaN()), 0);
        TEST_ASSERT_EQUAL(floordS64(std::numeric_limits<double>::infinity()), std::numeric_limits<int64_t>::max());
        TEST_ASSERT_EQUAL(floordS64(-std::numeric_limits<double>::infinity()), std::numeric_limits<int64_t>::min());
        TEST_ASSERT_EQUAL(floordS64(INFINITY), std::numeric_limits<int64_t>::max());
        TEST_ASSERT_EQUAL(floordS64(-INFINITY), std::numeric_limits<int64_t>::min());
#endif
        //TODO: clang
#endif
    }

    void testFunctions() {
        using math::abs;
        using math::max;
        using math::absMax;
        using math::clamp;
        using math::CheckFitsTypeRange;

        TEST_BEGIN("math::max");

        TEST_ASSERT_EQUAL(max(0, 1), 1);
        TEST_ASSERT_EQUAL(max(1, 0), 1);
        TEST_ASSERT_EQUAL(max(1, 1), 1);
        TEST_ASSERT_EQUAL(max(-1, 1), 1);
        TEST_ASSERT_EQUAL(max(-1, -3), -1);
        TEST_ASSERT_EQUAL(max(0, 0), 0);

        TEST_ASSERT_EQUAL(max<int64_t>(std::numeric_limits<int32_t>::max(), std::numeric_limits<int64_t>::max()), std::numeric_limits<int64_t>::max());
        TEST_ASSERT_EQUAL(max<int64_t>(std::numeric_limits<int32_t>::min(), std::numeric_limits<int64_t>::min()), std::numeric_limits<int32_t>::min());

        TEST_END();
        TEST_BEGIN("math::abs");
        TEST_ASSERT_EQUAL(abs(  4 ), 4 );
        TEST_ASSERT_EQUAL(abs( -4 ), 4 );

        TEST_ASSERT_EQUAL(abs( std::numeric_limits<int64_t>::max() ), std::numeric_limits<int64_t>::max() );

        TEST_ASSERT_EQUAL(fp_math::isinff(std::numeric_limits<float>::infinity()), true);
        TEST_ASSERT_EQUAL(fp_math::isinff(INFINITY), true);
        TEST_ASSERT_EQUAL(fp_math::isinff(abs(INFINITY)), true);
#if defined(TEST_FASTMATH) && defined(__clang__)
#else

        /* int32_t overflow. Implementation defined. I expect -2147483648 */
        TEST_ASSERT_EQUAL(abs( std::numeric_limits<int32_t>::min() ), std::numeric_limits<int32_t>::min() );
        TEST_ASSERT_EQUAL(abs(INFINITY), INFINITY);
        TEST_ASSERT_EQUAL(abs(-INFINITY), INFINITY);
        TEST_ASSERT_EQUAL(absMax(INFINITY, float{ -20000 }), INFINITY);

        /* sint64 overflow */
        TEST_ASSERT_EQUAL(absMax<int64_t>(
                                  std::numeric_limits<int32_t>::min(),
                                  std::numeric_limits<int64_t>::min()),
                          std::numeric_limits<int32_t>::min());
        /* sint32 overflow */
        TEST_ASSERT_EQUAL(absMax<int32_t>(
                                  -4,
                                  std::numeric_limits<int32_t>::min()),
                          -4);
#endif
        TEST_ASSERT_EQUAL(abs( std::numeric_limits<int64_t>::max() ), std::numeric_limits<int64_t>::max() );
        TEST_ASSERT_EQUAL(abs( std::numeric_limits<uint64_t>::max() ), std::numeric_limits<uint64_t>::max() );
        TEST_ASSERT_EQUAL(abs( std::numeric_limits<uint8_t>::max() ), std::numeric_limits<uint8_t>::max() );
        TEST_ASSERT_EQUAL(abs( char{-32} ), 32 );
        TEST_ASSERT_EQUAL(abs(std::numeric_limits<float>::lowest()), std::numeric_limits<float>::max());
        TEST_ASSERT_EQUAL(abs(std::numeric_limits<float>::min()), std::numeric_limits<float>::min());
        TEST_ASSERT_EQUAL(abs(std::numeric_limits<float>::denorm_min()), std::numeric_limits<float>::denorm_min());
        TEST_ASSERT_EQUAL(abs(std::numeric_limits<float>::infinity()), std::numeric_limits<float>::infinity());
        TEST_ASSERT_EQUAL(abs(-std::numeric_limits<float>::infinity()), std::numeric_limits<float>::infinity());
        // TEST_ASSERT_THROW(std::isnan(abs(std::numeric_limits<float>::quiet_NaN())));
        TEST_ASSERT_EQUAL(abs(double{-1000.0/32.0}), double{1000/32.0});
        TEST_END();

        TEST_BEGIN("math::absMax");

        TEST_ASSERT_EQUAL(absMax(0, 1), 1);
        TEST_ASSERT_EQUAL(absMax(1, 0), 1);
        TEST_ASSERT_EQUAL(absMax(1, 1), 1);
        TEST_ASSERT_EQUAL(absMax(-1, 1), 1);
        TEST_ASSERT_EQUAL(absMax(-1, -3), -3);
        TEST_ASSERT_EQUAL(absMax(0, 0), 0);
        TEST_ASSERT_EQUAL(absMax(-std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity()),
                          std::numeric_limits<float>::infinity());

        TEST_ASSERT_EQUAL(absMax<int64_t>(std::numeric_limits<int32_t>::max(), std::numeric_limits<int64_t>::max()),
                          std::numeric_limits<int64_t>::max());

        TEST_ASSERT_EQUAL(absMax<int64_t>(
                                  std::numeric_limits<int32_t>::min(),
                                  std::numeric_limits<int64_t>::min()+1),
                          std::numeric_limits<int64_t>::min()+1);

        TEST_ASSERT_EQUAL(absMax<float>(-32.0f, -10000.0f), -10000.0f);
        TEST_ASSERT_EQUAL(absMax<float>(-32.0f, 10000.0f), 10000.0f);
        TEST_END();

        TEST_BEGIN("math::clamp");
        TEST_ASSERT_EQUAL(clamp<float>(55.0f, -32.0f, 10000.0f), 55.0f);
        TEST_ASSERT_EQUAL(clamp<float>(55.0f, 232.0f, 10000.0f), 232.0f);
        // incorrect usage
        TEST_ASSERT_EQUAL(clamp<float>(55.0f, 0.0f, -10000.0f), -10000.0f);
        TEST_ASSERT_EQUAL(clamp<float>(-32.0f, 0.0f, -10000.0f), 0.0f);

        TEST_ASSERT_EQUAL(clamp<int32_t>(55, -32, 10000), 55);
        TEST_ASSERT_EQUAL(clamp<int32_t>(55, 232, 10000), 232);
        // incorrect usage
        TEST_ASSERT_EQUAL(clamp<int32_t>(55, 0, -10000), -10000);
        TEST_ASSERT_EQUAL(clamp<int32_t>(-32, 0, -10000), 0);
        TEST_END();

        TEST_BEGIN("math::CheckFitsTypeRange");
        TEST_ASSERT_EQUAL(CheckFitsTypeRange<int32_t>(10000.0f), true);
        TEST_ASSERT_EQUAL(CheckFitsTypeRange<int32_t>(10000000), true);
        TEST_ASSERT_EQUAL(CheckFitsTypeRange<int32_t>(getFloatAboveS32()), false);
        TEST_ASSERT_EQUAL(CheckFitsTypeRange<int32_t>(getFloatBelowS32()), false);
        TEST_ASSERT_EQUAL(CheckFitsTypeRange<int32_t>(getS64AboveS32()), false);
        TEST_ASSERT_EQUAL(CheckFitsTypeRange<int32_t>(getS64BelowS32()), false);
        TEST_ASSERT_EQUAL(CheckFitsTypeRange<uint32_t>(getS64AboveU32()), false);
        TEST_ASSERT_EQUAL(CheckFitsTypeRange<uint32_t>(-1), false);
        TEST_ASSERT_EQUAL(CheckFitsTypeRange<int32_t>(std::numeric_limits<int32_t>::min()), true);
        TEST_ASSERT_EQUAL(CheckFitsTypeRange<int32_t>(std::numeric_limits<int32_t>::max()), true);
        TEST_ASSERT_EQUAL(CheckFitsTypeRange<uint32_t>(std::numeric_limits<uint32_t>::min()), true);
        TEST_ASSERT_EQUAL(CheckFitsTypeRange<uint32_t>(std::numeric_limits<uint32_t>::max()), true);
        TEST_ASSERT_EQUAL(CheckFitsTypeRange<int64_t>(std::numeric_limits<int64_t>::min()), true);
        TEST_ASSERT_EQUAL(CheckFitsTypeRange<int64_t>(std::numeric_limits<int64_t>::max()), true);
        TEST_ASSERT_EQUAL(CheckFitsTypeRange<uint64_t>(-1), false);
        TEST_ASSERT_EQUAL(CheckFitsTypeRange<uint32_t>(-1), false);
        TEST_ASSERT_EQUAL(CheckFitsTypeRange<uint16_t>(-1), false);
        TEST_ASSERT_EQUAL(CheckFitsTypeRange<uint8_t>(-1), false);
        TEST_ASSERT_EQUAL(CheckFitsTypeRange<int8_t>(-1), true);
        TEST_ASSERT_EQUAL(CheckFitsTypeRange<int8_t>(-128), true);
        TEST_ASSERT_EQUAL(CheckFitsTypeRange<int8_t>(127), true);
        TEST_ASSERT_EQUAL(CheckFitsTypeRange<int8_t>(128), false);
        TEST_END();
    }
    void testVecMath() {
        using math::maxvec2;
        using math::minvec2;
        using math::maxvec2f;
        using math::absvec2;
        using math::distvec2;
        using math::distancePointLine;
        TEST_BEGIN("math::maxvec2");
        TEST_ASSERT_EQUAL(maxvec2(ivec2(1), ivec2(0, 16)), ivec2(1, 16));
        TEST_ASSERT_EQUAL(maxvec2(ivec2(-1), ivec2(0, 16)), ivec2(0, 16));
        TEST_ASSERT_EQUAL(maxvec2(ivec2(-1), ivec2(0, -2)), ivec2(0, -1));
        TEST_ASSERT_EQUAL(maxvec2(ivec2(4, 32), ivec2(0, -2)), ivec2(4, 32));
        TEST_ASSERT_EQUAL(maxvec2(ivec2(-4, -32), ivec2(0, -2)), ivec2(0, -2));
        TEST_END();
        TEST_BEGIN("math::minvec2");
        TEST_ASSERT_EQUAL(minvec2(ivec2(1), ivec2(0, 16)), ivec2(0, 1));
        TEST_ASSERT_EQUAL(minvec2(ivec2(-1), ivec2(0, 16)), ivec2(-1));
        TEST_ASSERT_EQUAL(minvec2(ivec2(-1), ivec2(0, -2)), ivec2(-1, -2));
        TEST_ASSERT_EQUAL(minvec2(ivec2(4, 32), ivec2(0, -2)), ivec2(0, -2));
        TEST_ASSERT_EQUAL(minvec2(ivec2(-4, -32), ivec2(0, -2)), ivec2(-4, -32));
        TEST_END();
        TEST_BEGIN("math::maxvec2f");
        TEST_ASSERT_EQUAL(maxvec2f(vec2(1.123f), vec2(0, 16)), vec2(1.123f, 16));
        TEST_ASSERT_EQUAL(maxvec2f(vec2(-1), vec2(0, 16)), vec2(0, 16));
        TEST_ASSERT_EQUAL(maxvec2f(vec2(-1.123f), vec2(0, -2.123f)), vec2(0, -1.123f));
        TEST_ASSERT_EQUAL(maxvec2f(vec2(4, 32.123f), vec2(0, -2)), vec2(4, 32.123f));
        TEST_ASSERT_EQUAL(maxvec2f(vec2(-4.123f, -32), vec2(0, -2)), vec2(0, -2));
        TEST_END();
        TEST_BEGIN("math::absvec2");
        TEST_ASSERT_EQUAL(absvec2(ivec2(1)), (ivec2(1)));
        TEST_ASSERT_EQUAL(absvec2(ivec2(-20)), (ivec2(20)));
        TEST_ASSERT_EQUAL(absvec2(ivec2(32, -30)), (ivec2(32, 30)));
        TEST_ASSERT_EQUAL(absvec2(ivec2(-32, 30)), (ivec2(32, 30)));
        // truncation to int
        TEST_ASSERT_EQUAL(absvec2((ivec2)vec2(-0.1f)), (ivec2(0)));
        TEST_ASSERT_EQUAL(absvec2((ivec2)vec2(-1.9f)), (ivec2(1)));
        TEST_ASSERT_EQUAL(absvec2((ivec2)vec2(-2.9f)), (ivec2(2)));
        TEST_ASSERT_EQUAL(absvec2((ivec2)vec2(-0.7f)), (ivec2(0)));
        TEST_ASSERT_EQUAL(absvec2((ivec2)vec2(0.7f)), (ivec2(0)));
        TEST_ASSERT_EQUAL(absvec2((ivec2)vec2(1.1f)), (ivec2(1)));
        TEST_END();
        TEST_BEGIN("math::distvec2");
        TEST_ASSERT_EQUAL(distvec2(vec2(1.1f), vec2(1.1f)), 0.0f);
        TEST_ASSERT_EQUAL(distvec2(vec2(0, 10.0f), vec2(0, 2.0f)), 8.0f);

        auto ptA = vec2(123456.0f, -123456.0f);
        auto ptB = vec2(600.0f, 2222.0f);
        for (int i = 0; i < 32; i++) {

            float fX = ptB.x - ptA.x;
            float fY = ptB.y - ptA.y;
            float fDist = sqrtf(fX * fX + fY * fY);
            TEST_ASSERT_THROW(math::almost_equal(distvec2(ptA, ptB), fDist, 3));
            ptA *= 1.0f + 1.0f/3.0f;
            ptB += 2131.0f/8.0f;
        }
        TEST_BEGIN("math::distancePointLine");
        TEST_ASSERT_EQUAL(distancePointLine(vec2(0), vec2(-5, 10), vec2(5, 10)), 10.0f);
        TEST_ASSERT_EQUAL(distancePointLine(vec2(-7.7f), vec2(-52, 10), vec2(55, 10000)), 47.705135f);
        TEST_END();
    }
    void test_seq_rand() {
        TEST_BEGIN("seq_rand");
        seq_rand rand;
        rand.rng_seed(static_cast<uint64_t>(0x12123*M_PI*13.+55.*37.));
        for (size_t i = 0; i < 100000; i++) {
            TEST_ASSERT_THROW(rand.rng_rand(32) < 32);
            TEST_ASSERT_THROW(rand.rng_rand(32) >= 0);
            TEST_ASSERT_THROW(rand.rng_bits(4) < 16);
            TEST_ASSERT_THROW(rand.rng_bits(4) >= 0);
            TEST_ASSERT_THROW(rand.rng_bits(12) < (1<<12));
            TEST_ASSERT_THROW(rand.rng_bits(12) >= 0);
            TEST_ASSERT_THROW(rand.rng_double() >= 0);
            TEST_ASSERT_THROW(rand.rng_double() < 1.0);
        }
        for (size_t i = 0; i < 10; i++) {
            log_lf(Log::L_INFO, "%d: rng_double %f %f %f %f\n", i, rand.rng_double(), rand.rng_double(), rand.rng_double(), rand.rng_double());
        }
        for (size_t i = 0; i < 10; i++) {
            log_lf(Log::L_INFO, "%d: rng_rand %08X %08X\n", i, rand.rng_rand(), rand.rng_rand());
        }

        TEST_END();
    }
}// namespace test_math

int main() {

    try {
        // Asserts floating point compatibility at compile time
        static_assert(std::numeric_limits<float>::is_iec559, "IEEE 754 required");

        // Print for reference
        auto largeInt    = test_math::getS64AboveS32();
        auto aboveS32Max = static_cast<float>(largeInt);
        log_printf("roundF32toS32 float(%012X) = %012X\n", largeInt, (int32_t)math::roundfS32((float)aboveS32Max));
        log_printf("roundF32toS64 float(%012X) = %012X\n", largeInt, math::roundfS64(aboveS32Max));
        log_printf("sint64 max = %zd\n", 1ULL << 63);
        log_printf("floorS64(std::numeric_limits<float>::infinity()) = %zd\n", math::floorfS64(std::numeric_limits<float>::infinity()));
        log_printf("floorS64(std::numeric_limits<float>::max()) = %zd\n", math::floorfS64(std::numeric_limits<float>::max()));

        log_printf("%zd min\n", int64_t{ std::numeric_limits<int32_t>::min() });
        log_printf("%zd max\n", int64_t{ std::numeric_limits<int32_t>::max() });

        test_math::testTemplateFunctionDeduction<>((int32_t(*)(float))math::roundfS32);

        TEST_BEGIN("math::roundfS32");
        test_math::testRoundS32(math::roundfS32);
        TEST_END();
        TEST_BEGIN("math::roundS64");
        test_math::testRoundS64(math::roundfS64);
        TEST_END();
        TEST_BEGIN("math::floorS32");
        test_math::testFloorS32();
        TEST_END();
        TEST_BEGIN("math::floorU32");
        test_math::testFloorU32();
        TEST_END();
        TEST_BEGIN("math::floorS64F");
        test_math::testFloorS64F();
        TEST_END();
        TEST_BEGIN("math::floorS64D");
        test_math::testFloorS64D();
        TEST_END();
        test_math::testFunctions();
        test_math::testVecMath();
        test_math::test_seq_rand();
    } catch (std::exception& e) {
        log_printf("Caught exception: %s\n", e.what());
        return -1;
    }
    return 0;
}


#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <type_traits>
#include "vec.h"
#include "assert_dbg.h"

#define CLAMP_I(x, min, max) (x > max ? max : x < min ? min : x)
#define CLAMP_F(x) (x > 1.f ? 1.f : x < 0.f ? 0.f : x)
#ifndef M_PI
#define M_PI 3.14159265358979323846 /* pi */
#endif

#define FLOAT_PI 3.141592653f

namespace math {
    static float const F_MIN = 1E-12F;

    template<class T>
    inline typename std::enable_if<!std::numeric_limits<T>::is_integer, bool>::type
    almost_equal(T x, T y, int ulp)
    {
        // the machine epsilon has to be scaled to the magnitude of the values used
        // and multiplied by the desired precision in ULPs (units in the last place)
        return std::fabs(x-y) <= std::numeric_limits<T>::epsilon() * std::fabs(x+y) * ulp
               // unless the result is subnormal
               || std::fabs(x-y) < std::numeric_limits<T>::min();
    }

    template<typename T>
    inline T min(T a, T b) {
        return a < b ? a : b;
    }

    template<typename T>
    inline T max(T a, T b) {
        return a > b ? a : b;
    }

    /* abs(NotInt) */
    template<typename T>
    inline std::enable_if_t<!std::is_integral<T>::value, T>
    abs(T a) {
        return a < 0 ? -a : a;
    }
    /* abs(SignedInt) */
    template<typename T>
    inline std::make_unsigned_t<std::enable_if_t<std::is_integral<T>::value && !std::is_unsigned<T>::value, T>>
    abs(T a) {
        return a < 0 ? -a : a;
    }
    /* abs(UnsignedInt) */
    template<typename T>
    inline std::enable_if_t<std::is_integral<T>::value && std::is_unsigned<T>::value, T>
    abs(T a) {
        return a;
    }

    template<typename T>
    inline T absMax(T a, T b) {
        return abs<T>(a) > abs<T>(b) ? a : b;
    }

    template<typename T>
    inline T absMin(T a, T b) {
        return abs<T>(a) < abs<T>(b) ? a : b;
    }

    /**
     * Round down to integer
     *
     * No range check is applied
     * NAN returns 0
     * INF return 0
     */
    inline double floord(double val) {
        if (std::isnan(val))
            return 0;
        if (std::isinf(val))
            return 0;
        return std::floor(val);
    }

    /**
     * Round down to integer
     *
     * No range check is applied before casting to int64
     * NAN returns 0
     * INF return 0
     */
    inline int64_t floorfS64(float val) {
        if (std::isnan(val))
            return 0;
        if (std::isinf(val))
            return 0;
        float valueFloat = std::floorf(val);
        return static_cast<int64_t>(valueFloat);
    }

    /**
     * Round down to integer
     *
     * No range check is applied before casting to int64
     * NAN returns 0
     * INF return 0
     */
    inline int64_t floordS64(double val) {
        if (std::isnan(val))
            return 0;
        if (std::isinf(val))
            return 0;
        double value = std::floor(val);
        return static_cast<int64_t>(value);
    }

    /**
     * Round down to integer
     *
     * Clamps out of sint32 range values to sint32 range.
     * NAN returns 0
     */
    inline int32_t floorfS32(float val) {
        if (std::isnan(val))
            return 0;
        float val_f = std::floorf(val);
        if (double(val_f) >= double{ std::numeric_limits<int32_t>::max() })
            return std::numeric_limits<int32_t>::max();
        if (double(val_f) <= double{ std::numeric_limits<int32_t>::min() })
            return std::numeric_limits<int32_t>::min();
        return static_cast<int32_t>(val_f);
    }

    /**
     * Round down to integer
     *
     * Clamps out of int32 range values to int32 range.
     * NAN returns 0
     */
    inline int32_t floordS32(double val) {
        if (std::isnan(val))
            return 0;
        double value = std::floor(val);
        if (value >= double{ std::numeric_limits<int32_t>::max() })
            return std::numeric_limits<int32_t>::max();
        if (value <= double{ std::numeric_limits<int32_t>::min() })
            return std::numeric_limits<int32_t>::min();
        return static_cast<int32_t>(value);
    }

    /**
     * Round down to integer
     *
     * Clamps out of uint32 range values to uint32 range.
     * NAN returns 0
     */
    inline uint32_t floorfU32(float val) {
        if (std::isnan(val))
            return 0;
        float val_f = std::floorf(val);
        if (double(val_f) >= double{ std::numeric_limits<uint32_t>::max() })
            return std::numeric_limits<uint32_t>::max();
        if (double(val_f) <= double{ std::numeric_limits<uint32_t>::min() })
            return std::numeric_limits<uint32_t>::min();
        return static_cast<uint32_t>(val_f);
    }

    /**
     * Round down to integer
     *
     * Clamps out of uint32 range values to uint32 range.
     * NAN returns 0
     */
    inline uint32_t floordU32(double val) {
        if (std::isnan(val))
            return 0;
        double value = std::floor(val);
        if (value >= double{ std::numeric_limits<uint32_t>::max() })
            return std::numeric_limits<uint32_t>::max();
        if (value <= double{ std::numeric_limits<uint32_t>::min() })
            return std::numeric_limits<uint32_t>::min();
        return static_cast<uint32_t>(value);
    }

    /**
     * Round up to integer
     *
     * No range check is applied
     * NAN returns 0
     * INF return 0
     */
    inline double ceild(double val) {
        if (std::isnan(val))
            return 0;
        if (std::isinf(val))
            return 0;
        return std::ceil(val);
    }

    /**
     * Round up to integer
     *
     * No range check is applied before casting to int64
     * NAN returns 0
     * INF return 0
     */
    inline int64_t ceilfS64(float val) {
        if (std::isnan(val))
            return 0;
        if (std::isinf(val))
            return 0;
        float valueFloat = std::ceilf(val);
        return static_cast<int64_t>(valueFloat);
    }

    /**
     * Round up to integer
     *
     * No range check is applied before casting to int64
     * NAN returns 0
     * INF return 0
     */
    inline int64_t ceildS64(double val) {
        if (std::isnan(val))
            return 0;
        if (std::isinf(val))
            return 0;
        double value = std::ceil(val);
        return static_cast<int64_t>(value);
    }

    /**
     * Round up to integer
     *
     * Clamps out of sint32 range values to sint32 range.
     * NAN returns 0
     */
    inline int32_t ceilfS32(float val) {
        if (std::isnan(val))
            return 0;
        float val_f = std::ceilf(val);
        if (double(val_f) >= double{ std::numeric_limits<int32_t>::max() })
            return std::numeric_limits<int32_t>::max();
        if (double(val_f) <= double{ std::numeric_limits<int32_t>::min() })
            return std::numeric_limits<int32_t>::min();
        return static_cast<int32_t>(val_f);
    }

    /**
     * Round up to integer
     *
     * Clamps out of int32 range values to int32 range.
     * NAN returns 0
     */
    inline int32_t ceildS32(double val) {
        if (std::isnan(val))
            return 0;
        double value = std::ceil(val);
        if (value >= double{ std::numeric_limits<int32_t>::max() })
            return std::numeric_limits<int32_t>::max();
        if (value <= double{ std::numeric_limits<int32_t>::min() })
            return std::numeric_limits<int32_t>::min();
        return static_cast<int32_t>(value);
    }

    /**
     * Round up to integer
     *
     * Clamps out of uint32 range values to uint32 range.
     * NAN returns 0
     */
    inline uint32_t ceilfU32(float val) {
        if (std::isnan(val))
            return 0;
        float val_f = std::ceilf(val);
        if (double(val_f) >= double{ std::numeric_limits<uint32_t>::max() })
            return std::numeric_limits<uint32_t>::max();
        if (double(val_f) <= double{ std::numeric_limits<uint32_t>::min() })
            return std::numeric_limits<uint32_t>::min();
        return static_cast<uint32_t>(val_f);
    }

    /**
     * Round up to integer
     *
     * Clamps out of uint32 range values to uint32 range.
     * NAN returns 0
     */
    inline uint32_t ceildU32(double val) {
        if (std::isnan(val))
            return 0;
        double value = std::ceil(val);
        if (value >= double{ std::numeric_limits<uint32_t>::max() })
            return std::numeric_limits<uint32_t>::max();
        if (value <= double{ std::numeric_limits<uint32_t>::min() })
            return std::numeric_limits<uint32_t>::min();
        return static_cast<uint32_t>(value);
    }

    /**
     * Round float to neareast integer
     *
     * NAN returns 0
     */
    inline float froundf(float val) {
        // otherwise implementation defined
        if (std::isnan(val))
            return 0;
        return std::roundf(val);
    }

    /**
     * Round float to neareast sint32 integer
     *
     * Clamps out of sint32 range values to sint32 range.
     * NAN returns 0
     */
    inline int32_t roundfS32(float val) {
        // otherwise implementation defined
        if (std::isnan(val))
            return 0;
        if (double(val) >= double{ std::numeric_limits<int32_t>::max() })
            return std::numeric_limits<int32_t>::max();
        if (double(val) <= double{ std::numeric_limits<int32_t>::min() })
            return std::numeric_limits<int32_t>::min();
        int64_t val_s64 = std::lroundf(val);
        dbgassert(val_s64 >= int64_t{ std::numeric_limits<int32_t>::min() });
        dbgassert(val_s64 <= int64_t{ std::numeric_limits<int32_t>::max() });
        return static_cast<int32_t>(val_s64);
    }

    /**
     * Round float to neareast uint32 integer
     *
     * Clamps out of uint32 range values to sint32 range.
     * NAN returns 0
     */
    inline uint32_t roundfU32(float val) {
        // otherwise implementation defined
        if (std::isnan(val))
            return 0;
        if (double(val) >= double{ std::numeric_limits<uint32_t>::max() })
            return std::numeric_limits<uint32_t>::max();
        if (double(val) <= double{ std::numeric_limits<uint32_t>::min() })
            return std::numeric_limits<uint32_t>::min();
        int64_t val_s64 = std::lroundf(val);
        dbgassert(val_s64 >= int64_t{ std::numeric_limits<uint32_t>::min() });
        dbgassert(val_s64 <= int64_t{ std::numeric_limits<uint32_t>::max() });
        return static_cast<uint32_t>(val_s64);
    }

    /**
     * Round float to neareast sint64 integer without
     * additional runtime checks.
     * NAN returns 0
     */
    inline int64_t roundfS64(float val) {
        // otherwise implementation defined
        if (std::isnan(val))
            return 0;
        if (val >= static_cast<double>(std::numeric_limits<int64_t>::max()))
            return std::numeric_limits<int64_t>::max();
        if (val <= static_cast<double>(std::numeric_limits<int64_t>::min()))
            return std::numeric_limits<int64_t>::min();
        int64_t val_s64 = std::llroundf(val);
        dbgassert(val_s64 >= int64_t{ std::numeric_limits<int64_t>::min() });
        dbgassert(val_s64 <= int64_t{ std::numeric_limits<int64_t>::max() });
        return val_s64;
    }

    /**
     * Round float to neareast sint64 integer without
     * additional runtime checks.
     * NAN returns 0
     */
    inline int64_t rounddS64(double val) {
        // otherwise implementation defined
        if (std::isnan(val))
            return 0;
        if (val >= static_cast<double>(std::numeric_limits<int64_t>::max()))
            return std::numeric_limits<int64_t>::max();
        if (val <= static_cast<double>(std::numeric_limits<int64_t>::min()))
            return std::numeric_limits<int64_t>::min();
        int64_t val_s64 = std::llround(val);
        dbgassert(val_s64 >= int64_t{ std::numeric_limits<int64_t>::min() });
        dbgassert(val_s64 <= int64_t{ std::numeric_limits<int64_t>::max() });
        return val_s64;
    }

    /**
     * Round double to neareast sint32 integer
     *
     * Clamps out of sint32 range values to sint32 range.
     * NAN returns 0
     */
    inline int32_t rounddS32(double val) {
        // otherwise implementation defined
        if (std::isnan(val))
            return 0;
        int64_t val_s64 = std::lround(val);
        if (val_s64 >= std::numeric_limits<int32_t>::max())
            return std::numeric_limits<int32_t>::max();
        if (val_s64 <= std::numeric_limits<int32_t>::min())
            return std::numeric_limits<int32_t>::min();
        return static_cast<int32_t>(val_s64);
    }

    inline float powf(float a, float b) {
        return std::pow(a, b);
    }

    template<typename T, typename U>
    inline bool CheckFitsTypeRange(const U value) {
        return double(value) >= std::numeric_limits<T>::min() && double(value) <= std::numeric_limits<T>::max();
    }

    template<typename T>
    inline T clamp(T a, T tmin, T tmax) {
        return a < tmin ? tmin : a > tmax ? tmax : a;
    }
    inline ivec2 maxvec2(const ivec2& a, const ivec2& b) {
        return { math::max(a.x, b.x), math::max(a.y, b.y) };
    }
    inline ivec2 minvec2(const ivec2& a, const ivec2& b) {
        return { math::min(a.x, b.x), math::min(a.y, b.y) };
    }
    inline vec2 maxvec2f(const vec2& a, const vec2& b) {
        return { math::max(a.x, b.x), math::max(a.y, b.y) };
    }
    inline ivec2 absvec2(const ivec2 a) {
        return { math::abs(a.x), math::abs(a.y) };
    }
    inline float distvec2(const vec2 a, const vec2 b) {
        auto vLen = vec2(b - a);
        return glm::length(vLen);
    }
    inline float distancePointLine(const vec2 pt, const vec2 a, const vec2 b) {
        vec2 v      = b - a;
        float lenSq = glm::dot(v, v);
        if (lenSq < 1E-4F) {
            return glm::distance(vec2(pt), vec2(a));
        }
        float t      = math::max(0.0f, math::min(1.0f, glm::dot(vec2(pt - a), v) / lenSq));
        const vec2 p = vec2(a) + t * v;
        return glm::distance(vec2(pt), p);
    }
    inline float calcExponentForScale(float inValue, float outValue, float scaleMin = 0.0f, float scaleMax = 1.0f) {
        float scale = scaleMax - scaleMin;
        return std::log10((outValue - scaleMin) / scale) / std::log10(inValue);
    }
    inline float calcMappedValueForScale(float inValue, float expo, float scaleMin = 0.0f, float scaleMax = 1.0f) {
        return std::pow(inValue, expo) * (scaleMax - scaleMin) + scaleMin;
    }
}// namespace math

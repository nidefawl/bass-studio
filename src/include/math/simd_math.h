#pragma once
#include <immintrin.h>
#include "types.h"
#include "compiler.h"

namespace math::simd {

/**
 * cosSimd: SIMD cosine approximation
 * See: https://stackoverflow.com/a/72098606 
 */
template<typename Type, int Simd>
FUNC_NOINLINE
void 
cos(
    const Type * const PARAM_RESTRICT data,
    Type * const PARAM_RESTRICT result) noexcept
{
    alignas(64)
    Type xSqr[Simd];

    alignas(64)
    Type xSqrSqr[Simd];

    alignas(64)
    Type xSqrSqrSqr[Simd];

    alignas(64)
    Type xSqrSqrSqrSqr[Simd];

    #pragma omp simd
    for(int i=0;i<Simd;i++)
    {
        xSqr[i] =   data[i]*data[i];
    }

    #pragma omp simd
    for(int i=0;i<Simd;i++)
    {
        xSqrSqr[i] =    xSqr[i]*xSqr[i];
    }

    #pragma omp simd
    for(int i=0;i<Simd;i++)
    {
        xSqrSqrSqr[i] =     xSqrSqr[i]*xSqr[i];
    }


    #pragma omp simd
    for(int i=0;i<Simd;i++)
    {
        xSqrSqrSqrSqr[i] =  xSqrSqr[i]*xSqrSqr[i];
    }

    #pragma omp simd
    for(int i=0;i<Simd;i++)
    {
        result[i] =     Type(2.37711074060342753000441e-05)*xSqrSqrSqrSqr[i] +
                            Type(-0.001387712893937020908197155)*xSqrSqrSqr[i] +
                            Type(0.04166611039514833692010143)*xSqrSqr[i] +
                            Type(-0.4999998698566363586337502)*xSqr[i] +
                            Type(0.9999999941252593060880827);
    }
}

#define XM_CONST constexpr
XM_CONST float XM_PI        = 3.141592654f;
XM_CONST float XM_2PI       = 6.283185307f;
XM_CONST float XM_1DIVPI    = 0.318309886f;
XM_CONST float XM_1DIV2PI   = 0.159154943f;
XM_CONST float XM_PIDIV2    = 1.570796327f;
XM_CONST float XM_PIDIV4    = 0.785398163f;

XM_CONST uint32_t XM_SELECT_0   = 0x00000000;
XM_CONST uint32_t XM_SELECT_1   = 0xFFFFFFFF;

XM_CONST uint32_t XM_PERMUTE_0X = 0;
XM_CONST uint32_t XM_PERMUTE_0Y = 1;
XM_CONST uint32_t XM_PERMUTE_0Z = 2;
XM_CONST uint32_t XM_PERMUTE_0W = 3;
XM_CONST uint32_t XM_PERMUTE_1X = 4;
XM_CONST uint32_t XM_PERMUTE_1Y = 5;
XM_CONST uint32_t XM_PERMUTE_1Z = 6;
XM_CONST uint32_t XM_PERMUTE_1W = 7;

XM_CONST uint32_t XM_SWIZZLE_X  = 0;
XM_CONST uint32_t XM_SWIZZLE_Y  = 1;
XM_CONST uint32_t XM_SWIZZLE_Z  = 2;
XM_CONST uint32_t XM_SWIZZLE_W  = 3;

XM_CONST uint32_t XM_CRMASK_CR6         = 0x000000F0;
XM_CONST uint32_t XM_CRMASK_CR6TRUE     = 0x00000080;
XM_CONST uint32_t XM_CRMASK_CR6FALSE    = 0x00000020;
XM_CONST uint32_t XM_CRMASK_CR6BOUNDS   = XM_CRMASK_CR6FALSE;

XM_CONST size_t XM_CACHE_LINE_SIZE = 64;

XM_CONST float XM_LN2 = float(0.69314718055994530942); /* log_e 2 */

inline float XMScalarSin
(
    float Value
)
{
    // Map Value to y in [-pi,pi], x = 2*pi*quotient + remainder.
    float quotient = XM_1DIV2PI*Value;
    if (Value >= 0.0f)
    {
        quotient = static_cast<float>(static_cast<int>(quotient + 0.5f));
    }
    else
    {
        quotient = static_cast<float>(static_cast<int>(quotient - 0.5f));
    }
    float y = Value - XM_2PI*quotient;

    // Map y to [-pi/2,pi/2] with sin(y) = sin(Value).
    if (y > XM_PIDIV2)
    {
        y = XM_PI - y;
    }
    else if (y < -XM_PIDIV2)
    {
        y = -XM_PI - y;
    }

    // 11-degree minimax approximation
    float y2 = y * y;
    return ( ( ( ( (-2.3889859e-08f * y2 + 2.7525562e-06f) * y2 - 0.00019840874f ) * y2 + 0.0083333310f ) * y2 - 0.16666667f ) * y2 + 1.0f ) * y;
}


inline float XMScalarCos
(
    float Value
)
{
    // Map Value to y in [-pi,pi], x = 2*pi*quotient + remainder.
    float quotient = XM_1DIV2PI*Value;
    if (Value >= 0.0f)
    {
        quotient = static_cast<float>(static_cast<int>(quotient + 0.5f));
    }
    else
    {
        quotient = static_cast<float>(static_cast<int>(quotient - 0.5f));
    }
    float y = Value - XM_2PI*quotient;

    // Map y to [-pi/2,pi/2] with cos(y) = sign*cos(x).
    float sign;
    if (y > XM_PIDIV2)
    {
        y = XM_PI - y;
        sign = -1.0f;
    }
    else if (y < -XM_PIDIV2)
    {
        y = -XM_PI - y;
        sign = -1.0f;
    }
    else
    {
        sign = +1.0f;
    }

    // 10-degree minimax approximation
    float y2 = y*y;
    float p = ( ( ( ( -2.6051615e-07f * y2 + 2.4760495e-05f ) * y2 - 0.0013888378f ) * y2 + 0.041666638f ) * y2 - 0.5f ) * y2 + 1.0f;
    return sign*p;
}
inline float sine1(float x) {
    const auto PI      = XM_PI;
    const float B      = 4 / PI;
    const float C      = -4 / (PI * PI);
    const float recip2 = 1 / (PI * 2);
    const float pi2    = PI * 2;
    float y;

    //convert the input value to a range of 0 to 1
    x = (x + PI) * recip2;
    //make it loop
    x -= (long) (x - (x < 0));
    //convert back from 0-1 to -pi to +pi.
    x = (x * pi2) - PI;
    //original function
    y = B * x + C * x * abs(x);//can abs be inlined? It’s a matter of turning off the sign bit.
    return (y);
    //left out the higher precision.
}
inline float sine2(float x) {
    const auto PI = XM_PI;
    // Convert the input value to a range of -1 to 1
    x = x * (1.0f / PI);

    // Wrap around
    volatile float z = (x + 25165824.0f);
    x                = x - (z - 25165824.0f);

#if LOW_SINE_PRECISION
    return 4.0f * (x - x * abs(x));
#else
    float y = x - x * abs(x);

    const float Q = 3.1f;
    const float P = 3.6f;

    return y * (Q + P * abs(y));
#endif
}
template<typename Type, int Simd>
inline
void sin_test2(
    const Type * const PARAM_RESTRICT data,
    Type * const PARAM_RESTRICT result) noexcept
{
    #pragma omp simd
    for(int i=0;i<Simd;i++)
    {
        result[i] = sine1(data[i]);
    }
}
template<typename Type, int Simd>
inline
void sin_test(
    const Type * const PARAM_RESTRICT data,
    Type * const PARAM_RESTRICT result) noexcept
{
    #pragma omp simd
    for(int i=0;i<Simd;i++)
    {
        const Type y = data[i] * XM_PI;
        // 11-degree minimax approximation
        const Type y2 = y * y;
        result[i] = ( ( ( ( (-2.3889859e-08f * y2 + 2.7525562e-06f) * y2 - 0.00019840874f ) * y2 + 0.0083333310f ) * y2 - 0.16666667f ) * y2 + 1.0f ) * y;
    }
}
template<typename Type, int Simd>
inline
void cos_test(
    const Type * const PARAM_RESTRICT data,
    Type * const PARAM_RESTRICT result) noexcept
{
    #pragma omp simd
    for(int i=0;i<Simd;i++)
    {
        const Type y = data[i] * XM_PI;
        const Type y2 = y * y;
        const Type p = ( ( ( ( -2.6051615e-07f * y2 + 2.4760495e-05f ) * y2 - 0.0013888378f ) * y2 + 0.041666638f ) * y2 - 0.5f ) * y2 + 1.0f;
        result[i] = p;
    }
}
template<typename Type, int Simd>
inline
void clamp_zero_one(
    const Type * const PARAM_RESTRICT data,
    Type * const PARAM_RESTRICT result) noexcept
{
    #pragma omp simd
    for(int i=0;i<Simd;i++)
    {
        result[i] = data[i] < 0 ? 0 : data[i] > 1 ? 1 : data[i];
    }
}
template<typename Type, int Simd>
inline
void clamp_neg_one_one(
    const Type * const PARAM_RESTRICT data,
    Type * const PARAM_RESTRICT result) noexcept
{
    #pragma omp simd
    for(int i=0;i<Simd;i++)
    {
        result[i] = data[i] < -1 ? -1 : data[i] > 1 ? 1 : data[i];
    }
}

inline __m128 exp_v4f(__m128 x) {
    const __m128 a  = _mm_set1_ps((1 << 22) / XM_LN2);// to get exp(x/2)
    const __m128i b = _mm_set1_epi32(127 * (1 << 23));      // NB: zero shift!
    __m128i r       = _mm_cvtps_epi32(_mm_mul_ps(a, x));
    __m128i s       = _mm_add_epi32(b, r);
    __m128i t       = _mm_sub_epi32(b, r);
    return _mm_div_ps(_mm_castsi128_ps(s), _mm_castsi128_ps(t));
}

inline __m256 mm256_fmaf(__m256 a, __m256 b, __m256 c) {
    return _mm256_add_ps(_mm256_mul_ps(a, b), c);
}
//https://stackoverflow.com/a/39822314/9007125
//https://stackoverflow.com/a/65537754/9007125
// vectorized version of the answer by njuffa
/* natural log on [0x1.f7a5ecp-127, 0x1.fffffep127]. Maximum relative error 9.4529e-5 */
inline __m256 log_v8f(__m256 a) {
    __m256i aInt = *(__m256i*) (&a);
    __m256i e    = _mm256_sub_epi32(aInt, _mm256_set1_epi32(0x3f2aaaab));
    e            = _mm256_and_si256(e, _mm256_set1_epi32(0xff800000));

    __m256i subtr = _mm256_sub_epi32(aInt, e);
    __m256 m      = *(__m256*) &subtr;
    __m256 i      = _mm256_mul_ps(_mm256_cvtepi32_ps(e), _mm256_set1_ps(1.19209290e-7f));// 0x1.0p-23
    /* m in [2/3, 4/3] */
    __m256 f = _mm256_sub_ps(m, _mm256_set1_ps(1.0f));
    __m256 s = _mm256_mul_ps(f, f);
    /* Compute log1p(f) for f in [-1/3, 1/3] */
    __m256 r = mm256_fmaf(_mm256_set1_ps(0.230836749f), f, _mm256_set1_ps(-0.279208571f));// 0x1.d8c0f0p-3, -0x1.1de8dap-2
    __m256 t = mm256_fmaf(_mm256_set1_ps(0.331826031f), f, _mm256_set1_ps(-0.498910338f));// 0x1.53ca34p-2, -0x1.fee25ap-2

    r = mm256_fmaf(r, s, t);
    r = mm256_fmaf(r, s, f);
    r = mm256_fmaf(i, _mm256_set1_ps(0.693147182f), r);// 0x1.62e430p-1 // log(2)
    return r;
}
inline __m256 one_minus(__m256 a) {
    return _mm256_sub_ps(_mm256_set1_ps(1.0f), a);
}
inline __m256 clamp_one_zero(__m256 a) {
    return _mm256_min_ps(_mm256_max_ps(a, _mm256_set1_ps(0.0f)), _mm256_set1_ps(1.0f));
}
inline __m256 clamp_neg_one_one(__m256 a) {
    return _mm256_min_ps(_mm256_max_ps(a, _mm256_set1_ps(-1.0f)), _mm256_set1_ps(1.0f));
}


} // namespace math

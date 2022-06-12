#if defined(__clang__) || defined(__GNUC__)
namespace fp_math {
    bool isinff(float f) {
        return __builtin_isinf(f);
    }
    bool isinfd(double d) {
        return __builtin_isinf(d);
    }
    bool isnanf(float f) {
        return __builtin_isnan(f);
    }
    bool isnand(double d) {
        return __builtin_isnan(d);
    }
    bool isNanOrInff(float f) {
        return __builtin_isnan(f) || __builtin_isinf(f);
    }
    bool isNanOrInfd(double d) {
        return __builtin_isnan(d) || __builtin_isinf(d);
    }
    float silenceNanInff(float f) {
        return (__builtin_isnan(f) || __builtin_isinf(f)) ? 0.0f : f;
    }
    double silenceNanInfd(double d) {
        return (__builtin_isnan(d) || __builtin_isinf(d)) ? 0.0 : d;
    }
} // namespace fp_math
#else //MSVC and other compilers
#include <cmath>

namespace fp_math {
    bool isinff(float f) {
        return std::isinf(f);
    }
    bool isinfd(double d) {
        return std::isinf(d);
    }
    bool isnanf(float f) {
        return std::isnan(f);
    }
    bool isnand(double d) {
        return std::isnan(d);
    }
    bool isNanOrInff(float f) {
        return std::isnan(f) || std::isinf(f);
    }
    bool isNanOrInfd(double d) {
        return std::isnan(d) || std::isinf(d);
    }
    float silenceNanInff(float f) {
        return (std::isnan(f) || std::isinf(f)) ? 0.0f : f;
    }
    double silenceNanInfd(double d) {
        return (std::isnan(d) || std::isinf(d)) ? 0.0 : d;
    }
} // namespace fp_math
#endif
#pragma once
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
#define hint_likely(expr) (expr)
#define hint_unlikely(expr) (expr)
[[noreturn]] __forceinline void unreachable() {__assume(false);}
#define FUNC_NOINLINE __declspec(noinline)
#elif defined(__GNUC__)
[[noreturn]] inline __attribute__((always_inline)) void unreachable() {__builtin_unreachable();}
#define hint_likely(expr) __builtin_expect((expr), 1)
#define hint_unlikely(expr) __builtin_expect((expr), 0)
#define FUNC_NOINLINE [[gnu::noinline]]
#else
inline void unreachable() {}
#endif

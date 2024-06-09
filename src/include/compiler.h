#pragma once
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
#define DAW_CXX_CONSTINIT
#define hint_likely(expr) (expr)
#define hint_unlikely(expr) (expr)
[[noreturn]] __forceinline void unreachable() {__assume(false);}
#define FUNC_NOINLINE __declspec(noinline)
#define PARAM_RESTRICT __restrict
#elif defined(__GNUC__)
#define DAW_CXX_CONSTINIT constinit
[[noreturn]] inline __attribute__((always_inline)) void unreachable() {__builtin_unreachable();}
#define hint_likely(expr) __builtin_expect((expr), 1)
#define hint_unlikely(expr) __builtin_expect((expr), 0)
#define FUNC_NOINLINE [[gnu::noinline]]
#define PARAM_RESTRICT __restrict__
#else
#error "Compiler not supported"
#endif

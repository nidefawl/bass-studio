#include "basectrl.hpp"
#include "hires_timer.hpp"
#include "platform.hpp"
#include "math/simd_math.hpp"
#include <benchmark/benchmark.h>
#include <cmath>
#include <iostream>
#include <array>

using FPType = float;

constexpr size_t INPUT_LEN = 32;
constexpr size_t ALIGNMENT = 512;
constexpr size_t SIMD_WIDTH = 8;//((256/8) / sizeof(float));
template<size_t L>
std::array<FPType, L> getInputs() {
    std::array<FPType, L> inputsTmp;
    FPType fScale = 1.0f / FPType(L - 1);
    for (size_t i = 0; i < L; i++) {
        inputsTmp[i] = (i * fScale * 2.0f - 1.0f) * FPType(M_PI);
    }
    return inputsTmp;
}
alignas(ALIGNMENT) 
auto inputs = getInputs<INPUT_LEN>();

template<size_t L>
void printError(std::array<FPType, L>& inputs, std::array<FPType, L>& outputs)
{
    FPType totalError = FPType(0);
    for (size_t i = 0; i < L; i++) {
        FPType error = std::abs(std::cos(inputs[i]) - outputs[i]);
        totalError += error;
    }
    // std::cout << "Total error: " << totalError << std::endl;
}

struct cs1 {
    static FPType sin(FPType x) {
        // rdtsc/val: 1.18 rel.err: 4.9538e-06 abs.err 3.5021e-06 [-pi/4, pi/4]{
        benchmark::DoNotOptimize(x);
        x        = x * 0.0f + x;
        float x2 = x * x;
        float y  = (0.00810241699f * x2 - 0.166606188f) * x2 + 1.0f;
        return x * y;
    }
};
struct cs2 {
    static FPType sin(FPType x) {
        // rdtsc/val: 1.18 rel.err: 4.9538e-06 abs.err 3.5021e-06 [-pi/4, pi/4]{
        benchmark::DoNotOptimize(x);
        x           = x * 0.0f + x;
        float xx    = x * x;
        float xs[3] = { 0.0f, 0.19634954084f, 0.78539816339f };
        float ys[3] = { 1.0f, 0.967594854836051f, 0.874145640568115f };

        float A = ys[0] / ((xs[0] - xs[1]) * (xs[0] - xs[2])) +
                  ys[1] / ((xs[1] - xs[0]) * (xs[1] - xs[2])) +
                  ys[2] / ((xs[2] - xs[0]) * (xs[2] - xs[1]));
        float B = (-xs[2] * ys[0]) / ((xs[0] - xs[1]) * (xs[0] - xs[2])) +
                  (-xs[1] * ys[0]) / ((xs[0] - xs[1]) * (xs[0] - xs[2])) +
                  (-xs[2] * ys[1]) / ((xs[1] - xs[0]) * (xs[1] - xs[2])) +
                  (-xs[0] * ys[1]) / ((xs[1] - xs[0]) * (xs[1] - xs[2])) +
                  (-xs[1] * ys[2]) / ((xs[2] - xs[0]) * (xs[2] - xs[1])) +
                  (-xs[0] * ys[2]) / ((xs[2] - xs[0]) * (xs[2] - xs[1]));
        float C = (xs[1] * xs[2] * ys[0]) / ((xs[0] - xs[1]) * (xs[0] - xs[2])) +
                  (xs[0] * xs[2] * ys[1]) / ((xs[1] - xs[0]) * (xs[1] - xs[2])) +
                  (xs[0] * xs[1] * ys[2]) / ((xs[2] - xs[0]) * (xs[2] - xs[1]));

        float y = xx * (A * xx + B) + C;

        return x * y;
    }
};


// measures

static void M_SIN_1(benchmark::State& state) {
    for (auto _ : state) {
        benchmark::ClobberMemory();
        auto inputsCopy  = inputs;
        auto iteratorOut = inputsCopy.begin();
        for (auto y : inputs) {
            *iteratorOut++ = cs1::sin(y);
        }
        benchmark::DoNotOptimize(inputsCopy);
    }
}

static void M_SIN_2(benchmark::State& state) {
    for (auto _ : state) {
        benchmark::ClobberMemory();
        auto inputsCopy  = inputs;
        auto iteratorOut = inputsCopy.begin();
        for (auto y : inputs) {
            *iteratorOut++ = cs2::sin(y);
        }
        benchmark::DoNotOptimize(inputsCopy);
    }
}

static void M_StandardLibrary_Sin(benchmark::State& state) {
    for (auto _ : state) {
        benchmark::ClobberMemory();
        alignas(ALIGNMENT) auto inputsCopy  = inputs;
        auto iteratorOut = inputsCopy.begin();
        for (auto y : inputs) {
            *iteratorOut++ = std::sin(y);
        }
        benchmark::DoNotOptimize(inputsCopy);
    }
}

static void M_MICROSOFT_DX_Sin(benchmark::State& state) {
    for (auto _ : state) {
        benchmark::ClobberMemory();
        alignas(ALIGNMENT) auto inputsCopy  = inputs;
        auto iteratorOut = inputsCopy.begin();
        for (auto y : inputs) {
            *iteratorOut++ = math::simd::XMScalarSin(y);
        }
        benchmark::DoNotOptimize(inputsCopy);
    }
}

static void M_MICROSOFT_DX_Cos(benchmark::State& state) {
    for (auto _ : state) {
        benchmark::ClobberMemory();
        alignas(ALIGNMENT) auto inputsCopy  = inputs;
        auto iteratorOut = inputsCopy.begin();
        for (auto y : inputs) {
            *iteratorOut++ = math::simd::XMScalarCos(y);
            benchmark::ClobberMemory();
        }
        benchmark::DoNotOptimize(inputsCopy);
    }
}

static void M_SIMD_cos(benchmark::State& state) {
    benchmark::ClobberMemory();
    alignas(ALIGNMENT) auto inputsCopy = inputs;
    benchmark::DoNotOptimize(inputsCopy);
    benchmark::DoNotOptimize(inputs);
    for (auto _ : state) {
        benchmark::ClobberMemory();
        inputsCopy = inputs;
        auto pDataIn  = inputs.data();
        auto pDataOut = inputsCopy.data();
        benchmark::DoNotOptimize(pDataIn);
        benchmark::DoNotOptimize(pDataOut);
        benchmark::DoNotOptimize(inputs);
        benchmark::DoNotOptimize(inputsCopy);
        benchmark::ClobberMemory();
        for (size_t i = 0; i < INPUT_LEN; i += SIMD_WIDTH) {
            benchmark::DoNotOptimize(pDataIn);
            benchmark::DoNotOptimize(pDataOut);
            benchmark::DoNotOptimize(inputs);
            benchmark::DoNotOptimize(inputsCopy);
            benchmark::ClobberMemory();
            math::simd::cos<FPType, SIMD_WIDTH>(pDataIn, pDataOut);
            benchmark::DoNotOptimize(pDataIn);
            benchmark::DoNotOptimize(pDataOut);
            benchmark::DoNotOptimize(inputs);
            benchmark::DoNotOptimize(inputsCopy);
            benchmark::ClobberMemory();
            pDataIn += SIMD_WIDTH;
            pDataOut += SIMD_WIDTH;
        }
        benchmark::ClobberMemory();
        benchmark::DoNotOptimize(pDataIn);
        benchmark::DoNotOptimize(pDataOut);
        benchmark::DoNotOptimize(inputs);
        benchmark::DoNotOptimize(inputsCopy);
    }
    printError(inputs, inputsCopy);
}
static void M_SIMD_cos_test(benchmark::State& state) {
    alignas(ALIGNMENT) auto inputsCopy = inputs;
    benchmark::DoNotOptimize(inputsCopy);
    benchmark::DoNotOptimize(inputs);
    for (auto _ : state) {
        benchmark::ClobberMemory();
        inputsCopy = inputs;
        auto pDataIn  = inputs.data();
        auto pDataOut = inputsCopy.data();
        benchmark::DoNotOptimize(pDataIn);
        benchmark::DoNotOptimize(pDataOut);
        benchmark::DoNotOptimize(inputs);
        benchmark::DoNotOptimize(inputsCopy);
        benchmark::ClobberMemory();
        for (size_t i = 0; i < INPUT_LEN; i += SIMD_WIDTH) {
            benchmark::DoNotOptimize(pDataIn);
            benchmark::DoNotOptimize(pDataOut);
            benchmark::DoNotOptimize(inputs);
            benchmark::DoNotOptimize(inputsCopy);
            benchmark::ClobberMemory();
            math::simd::cos_test<FPType, SIMD_WIDTH>(pDataIn, pDataOut);
            benchmark::DoNotOptimize(pDataIn);
            benchmark::DoNotOptimize(pDataOut);
            benchmark::DoNotOptimize(inputs);
            benchmark::DoNotOptimize(inputsCopy);
            benchmark::ClobberMemory();
            pDataIn += SIMD_WIDTH;
            pDataOut += SIMD_WIDTH;
        }
        benchmark::ClobberMemory();
        benchmark::DoNotOptimize(pDataIn);
        benchmark::DoNotOptimize(pDataOut);
        benchmark::DoNotOptimize(inputs);
        benchmark::DoNotOptimize(inputsCopy);
    }
    printError(inputs, inputsCopy);
}
static void M_SIMD_sin_test(benchmark::State& state) {
    alignas(ALIGNMENT) auto inputsCopy = inputs;
    benchmark::DoNotOptimize(inputsCopy);
    benchmark::DoNotOptimize(inputs);
    for (auto _ : state) {
        benchmark::ClobberMemory();
        inputsCopy = inputs;
        auto pDataIn  = inputs.data();
        auto pDataOut = inputsCopy.data();
        benchmark::DoNotOptimize(pDataIn);
        benchmark::DoNotOptimize(pDataOut);
        benchmark::DoNotOptimize(inputs);
        benchmark::DoNotOptimize(inputsCopy);
        benchmark::ClobberMemory();
        for (size_t i = 0; i < INPUT_LEN; i += SIMD_WIDTH) {
            benchmark::DoNotOptimize(pDataIn);
            benchmark::DoNotOptimize(pDataOut);
            benchmark::DoNotOptimize(inputs);
            benchmark::DoNotOptimize(inputsCopy);
            benchmark::ClobberMemory();
            math::simd::sin_test<FPType, SIMD_WIDTH>(pDataIn, pDataOut);
            benchmark::DoNotOptimize(pDataIn);
            benchmark::DoNotOptimize(pDataOut);
            benchmark::DoNotOptimize(inputs);
            benchmark::DoNotOptimize(inputsCopy);
            benchmark::ClobberMemory();
            pDataIn += SIMD_WIDTH;
            pDataOut += SIMD_WIDTH;
        }
        benchmark::ClobberMemory();
        benchmark::DoNotOptimize(pDataIn);
        benchmark::DoNotOptimize(pDataOut);
        benchmark::DoNotOptimize(inputs);
        benchmark::DoNotOptimize(inputsCopy);
    }
    printError(inputs, inputsCopy);
}
static void M_SIMD_sin_test_2(benchmark::State& state) {
    alignas(ALIGNMENT) auto inputsCopy = inputs;
    benchmark::DoNotOptimize(inputsCopy);
    for (auto _ : state) {
        benchmark::ClobberMemory();
        benchmark::DoNotOptimize(inputs);
        inputsCopy = inputs;
        auto pDataIn    = inputs.data();
        auto pDataOut   = inputsCopy.data();
        // benchmark::DoNotOptimize(pDataIn);
        // benchmark::DoNotOptimize(pDataOut);
        // benchmark::DoNotOptimize(inputs);
        // benchmark::DoNotOptimize(inputsCopy);
        // benchmark::ClobberMemory();
        for (size_t i = 0; i < INPUT_LEN; i += SIMD_WIDTH) {
            benchmark::DoNotOptimize(pDataIn);
            math::simd::sin_test<FPType, SIMD_WIDTH>(pDataIn, pDataOut);
            // benchmark::DoNotOptimize(pDataOut);
            pDataIn += SIMD_WIDTH;
            pDataOut += SIMD_WIDTH;
        }
    }
    printError(inputs, inputsCopy);
}

BENCHMARK(M_SIN_1);
BENCHMARK(M_SIN_2);
BENCHMARK(M_MICROSOFT_DX_Sin);
BENCHMARK(M_MICROSOFT_DX_Cos);
BENCHMARK(M_SIMD_cos);
BENCHMARK(M_SIMD_cos_test);
BENCHMARK(M_SIMD_sin_test);
BENCHMARK(M_SIMD_sin_test_2);
BENCHMARK(M_StandardLibrary_Sin);


BENCHMARK_MAIN();

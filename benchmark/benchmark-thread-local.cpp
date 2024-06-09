#include "basectrl.h"
#include "hires_timer.h"
#include "platform.h"
#include <benchmark/benchmark.h>
#include <cstdint>
#define NUM_PASS_STEP 1
#define NUM_EL 100
#define NUM_ALLOC 400
struct data_t {
  int64_t test[4];
};
void FillVector(std::vector<data_t> &vec) {
  int count = NUM_EL;
  vec.reserve(count);
  for (int i = 0; i < count; ++i) {
    vec.push_back({i});
  }
}

void BenchmarkVectorLocal(benchmark::State &state) {
  int64_t pass = 0;
  for (auto _ : state) {
    std::vector<data_t> data;
    if (pass % NUM_PASS_STEP == 0) {
      FillVector(data);
    }
    benchmark::DoNotOptimize(data.data());
    benchmark::ClobberMemory();
    pass++;
  }
}
void BenchmarkVectorLocalOuter(benchmark::State &state) {
  int64_t pass = 0;
  std::vector<data_t> data;
  for (auto _ : state) {
    data.clear();
    if (pass % NUM_PASS_STEP == 0) {
      FillVector(data);
    }
    benchmark::DoNotOptimize(data.data());
    benchmark::ClobberMemory();
    pass++;
  }
}
void BenchmarkVectorStatic(benchmark::State &state) {
  int64_t pass = 0;
  static std::vector<data_t> data;
  for (auto _ : state) {
    data.clear();
    if (pass % NUM_PASS_STEP == 0) {
      FillVector(data);
    }
    benchmark::DoNotOptimize(data.data());
    benchmark::ClobberMemory();
    pass++;
  }
}


void BenchmarkThreadLocal(benchmark::State &state) {
  static thread_local std::vector<data_t> dataThreadLocal;
  int64_t pass = 0;
  for (auto _ : state) {
    auto& data = dataThreadLocal;
    if (data.capacity() == 0) {
      data.reserve(NUM_ALLOC);
    } else {
      data.clear();
    }
    if (pass % NUM_PASS_STEP == 0) {
      FillVector(data);
    }
    benchmark::DoNotOptimize(data.data());
    benchmark::ClobberMemory();
    pass++;
  }
}
void BenchmarkThreadLocalConstInit(benchmark::State &state) {
  thread_local constinit std::vector<data_t> dataThreadLocalConstInit;
  int64_t pass = 0;
  for (auto _ : state) {
    auto& data = dataThreadLocalConstInit;
    if (data.capacity() == 0) {
      data.reserve(NUM_ALLOC);
    } else {
      data.clear();
    }
    if (pass % NUM_PASS_STEP == 0) {
      FillVector(data);
    }
    benchmark::DoNotOptimize(data.data());
    benchmark::ClobberMemory();
    pass++;
  }
}

void BenchmarkThreadLocalArray(benchmark::State &state) {
  static thread_local std::array<data_t, NUM_EL> dataThreadLocal;
  int64_t pass = 0;
  for (auto _ : state) {
    auto& data = dataThreadLocal;
    if (pass % NUM_PASS_STEP == 0) {
      for (int i = 0; i < NUM_EL; ++i) {
        data[i] = {i};
      }
    }
    benchmark::DoNotOptimize(data.data());
    benchmark::ClobberMemory();
    pass++;
  }
}
void BenchmarkThreadLocalArrayConstInit(benchmark::State &state) {
  static constinit thread_local std::array<data_t, NUM_EL> dataThreadLocalConstInit{};
  int64_t pass = 0;
  for (auto _ : state) {
    auto& data = dataThreadLocalConstInit;
    if (pass % NUM_PASS_STEP == 0) {
      for (int i = 0; i < NUM_EL; ++i) {
        data[i] = {i};
      }
    }
    benchmark::DoNotOptimize(data.data());
    benchmark::ClobberMemory();
    pass++;
  }
}
BENCHMARK(BenchmarkVectorLocal);
BENCHMARK(BenchmarkVectorLocalOuter);
BENCHMARK(BenchmarkVectorStatic);
BENCHMARK(BenchmarkThreadLocal);
BENCHMARK(BenchmarkThreadLocalConstInit);
BENCHMARK(BenchmarkThreadLocalArray);
BENCHMARK(BenchmarkThreadLocalArrayConstInit);

BENCHMARK_MAIN();
#include "basectrl.h"
#include "hires_timer.h"
#include "platform.h"
#include <benchmark/benchmark.h>
#include <cstdint>

struct data_t {
  int64_t test[4];
};
void FillVector(std::vector<data_t> &vec, int count) {
  vec.reserve(count);
  for (int i = 0; i < count; ++i) {
    vec.push_back({i});
  }
}
void BenchmarkThreadLocal(benchmark::State &state) {
  static thread_local std::vector<data_t> dataThreadLocal;
  int64_t pass = 0;
  for (auto _ : state) {
    auto& data = dataThreadLocal;
    if (data.capacity() == 0) {
      data.reserve(128);
    } else {
      data.clear();
    }
    if (pass % 13 == 0) {
      FillVector(data, 5);
    }
    benchmark::DoNotOptimize(data.data());
    benchmark::ClobberMemory();
    pass++;
  }
}
void BenchmarkVector(benchmark::State &state) {
  int64_t pass = 0;
  for (auto _ : state) {
    std::vector<data_t> data;
    if (pass % 13 == 0) {
      FillVector(data, 5);
    }
    benchmark::DoNotOptimize(data.data());
    benchmark::ClobberMemory();
    pass++;
  }
}
void BenchmarkVector2(benchmark::State &state) {
  int64_t pass = 0;
  std::vector<data_t> data;
  for (auto _ : state) {
    data.clear();
    if (pass % 13 == 0) {
      FillVector(data, 5);
    }
    benchmark::DoNotOptimize(data.data());
    benchmark::ClobberMemory();
    pass++;
  }
}

BENCHMARK(BenchmarkVector);
BENCHMARK(BenchmarkVector2);
BENCHMARK(BenchmarkThreadLocal);

BENCHMARK_MAIN();
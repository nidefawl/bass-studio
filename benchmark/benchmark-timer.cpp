#include "basectrl.hpp"
#include "hires_timer.hpp"
#include "platform.hpp"
#include <benchmark/benchmark.h>

void BenchmarkTimerInstance(benchmark::State &state) {
  hires_timer_t timer;
  for (auto _ : state) {
    benchmark::DoNotOptimize(timer.getTime());
  }
}
void BenchmarkGlobalGetTime(benchmark::State &state) {
  for (auto _ : state) {
    benchmark::DoNotOptimize(getTimeMicros());
  }
}

BENCHMARK(BenchmarkTimerInstance);
BENCHMARK(BenchmarkGlobalGetTime);

BENCHMARK_MAIN();
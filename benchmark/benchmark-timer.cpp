#include "basectrl.h"
#include "hires_timer.h"
#include "platform.h"
#include <benchmark/benchmark.h>

extern volatile bool fatalError;

void deleteApp() {
}

std::shared_ptr<AppCtrl> makeApp(const std::vector<String>& args) {
  return nullptr;
}
void startApp(std::shared_ptr<AppCtrl>& app) {
}


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
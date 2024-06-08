#include <benchmark/benchmark.h>
#include <array>
#include <cstdio>
#include "host/audiobuffer/audioblock.h"
#include "logging.h"
#include "rand.h"
#include "str_util.h"
#include "exceptions.h"
#include "host/meter/meter.h"
#include "host/meter/meter_old.h"
#include "platform.h"

#include <functional>
#include <memory>
#include "sse.h"

volatile bool fatalError;

int main(int argc, char** argv) {
    std::vector<String> args(&argv[0], &argv[argc]);
    setSSEFlushDenormals();
    // App::Platform::initPlatformEnvironment(BuildInfo::PRODUCT_NAME_LOWER);
    try {
        AudioBlock block(32, 512, false);
        AudioBlock block1024(32, 1024, false);
        // getGlobalLogger()->setLevel(Log::L_WARN);
        benchmark::RegisterBenchmark("running_sum.update", [&block](benchmark::State& state) {
            setSSEFlushDenormals();
            seq_rand rnd;
            rnd.rng_seed(4123123);
            block.fillNoise(rnd, 1.0f);
            DAW::meter_runningsum rs;
            rs.update(block.buf[0], block.samples/16, 1.0f);
            for (auto _ : state) {
                benchmark::ClobberMemory();
                rs.update(block.buf[0], block.samples/16, 1.0f);
                benchmark::DoNotOptimize(rs.getLevels());
            };
        });
        benchmark::RegisterBenchmark("running_sum.update512Fixed", [&block](benchmark::State& state) {
            setSSEFlushDenormals();
            seq_rand rnd;
            rnd.rng_seed(4123123);
            block.fillNoise(rnd, 1.0f);
            DAW::meter_runningsum rs;
            rs.update(block.buf[0], block.samples/16, 1.0f);
            for (auto _ : state) {
                benchmark::ClobberMemory();
                rs.update512Fixed(block.buf[0]);
                benchmark::DoNotOptimize(rs.getLevels());
            };
        });
#if 1

        struct BenchmarkMeter {
            const char* benchmarkName;
            int numChannels;
            AudioBlock* blockInput;
            std::shared_ptr<DAW::MeterOld::meter_runningsum[]> meterData;
            DAW::MeterOld::rmsmeter meter;
            bool isSetupComplete = false;
            BenchmarkMeter(const char* benchmarkName, int numChannels, AudioBlock* blockInput)
                : benchmarkName(benchmarkName), numChannels(numChannels), blockInput(blockInput) {}
        };

        auto BenchMarkRun = [](benchmark::State& state, BenchmarkMeter* context) {
            if (!context->isSetupComplete) {
                context->meterData = std::shared_ptr<DAW::MeterOld::meter_runningsum[]>(new DAW::MeterOld::meter_runningsum[context->numChannels]);
                context->meter     = DAW::MeterOld::rmsmeter(context->meterData.get(), context->numChannels);
                seq_rand rnd;
                rnd.rng_seed(13);
                for (int i = 0; i < 16; i++) {
                    context->blockInput->fillNoise(rnd, 1.0f);
                    context->meter.update(context->blockInput, 1.0f);
                }
                auto lvls = context->meter.getLevels();
                // for (auto& level : lvls) {
                //     log_printf("Levels: fMax %f, fPeak %f, fLvl %f\n", level.fMax, level.fPeak, level.fLvl);
                // }
                context->isSetupComplete = true;
            }
            for (auto _ : state) {
                context->meter.update(context->blockInput, 1.0f);
            };
        };
        std::array<BenchmarkMeter, 5> allBenchmarks = {
            BenchmarkMeter{ "Old 1 Channel 512 Samples", 1, &block },
            BenchmarkMeter{ "Old 2 Channel 512 Samples", 2, &block },
            BenchmarkMeter{ "Old 4 Channel 512 Samples", 4, &block },
            BenchmarkMeter{ "Old 2 Channel 1024 Samples", 2, &block1024 },
            BenchmarkMeter{ "Old 4 Channel 1024 Samples", 4, &block1024 },
        };

        for (BenchmarkMeter& benchmarkCtxt : allBenchmarks) {
            benchmark::RegisterBenchmark(benchmarkCtxt.benchmarkName, BenchMarkRun, &benchmarkCtxt);
        }

        struct BenchmarkMeterMulti {
            const char* benchmarkName;
            int numChannels;
            AudioBlock* blockInput;
            std::shared_ptr<DAW::meter_runningsum[]> meterData;
            DAW::rmsmeter meter;
            bool isSetupComplete = false;
            BenchmarkMeterMulti(const char* benchmarkName, int numChannels, AudioBlock* blockInput)
                : benchmarkName(benchmarkName), numChannels(numChannels), blockInput(blockInput) {}
        };

        auto BenchMarkRunMulti = [](benchmark::State& state, BenchmarkMeterMulti* context) {
            if (!context->isSetupComplete) {
                context->meterData = std::shared_ptr<DAW::meter_runningsum[]>(new DAW::meter_runningsum[context->numChannels]);
                context->meter     = DAW::rmsmeter(context->meterData.get(), context->numChannels);
                seq_rand rnd;
                rnd.rng_seed(13);
                for (int i = 0; i < 16; i++) {
                    context->blockInput->fillNoise(rnd, 1.0f);
                    context->meter.update(context->blockInput, 1.0f);
                }
                // auto& lvls = context->meter.getLevels();
                // for (auto& level : lvls) {
                //     log_printf("Levels: fMax %f, fPeak %f, fLvl %f\n", level.fMax, level.fPeak, level.fLvl);
                // }
                context->isSetupComplete = true;
            }
            for (auto _ : state) {
                context->meter.update(context->blockInput, 1.0f);
            };
        };
        std::array<BenchmarkMeterMulti, 5> allBenchmarks2 = {
            BenchmarkMeterMulti{ "New 1 Channel 512 Samples", 1, &block },
            BenchmarkMeterMulti{ "New 2 Channel 512 Samples", 2, &block },
            BenchmarkMeterMulti{ "New 4 Channel 512 Samples", 4, &block },
            BenchmarkMeterMulti{ "New 2 Channel 1024 Samples", 2, &block1024 },
            BenchmarkMeterMulti{ "New 4 Channel 1024 Samples", 4, &block1024 },
        };

        for (BenchmarkMeterMulti& benchmarkCtxt : allBenchmarks2) {
            benchmark::RegisterBenchmark(benchmarkCtxt.benchmarkName, BenchMarkRunMulti, &benchmarkCtxt);
        }
#endif

        benchmark::Initialize(&argc, argv);
        if (::benchmark::ReportUnrecognizedArguments(argc, argv))
            return 1;
        benchmark::RunSpecifiedBenchmarks();
        benchmark::Shutdown();
    } catch (std::exception& e) {
        log_lf(Log::L_ERROR, "exception %s\n", e.what());
    }
    return 0;
}

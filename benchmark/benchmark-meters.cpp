#include <benchmark/benchmark.h>
#include <array>
#include <cstdio>
#include "audioblock.h"
#include "logging.h"
#include "str_util.h"
#include "exceptions.h"
#include "meter.h"
#include "meter_old.h"
#include "platform.h"

#include <functional>
#include <memory>

extern volatile bool fatalError;

int main(int argc, char** argv) {
    std::vector<String> args(&argv[0], &argv[argc]);
    // App::Platform::initPlatformEnvironment("daw");
    try {
        AudioBlock block(32, 512, false);
        AudioBlock block1024(32, 1024, false);
        // getGlobalLogger()->setLevel(Log::L_WARN);
        benchmark::RegisterBenchmark("running_sum.update", [&block](benchmark::State& state) {
            block.fillNoise(4123123);
            log_printf("block.samples %u\n", block.samples);
            DAW::meter_runningsum rs;
            for (auto _ : state) {
                rs.update(block.buf[0], block.samples/16, 1.0f);
            };
        });
        benchmark::RegisterBenchmark("running_sum.update512Fixed", [&block](benchmark::State& state) {
            block.fillNoise(4123123);
            log_printf("block.samples %u\n", block.samples);
            DAW::meter_runningsum rs;
            for (auto _ : state) {
                rs.update512Fixed(block.buf[0]);
            };
        });

        struct BenchmarkMeter {
            const char* benchmarkName;
            int numChannels;
            AudioBlock* blockInput;
            std::shared_ptr<DAW::MeterOld::meter_runningsum[]> meterData;
            DAW::MeterOld::rmsmeter meter;
            bool isSetupComplete = false;
        };

        auto BenchMarkRun = [](benchmark::State& state, BenchmarkMeter* context) {
            if (!context->isSetupComplete) {
                context->meterData = std::shared_ptr<DAW::MeterOld::meter_runningsum[]>(new DAW::MeterOld::meter_runningsum[context->numChannels]);
                context->meter     = DAW::MeterOld::rmsmeter(context->meterData.get(), context->numChannels);
                uint32_t seed = 13;
                for (int i = 0; i < 16; i++) {
                    context->blockInput->fillNoise(seed++);
                    context->meter.update(context->blockInput, 1.0f);
                }
                auto lvls = context->meter.getLevels();
                for (auto& level : lvls) {
                    log_printf("Levels: fMax %f, fPeak %f, fLvl %f\n", level.fMax, level.fPeak, level.fLvl);
                }
                context->isSetupComplete = true;
            }
            for (auto _ : state) {
                context->meter.update(context->blockInput, 1.0f);
            };
        };
        std::array<BenchmarkMeter, 5> allBenchmarks = {
            BenchmarkMeter{ "Process 1 Channel 512 Samples", 1, &block },
            BenchmarkMeter{ "Process 2 Channel 512 Samples", 2, &block },
            BenchmarkMeter{ "Process 4 Channel 512 Samples", 4, &block },
            BenchmarkMeter{ "Process 2 Channel 1024 Samples", 2, &block1024 },
            BenchmarkMeter{ "Process 4 Channel 1024 Samples", 4, &block1024 },
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
        };
        auto BenchMarkRunMulti = [](benchmark::State& state, BenchmarkMeterMulti* context) {
            if (!context->isSetupComplete) {
                context->meterData = std::shared_ptr<DAW::meter_runningsum[]>(new DAW::meter_runningsum[context->numChannels]);
                context->meter     = DAW::rmsmeter(context->meterData.get(), context->numChannels);
                uint32_t seed = 13;
                for (int i = 0; i < 16; i++) {
                    context->blockInput->fillNoise(seed++);
                    context->meter.update(context->blockInput, 1.0f);
                }
                auto lvls = context->meter.getLevels();
                for (auto& level : lvls) {
                    log_printf("Levels: fMax %f, fPeak %f, fLvl %f\n", level.fMax, level.fPeak, level.fLvl);
                }
                context->isSetupComplete = true;
            }
            for (auto _ : state) {
                context->meter.update(context->blockInput, 1.0f);
            };
        };
        std::array<BenchmarkMeterMulti, 5> allBenchmarks2 = {
            BenchmarkMeterMulti{ "Multi Process 1 Channel 512 Samples", 1, &block },
            BenchmarkMeterMulti{ "Multi Process 2 Channel 512 Samples", 2, &block },
            BenchmarkMeterMulti{ "Multi Process 4 Channel 512 Samples", 4, &block },
            BenchmarkMeterMulti{ "Multi Process 2 Channel 1024 Samples", 2, &block1024 },
            BenchmarkMeterMulti{ "Multi Process 4 Channel 1024 Samples", 4, &block1024 },
        };

        for (BenchmarkMeterMulti& benchmarkCtxt : allBenchmarks2) {
            benchmark::RegisterBenchmark(benchmarkCtxt.benchmarkName, BenchMarkRunMulti, &benchmarkCtxt);
        }


        benchmark::Initialize(&argc, argv);
        if (::benchmark::ReportUnrecognizedArguments(argc, argv))
            return 1;
        benchmark::RunSpecifiedBenchmarks();
        benchmark::Shutdown();
    } catch (std::exception& e) {
        std::printf("exception %s\n", e.what());
    } catch (...) {
        std::printf("unhandled exception\n");
    }
    return 0;
}

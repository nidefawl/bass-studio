#include <benchmark/benchmark.h>
#include <array>
#include "str_util.h"
#include "seq_time.h"
#include "exceptions.h"
#include "platform.h"
#include "appsettings.h"
#include "project.h"
#include "host/projectcontroller.h"
#include "samplerate.h"
#include "track.h"
#include "host/vst_host.h"
#include "util/testing_environment.h"
#include "appconfig.h"

#include <memory>

#ifdef _WIN32
#include "platform/win/windowsize.h"
#include "platform/win/platform_win.h"
#endif

extern volatile bool fataError;

void deleteApp() {
}

std::shared_ptr<AppCtrl> makeApp(const std::vector<String>& args) {
  return nullptr;
}
void startApp(std::shared_ptr<AppCtrl>& app) {
}

int main(int argc, char **argv)
{
    std::vector<String> args(&argv[0], &argv[argc]);
    setExceptionHandler();

    String cwdPath;
    if (determineUserdataPath(cwdPath)) {
        setUserdataPath(cwdPath + "\\daw\\");
    }
    auto dawInstance = std::make_shared<DawInstance>();
    try 
    {
        using DAW::settings;
        settings  = loadSettings();
        settings.iosettings.midiconfigs.clear();
        settings.iosettings.configs.clear();
        settings.iosettings.asioConfig = {};
        settings.startEngine = false;

        dawInstance->initDaw();
        dawInstance->startDaw();
        dawInstance->initProcessingResources();

        
        auto& projectGlobals = dawInstance->getGlobals();
        auto host = dawInstance->getHost();

        projectGlobals.loopEnabled = false;
        host->prjGlobals = projectGlobals;

        struct TestData
        {
            int32_t numTracks;
            int32_t tickPos;
        };

        struct TestContext
        {
            bool isSetupComplete;
            DawInstance* dawInstance;
            TestData testData;
        };

        auto BenchMarkRun = [](benchmark::State &state, TestContext* context)
        {
            DawInstance* dawInstance = context->dawInstance;
            TestData& testData = context->testData;

            if (!context->isSetupComplete) {
                dawInstance->setEmptyProject();
                for (int i = 0; i < testData.numTracks; ++i) {
                    track_t* track1 = new track_t(TRACK_TYPE_MIDI, StringFormat("track%d", i), true);
                    dawInstance->addTrackImpl(-1, track1, 0);
                }

                track_t* trackMaster = new track_t(TRACK_TYPE_MASTER, "master", true);
                dawInstance->addTrackImpl(0, trackMaster, 0);

                context->isSetupComplete = true;
            }
            auto host = dawInstance->getHost();
            const auto tempo100 = dawInstance->getGlobals().tempo100;
            const auto sf = host->m_sampleFormatInternal;
            const auto& sr = sf.sampleRate;
            const double ticksPerBlock = sampleToTickConvert<double, roundmode::none>(sf.blockSize, tempo100, sf.sampleRate);

            tick_t tickPos    = testData.tickPos;
            int32_t samplePos = tickToSampleConvert<int32_t, roundmode::floor>(tickPos, tempo100, sf.sampleRate);
            
            // puts("Benchmark warmup");
            
            for (int i = 0; i < 100; i++)
            {
                int32_t processedBlock = host->processRender(dawInstance, samplePos, tickPos);
                dbgassert(processedBlock > 0);
                samplePos += host->m_sampleFormatInternal.blockSize * processedBlock;
                tickPos += ticksPerBlock * processedBlock;
            };

            tickPos    = testData.tickPos;
            samplePos  = tickToSampleConvert<int32_t, roundmode::floor>(tickPos, tempo100, sr);

            // puts("Benchmark run");
            
            for (auto _ : state)
            {
                int32_t processedBlock = host->processRender(dawInstance, samplePos, tickPos);
                dbgassert(processedBlock > 0);
                samplePos += host->m_sampleFormatInternal.blockSize * processedBlock;
                tickPos += ticksPerBlock * processedBlock;
            };
        };

        std::array<TestContext, 3> testCtxts = {
            TestContext{false, dawInstance.get(), TestData{  0, 0 }},
            TestContext{false, dawInstance.get(), TestData{  2, 0 }},
            TestContext{false, dawInstance.get(), TestData{ 32, 0 }},
        };
        benchmark::RegisterBenchmark("processRender 0 Tracks", BenchMarkRun, &testCtxts[0]);
        benchmark::RegisterBenchmark("processRender 2 Tracks", BenchMarkRun, &testCtxts[1]);
        benchmark::RegisterBenchmark("processRender 32 Tracks", BenchMarkRun, &testCtxts[2]);

/*       

        for (auto &test_input : testInputs)
        {
            puts("RegisterBenchmark");
            benchmark::RegisterBenchmark(
                test_input.name,
                BenchMarkRun,
                test_input.data);
        } */


        benchmark::Initialize(&argc, argv);
        if (::benchmark::ReportUnrecognizedArguments(argc, argv))
            return 1;
        benchmark::RunSpecifiedBenchmarks();
        benchmark::Shutdown();
    } catch (std::exception& e) {
        log_printf("exception %s\n", e.what());
    } catch (...) {
        log_printf("unhandled exception\n", 0);
    }
        
    dawInstance->destroy();
    return 0;
}
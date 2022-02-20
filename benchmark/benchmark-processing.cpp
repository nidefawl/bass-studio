#include <benchmark/benchmark.h>
#include <array>
#include "automation.h"
#include "logging.h"
#include "note.h"
#include "str_util.h"
#include "exceptions.h"
#include "appconfig.h"
#include "appsettings.h"
#include "platform.h"
#include "basectrl.h"
#include "samplerate.h"
#include "seq_time.h"
#include "track.h"
#include "track_impl.h"
#include "project.h"
#include "host/audio_config.h"
#include "host/mainctrl.h"
#include "host/projectcontroller.h"
#include "host/vst_host.h"
#include "util/testing_environment.h"

#include <memory>

#ifdef _WIN32
#include "platform/win/windowsize.h"
#include "platform/win/platform_win.h"
#endif

extern volatile bool fatalError;
namespace {
class FakeAudioStream : public AudioIO::AudioStream {
    const sampleformat_t sampleformat;
    AudioBuffer* const bufferInput;
    AudioBuffer* const bufferOutput;
    int32_t inputQueueSize = 0;
    int32_t outputQueueSize = 0;
public:
    explicit FakeAudioStream(sampleformat_t sf)
            : sampleformat(sf),
              bufferInput(allocateBuffer(32)),
              bufferOutput(allocateBuffer(32))
    {
    }

    ~FakeAudioStream() override = default;

    void enqueue(AudioBuffer* buf) override {
        //if (buf)
        //    buf->inUse = false;
        ++outputQueueSize;
    }
    bool try_dequeue(AudioBuffer*& buf) override {
        if (outputQueueSize) {
            --outputQueueSize;
            buf = bufferOutput;
            return true;
        }
        return false;
    }
    void enqueueInput(AudioBuffer* buf) override {
        //if (buf)
        //    buf->inUse = false;
        ++inputQueueSize;
    }
    bool try_dequeueInput(AudioBuffer*& buf) override {
        if (inputQueueSize) {
            --inputQueueSize;
            buf = bufferInput;
            return true;
        }
        return false;
    }
    int32_t getOutputQueueSize() const override {
        return outputQueueSize;
    }
    int32_t getInputQueueSize() const override {
        return inputQueueSize;
    }
    samplerate_t getSampleRate() const override {
        return sampleformat.sampleRate;
    }
    uint16_t getBlockSize() const override {
        return sampleformat.blockSize;
    }
    bool isActive() const override {
        return true;
    }
};
}
int main(int argc, char** argv) {
    getGlobalLogger()->setLevel(Log::L_WARN);
    std::vector<String> args(&argv[0], &argv[argc]);
    setExceptionHandler();

    App::Platform::initPlatformEnvironment("daw");

    auto dawInstance = std::make_shared<DawInstance>();
    try {
        sampleformat_t sampleformat = {44100, 512, sampleformat_bits_t::FLOAT_32};
        std::shared_ptr<AudioIO::AudioStream> audiostream = std::make_shared<FakeAudioStream>(sampleformat);

        using DAW::settings;
        settings = loadSettings();
        settings.iosettings.midiconfigs.clear();
        settings.iosettings.configs.clear();
        settings.iosettings.asioConfig = {};
        settings.iosettings.samplerate = sampleformat.sampleRate;
        settings.iosettings.blocksize = sampleformat.sampleRate;
        settings.iosettings.internalBlocksize = sampleformat.blockSize;
        settings.iosettings.blocksize = sampleformat.blockSize;
        settings.startEngine           = false;

        dawInstance->initDaw();
        dawInstance->startDaw();
        dawInstance->initProcessingResources();


        auto& projectGlobals = dawInstance->getGlobals();
        auto host            = dawInstance->getHost();

        projectGlobals.loopEnabled = true;
        projectGlobals.loopStart   = 0;
        projectGlobals.loopLen     = TICKS_BAR * 4;
        host->prjGlobals           = projectGlobals;
        host->setOutput(audiostream);

        struct TestContext {
            bool isSetupComplete;
            DawInstance* dawInstance;
            std::function<void(TestContext*)> setupTestCase;
        };

        auto BenchMarkRun = [stream=audiostream.get()](benchmark::State& state, TestContext* context) {
            DawInstance* dawInstance = context->dawInstance;

            if (!context->isSetupComplete) {
                dawInstance->setEmptyProject();
                context->setupTestCase(context);
                context->isSetupComplete = true;
            }
            auto host                  = dawInstance->getHost();
            const auto& projGlobals    = dawInstance->getGlobals();
            const auto tempo100        = projGlobals.tempo100;
            const auto sf              = host->m_sampleFormatInternal;
            const auto& sr             = sf.sampleRate;
            const double ticksPerBlock = sampleToTickConvert<double, roundmode::none>(sf.blockSize, tempo100, sf.sampleRate);

            double tickPos    = projGlobals.loopStart;
            int32_t samplePos = tickToSampleConvert<int32_t, roundmode::floor>(tickPos, tempo100, sf.sampleRate);

            // puts("Benchmark warmup");

            //for (int i = 0; i < 100; i++) {
            //    const bool isLoopAround = tickPos + ticksPerBlock >= projGlobals.loopStart + projGlobals.loopLen;
            //    const bool inLoop = projGlobals.loopEnabled && (tickPos >= projGlobals.loopStart) && (tickPos < projGlobals.loopStart + projGlobals.loopLen);
            //    int32_t processedBlock = host->processPlayback(dawInstance, samplePos, tickPos, playback_state::status_playback, inLoop, isLoopAround);
            //
            //    dbgassert(processedBlock > 0);
            //    samplePos += host->m_sampleFormatInternal.blockSize * processedBlock;
            //    tickPos += ticksPerBlock * processedBlock;
            //    if (isLoopAround) {
            //        tickPos   = projGlobals.loopStart;
            //        samplePos = tickToSampleConvert<int32_t, roundmode::floor>(tickPos, tempo100, sf.sampleRate);
            //    }
            //};

            tickPos   = projGlobals.loopStart;
            samplePos = tickToSampleConvert<int32_t, roundmode::floor>(tickPos, tempo100, sr);
            //log_lf(Log::L_WARN, "START ON %s seconds: %.2f - sample %d\n", StringAsCStr(tickAsBeatString(tickPos)), toSeconds(tickPos, tempo100), samplePos);

            // puts("Benchmark run");

            host->onStartPlayback(dawInstance);
            for (auto _ : state) {
                const bool isLoopAround = tickPos + ticksPerBlock >= projGlobals.loopStart + projGlobals.loopLen;
                const bool inLoop = projGlobals.loopEnabled && (tickPos >= projGlobals.loopStart) && (tickPos < projGlobals.loopStart + projGlobals.loopLen);
                while (stream->getInputQueueSize() < 2) {
                    stream->enqueueInput(nullptr);
                }
                int32_t processedBlock = host->processPlayback(dawInstance, samplePos, tickPos, playback_state::status_playback, inLoop, isLoopAround);
                while (stream->getOutputQueueSize() > 0) {
                    AudioBuffer* dequeuedBuf = nullptr;
                    dbgassert(stream->try_dequeue(dequeuedBuf));
                    if (dequeuedBuf) dequeuedBuf->inUse = false;
                }
                if (processedBlock < 1) {
                    log_lf(Log::L_WARN, "Unexpected: %d processed blocks\n", processedBlock);
                }
                samplePos += host->m_sampleFormatInternal.blockSize * processedBlock;
                tickPos += ticksPerBlock * processedBlock;
                if (isLoopAround && processedBlock) {
                    tickPos   = projGlobals.loopStart;
                    samplePos = tickToSampleConvert<int32_t, roundmode::floor>(tickPos, tempo100, sf.sampleRate);
                    log_printf("");
                }
            };
            host->onStopPlayback(dawInstance);
        };
        trackdata_midi_t trDataMidi;
        {
            auto clip = new clip_t;
            for (int i = 0; i < 4; i++) {
                note_t note;
                note.time     = i * TICKS_BAR;
                note.len      = TICKS_BAR;
                note.velocity = 123;
                note.pitch    = 32 + 0;
                clip->notes.add(note);
                note.pitch = 32 + 3;
                clip->notes.add(note);
                note.pitch = 32 + 7;
                clip->notes.add(note);
                note.pitch = 32 + 12;
                clip->notes.add(note);
            }
            clip->notes.updateBounds();
            clip->loopEnabled = true;
            clip->loopStart   = 0;
            clip->loopLen     = 4 * TICKS_BAR;
            clip->len         = 64 * TICKS_BAR;
            clip->time        = 0 * TICKS_BAR;
            trDataMidi.addClip(clip);
        }
        auto setupNTracks = [](DawInstance* dawInstance, int numTracks) {
            for (int i = 0; i < numTracks; ++i) {
                auto track1 = new track_t(TRACK_TYPE_MIDI, StringFormat("track%d", i), true);
                dawInstance->addTrackImpl(-1, track1, 0);
            }
            auto trackMaster = new track_t(TRACK_TYPE_MASTER, "master", true);
            dawInstance->addTrackImpl(0, trackMaster, 0);
        };

        auto testCase0Tracks = [&setupNTracks](TestContext* context) {
            DawInstance* dawInstance = context->dawInstance;
            setupNTracks(dawInstance, 0);
        };

        auto testCase2Tracks = [&setupNTracks](TestContext* context) {
            DawInstance* dawInstance = context->dawInstance;
            setupNTracks(dawInstance, 2);
        };

        auto testCase32Tracks = [&setupNTracks](TestContext* context) {
            DawInstance* dawInstance = context->dawInstance;
            setupNTracks(dawInstance, 32);
        };

        auto testCase32TracksWithMidi = [&trDataMidi](TestContext* context) {
            DawInstance* dawInstance = context->dawInstance;
            for (int i = 0; i < 32; ++i) {
                auto track1 = new track_t(TRACK_TYPE_MIDI, StringFormat("track%d", i), true);
                // deep copy (of clip_t instances)
                track1->getMidi() = trDataMidi;
                dawInstance->addTrackImpl(-1, track1, 0);
            }

            auto trackMaster = new track_t(TRACK_TYPE_MASTER, "master", true);
            dawInstance->addTrackImpl(0, trackMaster, 0);
        };

        std::array<TestContext, 4> testCtxts = {
            TestContext{ false, dawInstance.get(), testCase0Tracks },
            TestContext{ false, dawInstance.get(), testCase2Tracks },
            TestContext{ false, dawInstance.get(), testCase32Tracks },
            TestContext{ false, dawInstance.get(), testCase32TracksWithMidi },
        };
        benchmark::RegisterBenchmark("processRender 0 Tracks", BenchMarkRun, &testCtxts[0]);
        benchmark::RegisterBenchmark("processRender 2 Tracks", BenchMarkRun, &testCtxts[1]);
        benchmark::RegisterBenchmark("processRender 32 Tracks", BenchMarkRun, &testCtxts[2]);
        benchmark::RegisterBenchmark("processRender 32 Tracks + Midi", BenchMarkRun, &testCtxts[3]);

        benchmark::Initialize(&argc, argv);
        if (::benchmark::ReportUnrecognizedArguments(argc, argv))
            return 1;
        benchmark::RunSpecifiedBenchmarks();
        benchmark::Shutdown();
        trDataMidi.deleteClips(nullptr);
    } catch (std::exception& e) {
        log_printf("exception %s\n", e.what());
    } catch (...) {
        log_printf("unhandled exception\n", 0);
    }

    dawInstance->destroy();
    return 0;
}

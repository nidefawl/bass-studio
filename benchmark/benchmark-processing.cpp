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
#include "host/midiarp.h"
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
    //AudioBuffer* const bufferOutput;
    int32_t inputQueueSize = 0;
    int32_t outputQueueSize = 0;
public:
    explicit FakeAudioStream(sampleformat_t sf)
            : sampleformat(sf),
              bufferInput(allocateBuffer(32))
          //,
          //    bufferOutput(allocateBuffer(32))
    {
        bufferInput->output->realloc(sf.blockSize);
        //bufferOutput->output->realloc(sf.blockSize);
    }

    ~FakeAudioStream() override = default;

    // Called from vst_host
    void enqueue(AudioBuffer* buf) override {
        if (buf)
            buf->inUse = false;
        ++outputQueueSize;
    }

    // Called from external side
    bool try_dequeue(AudioBuffer*& buf) override {
        if (outputQueueSize) {
            --outputQueueSize;
            buf = nullptr;
            return true;
        }
        return false;
    }
    // Called from external side
    void enqueueInput(AudioBuffer* buf) override {
        dbgassert(!buf);
        ++inputQueueSize;
    }

    // Called from vst_host
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
    std::vector<String> args(&argv[0], &argv[argc]);
    setExceptionHandler();

    App::Platform::initPlatformEnvironment("daw");

    auto dawInstance = std::make_shared<DawInstance>();
    try {
        sampleformat_t sampleformat = {44100, 512, sampleformat_bits_t::FLOAT_32};
        log_out("Testing Samplerate %uHz at Blocksize %u\n", sampleformat.sampleRate, sampleformat.blockSize);
        log_out("Deadline for block: %uns\n", (sampleformat.blockSize*1000000000ULL)/sampleformat.sampleRate);
        getGlobalLogger()->setLevel(Log::L_WARN);

        std::shared_ptr<AudioIO::AudioStream> audiostream = std::make_shared<FakeAudioStream>(sampleformat);

        using DAW::settings;
        settings = loadSettings();
        settings.iosettings.midiconfigs.clear();
        settings.iosettings.configs.clear();
        settings.iosettings.asioConfig = {};
        settings.iosettings.samplerate = sampleformat.sampleRate;
        settings.iosettings.internalSamplerate = sampleformat.sampleRate;
        settings.iosettings.internalBlocksize = sampleformat.blockSize;
        settings.iosettings.blocksize = sampleformat.blockSize;
        settings.startEngine           = false;

        dawInstance->initDaw();
        dbgassert(dawInstance->getHost()->m_sampleFormatInternal == sampleformat);
        dawInstance->startDaw();
        dawInstance->initProcessingResources();


        auto& projectGlobals = dawInstance->getGlobals();
        auto host            = dawInstance->getHost();

        projectGlobals.loopEnabled = true;
        projectGlobals.loopStart   = 0;
        projectGlobals.loopLen     = TICKS_BAR * 32;
        host->prjGlobals           = projectGlobals;
        host->setThreadCount(8);
        host->setOutput(audiostream);

        struct TestContext {
            const char* benchmarkName;
            bool isSetupComplete;
            DawInstance* dawInstance;
            std::function<void(TestContext*)> setupTestCase;
        };

        auto BenchMarkRun = [stream=audiostream.get()](benchmark::State& state, TestContext* context) {
            DawInstance* dawInstance = context->dawInstance;

            if (!context->isSetupComplete) {
                dawInstance->unloadProject();
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

            tickPos   = projGlobals.loopStart;
            samplePos = tickToSampleConvert<int32_t, roundmode::floor>(tickPos, tempo100, sr);
            //log_lf(Log::L_WARN, "START ON %s seconds: %.2f - sample %d\n", StringAsCStr(tickAsBeatString(tickPos)), toSeconds(tickPos, tempo100), samplePos);

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

        auto test32TracksEmpty = [&setupNTracks](TestContext* context) {
            DawInstance* dawInstance = context->dawInstance;
            setupNTracks(dawInstance, 32);
        };

        auto test32TracksEmptyNoArp = [&trDataMidi](TestContext* context) {
            DawInstance* dawInstance = context->dawInstance;
            for (int i = 0; i < 32; ++i) {
                auto track1 = new track_t(TRACK_TYPE_MIDI, StringFormat("track%d", i), true);
                dawInstance->addTrackImpl(-1, track1, 0);
                delete track1->getStage()->arp;
                track1->getStage()->arp = nullptr;
            }

            auto trackMaster = new track_t(TRACK_TYPE_MASTER, "master", true);
            dawInstance->addTrackImpl(0, trackMaster, 0);
            delete trackMaster->getStage()->arp;
            trackMaster->getStage()->arp = nullptr;
        };

        auto test32TracksMidi = [&trDataMidi](TestContext* context) {
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

        auto test32TracksMidiNoArp = [&trDataMidi](TestContext* context) {
            DawInstance* dawInstance = context->dawInstance;
            for (int i = 0; i < 32; ++i) {
                auto track1 = new track_t(TRACK_TYPE_MIDI, StringFormat("track%d", i), true);
                // deep copy (of clip_t instances)
                track1->getMidi() = trDataMidi;
                dawInstance->addTrackImpl(-1, track1, 0);
                delete track1->getStage()->arp;
                track1->getStage()->arp = nullptr;
            }

            auto trackMaster = new track_t(TRACK_TYPE_MASTER, "master", true);
            dawInstance->addTrackImpl(0, trackMaster, 0);
            delete trackMaster->getStage()->arp;
            trackMaster->getStage()->arp = nullptr;
        };

        auto test32TracksMidiAndArp = [&trDataMidi](TestContext* context) {
            DawInstance* dawInstance = context->dawInstance;
            for (int i = 0; i < 32; ++i) {
                auto track1 = new track_t(TRACK_TYPE_MIDI, StringFormat("track%d", i), true);
                // deep copy (of clip_t instances)
                track1->getMidi() = trDataMidi;
                dawInstance->addTrackImpl(-1, track1, 0);
                track1->getStage()->arp->setParamValue(PARAM_ENABLE, 1.0f, FLG_PAR_UPDATE_INIT);
                track1->getStage()->arp->setParamValue(ARP_PARAM_CLOCK, 0.4f, FLG_PAR_UPDATE_INIT);
                track1->getStage()->arp->setParamValue(ARP_PARAM_PATTERN, 1.0f, FLG_PAR_UPDATE_INIT);
                track1->getStage()->arp->setParamValue(ARP_PARAM_RAND_VEL, 0.7f, FLG_PAR_UPDATE_INIT);
                track1->getStage()->arp->setParamValue(ARP_PARAM_GATE, 0.55f, FLG_PAR_UPDATE_INIT);
            }

            auto trackMaster = new track_t(TRACK_TYPE_MASTER, "master", true);
            dawInstance->addTrackImpl(0, trackMaster, 0);
        };

        auto testGroups = [&trDataMidi](TestContext* context) {
            DawInstance* dawInstance = context->dawInstance;
            auto host = dawInstance->getHost();
            // create 4 busses that look like:
            // BUS_TOP_n
            //   SUB_BUS_1
            //     TRACK_1
            //     TRACK_2
            //     TRACK_3
            //     TRACK_4
            //   SUB_BUS_2
            //     TRACK_1
            //     TRACK_2
            //     TRACK_3
            //     TRACK_4
            for (int topGrps = 0; topGrps < 4; ++topGrps) {
                auto trTop = dawInstance->createNewTrack(TRACK_TYPE_AUDIO);
                trTop->name = StringFormat("Top Bus %d", topGrps);
                dawInstance->addTrackImpl(-1, trTop, 0);
                for (int subGrps = 0; subGrps < 2; ++subGrps) {
                    auto trSubGrp = dawInstance->createNewTrack(TRACK_TYPE_AUDIO);
                    trSubGrp->name = StringFormat("Sub Bus %d.%d", topGrps, subGrps);
                    trTop->addChild(trSubGrp);
                    dawInstance->addTrackImpl(-1, trSubGrp, 0);
                    for (int i = 0; i < 4; ++i) {
                        auto track1 = new track_t(TRACK_TYPE_MIDI, StringFormat("Track #%d.%d.%d", topGrps, subGrps, i), true);
                        trSubGrp->addChild(track1);
                        // deep copy (of clip_t instances)
                        track1->getMidi() = trDataMidi;
                        dawInstance->addTrackImpl(-1, track1, 0);

                        auto pluginInstance = dawInstance->getHost()->makeModuleInstance(PLUGIN_TYPE_INTERNAL_EFFECT, PLUG_INT_HOSTINFO, -1);
                        dbgassert(pluginInstance);
                        host->insertNewPlugin(track1->getStage(), pluginInstance, 0);
                        pluginInstance->resume();
                        track1->getStage()->pluginsChanged();
                    }
                }
            }

            auto trackMaster = new track_t(TRACK_TYPE_MASTER, "master", true);
            dawInstance->addTrackImpl(0, trackMaster, 0);
        };

        auto testGroups2 = [&trDataMidi](TestContext* context) {
            DawInstance* dawInstance = context->dawInstance;
            auto host = dawInstance->getHost();
            // create 4 busses that look like:
            // BUS_TOP_n
            //   SUB_BUS_1
            //     TRACK_1
            //     TRACK_2
            //     TRACK_3
            //     TRACK_4
            //   SUB_BUS_2
            //     TRACK_1
            //     TRACK_2
            //     TRACK_3
            //     TRACK_4
            for (int topGrps = 0; topGrps < 4; ++topGrps) {
                auto trTop = dawInstance->createNewTrack(TRACK_TYPE_AUDIO);
                trTop->name = StringFormat("Top Bus %d", topGrps);
                dawInstance->addTrackImpl(-1, trTop, 0);
                for (int subGrps = 0; subGrps < 2; ++subGrps) {
                    auto trSubGrp = dawInstance->createNewTrack(TRACK_TYPE_AUDIO);
                    trSubGrp->name = StringFormat("Sub Bus %d.%d", topGrps, subGrps);
                    trTop->addChild(trSubGrp);
                    dawInstance->addTrackImpl(-1, trSubGrp, 0);
                    for (int i = 0; i < 4; ++i) {
                        auto track1 = new track_t(TRACK_TYPE_MIDI, StringFormat("Track #%d.%d.%d", topGrps, subGrps, i), true);
                        trSubGrp->addChild(track1);
                        // deep copy (of clip_t instances)
                        track1->getMidi() = trDataMidi;
                        dawInstance->addTrackImpl(-1, track1, 0);
                        track1->getStage()->arp->setParamValue(PARAM_ENABLE, 1.0f, FLG_PAR_UPDATE_INIT);
                        track1->getStage()->arp->setParamValue(ARP_PARAM_CLOCK, 0.4f, FLG_PAR_UPDATE_INIT);
                        track1->getStage()->arp->setParamValue(ARP_PARAM_PATTERN, 1.0f, FLG_PAR_UPDATE_INIT);
                        track1->getStage()->arp->setParamValue(ARP_PARAM_RAND_VEL, 0.7f, FLG_PAR_UPDATE_INIT);
                        track1->getStage()->arp->setParamValue(ARP_PARAM_GATE, 0.55f, FLG_PAR_UPDATE_INIT);

                        auto pluginInstance = dawInstance->getHost()->makeModuleInstance(PLUGIN_TYPE_INTERNAL_EFFECT, PLUG_INT_HOSTINFO, -1);
                        dbgassert(pluginInstance);
                        host->insertNewPlugin(track1->getStage(), pluginInstance, 0);
                        pluginInstance->resume();
                        track1->getStage()->pluginsChanged();
                        //pluginHostInfo->setParamValue(PARAM_OFFSET_EXTERNAL+0, 1.0f, FLG_PAR_UPDATE_INIT);

                    }
                }
            }

            auto trackMaster = new track_t(TRACK_TYPE_MASTER, "master", true);
            dawInstance->addTrackImpl(0, trackMaster, 0);
        };

        std::array<TestContext, 9> allBenchmarks = {
            TestContext{"Process 0 Tracks (Empty)", false, dawInstance.get(), testCase0Tracks },
            TestContext{"Process 2 Tracks (Empty)", false, dawInstance.get(), testCase2Tracks },
            TestContext{"Process 32 Tracks (Empty)", false, dawInstance.get(), test32TracksEmpty },
            TestContext{"Process 32 Tracks (Empty, Arp==null)", false, dawInstance.get(), test32TracksEmptyNoArp },
            TestContext{"Process 32 Tracks (Midi Clip, Arp==null)", false, dawInstance.get(), test32TracksMidiNoArp },
            TestContext{"Process 32 Tracks (Midi Clip, Arp instance)", false, dawInstance.get(), test32TracksMidi },
            TestContext{"Process 32 Tracks (Midi Clip, Arp Active)", false, dawInstance.get(), test32TracksMidiAndArp },
            TestContext{"Process 32 Tracks (4 Busses x 2 Busses x 4 Tracks, Midi Clip, Arp Instance)", false, dawInstance.get(), testGroups },
            TestContext{"Process 32 Tracks (4 Busses x 2 Busses x 4 Tracks, Midi Clip, Arp Active)", false, dawInstance.get(), testGroups2 },
        };

        for (TestContext& benchmarkCtxt : allBenchmarks) {
            benchmark::RegisterBenchmark(benchmarkCtxt.benchmarkName, BenchMarkRun, &benchmarkCtxt);
        }

        benchmark::Initialize(&argc, argv);
        if (::benchmark::ReportUnrecognizedArguments(argc, argv))
            return 1;
        benchmark::RunSpecifiedBenchmarks();
        benchmark::Shutdown();
        trDataMidi.deleteClips(nullptr);
        host->setOutput(nullptr);
    } catch (std::exception& e) {
        log_printf("exception %s\n", e.what());
    } catch (...) {
        log_printf("unhandled exception\n", 0);
    }

    dawInstance->destroy();
    return 0;
}

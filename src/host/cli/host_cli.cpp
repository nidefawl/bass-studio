#include "str_util.h"
#include "seq_time.h"
#include "fileio.h"
#include "exceptions.h"
#include "platform.h"
#include "appsettings.h"
#include "file/projectfile.h"
#include "tls.h"
#include "snapshot/track-snapshot.h"
#include "host/project/project.h"
#include "host/project/projectcontroller.h"
#include "samplerate.h"
#include "host/audiobuffer/audioblock.h"
#include "host/audiocache/audiocache.h"
#include "host/audiobuffer/audiobuffer.h"
#include "host/track/track.h"
#include "host/track/track_impl.h"
#include "host/audiohost/audio_host.h"
#include "host/midihost/midi_host.h"
#include "host/plugin/vst/vstplugin.h"
#include "host/plugin/vst/vstplugin-handles.h"
#include "host/plugindatabase/plugindatabase.h"
#include "threads/playbackthread.h"
#include <dr_libs/dr_wav.h>
#include "types.h"
#include "util/testing_environment.h"
#include "appconfig.h"
#include "host/host.h"
#include "host/host_pluginmanager.h"

#ifdef _WIN32
#include "platform/win/platform_win.h"
#endif
#ifdef __linux__
#include <unistd.h>
#include <climits>
#endif

#include <cstdlib>
#include <memory>

void openGlobalLog(const String& logFileName); // Forward declare from util/logging.cpp
void closeGlobalLog();                         // Forward declare from util/logging.cpp

extern volatile bool fatalError;

namespace HostCLI {
    
bool userSentQuitRequest = false;

#ifdef _WIN32
static BOOL WINAPI ConsoleHandler(DWORD dwType) {
    userSentQuitRequest = true;
    return true;
}
#endif

static void on_terminate() {
    log_printf("on_terminate\n");
}

#ifdef _WIN32
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            break;
        case WM_CLOSE:
            userSentQuitRequest = true;
            break;
        case WM_DESTROY:
            userSentQuitRequest = true;
            PostQuitMessage(0);
            return 0;
        default:
            break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}
#endif


bool hasCmdOption(const std::vector<String>& args, const String& option) {
    for (size_t i = 0; i < args.size(); ++i) {
        const String& arg = args[i];
        if (0 == arg.find(option)) {
            std::size_t found = arg.find_last_of(option);
            if (found != String::npos) {
                return true;
            }
        }
    }
    return false;
}

String getCmdOption(const std::vector<String>& args, const String& option, String defaultVal) {
    for (size_t i = 0; i < args.size(); ++i) {
        const String& arg = args[i];
        if (0 == arg.find(option)) {
            std::size_t found = arg.find_last_of(option);
            if (found != String::npos) {
                if (found + 2 < arg.length() && arg[found + 1] == '=') {
                    return arg.substr(found + 2);
                }
                if (i + 1 < args.size()) {
                    return args[i + 1];
                }
                return defaultVal;
            }
        }
    }
    return defaultVal;
}

double StringToF(const String& s) { return atof(s.c_str()); }

#ifdef _WIN32
void processWindowMessages() {
    DWORD timeout = 50;
    MsgWaitForMultipleObjects(0, nullptr, FALSE, timeout, QS_ALLEVENTS);
    MSG msg;
    int maxProcess = 500;
    while (!fatalError && PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE) && maxProcess-- > 0) {
        if (msg.message == WM_QUIT) {
            break;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}
#else
void processWindowMessages() {}
#endif

int runCommandLineHost(const std::vector<String>& args) {
    seqthreads::registerThread("mainthread", seqthreads::ThreadType::MainThread);
#ifdef _WIN32
    if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE) ConsoleHandler, TRUE)) {
        fprintf(stderr, "Unable to install handler!\n");
        return EXIT_FAILURE;
    }
    WNDCLASS wc;

    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.lpszClassName = "Window";
    wc.hInstance     = GetModuleHandle(nullptr);
    wc.hbrBackground = GetSysColorBrush(COLOR_3DFACE);
    wc.lpszMenuName  = nullptr;
    wc.lpfnWndProc   = WndProc;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon         = LoadIcon(nullptr, IDI_APPLICATION);

    RegisterClass(&wc);
    if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE)ConsoleHandler, TRUE)) {
        fprintf(stderr, "Unable to install handler!\n");
        return EXIT_FAILURE;
    }
    setExceptionHandler();
#ifndef NDEBUG
    _dup2(1, 2); // workaround: redirect stderr to stdout so stderr is visible when using gdb on eclipse (bug)
#endif
#endif
    std::set_terminate(on_terminate);


    try {
        App::Platform::initPlatformEnvironment(BuildInfo::PRODUCT_NAME_LOWER);
        auto& tls = daw_tls::initNewTls();
        loadSettings(*tls.settings);
        auto& settings = *tls.settings;

        String file           = getCmdOption(args, "-f", "");
        String fOutWave       = getCmdOption(args, "-o", "");
        String strLogFilename = getCmdOption(args, "--logfile", "");
        bool bRenderOnly      = hasCmdOption(args, "--render");
        double fStart         = StringToF(getCmdOption(args, "-s", "-1.0"));
        double fLength        = StringToF(getCmdOption(args, "-l", "-1.0"));
        bool activateDeferred = getCmdOption(args, "-d", "true") == "true";


        if (strLogFilename.length()) {
            openGlobalLog(App::Platform::toUserdataPath(strLogFilename));
        }

        if (file.empty()) {
            log_printf("please specify project file with -f <file>\n");
            return EXIT_FAILURE;
        }
        if (bRenderOnly && fOutWave.empty()) {
            log_printf("--render requires -o <file>\n");
            return EXIT_FAILURE;
        }
        std::shared_ptr<project_file> projectFile;
        ProjectFileType projectFileType = ProjectFileType::PROJECT_FILETYPE_JSON;
        if (!file.empty()) {
            try {
                projectFile = loadProjectFromJsonFile(file);
            } catch (std::exception& e) {
                log_printf("exception %s\n", e.what());
                return EXIT_FAILURE;
            } catch (...) {
                log_printf("unhandled exception\n");
                return EXIT_FAILURE;
            }
            if (!projectFile) {
                fprintf(stderr, "Error: failed loading file\n");
                return EXIT_FAILURE;
            }
        }

        auto host = std::make_unique<DAW::Host::Host>();
        auto pluginMgr = host.get();
        DAW::Host::PluginManager::assignMasterCallback(pluginMgr);

        project_t project;
        project_globals_t projectGlobals;
        project_controller_t projectController{&project, &projectGlobals};
        plugindatabase_t plugindb;
        audiocache cache(settings.iosettings.samplerate);
        tls.mainCtrl              = nullptr;
        tls.project               = &projectController;
        tls.host                  = host.get();
        tls.pluginManager         = pluginMgr;
        std::unique_ptr<audiohost> audioHost;
        std::unique_ptr<midihost> midiHost;
        if (!bRenderOnly) {
            audioHost = std::make_unique<audiohost>();
            midiHost  = std::make_unique<midihost>();
        } else {
            host->multithreadedProcessing = 0;
        }
        tls.audioHost      = audioHost.get();
        tls.midiHost       = midiHost.get();
        tls.audioCache     = &cache;
        tls.pluginDatabase = &plugindb;
        host->setTls(tls);
        host->setSampleFormat(sampleformat_t{static_cast<samplerate_t>(settings.iosettings.samplerate),
                                             settings.iosettings.blocksize, sampleformat_bits_t::FLOAT_32});

        dbgassert(host->m_sampleFormatInternal.sampleRate != 0);
        dbgassert(host->m_sampleFormatInternal.blockSize != 0);

        if (!bRenderOnly) {
            audioHost->initPa();
            midiHost->initPm();
            if (audioHost->startAudio(settings.iosettings)) {
                host->setOutput(audioHost->getStreamSharedPtr(0));
            } else {
                log_printf("audioHost->startAudio() failed\n");
                return 1;
            }
            midiHost->startMidi();
        }
        host->cacheAudioGraph = true;

        plugindb.openDatabase();
        host->initThreads();

#ifdef _WIN32
        HWND hwnd = CreateWindow(wc.lpszClassName, "Window", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100, 350, 250, nullptr,
                                 nullptr, wc.hInstance, nullptr);
        dbgassert(hwnd != nullptr);
        setMainHWND(hwnd);
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
#endif


        log_printf("START\n");
        {
            std::unique_ptr<PlaybackThread> playThread;
            if (!bRenderOnly) {
                playThread = std::make_unique<PlaybackThread>();
                playThread->setTls(tls);
                playThread->startThread(&projectController);
                playThread->addRequest(REQ_STATE, (int)playback_state::status_no_process, true);
            }
            if (projectFile) {
                project_snapshot_t& snapshot = projectFile->project;
                project.copyFrom(snapshot);
                projectGlobals = snapshot.globals;

                String projectDirectory;
                SplitPath(projectFile->path, &projectDirectory, nullptr, nullptr, nullptr);
                cache.load(projectFile->sampleFileIndex, projectFileType, projectFile->path, projectDirectory);

                /** create all audio instances **/
                for (track_t* t : projectController.getTracks()) {
                    t->updateAudioClipLengths(projectGlobals.tempo100, projectFile->project.samplerate, host->m_sampleFormatInternal.sampleRate);
                    host->createAudio(t);
                }

                /** pre-load all plugin instances **/
                project.trackList.loadProjectSnapshot(host.get(), snapshot);

                /** reset maximum stage id and determine new maximum stage id **/
                host->updateMaximumStageId();

                /** remove routings to missing track **/
                DAW::validateTrackRoutings(host.get(), project.getTracksFlatVec());

                /** inform host about track layout changes so it resets and updates internal structures **/
                host->onTrackLayoutChange();


                /** fully load all plugin instances **/
                if (activateDeferred) {
                    std::vector<effectbase*> pluginsDeferred;
                    host->getDeferredEffects(pluginsDeferred);
                    log_printf("loading %zu plugins\n", pluginsDeferred.size());
                    for (auto plugin : pluginsDeferred) {
                        log_printf("activate %s\n", StringAsCStr(plugin->sName));

                        host->activateDeferred(plugin, DAW::Host::PluginManager::FLAG_HOST_UNLOAD_PLUGIN_NO_NOTIFY);
                        //                        if (effectLoaded) {
                        //                            effectLoaded->show();
                        //                        }
                    }
                }

            } else {
                projectGlobals.cursor.cursorPos = 0;
                projectGlobals.loopEnabled      = false;

                track_t* track1 = new track_t(TRACK_TYPE_MIDI, "track1", true);
                projectController.addTrackImpl(-1, track1, 0);

                track_t* track2 = new track_t(TRACK_TYPE_MIDI, "track2", true);
                projectController.addTrackImpl(-1, track2, 0);

                track_t* trackMaster = new track_t(TRACK_TYPE_MASTER, "master", true);
                projectController.addTrackImpl(0, trackMaster, 0);

                String pathTracks = "test.tracks";

                std::shared_ptr<trackcontainer_snapshot_t> ctr = loadTrackContainer(pathTracks);
                std::vector<track_t*> loadedChildTracks;

                dbgassert(ctr);
                if (ctr) {
                    for (track_snapshot_t& ts : ctr->tracks) {
                        track_t* tr = new track_t(ts);
                        loadedChildTracks.push_back(tr);
                        track2->addChild(tr);
                        projectController.addTrackImpl(-1, tr, 0);
                        ts.trackLoaded = tr;
                        log_printf("add track %s\n", StringAsCStr(tr->name));
                    }

                    // load plugins
                    for (track_snapshot_t& ts : ctr->tracks) {
                        log_printf("track '%s' loading %zu plugins\n", StringAsCStr(ts.trackLoaded->name), ts.data.pluginSnapshots.size());
                        ts.trackLoaded->loadSnapshot(host.get(), ts);
                    }
                    if (activateDeferred) {
                        // activate plugins
                        for (track_snapshot_t& ts : ctr->tracks) {
                            std::vector<effectbase*> effects = ts.trackLoaded->audio->deferredEffects;
                            for (auto eff : effects) {
                                log_printf("activate plugin %s\n", StringAsCStr(eff->getName()));
                                host->activateDeferred(eff, DAW::Host::PluginManager::FLAG_HOST_UNLOAD_PLUGIN_NO_NOTIFY);
                            }
                        }
                    }
                    host->onTrackLayoutChange();
                }

                /** reset maximum stage id and determine new maximum stage id **/
                host->updateMaximumStageId();

                /** remove routings to missing track **/
                DAW::validateTrackRoutings(host.get(), project.getTracksFlatVec());

                /** inform host about track layout changes so it resets and updates internal structures **/
                host->onTrackLayoutChange();

                auto treePos = track_tree_pos_t{TRACK_CTR_MIDIAUDIO, nullptr, 0};
                project.trackList.moveTracks(loadedChildTracks, treePos);
            }
            plugindb.closeDatabase();
            
            if (!fOutWave.empty()) {
                for (auto* trackMaster : project.trackMasterCtr) {
                    trackMaster->getStage()->flags |= audiostageflags_t::RECORD_OUTPUT;
                    trackMaster->getStage()->flags |= audiostageflags_t::CONVERT_OUTPUT;
                }
            }

            /** inform host about track layout changes so it resets and updates internal structures */
            host->onTrackLayoutChange();

            auto tLastMsg = getTimeSecondsD();
            int64_t samplesWritten = 0;

            projectGlobals.cursor.cursorPos = projectGlobals.loopStart;
            if (fStart >= 0.0 && fLength >= 0.0) {
                projectGlobals.loopEnabled = false;
                //                project.loopLen = math::roundD(fLength*TICKS_BAR);
            } else if (projectGlobals.loopEnabled) {
                fStart = projectGlobals.loopStart / float(TICKS_BAR);
                fLength = projectGlobals.loopLen / float(TICKS_BAR);
            }
            if (fStart >= 0.0) {
                projectGlobals.cursor.cursorPos = math::rounddS32(fStart * TICKS_BAR);
                projectGlobals.loopStart        = math::rounddS32(fStart * TICKS_BAR);
            }

            host->prjGlobals = projectGlobals;

            log_printf("host->sampleFormat.sampleRate: %u\n", host->m_sampleFormatInternal.sampleRate);
            log_printf("host->sampleFormat.blockSize: %u\n", host->m_sampleFormatInternal.blockSize);

            log_printf("projectController.getCursorPos: %d\n", projectController.getCursorPos());
            log_printf("projectController.getCurrentTempo: %d\n", projectController.getCurrentTempo());
            log_printf("projectController.getCursorPos: %d\n", projectController.getCursorPos());
            log_printf("projectController.loopEnabled: %d\n", projectController.getGlobals().loopEnabled);
            log_printf("projectController.loopStart: %d\n", projectController.getGlobals().loopStart);
            log_printf("projectController.loopLen: %d\n", projectController.getGlobals().loopLen);

            double tickPos    = projectGlobals.cursor.cursorPos;
            log_printf("playback start at %s\n", StringAsCStr(tickAsBeatString(tickPos, false)));

            const double ticksPerBlock = sampleToTickConvert<double, roundmode::none>(host->m_sampleFormatInternal.blockSize,
                                                                                      host->prjGlobals.tempo100,
                                                                                      host->m_sampleFormatInternal.sampleRate);

            int32_t samplePos = tickToSampleConvert<int32_t, roundmode::floor>(projectGlobals.cursor.cursorPos,
                                                                               projectGlobals.tempo100,
                                                                               host->m_sampleFormatInternal.sampleRate);
            if (!bRenderOnly) {
                playThread->addRequest(REQ_STATE, (int)playback_state::status_playback, true);
            } else {
                /**
                 * Validate audio routing by building the audio graph once
                 */
                std::shared_ptr<DAW::processing_graph_t> processingGraph;
                if (!DAW::buildProcessingGraph(host.get(), &project, project.trackList.getAllTracksFlatVecRef(), processingGraph)) {
                    log_lf(Log::L_ERROR, "Failed building track graph\n");
                    return -1;
                }
                log_printf("START ON seconds: %.2f - sample %d\n", toSeconds(tickPos, host->prjGlobals.tempo100), samplePos);
                host->onStartPlayback(&projectController);
            }

            while (!userSentQuitRequest) {
                auto tNow = getTimeSecondsD();
                if (tNow - tLastMsg >= 1.0) {
                    tLastMsg = tNow;
                    // require locking here
                    host_stats_t stats;
                    host->getStats(stats);

                    String strProgress = "x";
                    if (fStart >= 0.0 && fLength >= 0.0) {
                        auto fProgress = (projectGlobals.playbackPos / (double)TICKS_BAR - fStart) / fLength;
                        strProgress     = StringFormat("%0.2f%%", fProgress * 100.0);
                    }
                    log_printf("PROCESS[render=%d,sr=%0.1fk,bs=%d] %s playbackPos %d/%.0f, %d blocks, %d samples\n",
                               bRenderOnly, host->m_sampleFormatInternal.sampleRate / 1000.0f, host->m_sampleFormatInternal.blockSize,
                               StringAsCStr(strProgress), projectGlobals.playbackPos, (fStart + fLength) * TICKS_BAR,
                               stats.blocksProcessed, stats.samplesProcessed);
                }

                /*
                #define MAX_GAIN 0.0f
                auto since = tNow - tLastMsg;
                {
                    ThreadLock lock = playThread->lockThread();
                    float gain = math::clamp<float>(since*MAX_GAIN, MAX_GAIN, 0.0f);
                    for (auto* trackMaster : project.trackMasterCtr) {
                        trackMaster->audio->mixer.setParamValue(PARAM_TRACK_GAIN, math::min(gain, MAX_GAIN),
                        FLG_PAR_UPDATE_AUTOMATED);
                    }
                }*/
                if (bRenderOnly) {
                    int32_t processedBlock = host->processRender(tls.project, samplePos, tickPos);
                    dbgassert(processedBlock > 0);

                    samplePos += host->m_sampleFormatInternal.blockSize * processedBlock;
                    tickPos += ticksPerBlock * processedBlock;
                    projectController.getPlaybackPos() = (tick_t) floor(tickPos);
                }
                if (fStart >= 0.0 && fLength >= 0.0) {
                    if ((projectController.getPlaybackPos()) / (double)TICKS_BAR - fStart >= fLength) {
                        if (playThread) {
                            playThread->addRequest(REQ_STATE, (int)playback_state::status_no_process, true);
                        }

                        userSentQuitRequest = true;
                    }
                }

                processWindowMessages();

                if (daw_test::runTest(daw_test::TestCases::TEST_HOST_EXCEPTIONS)) {
                    seqthreads::threadSleep(20);
                    static int nTestLoops = 100;
                    if (nTestLoops-- == 0) {
                        log_printf("Invoking test code\n");
                        logStackTrace();
                        throw applogicexception("TEST_HOST_EXCEPTIONS: Testing exception handling");
                    }
                }
            }

            log_printf("playback end..\n");

            if (playThread) {
                playThread->addRequest(REQ_STATE, (int)playback_state::status_no_process, true);
            }

            int trackIndex      = 0;
            AudioBlock blockTrack;
            for (auto* trackMaster : project.trackMasterCtr) {
                auto* trImpl = trackMaster->getStage();
                if (isSet(trImpl->flags, audiostageflags_t::CONVERT_OUTPUT | audiostageflags_t::RECORD_OUTPUT)) {
                    trImpl->audioOutput.convertToSamples(tls.host);
                    auto sampleBegin = tickToSampleConvert<samplecount_t, roundmode::floor>(fStart * TICKS_BAR, host->prjGlobals.tempo100,
                                                                         host->m_sampleFormatInternal.sampleRate);
                    auto sampleLen = tickToSampleConvert<samplecount_t, roundmode::ceil>(fLength * TICKS_BAR,
                                                                            host->prjGlobals.tempo100,
                                                                            host->m_sampleFormatInternal.sampleRate);
                    std::vector<audiotrack_split_t*> samples;
                    trImpl->audioOutput.visitSamples_NoLock([&samples,sampleBegin,sampleLen](std::shared_ptr<audiotrack_split_t>& split) {
                        auto* ptrSplit = split.get();
                        if (ptrSplit && !(ptrSplit->samplePos >= sampleBegin + sampleLen || ptrSplit->samplePos + ptrSplit->getSample()->nSamples <= sampleBegin)) {
                            samples.push_back(ptrSplit);
                        }
                    });
                    if (!samples.empty()) {
                        std::sort(samples.begin(), samples.end(), [](audiotrack_split_t* lhs, audiotrack_split_t* rhs) {
                            return lhs->samplePos < rhs->samplePos;
                        });
                        samplecount_t splitSize = samples.front()->sample.nSamples;
                        auto totalLen = samplecount_t(samples.size()*splitSize);

                        drwav_data_format format;
                        format.container = drwav_container_riff; // <-- drwav_container_riff = normal WAV files,
                                                                 // drwav_container_w64 = Sony Wave64.
                        format.format        = DR_WAVE_FORMAT_IEEE_FLOAT; // <-- Any of the DR_WAVE_FORMAT_* codes.
                        format.channels      = trImpl->output.channels;
                        format.sampleRate    = trImpl->sampleFormat.sampleRate;
                        format.bitsPerSample = 32;
                        if (!fOutWave.empty()) {
                            String nameWaveFileTrack = fOutWave + "_" + std::to_string(trackIndex) + "_" + trackMaster->name + "_f32.wav";
                            drwav wav;
                            if (!drwav_init_file_write_sequential_pcm_frames(&wav, StringAsCStr(nameWaveFileTrack), &format, totalLen, nullptr)) {
                                log_lf(Log::L_WARN, "drwav_init_file_write_sequential_pcm_frames failed\n");
                                return 0;
                            }
                            struct close_wave_file_write {
                                drwav* wav;
                                ~close_wave_file_write() { drwav_uninit(wav); }
                            } closeWaveFile{&wav};
                            for (audiotrack_split_t* split : samples) {
                                auto* sample = split->getSample();
                                dbgassert(sample->nChannels == trImpl->output.channels);
                                dbgassert(sample->nChannels == sample->samples.size());
                                dbgassert(sample->nSamples == static_cast<int64_t>(sample->samples[0].size()));
                                dbgassert(sample->nSamples == static_cast<int64_t>(sample->samples[1].size()));
                                if (blockTrack.samples < sample->nSamples * sample->nChannels) {
                                    blockTrack = AudioBlock(1, sample->nSamples * sample->nChannels);
                                }
                                auto beginOffset = math::max<samplecount_t>(0, sampleBegin - split->samplePos);
                                auto readLen     = math::min<samplecount_t>(sample->nSamples, (sampleBegin + sampleLen) - split->samplePos) - beginOffset;
                                float* largeBuf = blockTrack.buf[0];
                                for (int64_t nSample = 0; nSample < readLen; ++nSample) {
                                    for (channelnum_t ch = 0; ch < sample->nChannels; ++ch) {
                                        *largeBuf++ = sample->samples[ch][beginOffset + nSample];
                                    }
                                }
                                samplesWritten += samplecount_t(drwav_write_pcm_frames(&wav, readLen, blockTrack.buf[0]));
                            }
                            log_printf("wrote %zd samples to %s\n", samplesWritten, StringAsCStr(nameWaveFileTrack));
                        }
                    }
                }
                trackIndex++;
            }


            std::vector<track_t*> _tracks = project.trackList.getAllTracksFlatVec();
            for (track_t* tr : _tracks) {
                host->unloadTrack(tr);
                project.trackList.removeTrack(tr);
            }
            project.trackList.clear();

            for (track_t* tr : _tracks) {
                releaseTrackResources(tr, nullptr);
                delete tr;
            }
            if (playThread) {
                playThread->stopThread();
                playThread->joinThread();
            }
        }
        if (!bRenderOnly) {
            host->setOutput(nullptr);
            midiHost->stopMidi();
            audioHost->stopAudio();
            midiHost->deinitPm();
            audioHost->deinitPa();
        }
        host->destroy();
        log_printf("END\n");
    } catch (std::exception& e) {
        log_printf("exception %s\n", e.what());
        return 1;
    } catch (...) {
        log_printf("unhandled exception\n");
        return 1;
    }
    closeGlobalLog();
    return 0;
}

}
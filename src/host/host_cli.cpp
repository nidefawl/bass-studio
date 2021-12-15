#include "str_util.h"
#include "../vstsdk-host-2.4/aeffectx.h"
#include "../host/vst_host.h"
#include "../host/plugin/vst_plugin.h"
#include "../host/plugin/vst_plugin_handles.h"
#include "fileio.h"
#include "exceptions.h"
#include "../threads/childprocessthread.h"
#include "platform.h"
#include <SQLiteCpp/SQLiteCpp.h>
#include <SQLiteCpp/VariadicBind.h>
#include <iostream>
#include <memory>
#include "ipc.h"
#include "appsettings.h"
#include "projectfile.h"
#include "project.h"
#include "samplerate.h"
#include "track.h"
#include "threads/playbackthread.h"
#include "audioblock.h"
#include "audiocache.h"
#include "audiobuffer.h"
#include "projectcontroller.h"
#include "plugindatabase.h"
#include "fileio.h"
#include "wave/dr_wav.h"
#include "basectrl.h"
#include "audio_host.h"
#include "midi_host.h"
#include "track.h"
#include "track_impl.h"
#include "track_snapshot.h"

#ifdef _WIN32
#include "../platform/win/platform_win.h"
#include <windows.h>
#endif
#ifdef __linux__
#include <unistd.h>
#include <limits.h>
#endif

#include <stdlib.h>


struct vst_metadata {
	uint32_t id;
	uint32_t version;
	uint32_t vstVersion;
	uint32_t pluginCategory;
	bool isSynth;
	char szPath[1024];
	char szName[256];
	char szVendorName[256];
};

void getPluginData(vstplugin* plugin, vst_metadata* _out) {
	AEffect* aeffect = plugin->handle->aeffect;
	_out->id = aeffect->uniqueID;
	_out->version = aeffect->version;
	_out->vstVersion = plugin->vstVersion;
	_out->pluginCategory = plugin->pluginCategory;
	strncpy(_out->szName, StringAsCStr(plugin->sName), plugin->sName.length());
	if (!plugin->dispatch(effGetVendorString, 0, 0, (void*)_out->szVendorName)) {
		_out->szVendorName[0] = 0;
	}
	_out->isSynth = plugin->isSynth;
}
bool userSentQuitRequest = false;
#ifdef _WIN32
static BOOL WINAPI ConsoleHandler(DWORD dwType)
{
    switch(dwType) {
    case CTRL_C_EVENT:
		log_printf("CTRL_C\n", 0);
    	userSentQuitRequest = true;
        break;
    }
    return TRUE;
}
#endif
void releaseTrackResources(track_t* tr, delete_cb *cb);
static void on_terminate1() {
	log_printf("on_terminate\n", 0);
//	exit(1); // required on mingw (at least)
}
static  void on_unexpected1() {
	log_printf("on_unexpected\n", 0);
	logStackTrace();
//	exit(1); // required on mingw (at least)
}
#ifdef _WIN32
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch(msg)
    {
	case WM_CREATE:
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

    return DefWindowProc(hwnd, msg, wParam, lParam);
}
#endif


extern volatile bool fataError;
bool hasCmdOption(int argc, const char* argv[], const String& option)
{
	for (int i = 0; i < argc; ++i) {
		String arg = argv[i];
		if (0 == arg.find(option)) {
			std::size_t found = arg.find_last_of(option);
			if (found != String::npos) {
				return true;
			}
		}
	}
    return false;
}
String getCmdOption(int argc, const char* argv[], const String& option, String defaultVal)
{
     for( int i = 0; i < argc; ++i)
     {
    	 String arg = argv[i];
          if(0 == arg.find(option))
          {
               std::size_t found = arg.find_last_of(option);
               if (found != String::npos)
               {
            	   if (found+2 < arg.length() && arg[found+1] == '=') {
                       return arg.substr(found + 2);
            	   } else if (i + 1 < argc) {
            		   return argv[i+1];
            	   } else {
            		   return defaultVal;
            	   }
               }
          }
     }
     return defaultVal;
}
float StringToFloat(String s) {
	return atof(s.c_str());
}
#ifdef _WIN32
void processWindowMessages() {
				DWORD timeout = 5;
				MsgWaitForMultipleObjects(0, NULL, FALSE, timeout, QS_ALLEVENTS);
			    MSG msg;
			    int maxProcess = 500;
			    while (!fataError && PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE) && maxProcess-- > 0)
			    {
		//	    	logEveryMsec(0, 5000, "Main msg loop");
			        if (msg.message == WM_QUIT)
			        {
//			        	glfwSetWindowShouldClose(glfwHandle, 1);
			        }
			        else
			        {

//			            switch (msg.message) {
//		#if BUILD_VSTHOST
//			            	case WM_KEYDOWN:
//							case WM_SYSKEYDOWN:
//							case WM_KEYUP:
//							case WM_SYSKEYUP: {
//								if (vst_window_mgr::isVstWindow(msg.hwnd)) {
//									msg.hwnd = hwnd;
//								}
//							}
//		#endif
//							//no break
//							default:
								TranslateMessage(&msg);
					            DispatchMessageW(&msg);
//								break;
//			            }


			        }
			    }
//    			threadSleep(10);
}
#else
void processWindowMessages() {

}
#endif
int runCommandLineHost(int argc, const char* argv[]) {
#ifdef _WIN32
    WNDCLASS wc;

    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.lpszClassName = "Window";
    wc.hInstance     = GetModuleHandle(NULL);
    wc.hbrBackground = GetSysColorBrush(COLOR_3DFACE);
    wc.lpszMenuName  = NULL;
    wc.lpfnWndProc   = WndProc;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);

    RegisterClass(&wc);
    if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE)ConsoleHandler,TRUE)) {
        fprintf(stderr, "Unable to install handler!\n");
        return EXIT_FAILURE;
    }
	setExceptionHandler();
#ifndef NDEBUG
    _dup2( 1, 2 ); //workaround: redirect stderr to stdout so stderr is visible when using gdb on eclipse (bug)
#endif
#endif
	std::set_terminate(on_terminate1);
	std::set_unexpected(on_unexpected1);

    String cwdPath = "";
    if (determineUserdataPath(cwdPath)) {
        setUserdataPath(cwdPath+"\\daw\\");
    }
    appsettings settings = loadSettings();
	String file = getCmdOption(argc, argv, "-f", "");
	String fOutWave = getCmdOption(argc, argv, "-o", "");
	bool bRenderOnly = hasCmdOption(argc, argv, "--render");
	float fStart = StringToFloat(getCmdOption(argc, argv, "-s", "-1.0"));
	float fLength = StringToFloat(getCmdOption(argc, argv, "-l", "-1.0"));
	bool activateDeferred = !(getCmdOption(argc, argv, "-d", "false")!="false");
	
	if (file.empty()) {
		log_printf("please specify project file with -f <file>\n", 0);
		return 1;
	}
	if (bRenderOnly && fOutWave.empty()) {
		log_printf("--render requires -o <file>\n", 0);
		return 1;
	}

	int64_t time;
    try {
    	FileTimeGetter filetime(file);
    	time = filetime.getWriteTimeI64();

    	auto host = std::make_unique<vsthost>();
    	vsthost::assignMasterCallback(host.get());
		host->setSampleFormat(sampleformat_t{static_cast<samplerate_t>(settings.iosettings.samplerate), settings.iosettings.blocksize, sampleformat_bits_t::FLOAT_32});
		
		dbgassert(host->sampleFormat.sampleRate != 0);
		dbgassert(host->sampleFormat.blockSize != 0);

		project_t project;
		project_globals_t projectGlobals;
    	project_controller_t projectController{&project, &projectGlobals};
    	plugindatabase_t plugindb;
    	audiocache cache(settings.iosettings.samplerate);
    	daw_tls::tlsinstance& tls = daw_tls::getTls();
    	tls.mainCtrl = nullptr;
    	tls.project = &projectController;
    	tls.host = host.get();
    	std::unique_ptr<audiohost> audioHost;
    	std::unique_ptr<midihost> midiHost;
		if (!bRenderOnly) {
			audioHost = std::make_unique<audiohost>();
			midiHost = std::make_unique<midihost>();
		}
    	tls.audioHost = audioHost.get();
    	tls.midiHost = midiHost.get();
    	tls.audioCache = &cache;
    	tls.waveform = nullptr;
    	tls.pluginDatabase = &plugindb;

		if (!bRenderOnly) {
			tls.audioHost->initPa();
			tls.midiHost->initPm();
			if (tls.audioHost->startAudio(settings.iosettings)) {
				host->setOutput(tls.audioHost);
			} else {
				log_printf("audioHost->startAudio() failed\n", 0);
				return 1;
			}
			midihost::getInstance()->startMidi();
		}

    	plugindb.openDatabase();
    	vsthost::getInstance()->initThreads();

#ifdef _WIN32
        HWND hwnd = CreateWindow(wc.lpszClassName, "Window",
                    WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                    100, 100, 350, 250, NULL, NULL, wc.hInstance, NULL);
        dbgassert(hwnd != NULL);
        setMainHWND(hwnd);
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);

//        while  (GetMessage(&msg, NULL, 0, 0))
//        {
//            DispatchMessage(&msg);
//        }
#endif


    	log_printf("START\n", 0);
    	{
    		std::unique_ptr<PlaybackThread> playThread;
    		if (!bRenderOnly) {
        		playThread = std::make_unique<PlaybackThread>();
    			playThread->setTls(tls);
    			playThread->startThread(&projectController);
    			playThread->addRequest(REQ_STATE, (int) playback_state::status_no_process, true);
    		}
			if (!file.empty()) {
	    		auto pf = loadProjectFile(file);
	    		if (!pf) {
	    			fprintf(stderr, "Error: failed loading file\n");
	    			return EXIT_FAILURE;
	    		}
				project_snapshot_t& snapshot = pf->project;
				project.copyFrom(snapshot);
				audiocache::getInstance()->load(pf->sampleFileIndex);

				/** create all audio instances **/
				for (track_t* t : projectController.getTracks()) {
					t->fixClipLengths();
					host->createAudio(t);
				}

				/** pre-load all plugin instances **/
				project.trackList.loadPlugins(snapshot);

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
            		log_printf("loading %d plugins\n", pluginsDeferred.size());
            		for (auto plugin : pluginsDeferred) {
                		log_printf("activate %s\n", StringAsCStr(plugin->sName));

            			host->activateDeferred(plugin, vsthost::FLAG_HOST_UNLOAD_PLUGIN_NO_NOTIFY);
//            			if (effectLoaded) {
//            				effectLoaded->show();
//            			}
            		}
        		}

			} else {
	    		projectGlobals.cursor.cursorPos = 0;
	    		projectGlobals.loopEnabled = false;
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

	        		//load plugins
	        		for (track_snapshot_t& ts : ctr->tracks) {
	            		log_printf("track '%s' loading %d plugins\n", StringAsCStr(ts.trackLoaded->name), ts.plugins.pluginSnapshots.size());
	        			ts.trackLoaded->loadSnapshot(ts);
	        		}
            		if (activateDeferred) {
    	    			vsthost* host = vsthost::getInstance();
    	        		//load plugins
    	        		for (track_snapshot_t& ts : ctr->tracks) {
    	            		log_printf("track '%s' loading %d plugins\n", StringAsCStr(ts.trackLoaded->name), ts.plugins.pluginSnapshots.size());
    	        			ts.trackLoaded->loadSnapshot(ts);

    		    			std::vector<effectbase*> effects = ts.trackLoaded->audio->deferredEffects;
    		    			for (auto eff : effects) {
    	            			host->activateDeferred(eff, vsthost::FLAG_HOST_UNLOAD_PLUGIN_NO_NOTIFY);
    		    			}
    	        		}
            		}

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
			if (!fOutWave.empty()) {
				for (auto* trackMaster : project.trackMasterCtr) {
					trackMaster->getStage()->flags |= audiostageflags_t::WRITE_OUTPUT;
					trackMaster->getStage()->flags |= audiostageflags_t::CONVERT_OUTPUT;
	    		}
			}
			for (auto* trackMaster : project.trackMasterCtr) {
				trackMaster->audio->mixer.setParamValue(PARAM_TRACK_GAIN, 0.8f, FLG_PAR_UPDATE_INIT);
			}
			/** inform host about track layout changes so it resets and updates internal structures */
			host->onTrackLayoutChange();


    		AudioBlock block(2, host->sampleFormat.blockSize);
    		AudioBlock blockFull(1, host->sampleFormat.blockSize*2);
    		double tLastMsg = getTimeMillis()/1000.0;
    		int64_t nBlocks = 0;
    		int64_t samplesWritten = 0;



			projectGlobals.cursor.cursorPos = projectGlobals.loopStart;
			if (fStart >= 0.0f) {
				projectGlobals.cursor.cursorPos = math::round(fStart*TICKS_BAR);
				projectGlobals.loopStart = math::round(fStart*TICKS_BAR);
			}
			if (fStart >= 0.0f && fLength >= 0.0f) {
				projectGlobals.loopEnabled = false;
//				project.loopLen = math::round(fLength*TICKS_BAR);
			}

    		std::shared_ptr<DAW::processing_graph_t> processingGraph;
			AudioBlock blockIn(host->numChannels, host->sampleFormat.blockSize);
			AudioBlock blockOut(host->numChannels, host->sampleFormat.blockSize);
			host->prjGlobals = projectGlobals;

			
			
    		log_printf("host->sampleFormat.sampleRate: %u\n", host->sampleFormat.sampleRate);
    		log_printf("host->sampleFormat.blockSize: %u\n", host->sampleFormat.blockSize);

    		log_printf("projectController.getCursorPos: %d\n", projectController.getCursorPos());
    		log_printf("projectController.getCurrentTempo: %d\n", projectController.getCurrentTempo());
    		log_printf("projectController.getCursorPos: %d\n", projectController.getCursorPos());
    		log_printf("projectController.loopEnabled: %d\n", projectController.getGlobals().loopEnabled);
    		log_printf("projectController.loopStart: %d\n", projectController.getGlobals().loopStart);
    		log_printf("projectController.loopLen: %d\n", projectController.getGlobals().loopLen);

			log_printf("playback start...\n", 0);

			
			
			const double blocksPerS = host->sampleFormat.sampleRate / (double) host->sampleFormat.blockSize;
			const double msPerBlock = 1000.0 / blocksPerS;
			const double ticksPerBlock = toTickPrecise(host->sampleFormat.blockSize/(double)host->sampleFormat.sampleRate, host->prjGlobals.tempo100);

			double tickPos = projectGlobals.cursor.cursorPos;
			int32_t samplePos = tickToSample(tickPos, projectGlobals.tempo100, host->sampleFormat.sampleRate);
			
			bool firstBlock = false;
			bool isLoopAround = false;


			if (!bRenderOnly) {
				playThread->addRequest(REQ_STATE, (int) playback_state::status_playback, true);
			} else {
	    		/*
	    		 * Process audio/midi tracks
	    		 */
	    		auto tracksFlatAll = project.trackList.getAllTracksFlatVec();
	    		/**
	    		 * process in reverse order: first children, then parents
	    		 */

	    		/** turn tree structure into linear pointer array with parents followed by their children **/
	    		if (!DAW::buildProcessingGraph(host.get(), &project, tracksFlatAll, processingGraph)) {
	    			log_printf("Failed building track graph\n", 0);
	    			return -1;
	    		}
				log_printf("START ON seconds: %.2f - sample %d\n", toSeconds(tickPos, host->prjGlobals.tempo100), samplePos);
				host->onStartPlayback(&projectController);
			}

    		while (!userSentQuitRequest) {
				auto tNow = getTimeMillis()/1000.0;
				if (tNow - tLastMsg >= 1.0) {
					tLastMsg = tNow;
					//require locking here
					host_stats_t stats;
					host->getStats(stats);

					String strProgress = "x";
					if (fStart >= 0.0f && fLength >= 0.0f) {
						float fProgress = ((projectGlobals.playbackPos)/(float)TICKS_BAR - fStart)/fLength;
						strProgress = StringFormat("%0.2f%%", fProgress*100.0f);
					}
					log_printf("PROCESS[render=%d,sr=%0.1fk,bs=%d] %s playbackPos %d/%.0f, %d blocks, %d samples\n", 
							bRenderOnly, host->sampleFormat.sampleRate/1000.0f, host->sampleFormat.blockSize,
							StringAsCStr(strProgress),
							projectGlobals.playbackPos, (fStart+fLength)*TICKS_BAR, stats.blocksProcessed, stats.samplesProcessed);


				}

    				//#define MAX_GAIN 0.0f
    //				auto since = tNow - tLastMsg;
    //				{
    //					ThreadLock lock = playThread->lockThread();
    //					float gain = math::clamp<float>(since*MAX_GAIN, MAX_GAIN, 0.0f);
    //					for (auto* trackMaster : project.trackMasterCtr) {
    //						trackMaster->audio->mixer.setParamValue(PARAM_TRACK_GAIN, math::min(gain, MAX_GAIN), FLG_PAR_UPDATE_AUTOMATED);
    //					}
    //				}
    			if (bRenderOnly) {

    	            int32_t processedBlock = host->processRender(tls.project, samplePos, tickPos);
	            	dbgassert(processedBlock > 0);

					samplePos += host->sampleFormat.blockSize*processedBlock;
					tickPos += ticksPerBlock*processedBlock;
					projectController.getPlaybackPos() = tickPos;
    			}
				if (fStart >= 0.0f && fLength >= 0.0f) {
					if ((projectController.getPlaybackPos())/(float)TICKS_BAR - fStart >= fLength) {
						if (playThread) {
							playThread->addRequest(REQ_STATE, (int) playback_state::status_no_process, true);
						}

						userSentQuitRequest = true;
					}
				}

				processWindowMessages();
    		}
			log_printf("playback end..\n", 0);
			if (playThread) {
				playThread->addRequest(REQ_STATE, (int) playback_state::status_no_process, true);
			}




    		int64_t bytesCopied = 0;
    		int trackIndex = 0;
			for (auto* trackMaster : project.trackMasterCtr) {
				auto* trImpl = trackMaster->getStage();
				if (isSet(trImpl->flags, audiostageflags_t::CONVERT_OUTPUT | audiostageflags_t::WRITE_OUTPUT)) {
					bytesCopied += trImpl->audioOutput.convertToSamples(tls.host);
					std::vector<audiotrack_split_t*> samples;
					trImpl->audioOutput.visitSamples_NoLock([&samples](std::shared_ptr<audiotrack_split_t>& split) {
						auto* ptrSplit = split.get();
						if (ptrSplit) {
							samples.push_back(ptrSplit);
						}
					});
					if (!samples.empty()) {
						static constexpr int32_t PER_BLOCK_BYTES = (1024*512);
						static constexpr int32_t PER_BLOCK_SAMPLES = (PER_BLOCK_BYTES/(sizeof(float)));
			    		AudioBlock blockFull(1, PER_BLOCK_SAMPLES*trImpl->output.channels);
						std::sort(samples.begin(), samples.end(), [](audiotrack_split_t* lhs, audiotrack_split_t* rhs) {
							return lhs->samplePos < rhs->samplePos;
						});

						drwav_data_format format;
						format.container = drwav_container_riff; // <-- drwav_container_riff = normal WAV files, drwav_container_w64 = Sony Wave64.
						format.format = DR_WAVE_FORMAT_IEEE_FLOAT;          // <-- Any of the DR_WAVE_FORMAT_* codes.
						format.channels = trImpl->output.channels;
						format.sampleRate = trImpl->sampleFormat.sampleRate;
						format.bitsPerSample = 32;
						drwav *pWav;
						if (!fOutWave.empty()) {
							String nameWaveFileTrack = fOutWave+"_"+std::to_string(trackIndex)+"_"+trackMaster->name+"_f32.wav";
							pWav = drwav_open_file_write(StringAsCStr(nameWaveFileTrack), &format);
							for (audiotrack_split_t* split : samples) {
								auto* sample = split->getSample();
								dbgassert(sample->nChannels==trImpl->output.channels);
								dbgassert(sample->nChannels==sample->samples.size());
								dbgassert(sample->nSamples==sample->samples[0].size());
								dbgassert(sample->nSamples==sample->samples[1].size());
		    					dbgassert(blockFull.samples == sample->nSamples*2);
		    					dbgassert(blockFull.samples >= sample->nSamples*2);
		    					float* in1 = sample->samples[0].data();
		    					float* in2 = sample->samples[1].data();
		    					float* largeBuf = blockFull.buf[0];
								for (uint64_t nSample = 0; nSample < sample->nSamples; nSample++) {
									*largeBuf++ = *in1++;
									*largeBuf++ = *in2++;
								}
//								for (uint16_t nChannel = 0; nChannel < sample->nChannels; nChannel++) {
//								}
								samplesWritten += drwav_write(pWav, blockFull.samples, blockFull.buf[0]);
							};
			    			log_printf("wrote %lld samples to %s\n", samplesWritten, StringAsCStr(nameWaveFileTrack));
							drwav_close(pWav);

						}
					}
				}
				trackIndex++;
    		}




    		std::vector<track_t*> _tracks = project.trackList.getAllTracksFlatVec();
    		log_printf("DELETE _tracks %d\n", _tracks.size());
    		for (track_t* tr : _tracks) {
    			log_printf("DELETE TRACK %s\n", StringAsCStr(tr->name));
    			vsthost::getInstance()->unloadTrack(tr);
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
			tls.midiHost->stopMidi();
			tls.audioHost->stopAudio();
			tls.midiHost->deinitPm();
			tls.audioHost->deinitPa();
		}
    	host->destroy();
    	log_printf("END\n", 0);
    } catch (std::exception& e) {
    	log_printf("exception %s\n", e.what());
    } catch (...) {
    	log_printf("unhandled exception\n", 0);
    }
	return 0;
}

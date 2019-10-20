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
#include "track_impl.h"

#ifdef _WIN32
#include <windows.h>
#endif
#ifdef __linux__
#include <unistd.h>
#include <limits.h>
#endif

void deleteApp() {

}

std::shared_ptr<AppCtrl> makeApp() {
	return nullptr;
}

#define LOG(fmtString,...) printf(fmtString "\n", ##__VA_ARGS__); fflush(stdout)


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
bool quit = false;
#ifdef _WIN32
BOOL WINAPI ConsoleHandler(DWORD dwType)
{
    switch(dwType) {
    case CTRL_C_EVENT:
		LOG("CTRL_C");
    	quit = true;
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
int main(int argc, char* argv[]) {
	LOG("ARGC %d", argc);
	for (int i = 0; i < argc; i++) {
		LOG("argv[%d] %s", i, argv[i]);
	}
#ifdef _WIN32
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
    appsettings settings = loadSettings();
    if (argc < 2) {
    	return 0;
    }
	String file = String(argv[1]);
	String fOutWave = "out.wav";
	if (argc > 2) {
		fOutWave = String(argv[2]);
	}
	int64_t time;
    try {
    	FileTimeGetter filetime(file);
    	time = filetime.getWriteTimeI64();

    	auto audioHost = std::make_unique<audiohost>();
    	auto midiHost = std::make_unique<midihost>();
    	auto host = std::make_unique<vsthost>();
    	vsthost::assignMasterCallback(host.get());
		host->setSamplerateBlockSize(settings.iosettings.samplerate, settings.iosettings.blocksize);

    	project_controller_t project;
    	plugindatabase_t plugindb;
    	waveformrender renderer;
    	audiocache cache(settings.iosettings.samplerate);
    	daw_tls::tlsinstance& tls = daw_tls::getTls();
    	tls.mainCtrl = nullptr;
    	tls.project = &project;
    	tls.host = host.get();
    	tls.audioHost = audioHost.get();
    	tls.midiHost = midiHost.get();
    	tls.audioCache = &cache;
    	tls.waveform = &renderer;
    	tls.pluginDatabase = &plugindb;

		audiohost::getInstance()->initPa();
		midihost::getInstance()->initPm();
		if (audioHost->startAudio(settings.iosettings)) {
			host->setOutput(audioHost.get());
		} else {
			//notify user
			log_printf("audioHost->startAudio() failed\n", 0);
			return 1;
		}

		midihost::getInstance()->startMidi();
    	plugindb.openDatabase();
    	LOG("START");
    	{
    		std::unique_ptr<PlaybackThread> playThread = std::make_unique<PlaybackThread>();
    		playThread->setTls(tls);
    		playThread->startThread(&project);
    		playThread->addRequest(REQ_STATE, (int) playback_state::status_no_process, true);
			if (!file.empty()) {
	    		auto pf = loadProjectFile(file);
	    		if (!pf) {
	    			fprintf(stderr, "Error: failed loading file\n");
	    			return EXIT_FAILURE;
	    		}
				project_snapshot_t& snapshot = pf->project;
				project.copyFrom(snapshot);
				audiocache::getInstance()->load(pf->sampleFileIndex);

				vsthost* host = vsthost::getInstance();
				/** create all audio instances **/
				for (track_t* t : project.trackList) {
					host->createAudio(t);
				}

				/** pre-load all plugin instances **/
        		project.trackList.loadPlugins(snapshot);


				/** fully load all plugin instances **/
        		std::vector<effectbase*> pluginsDeferred;
        		host->getDeferredEffects(pluginsDeferred);
        		my_printf("loading %d plugins\n", pluginsDeferred.size());
        		for (auto plugin : pluginsDeferred) {
            		my_printf("load %s\n", StringAsCStr(plugin->sName));
        			host->activateDeferred(plugin);
        		}

			} else {
	    		project.cursor.cursorPos = 0;
	    		project.loopEnabled = false;
	    		track_t* track1 = new track_t(TRACK_TYPE_MIDI, "track1", true);
	    		project.addTrackImpl(-1, track1, 0);

	    		track_t* track2 = new track_t(TRACK_TYPE_MIDI, "track2", true);
	    		project.addTrackImpl(-1, track2, 0);

	    		track_t* trackMaster = new track_t(TRACK_TYPE_MASTER, "master", true);
	    		project.addTrackImpl(0, trackMaster, 0);
	        	String pathTracks = "test.tracks";
	        	std::shared_ptr<trackcontainer_snapshot_t> ctr = loadTrackContainer(pathTracks);
	        	dbgassert(ctr);
	        	if (ctr) {
	        		for (track_snapshot_t& ts : ctr->tracks) {
	        			track_t* tr = new track_t(ts);
	        			track2->addChild(tr);
	            		project.addTrackImpl(-1, tr, 0);
	            		ts.trackLoaded = tr;
	            		log_printf("add track %s\n", StringAsCStr(tr->name));
	        		}

	        		//load plugins
	        		for (track_snapshot_t& ts : ctr->tracks) {
	            		log_printf("track '%s' loading %d plugins\n", StringAsCStr(ts.trackLoaded->name), ts.plugins.pluginSnapshots.size());
	        			ts.trackLoaded->loadSnapshot(ts);
	        		}

	    			vsthost* host = vsthost::getInstance();
	        		//load plugins
	        		for (track_snapshot_t& ts : ctr->tracks) {
	            		log_printf("track '%s' loading %d plugins\n", StringAsCStr(ts.trackLoaded->name), ts.plugins.pluginSnapshots.size());
	        			ts.trackLoaded->loadSnapshot(ts);
		    			std::vector<effectbase*> effects = ts.trackLoaded->audio->deferredEffects;
		    			for (auto eff : effects) {
		    				host->activateDeferred(eff);
		    			}
	        		}
	        	}
			}

    		my_printf("Tempo100: %d\n", project.tempo100);
    		my_printf("project.cursor.cursorPos: %d\n", project.cursor.cursorPos);

    		AudioBlock block(2, host->lBlockSize);
    		AudioBlock blockFull(1, host->lBlockSize*2);
    		double tLastMsg = getTimeMillis()/1000.0;
    		int64_t nBlocks = 0;
    		int64_t samplesWritten = 0;
			drwav_data_format format;
			format.container = drwav_container_riff;     // <-- drwav_container_riff = normal WAV files, drwav_container_w64 = Sony Wave64.
			format.format = DR_WAVE_FORMAT_IEEE_FLOAT;          // <-- Any of the DR_WAVE_FORMAT_* codes.
			format.channels = 2;
			format.sampleRate = host->lSampleRate;
			format.bitsPerSample = 32;
//			 drwav* pWav = drwav_open_file_write(StringAsCStr(fOutWave), &format);

			my_printf("request playback start..\n", 0);

			project.cursor.cursorPos = TICKS_BAR*4;

			playThread->addRequest(REQ_STATE, (int) playback_state::status_play, true);

			my_printf("start playback\n", 0);

    		while (!quit) {
//    			dsp_util::fillBlock(block, 0.0f);
//    			//still a race condition on_terminate here
//    			AudioBuffer* buff = nullptr;
//    			auto* stream = audioHost->getStream(0);
//    			if (stream->try_dequeue(buff)) {
//    				dbgassert(host->lBlockSize == buff->output->samples);
//    				if (host->lBlockSize == buff->output->samples) {
//    					block.copyFrom(buff->output);
//    					auto tNow = getTimeMillis()/1000.0;
//    					if (tNow - tLastMsg > 1) {
//    						tLastMsg = tNow;
//    						log_printf("playbackPos %d, %d blocks\n", project.playbackPos, nBlocks);
//    						if (project.playbackPos > TICKS_BAR*4) {
//    							playThread->addRequest(REQ_STATE, (int) playback_state::status_no_process, true);
//    							quit = true;
//    							break;
//    						}
//    					}
//    					//    					float* in1 = block.buf[0];
//    					float* in1 = block.buf[0];
//    					float* in2 = block.buf[1];
//    					float* largeBuf = blockFull.buf[0];
//    					for (int i = 0; i < block.samples; i++) {
//    						*largeBuf++ = *in1++;
//    						*largeBuf++ = *in2++;
//    					}
//    					samplesWritten += drwav_write(pWav, blockFull.samples, blockFull.buf[0]);
//    				}
//    				buff->inUse = false;
//    			} else {
//    	//			host->bufferUnderuns++;
//    		//		dsp_util::fillSilence(inputs, framesPerBuffer);
//    			}
				auto tNow = getTimeMillis()/1000.0;
				if (tNow - tLastMsg >= 1.0) {
					tLastMsg = tNow;
					host_stats_t stats;
					host->getStats(stats);


					log_printf("playbackPos %d, %d blocks, %d samples\n", project.playbackPos, stats.blocksProcessed, stats.samplesProcessed);


					if (project.playbackPos > TICKS_BAR*32) {
						playThread->addRequest(REQ_STATE, (int) playback_state::status_no_process, true);
						quit = true;
						break;
					}
				}
    			threadSleep(10);
    		}
//    		drwav_close(pWav);
//    		file->flush();
//    		file.reset();
    		if (quit) {
    			log_printf("wrote %lld samples to %s\n", samplesWritten, StringAsCStr(fOutWave));
    		}
    //		if (!host) {
    //			return paAbort;
    //		}
    		playThread->addRequest(REQ_STATE, (int) playback_state::status_no_process, true);
    		std::vector<track_t*> _tracks = project.trackList.vec();  // iterate a copy
    		my_printf("DELETE _tracks %d\n", _tracks.size());
    		for (track_t* tr : _tracks) {
    			my_printf("DELETE TRACK %s\n", StringAsCStr(tr->name));
    			vsthost::getInstance()->unloadTrack(tr);
    			project.trackList.removeTrack(tr);
    		}
    		project.trackList.clear();

    		for (track_t* tr : _tracks) {
    			releaseTrackResources(tr, nullptr);
    			delete tr;
    		}
    		playThread->stopThread();
    		playThread->joinThread();
    	}
    	host->destroy();
    	LOG("END");
    } catch (std::exception& e) {
    	log_printf("exception %s\n", e.what());
    } catch (...) {
    	log_printf("unhandled exception\n", 0);
    }
	return 0;
}

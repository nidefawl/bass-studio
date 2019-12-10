#pragma once
#include "config.h"
#include "str_util.h"
#include "seq_time.h"
#include "dsp_util.h"
#include <vector>
#include <atomic>
#include <stdint.h>
#include <unordered_map>
#include <stdbool.h>
#include "../vstsdk-host-2.4/aeffectx.h"
#include "note.h"
#include "rand.h"
#include "hires_timer.h"
#include "project.h"
#include "audiobuffer.h"
#include "audioblock.h"
#include "saferef.h"
#include "track.h"
#include "track_graph.h"
#include "daw_channel.h"


#include <memory>
#ifdef __linux__
#define PLATFORM_PLUGIN_EXT "so"
#endif
#ifdef _WIN32
#define PLATFORM_PLUGIN_EXT "dll"
#endif

class audiohost;
class clip_notes_t;
class effectbase;
class vstplugin;
struct track_impl_t;
struct audio_stage_t;
struct track_audio_src;
struct audio_stage_ref_t;
class project_controller_t;
class AudioEffectX;

typedef AEffect*(VSTPluginMain_t)(audioMasterCallback audioMasterCB);
typedef	AudioEffectX* (*FnCreateModule) (audioMasterCallback);
struct builtin_module_reg_t {
	int id = -1;
	bool isSynth;
	String name;
	FnCreateModule fnNewInstance;
};

class vstpluginloadres {
public:
	vstpluginloadres(int32_t _result, vstplugin* _plugin) :
		result(_result), plugin(_plugin) { };
	int32_t result;
	vstplugin* plugin;
};
struct plugin_notes_t {
	std::vector<note_t> notes;
};
struct plugin_snapshot_t;
effectbase* loadEffectModule(const plugin_snapshot_t& pluginSnapshot);
void loadEffectParamsFromSnapshot(const plugin_snapshot_t& pluginSnapshot, effectbase* effect);

#define SYNCHRONIZED_RW
struct AudioBlock;
struct host_stats_t {
	int32_t tickBar = 0;
	int32_t samplesProcessed;
	int32_t blocksProcessed;
	int64_t timeLastBlock;
	std::unordered_map<String, int64_t> timings;
	double usage;
	int32_t maxLatencyAudioMidi = 0;
	int32_t maxLatencyReturn = 0;
	int32_t latencyToMaster = 0;
	int32_t inputBufferUnderuns = 0;

};
struct host_processing_stats_t {
	int32_t pluginId;
};
class vsthost {
public:
	samplerate_t lSampleRate = 0;
	uint16_t lBlockSize = 0;
	int32_t hostSlot = -1;
	uint8_t numChannels;

	project_globals_t project;
	audioMasterCallback masterCallBackSlot = nullptr;

	std::atomic<int32_t> pluginId{100};
	std::atomic<int32_t> audioStageId{100};
	std::atomic<int32_t> sampleId{(1<<30)}; //TODO: collides with audiocache::nextIdx

	SYNCHRONIZED_RW std::shared_ptr<DAW::track_graph_t> lastTrackGraph;
	SYNCHRONIZED_RW std::shared_ptr<DAW::processing_graph_t> lastProcessingList;

	SYNCHRONIZED_RW hires_timer_t timer; // timer for cpu-time profiling
	SYNCHRONIZED_RW hires_timer_t timer2;// timer for cpu-time profiling
private:
	SYNCHRONIZED_RW VstTimeInfo timeinfo = {};
	SYNCHRONIZED_RW double lastTickEndPos = 0;
	playback_state lastState = playback_state::status_stop;
	SYNCHRONIZED_RW host_stats_t stats{0};
	SYNCHRONIZED_RW host_processing_stats_t processing{0};


	AudioBlock* blockZero = nullptr;
	audiohost* audioHost = nullptr;
	SYNCHRONIZED_RW audiothread_ringbuffer_t ringbuffer;
	SYNCHRONIZED_RW clip_notes_t* midiRealtimeInput; //TODO: per device and channel

	std::vector<std::shared_ptr<DelayLine>> delayLines;


	class ModuleManager;
	ModuleManager* moduleMgr;
	std::vector<audio_stage_t*> allAudioStages;
	std::vector<track_impl_t*> trackAudioStages;
	std::vector<vstplugin*> pluginInstancesVST2;
	std::vector<effectbase*> pluginInstancesInternal;
	std::vector<effectbase*> pluginInstances;
	std::vector<effectbase*> pluginsDeferred;
	std::vector<builtin_module_reg_t> builtinModules;

	SafeRefStorage<effectbase> safeRefs;
	seq_rand rnd;
private:
	vstpluginloadres loadInternalPlugin(int32_t type, int32_t globalId = 0);
	int32_t getNextGlobalModuleId(int32_t n);
	audiostageid_i32 getNextGlobalAudioStageId(int32_t as);
	bool unloadAllPlugins();
	void updateTime(int32_t samplePos, double dTickPos, playback_state state);
	void setBlockSize(uint16_t blockSize);
	void registerPlugins();
public:
	vsthost();
	vsthost(vsthost const&) = delete;
	~vsthost();
	void setSamplerateBlockSize(int32_t sampleRate, int32_t blockSize);
	void setOutput(audiohost* host);
	void operator=(vsthost const&) = delete;
	static vsthost* getInstance();
	static bool assignMasterCallback(vsthost* host);
	audiothread_ringbuffer_t& getRingBuffer() {
		return ringbuffer;
	}
	int32_t getNextSampleId(int32_t id);
	void sendNotesOff(effectbase* plugin);
	std::vector<builtin_module_reg_t>& getBuiltinModuleRegistry() {
		return builtinModules;
	}
	std::vector<note_t> getRealtimeNotes();

	void onStartPlayback(project_controller_t* ctrl);
	void onStopPlayback(project_controller_t* ctrl);
	int32_t processPlayback(project_controller_t* ctrl, int32_t sample, double posDouble, playback_state state, bool inLoop, bool isLoopAround);
	void processAudio(audio_stage_t* channel, AudioBlock* input, AudioBlock* output, int32_t sample, int32_t samples, playback_state state);
	VstTimeInfo* getTimeInfo() {
		return &this->timeinfo;
	}
	void getStats(host_stats_t& stats) {
		stats = this->stats;
	}
	void getProcessingStats(host_processing_stats_t& stats) {
		stats = this->processing;
	}
	void updatePluginWindows();
	void destroy();
	void unload();
	bool postInit();
	bool onTick();
	bool isStreaming();
	void onTrackLayoutChange();

	bool canDo(const char *ptr)
	{
		if ((!strcmp(ptr, HostCanDos::canDoSendVstEvents)) ||
			(!strcmp(ptr, HostCanDos::canDoSendVstMidiEvent)) ||
			(!strcmp(ptr, HostCanDos::canDoReceiveVstEvents)) ||
			(!strcmp(ptr, HostCanDos::canDoReceiveVstMidiEvent)) ||
			(!strcmp(ptr, HostCanDos::canDoSizeWindow)) ||
			(!strcmp(ptr, HostCanDos::canDoSendVstMidiEventFlagIsRealtime)) ||
			(!strcmp(ptr, HostCanDos::canDoStartStopProcess)) ||
			0)
			return true;
		return false;
	}
	vstplugin* getPlugin(AEffect* aeffect);
	effectbase* getPluginById(int32_t projectGlobalId);
	void unloadPlugin(effectbase* plugin);
	void removePlugin(effectbase* plugin);
	void unloadTrack(track_t* track);
	effectbase* makeModuleInstance(int32_t moduleType, int32_t moduleId, int32_t globalid = -1);
	vstpluginloadres loadPlugin(String filepath, int32_t globalId = 0);
	void createAudio(track_t* track);
	void releaseAudio(track_t* track);
	audio_stage_t* createAudioStage();
	void releaseAudioStage(audio_stage_t* audioStage);
	audio_stage_t* getAudioStage(const audio_stage_ref_t& ref) const;
	bool movePlugins(audio_stage_t* dstTr, audio_stage_t* trp, int32_t src, int32_t len, int32_t dst);
	bool moveEffects(audio_stage_t* trp, int32_t src, int32_t dst, int32_t len);
	bool insertNewPlugin(audio_stage_t* trp, effectbase* plugin, int32_t dst);
	bool replacePlugin(audio_stage_t* trp, effectbase* plugin, int32_t dst, effectbase** prevPlugin);
	void getAllInstances(std::vector<effectbase*>& effects);
	std::vector<vstplugin*> getVst2Instances() {
		return pluginInstancesVST2;
	}
	void addDeferredEffect(effectbase* plugin) {
		pluginsDeferred.push_back(plugin);
	}
	void getDeferredEffects(std::vector<effectbase*>& effects) {
		effects = pluginsDeferred;
	}
	void activateDeferred(effectbase* const eff, effectbase** out_effectLoaded = nullptr);
	SafeRefStorage<effectbase>* getSafeRefStore() {
		return &safeRefs;
	}
	void updateMaximumStageId();
};

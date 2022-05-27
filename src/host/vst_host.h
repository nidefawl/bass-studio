#pragma once
#include "config.h"
#include "str_util.h"
#include "seq_time.h"
#include "dsp_util.h"
#include <utility>
#include <vector>
#include <atomic>
#include "tls.h"
#include "types.h"
#include <map>

#include <vstsdk-host-2.4/aeffectx.h>
#include "note.h"
#include "rand.h"
#include "hires_timer.h"
#include "project.h"
#include "audio_config.h"
#include "audiobuffer.h"
#include "audioblock.h"
#include "saferef.h"
#include "track.h"
#include "track_graph.h"
#include "effect_graph.h"
#include "daw_channel.h"
#include "util/profiling.h"


#include <memory>
#ifdef __linux__
#define PLATFORM_PLUGIN_EXT "so"
#endif
#ifdef _WIN32
#define PLATFORM_PLUGIN_EXT "dll"
#endif
#ifdef __APPLE__
#define PLATFORM_PLUGIN_EXT "vst"
#endif

class clip_notes_t;
class effectbase;
class vstplugin;
struct track_impl_t;
struct audio_stage_t;
struct track_audio_src;
struct audio_stage_ref_t;
class project_controller_t;
class AudioEffectX;
class DawInstance;

typedef AEffect*(VSTPluginMain_t) (audioMasterCallback audioMasterCB);
typedef AudioEffectX* (*FnCreateModule)(audioMasterCallback);
struct builtin_module_reg_t {
    int id = -1;
    bool isSynth;
    String name;
    FnCreateModule fnNewInstance;
};
struct handles_t;
class vstpluginloadres {
public:
    vstpluginloadres(int32_t _result, vstplugin* _plugin) : result(_result), plugin(_plugin), shellPluginHandle(nullptr){};
    vstpluginloadres(int32_t _result, vstplugin* _plugin, handles_t* _shellHandle, String _path, String _name)
        : result(_result), plugin(_plugin), shellPluginHandle(_shellHandle), path(std::move(_path)), name(std::move(_name)){};
    int32_t result;
    vstplugin* plugin;
    handles_t* shellPluginHandle;
    String path;
    String name;
};

#define SYNCHRONIZED_RW
struct AudioBlock;
struct host_processing_stats_t {
    int32_t pluginId;
};
struct process_scratch_buf_t;
struct thread_stats_process_timings_t {
    uint32_t threadIdx;
    audiostageid_i32 stageId;
    int64_t timeStart;
    int64_t timeEnd;
    thread_stats_process_timings_t(
            uint32_t _threadIdx,
            audiostageid_i32 _stageId,
            int64_t _timeStart,
            int64_t _timeEnd)
        : threadIdx(_threadIdx),
          stageId(_stageId),
          timeStart(_timeStart),
          timeEnd(_timeEnd) {
    }
};
#define MAX_AUDIOPROCESSING_THREADS 32
#define NUM_AUDIOPROCESSING_THREADS_INITIAL 6
#define DAW_DEBUG_AUDIOGRAPH 0

class vsthost {
public:
    class vsthost_impl;
    struct track_block_processing_task_t;
    struct audiostream_properties_t {
        double ticksPerBlock        = 0.0;
        int64_t microSecsPerBlock   = 0;
        uint32_t blockSizeResampled = 0;
        uint32_t numBlocksInternal  = 0;
        uint32_t numBlocksExternal  = 0;
    };

public:
    static const int FLAG_HOST_UNLOAD_PLUGIN_NO_NOTIFY    = 1;
    static const int FLAG_HOST_FORCELOAD_DISABLED_PLUGINS = 2;

    // create tracks with 2 channels
    static constexpr channelnum_t DEFAULT_CHANNEL_COUNT = 2;

private:
    vsthost_impl* const impl;

public:
    sampleformat_t m_sampleFormatInternal = { 44100, 512, sampleformat_bits_t::NONE };
    sampleformat_t m_sampleFormatExternal = { 44100, 512, sampleformat_bits_t::NONE };

    int32_t hostSlot    = -1;

    project_globals_t prjGlobals;
    audioMasterCallback masterCallBackSlot = nullptr;


    SYNCHRONIZED_RW std::atomic<int32_t> bypassEffectProcessing{ false };
    SYNCHRONIZED_RW std::atomic<int32_t> multithreadedProcessing{ 1 };
    SYNCHRONIZED_RW std::atomic<int32_t> bypassPlaybackProcessing{ false };
    SYNCHRONIZED_RW std::atomic<int32_t> bypassSampleConversion{ false };
    SYNCHRONIZED_RW std::atomic<int32_t> cacheAudioGraph{ false };
    std::atomic<int32_t> pluginId{ 1 << 16 };
    std::atomic<int32_t> audioStageId{ 100 };
    std::atomic<int32_t> sampleId{ (1 << 30) };//TODO: collides with audiocache::nextIdx
#if DAW_DEBUG_AUDIOGRAPH
    SYNCHRONIZED_RW std::shared_ptr<DAW::track_graph_t> lastTrackGraph;
    SYNCHRONIZED_RW std::shared_ptr<DAW::processing_graph_t> lastProcessingList;
    SYNCHRONIZED_RW std::map<audiostageid_i32, std::shared_ptr<DAW::processing_graph_t>> lastProcessingGraphs;
#endif
    SYNCHRONIZED_RW hires_timer_t timerAudioTick;// timer for cpu-time profiling
    SYNCHRONIZED_RW hires_timer_t timerBlock;    // timer for cpu-time profiling
    SYNCHRONIZED_RW hires_timer_t timerProfile;  // timer for cpu-time profiling

private:
    SYNCHRONIZED_RW clip_t* recordingClip = nullptr;
    SYNCHRONIZED_RW std::atomic<bool> hasNewRecordedData{};
    SYNCHRONIZED_RW clip_t* recordDataProcessed = nullptr;
    SYNCHRONIZED_RW VstTimeInfo m_sharedTimeInfo = {}; //TODO: remove
    SYNCHRONIZED_RW double lastTickEndPos       = 0;
    SYNCHRONIZED_RW host_stats_t stats{};
    SYNCHRONIZED_RW host_processing_stats_t processing{ 0 };

    SYNCHRONIZED_RW audiothread_ringbuffer_t ringbuffer;
    SYNCHRONIZED_RW clip_notes_t* midiRealtimeInput; //TODO: per device and channel
    SYNCHRONIZED_RW clip_notes_t* midiProcessedInput;//TODO: per device and channel

    //  std::vector<std::shared_ptr<DelayLine>> delayLines;


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

private:
    vstpluginloadres loadInternalPlugin(int32_t type, int32_t globalId = 0);
    bool unloadAllPlugins();
    void updateTime(VstTimeInfo& timeinfo, double samplePos, double dTickPos, playback_state state) const;
    void registerPlugins();
    void processMidiRealtimeInput(project_controller_t* ctrl, double posDouble, playback_state state);
    int32_t processGraph(project_controller_t* ctrl, const audiostream_properties_t& audioProp, DAW::processing_graph_t* processingGraph, AudioBlock* ptrExternalInputs, AudioBlock* ptrExternalOutputs, int32_t samplePosProcess, double tickPosProcess, playback_state state, bool inLoop, bool isLoopAround);
    int64_t writeTrackSamplesToDisk(String fOutWave, track_impl_t* trImpl, samplecount_t samplePos, samplecount_t numSamples);
    uint32_t finishTreadTasks(uint32_t tasksRunning, bool wait);
    void updateRecordingClip(tick_t tickBlockStart, tick_t tickBlockEnd, std::vector<note_t>& m_list);
    void finishRecordingClip(tick_t tickBlockStart, tick_t tickBlockEnd, std::vector<note_t>& m_list);
    void processMidiProcessedOutput(playback_state state, tick_t tickBlockStart, tick_t tickBlockEnd, std::vector<noteevent_t>& noteEventsProcessed);
    /* These are currently not called */
    void onPluginsChanged(audio_stage_t* stage);
    void updatePluginWindows();

public:
    vsthost();
    vsthost(vsthost const&) = delete;
    ~vsthost();
    void operator=(vsthost const&) = delete;

    static vsthost* getInstance();
    static bool assignMasterCallback(vsthost* host);

    void setTls(daw_tls::tlsinstance& tls);

    void destroy();

    void initThreads();
    void setThreadCount(uint32_t threadCount);
    uint32_t getThreadCount();
    uint32_t getMaxThreadCount();
    int32_t getPlayThreadId();

    void setSampleFormat(const sampleformat_t& _sampleFormat);
    void setOutput(std::shared_ptr<DAW::AudioIO::AudioStream> stream);
    audiostream_properties_t getAudioStreamProperties() const;
    bool isStreaming();

    int32_t processRender(project_controller_t* ctrl, int32_t sample, double posDouble);
    int32_t processPlayback(project_controller_t* ctrl, int32_t sample, double posDouble, playback_state state, bool inLoop, bool isLoopAround);
    int32_t processGraphNode(process_scratch_buf_t& tmp, track_block_processing_task_t& task) /*const*/;
    void processAudio(audio_stage_t* stage, AudioBlock* input, AudioBlock* output, const double tickLatencyCompensated, const double sampleLatencyCompensated, int32_t numSamples, playback_state state, const DAW::effect_processing_graph_t* const processingGraph) const;


    void unload();
    void releaseProjectResources();

    bool onTick();
    void onTrackLayoutChange();
    void onStartPlayback(project_controller_t* ctrl);
    void onStopPlayback(project_controller_t* ctrl);
    void onPlaybackJumpFromTo(project_controller_t* ctrl, int32_t fromSamplePos, double fromTickPos, int32_t toSamplePos, double toTickPos);

    int32_t getNextSampleId(int32_t id);
    std::vector<note_t> getRealtimeNotes();
    bool writeRecordedData(project_t* project);
    void sendNotesOff(effectbase* plugin);

    std::vector<builtin_module_reg_t>& getBuiltinModuleRegistry() {
        return builtinModules;
    }


    void getBlockThreadStats(std::vector<thread_stats_process_timings_t>&);
    void getShortStats(host_stats_reducted_t& stats) const {
        stats.usage             = this->stats.usage;
        stats.timeProcess       = this->stats.timeProcessPlugins;
        stats.timeProcessRaw    = this->stats.timeProcessPluginsRaw;
        stats.timePerBlock_usec = m_sampleFormatInternal.blockSize * 1000000 / m_sampleFormatInternal.sampleRate;
    }
    float getCpuUsage() const {
        return stats.usageRaw;
    }
    void getStats(host_stats_t& stats) {
        stats = this->stats;
    }
    void getProcessingStats(host_processing_stats_t& stats) {
        stats = this->processing;
    }

    VstTimeInfo* getTimeInfo() {
        return &this->m_sharedTimeInfo;
    }
    static bool canDo(const char* ptr) {
        if ((!strcmp(ptr, HostCanDos::canDoSendVstEvents)) ||
            (!strcmp(ptr, HostCanDos::canDoSendVstMidiEvent)) ||
            (!strcmp(ptr, HostCanDos::canDoSendVstTimeInfo)) ||
            (!strcmp(ptr, HostCanDos::canDoReceiveVstEvents)) ||
            (!strcmp(ptr, HostCanDos::canDoReceiveVstMidiEvent)) ||
            (!strcmp(ptr, HostCanDos::canDoReportConnectionChanges)) ||
            (!strcmp(ptr, HostCanDos::canDoAcceptIOChanges)) ||
            (!strcmp(ptr, HostCanDos::canDoSizeWindow)) ||
            (!strcmp(ptr, HostCanDos::canDoSendVstMidiEventFlagIsRealtime)) ||
            (!strcmp(ptr, HostCanDos::canDoStartStopProcess)) ||
            (!strcmp(ptr, HostCanDos::canDoShellCategory)))
            return true;
        return false;
    }

    vstplugin* getPlugin(AEffect* aeffect);
    effectbase* getPluginById(int32_t projectGlobalId, bool activeOnly = true) const;
    void getAllInstances(std::vector<effectbase*>& effects);
    std::vector<vstplugin*> getVst2Instances() {
        return pluginInstancesVST2;
    }
    bool addDeferredEffect(effectbase* plugin);
    void getDeferredEffects(std::vector<effectbase*>& effects) {
        effects = pluginsDeferred;
    }
    SafeRefStorage<effectbase>* getSafeRefStore() {
        return &safeRefs;
    }
    void unloadPlugin(effectbase* plugin, int flags = 0);
    void removePlugin(effectbase* plugin);
    void unloadTrack(track_t* track);
    effectbase* makeModuleInstance(int32_t moduleType, int32_t moduleId, int32_t globalid = -1);
    vstpluginloadres loadPlugin(String filepath, uint32_t uId, int32_t globalId = 0, uint64_t bugfixFlags = 0);
    void activateDeferred(effectbase* eff, int flags, effectbase** out_effectLoaded = nullptr);

    void createAudio(track_t* track);
    void releaseAudio(track_t* track);
    audio_stage_t* createAudioStage();
    void releaseAudioStage(audio_stage_t* audioStage);
    audio_stage_t* getAudioStage(const audio_stage_ref_t& ref) const;
    int32_t validateIds();
    void updateMaximumStageId();
    audio_stage_id_t getNextGlobalAudioStageId(int32_t globalId);
    bool isStageIdInUse(track_id_snapshot_t stageId);
    int32_t getNextGlobalModuleId(int32_t globalId);

    bool movePlugins(audio_stage_t* dstTr, audio_stage_t* trp, int32_t src, int32_t dst, int32_t len);
    bool moveEffects(audio_stage_t* trp, int32_t src, int32_t dst, int32_t len);
    bool insertNewPlugin(audio_stage_t* trp, effectbase* plugin, int32_t dst);
    bool postPluginLoaded(audio_stage_t* trp, effectbase* plugin);
    bool replacePlugin(audio_stage_t* trp, effectbase* plugin, int32_t dst, effectbase** prevPlugin);


    void checkScanner();
    void scanPlugins();
    bool isScanning();
    void stopScanner();

    void preExportBegin(project_controller_t* ctrl, export_settings_t& exportSettings);
    void postExportEnd(project_controller_t* ctrl, export_settings_t& exportSettings);
};

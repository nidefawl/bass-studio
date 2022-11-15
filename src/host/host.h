#pragma once
#include <atomic>
#include <map>
#include <memory>
#include <utility>
#include <vector>
#include "host/clip/clip.h"
#include "config.h"
#include "host/plugin/modules.h"
#include "str_util.h"
#include "seq_time.h"
#include "dsp_util.h"
#include "tls.h"
#include "types.h"
#include "note.h"
#include "midi-event.h"
#include "rand.h"
#include "hires_timer.h"
#include "host/project/project.h"
#include "audio_config.h"
#include "host/audiobuffer/audiobuffer.h"
#include "host/audiobuffer/audioblock.h"
#include "saferef.h"
#include "host/track/track.h"
#include "host/graph/track_graph.h"
#include "host/graph/effect_graph.h"
#include "daw_channel.h"
#include "util/profiling.h"
#include "host/host_pluginmanager.h"
#include "meter/meter.h"
#include <vstsdk-host-2.4/aeffectx.h>

class audiocache;
struct AudioBlock;

namespace DAW::Host {
class Host;

struct process_scratch_buf_t {
    VstTimeInfo timeinfo{};
    hires_timer_t timer;// timer for cpu-time profiling
    AudioBlock block;
    std::vector<std::vector<float>> scratchBuffers;
    std::vector<midievent_note_t> noteEventsTemp;
    std::vector<midievent_ctrl_t> ctrlEventsTemp;
    std::vector<DAW::track_source_t> trackSources;
};

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

Host* getInstance();

struct midi_data_t {
    clip_notes_t notes;
    midi_input_events_t events;
};

class Host final : public PluginManager {
public:
    class host_impl;
    struct track_block_processing_task_t;
    struct audiostream_properties_t {
        double ticksPerBlock        = 0.0;
        int64_t microSecsPerBlock   = 0;
        uint32_t blockSizeResampled = 0;
        uint32_t numBlocksInternal  = 0;
        uint32_t numBlocksExternal  = 0;
    };

private:
    host_impl* const impl;

public:
    sampleformat_t m_sampleFormatInternal = { 44100, 512, sampleformat_bits_t::NONE };
    sampleformat_t m_sampleFormatExternal = { 44100, 512, sampleformat_bits_t::NONE };

    /* gets updated by caller (PlayThread) before processing */
    project_globals_t prjGlobals;
    /* gets updated by Host before processing */
    Host::audiostream_properties_t audioProperties;

    std::atomic<int32_t> bypassEffectProcessing{ false };
    std::atomic<int32_t> multithreadedProcessing{ 1 };
    std::atomic<int32_t> bypassPlaybackProcessing{ false };
    std::atomic<int32_t> bypassSampleConversion{ false };
    std::atomic<int32_t> cacheAudioGraph{ false };
#if DAW_DEBUG_AUDIOGRAPH
    std::shared_ptr<DAW::track_graph_t> lastTrackGraph;
    std::shared_ptr<DAW::processing_graph_t> lastProcessingList;
    std::map<audiostageid_i32, std::shared_ptr<DAW::processing_graph_t>> lastProcessingGraphs;
#endif
    hires_timer_t timerAudioTick;// timer for cpu-time profiling
    hires_timer_t timerBlock;    // timer for cpu-time profiling
    hires_timer_t timerProfile;  // timer for cpu-time profiling

private:
    double lastTickEndPos       = 0;
    host_stats_t stats{};
    host_processing_stats_t processing{ 0 };

    audiothread_ringbuffer_t ringbuffer;
    midi_data_t midiRealtimeInput;

private:
    void processMidiRealtimeInput(project_controller_t* ctrl, double posDouble, playback_state state);
    int32_t processGraph(project_controller_t* ctrl, const audiostream_properties_t& audioProp, processing_graph_t* processingGraph, AudioBlock* ptrExternalInputs, AudioBlock* ptrExternalOutputs, int32_t samplePosProcess, double tickPosProcess, playback_state state, bool inLoop, bool isLoopAround);
    int64_t writeTrackSamplesToDisk(String fOutWave, track_impl_t* trImpl, samplecount_t samplePos, samplecount_t numSamples);
    uint32_t finishTreadTasks(uint32_t tasksRunning, bool wait);
public:
    Host();
    Host(Host const&) = delete;
    ~Host() override;
    void operator=(Host const&) = delete;
    std::shared_ptr<DAW::rmsmeter> getMeterInput();
    std::shared_ptr<DAW::rmsmeter> getMeterOutput();
    void onTrackLayoutChange() override;
    void onAudioStageChanged(audio_stage_t* stage) override;
    void setTls(daw_tls::tlsinstance& tls);
    void destroy();

    void initThreads();
    void setThreadCount(uint32_t threadCount);
    uint32_t getThreadCount();
    uint32_t getMaxThreadCount();
    int32_t getPlayThreadId();

    void setSampleFormat(const sampleformat_t& _sampleFormat);
    void setOutput(std::shared_ptr<AudioIO::AudioStream> stream);
    void resetResamplers();
    const audiostream_properties_t& getAudioStreamProperties() const;
    audiostream_properties_t& updateAudioStreamProperties();
    bool isStreaming();

    int32_t processRender(project_controller_t* ctrl, int32_t sample, double posDouble);
    int32_t processPlayback(project_controller_t* ctrl, int32_t sample, double posDouble, playback_state state, bool inLoop, bool isLoopAround);
    int32_t processGraphNode(process_scratch_buf_t& tmp, track_block_processing_task_t& task) /*const*/;
    void processAudio(process_scratch_buf_t& tmp, audio_stage_t* stage, AudioBlock* input, AudioBlock* output, const project_globals_t& globals, const double tickLatencyCompensated, const samplecount_t sampleLatencyCompensated, int32_t numSamples, playback_state state, const effect_processing_graph_t* const processingGraph) const;

    void unload();

    void onTick();
    void onStartPlayback(project_controller_t* ctrl);
    void onStopPlayback(project_controller_t* ctrl);
    void onPlaybackJumpFromTo(project_controller_t* ctrl, int32_t fromSamplePos, double fromTickPos, int32_t toSamplePos, double toTickPos);

    std::vector<note_t> getRealtimeNotes();
    bool writeRecordedData(project_controller_t* ctrl);
    void sendNotesOff(effectbase* plugin);

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

    void preExportBegin(project_controller_t* ctrl, export_settings_t& exportSettings);
    void postExportEnd(project_controller_t* ctrl, export_settings_t& exportSettings);
};
}

namespace DAW::Host {
    void MixWithGainAndPanAutomation(const Host* host, process_scratch_buf_t& tmp, AudioBlock* in, AudioBlock* out, const automated_param_connection_t& autParGain, const automated_param_connection_t& autParPan, double tickBegin, double tickEnd, playback_state state, float MTR_CEIL, float DBFS_MUTE_POS);
    void FillAudioBlockFromClips(audiocache* cache, const project_globals_t& prjGlobals, const std::vector<clip_t*>& clips, const sampleformat_t& dstSampleFormat, samplecount_t samplePosBegin, AudioBlock& out);
}
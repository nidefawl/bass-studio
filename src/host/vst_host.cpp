#include <cstdlib>
#include <algorithm>
#include <cmath>
#include <memory.h>
#include "math/seq_math.h"
#include "str_util.h"
#include "seq_util.h"
#include "seq_time.h"
#include "dsp_util.h"
#include "samplerate.h"

#include "project.h"
#include "profiling.h"
#include "vst_host.h"
#include "fileio.h"
#include "track.h"
#include "basectrl.h"
#include "host/mainctrl.h"

#include "plugin/base_plugin.h"
#include "plugin/vst_plugin.h"
#include "plugin/vst_plugin_handles.h"
#include "vst_window.h"

#include <vstsdk-host-2.4/aeffectx.h>
#include "appsettings.h"

#include "logging.h"
#include "audioblock.h"
#include "audiobuffer.h"
#include "platform.h"
#include "audio_host.h"
#include "midi_host.h"
#include "midi-defs.h"
#include "midi-msg.h"

#include "assert_dbg.h"
#include "track_impl.h"
#include "projectcontroller.h"
#include "thread.h"
#include "threads/threadlock.h"
#include "track_graph.h"
#include "effect_graph.h"
#include "resampler.h"
#include "threads/workerthread.h"
#include "threads/childprocessthread.h"
#include "sse.h"

#include <deque>

#ifdef _WIN32
#include <windows.h>
#endif
#if defined(__linux__) || defined(__APPLE__)
#include <dlfcn.h>
#endif

#include <dr_libs/dr_wav.h>

#define DBG_PRINT_CALLBACKS
#ifdef DBG_PRINT_CALLBACKS
#define MAX_LEN_MY_DBF 512

bool filterOpCode(int opcode) {
//    return opcode == audioMasterUpdateDisplay;
//    if ( opcode == audioMasterSizeWindow)
//        return true;
//    if ( opcode == audioMasterBeginEdit)
//        return true;
//    if ( opcode == audioMasterEndEdit)
//        return true;
//    if ( opcode == audioMasterAutomate)
//        return true;
//    if ( opcode == audioMasterGetInputLatency)
//        return false;
//    if ( opcode == audioMasterGetOutputLatency)
//        return false;
    return true;
}

void logPluginCb(vstplugin* plugin, const char* fmt, int opcode, int index, int64_t value, float opt = 0);

void logPluginCb(vstplugin* plugin, const char* fmt, int opcode, int index, int64_t value, float opt)
{
    if (filterOpCode(opcode)) {
        char buf[MAX_LEN_MY_DBF];
        snprintf(buf, MAX_LEN_MY_DBF - 1, fmt, opcode, index, value, opt);
        log_printf("%s %s", !plugin?"UNKNOWN":StringAsCStr(plugin->sName), buf);
    }
}

#else

void emptyPrinft(vstplugin* plugin, const char *fmt, ...) {
}
#define logPluginCb emptyPrinft

#endif


struct vsthost::track_block_processing_task_t {
    std::shared_ptr<DAW::effect_processing_graph_t> effectProcessingGraph;
    const DAW::processing_track_node_t* trackNode = nullptr;
    AudioBlock* ptrExternalInputs = nullptr;
    AudioBlock* ptrExternalOutputs = nullptr;
    audiostream_properties_t audioProp;
    int32_t samplePosProcess = 0;
    double tickPosProcess = 0.0;
    bool inLoop = false;
    playback_state playbackState = playback_state::status_no_process;
    int debugLogProcessing = 0;
};

struct process_scratch_buf_t {
    VstTimeInfo timeinfo{};
    AudioBlock tempBlock;
    SYNCHRONIZED_RW hires_timer_t timer;// timer for cpu-time profiling
    process_scratch_buf_t() : tempBlock(8, 256) {
    }

    ~process_scratch_buf_t() = default;
};

class TrackBlockProcessTask : public WorkerThread::ThreadTask {
    process_scratch_buf_t buf;
    vsthost::track_block_processing_task_t blockProcTask;
    std::atomic_bool isBusy{false};
    bool inUse = false;

public:
    struct process_task_stats_t {
        int64_t timeStart = 0;
        int64_t timeEnd = 0;
    };

    process_task_stats_t stats;
    uint32_t threadIdx = 0;

    TrackBlockProcessTask() : WorkerThread::ThreadTask() {
    }

    bool isInUse() const {
        return inUse;
    }

    void run() override {
        stats.timeStart = getTimeMicros();
        vsthost::getInstance()->processBlockTrack(buf, blockProcTask);
        stats.timeEnd = getTimeMicros();
        isBusy=false;
    }

    void setTask(vsthost::track_block_processing_task_t task) {
        reset();
        this->blockProcTask = task;
        isBusy=true;
        inUse = true;
    }

    void resetTask() {
        inUse = false;
    }

    bool getIsBusy() const {
        return isBusy;
    }

    vsthost::track_block_processing_task_t& getTask() {
        return blockProcTask;
    }
};


/**
 * VST Host implementation internals
 */
class vsthost::vsthost_impl {
public:
    std::vector<std::shared_ptr<resampler_t>> resamplers;

    process_scratch_buf_t singleThreadedBuf;
    WorkerThread threads[MAX_AUDIOPROCESSING_THREADS];
    TrackBlockProcessTask tasks[MAX_AUDIOPROCESSING_THREADS];
    int scanningState = 0;
    std::unique_ptr<ProcessThread> vstscannerProcessThread;
    std::map<uint32_t, std::shared_ptr<DelayLine>> delayLines;
    std::vector<thread_stats_process_timings_t> blockThreadStats;
    std::vector<thread_stats_process_timings_t> lastBlockThreadStats;
    std::vector<audiostageid_i32> waitingTasks;
    std::mutex mtx;
    uint32_t threadsRunningCount = 0;
    uint32_t threadCount = 4;
    uint32_t playThreadId = 0;

    VstInt32 vstShellCurrentUniqueId = 0;
    vsthost_impl() {
        uint32_t u = 0;
        for (TrackBlockProcessTask& task : tasks) {
            task.threadIdx = u++;
        }
    }
    ~vsthost_impl() = default;

    void resetDelaylines() {
        //delayLines.clear();//TODO: this might free a lot of memory and be expensive: profile!
    }

    DelayLine* getDelayLine(uint32_t id, int32_t numChannels) {
        using namespace std;
        lock_guard<mutex> hold(mtx);
        if (!delayLines.count(id)) {
            delayLines[id] = std::shared_ptr<DelayLine>(new DelayLine((uint32_t)numChannels, 16));
        }
        return delayLines[id].get();
    }

    std::shared_ptr<resampler_t> getResampler(sampleformat_t in, sampleformat_t out, uint32_t idx) {
        auto it = std::find_if(resamplers.begin(), resamplers.end(), [&in,&out,idx](std::shared_ptr<resampler_t>& ptr){
            return ptr->in == in && ptr->out == out && ptr->idx == idx;
        });
        if (it == resamplers.end()) {

            oversample_config_t config;
            config.inputSampleRate = in.sampleRate;
            config.outputSampleRate = out.sampleRate;
            config.numChannels = 32;
            config.setInputLength(in.blockSize);
            std::shared_ptr<resampler_t> resampler = std::make_shared<resampler_t>(idx, in, out, config);
            resamplers.push_back(resampler);
            return resampler;

        }
        return *it;
    }

    void resetBlock() {
        this->lastBlockThreadStats = std::move(this->blockThreadStats);
        this->blockThreadStats.clear();
        for (auto i = threadsRunningCount; i < threadCount && i < MAX_AUDIOPROCESSING_THREADS; i++) {
            threads[i].startThread();
            threadsRunningCount++;
        }
    }

    void startThreads() {
        uint32_t countStarted = 0;
        for (WorkerThread& thread : threads) {
            thread.startThread();
            countStarted++;
            if (countStarted == this->threadCount) {
                break;
            }
        }
        threadsRunningCount = countStarted;
    }

    void stopThreads() {
        uint32_t countStopped = 0;
        for (WorkerThread& thread : threads) {
            if (countStopped == this->threadsRunningCount) {
                break;
            }
            thread.stopThread();
            countStopped++;
        }
        countStopped = 0;
        for (WorkerThread& thread : threads) {
            if (countStopped == this->threadsRunningCount) {
                break;
            }
            thread.joinThread();
            countStopped++;
        }
    }
};

#define NUM_HOST_CB_SLOTS 4
namespace
{
struct vst_internal_hostslot {
    vsthost* g_instance = nullptr;
    vsthost::vsthost_impl* g_instanceImpl = nullptr;
};

vst_internal_hostslot g_hostslots[NUM_HOST_CB_SLOTS];
}

/**
 * VST Host AudioMasterCallback
 */
VstIntPtr audioMasterHost(vsthost* host, vsthost::vsthost_impl* impl, AEffect* effect, VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt) {
    dbgassert(host);
    // In case a plugin instance outlives the host
    if (!host)
        return 0;

    /**
     * Thread safety is guaranteed by only allowing internal threads to enter the callback
     * TODO: Find out what exact thread we got called from. @see notes
     */
    vstplugin *plugin = host->getPlugin(effect);
    if (!seqthreads::isInternalThread()) {

        log_printf("Ignore %s (own thread) opcode %d %d %zd %f\n", !plugin?"UNKNOWN":StringAsCStr(plugin->sName), opcode, index, value, opt);
        return 0;
    }
    /**
     * TODO: Detect reentrance and guard against it. @see notes
     */
    bool throttleLog = false;
    bool validProcessingState = false;
    if (plugin) {
        //TODO: getOpCodeStats is not a threadsafe implementation
        vst_opcode_stats_t& opcodeStats = plugin->getOpCodeStats(true, opcode);
        opcodeStats.numDispatches++;
        int32_t tmMillisS32 = static_cast<int32_t>(static_cast<uint64_t>(getTimeMillis()) & (0x7FFF'FFFFLL));
        int32_t tmSince = tmMillisS32 - opcodeStats.tmMillis;
        if (tmSince < 2000) {
            throttleLog = opcodeStats.numDispatches > 20;
        } else {
            opcodeStats.tmMillis = tmMillisS32;
        }

        /**
         * Validate that the plugin is currently fully loaded and setup and connected to an audiostage that is valid.
         * Currently plugins have an extended lifetime after removal inside the edithistory.
         *
         * TODO: This check is not well implemented
         * Add a lock free thread-safe way to check (from the callback) if a plugin is ready for processing
         */
        if (plugin->bIsSetup && plugin->trackImpl) {
            // get this from the host instead of the tls
            auto projCtrl = project_controller_t::get();
            if (projCtrl && projCtrl->getTracks().resolveTrack(plugin->trackImpl->toRef())) {
                validProcessingState = true;
            }
        }
        if (!validProcessingState) {
            log_printf("%s opCode %d in !validProcessingState\n", StringAsCStr(plugin->sName), opcode);
        }
    }
    switch (opcode)
    {
    case audioMasterAutomate:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterAutomate %d %d %zd %f\n", opcode, index, value, opt);
        if (plugin) {
            auto* effParam = plugin->getEffectParam(index);
            if (!effParam) {
                log_printf("%s audioMasterAutomate unknown param index %d %zd %f\n", StringAsCStr(plugin->getName()), index, value, opt);
            } else {

                // call to deactivateAutomation is not thread safe,
                plugin->deactivateAutomation(effParam->idx);
                plugin->recvPluginEditParamUpdate(effParam->internalIdx);
            }
        }
        return 1;
    case audioMasterVersion:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterVersion %d %d %zd\n", opcode, index, value, 0);
        return 2400L; //VST 2.4
    case audioMasterCurrentId:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterCurrentId %d %d %zd\n", opcode, index, value, 0);
        //return OnGetCurrentUniqueId(nEffect);
        if (plugin) {
            return (VstIntPtr)plugin->getLocalCurrentUniqueId();
        }
        return impl->vstShellCurrentUniqueId;
    case audioMasterIdle:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterIdle %d %d %zd\n", opcode, index, value, 0);
        //return OnIdle(nEffect);
        return 0L;
    case audioMasterGetTime:
        //{
        //    int32_t playThreadId = host->getPlayThreadId();
        //    int32_t localThreadId = getCurrentThreadId();
        //    if (localThreadId == playThreadId) {
        //        return (VstIntPtr)plugin->getLocalTimeInfoPtr();
        //    }
        //}
        //if (!throttleLog) logPluginCb(plugin, "audioMasterGetTime %d %d %zd\n", opcode, index, value);
        if (plugin) {
            return (VstIntPtr)plugin->getLocalTimeInfoPtr();
        }
        return (VstIntPtr)host->getTimeInfo();
        
    case audioMasterProcessEvents:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterProcessEvents %d %d %zd\n", opcode, index, value, 0);
        return 0;
    case audioMasterIOChanged:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterIOChanged %d %d %zd\n", opcode, index, value, 0);
        return 0;
    case audioMasterNeedIdle:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterNeedIdle %d %d %zd\n", opcode, index, value, 0);
        if (plugin) {
            plugin->bWantsEffIdle = true;
        }
        return 0;
    case audioMasterSizeWindow:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterSizeWindow %d %d %zd\n", opcode, index, value, 0);
        if (plugin) {
            plugin->updateWindowSize();
        }
        return 1;
    case audioMasterGetSampleRate:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterGetSampleRate %d %d %zd\n", opcode, index, value, 0);
        if (plugin) {
            return (long)plugin->format.sampleRate;
        }
        if (host) {
            return (long)host->m_sampleFormatInternal.sampleRate;
        }
        return 0;
    case audioMasterGetBlockSize:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterGetBlockSize %d %d %zd\n", opcode, index, value, 0);
        if (plugin) {
            return (long)plugin->format.blockSize;
        }
        if (host) {
            return (long)host->m_sampleFormatInternal.blockSize;
        }
        return 0;
    case audioMasterGetInputLatency:
        //if (!throttleLog) logPluginCb(plugin, "audioMasterGetInputLatency %d %d %zd\n", opcode, index, value);
        //TODO: find out if other hosts provide this info
        // IL Harmor requests this info
        return 0;
    case audioMasterGetOutputLatency:
        //if (!throttleLog) logPluginCb(plugin, "audioMasterGetOutputLatency %d %d %zd\n", opcode, index, value);
        //TODO: find out if other hosts provide this info
        // IL Harmor requests this info
        return 0;
    case audioMasterGetCurrentProcessLevel:
        //if (!throttleLog) logPluginCb(plugin, "audioMasterGetCurrentProcessLevel %d %d %zd\n", opcode, index, value);
        return VstProcessLevels::kVstProcessLevelRealtime;
    case audioMasterGetAutomationState:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterGetAutomationState %d %d %zd\n", opcode, index, value, 0);
        return kVstAutomationReadWrite;
    case audioMasterOfflineStart:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterOfflineStart %d %d %zd\n", opcode, index, value, 0);
        return 0;
    case audioMasterOfflineRead:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterOfflineRead %d %d %zd\n", opcode, index, value, 0);
        return 0;
    case audioMasterOfflineWrite:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterOfflineWrite %d %d %zd\n", opcode, index, value, 0);
        return 0;
    case audioMasterOfflineGetCurrentPass:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterOfflineGetCurrentPass %d %d %zd\n", opcode, index, value, 0);
        return 0;
    case audioMasterOfflineGetCurrentMetaPass:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterOfflineGetCurrentMetaPass %d %d %zd\n", opcode, index, value, 0);
        return 0;
    case audioMasterGetVendorString:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterGetVendorString %d %d %zd\n", opcode, index, value, 0);
         strcpy((char *)ptr, "NFMH");
        return 1L;
    case audioMasterGetProductString:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterGetProductString %d %d %zd\n", opcode, index, value, 0);
        strcpy((char *)ptr, "DAW");
        return 1L;
    case audioMasterGetVendorVersion:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterGetVendorVersion %d %d %zd\n", opcode, index, value, 0);
        return 1L;
    case audioMasterVendorSpecific:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterVendorSpecific %d %d %zd\n", opcode, index, value, 0);
        return 0;
    case audioMasterCanDo:
        if (!throttleLog) {
            log_printf("%s audioMasterCanDo %s\n", !plugin?"UNKNOWN":StringAsCStr(plugin->sName), (const char*)ptr);
        }
        return host->canDo((const char*)ptr);
    case audioMasterGetLanguage:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterGetLanguage %d %d %zd\n", opcode, index, value, 0);
        return 0;
    case audioMasterGetDirectory:
        if (plugin == NULL) {
            if (!throttleLog)
                logPluginCb(plugin, "audioMasterGetDirectory plugin == NULL %d %d %zd\n", opcode, index, value, 0);
            return 0;
        }
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterGetDirectory %d %d %zd\n", opcode, index, value, 0);
        return (VstIntPtr)plugin->getDir();
    case audioMasterUpdateDisplay:
        if (plugin == NULL) {
            if (!throttleLog)
                logPluginCb(plugin, "audioMasterUpdateDisplay plugin == NULL %d %d %zd\n", opcode, index, value, 0);
            return 0;
        }
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterUpdateDisplay %d %d %zd\n", opcode, index, value, 0);
        if (validProcessingState) {
            //TODO: flag plugin for parameter and program name update. To be executed on the UI thread
        }
        return true;
#ifdef VST_2_1_EXTENSIONS
    case audioMasterBeginEdit:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterBeginEdit %d %d %zd %f\n", opcode, index, value, opt);
        return 1;
    case audioMasterEndEdit:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterEndEdit %d %d %zd %f\n", opcode, index, value, opt);
        return 1;
    case audioMasterOpenFileSelector:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterOpenFileSelector %d %d %zd\n", opcode, index, value, 0);
        return 0;
#endif
#ifdef VST_2_2_EXTENSIONS
    case audioMasterCloseFileSelector:
        if (!throttleLog)
            logPluginCb(plugin, "audioMasterCloseFileSelector %d %d %zd\n", opcode, index, value, 0);
        return 0;
#endif
    case audioMasterWantMidi:
        if (!throttleLog)
            logPluginCb(plugin, "depr audioMasterWantMidi %d %d %zd\n", opcode, index, value, 0);
        return 0;
    default:
        if (!throttleLog)
            logPluginCb(plugin, "unhandled %d %d %zd %f\n", opcode, index, value, opt);

    }
    return 0L;
}

VstIntPtr VSTCALLBACK audioMaster1(AEffect* effect, VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt) {
    dbgassert(g_hostslots[0].g_instance);
    dbgassert(g_hostslots[0].g_instanceImpl);
    return audioMasterHost(g_hostslots[0].g_instance, g_hostslots[0].g_instanceImpl, effect, opcode, index, value, ptr, opt);
}

VstIntPtr VSTCALLBACK audioMaster2(AEffect* effect, VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt) {
    dbgassert(g_hostslots[1].g_instance);
    dbgassert(g_hostslots[1].g_instanceImpl);
    return audioMasterHost(g_hostslots[1].g_instance, g_hostslots[1].g_instanceImpl, effect, opcode, index, value, ptr, opt);
}

VstIntPtr VSTCALLBACK audioMaster3(AEffect* effect, VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt) {
    dbgassert(g_hostslots[2].g_instance);
    dbgassert(g_hostslots[2].g_instanceImpl);
    return audioMasterHost(g_hostslots[2].g_instance, g_hostslots[2].g_instanceImpl, effect, opcode, index, value, ptr, opt);
}

VstIntPtr VSTCALLBACK audioMaster4(AEffect* effect, VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt) {
    dbgassert(g_hostslots[3].g_instance);
    dbgassert(g_hostslots[3].g_instanceImpl);
    return audioMasterHost(g_hostslots[3].g_instance, g_hostslots[3].g_instanceImpl, effect, opcode, index, value, ptr, opt);
}

static const double fSmpteDiv[] =
{
    24.f,
    25.f,
    24.f,
    30.f,
    29.97f,
    30.f
};

bool setFlag(int& _out, int flag, bool state) {
    bool curState = _out&flag;
    if (state) {
        _out |= flag;
    } else {
        _out &= ~flag;
    }
    return curState != state;
}

#ifdef _WIN32
String getModuleName(HMODULE);
#endif

class vsthost::ModuleManager {
public:
    ModuleManager() = default;

    void releaseModule(void* module) {
#ifdef _WIN32
        String moduleName = getModuleName((HMODULE)module);
        my_printf("Unload %s\n", StringAsCStr(moduleName));
        FreeLibrary((HMODULE)module);
#endif
#if defined(__linux__) || defined(__APPLE__)
        dlclose(module);
#endif
    }
};

void vsthost::getBlockThreadStats(std::vector<thread_stats_process_timings_t>& stats) {
    stats = impl->lastBlockThreadStats;
}

void vsthost::setThreadCount(uint32_t threadCount) {
    impl->threadCount = math::clamp<uint32_t>(threadCount, 1U, MAX_AUDIOPROCESSING_THREADS);
}

uint32_t vsthost::getThreadCount() {
    return impl->threadCount;
}

uint32_t vsthost::getMaxThreadCount() {
    return MAX_AUDIOPROCESSING_THREADS;
}

vsthost::vsthost()
    : impl(new vsthost_impl{}),
      numChannels(OUTPUT_CHANNELS),
      moduleMgr{new vsthost::ModuleManager{}}
{
    memset(&m_sharedTimeInfo, 0, sizeof(m_sharedTimeInfo));
    allocRingBuffer(ringbuffer, 32);
    updateTime(m_sharedTimeInfo, 0, 0.0, playback_state::status_stop);
    midiRealtimeInput = new clip_notes_t;
    midiProcessedInput = new clip_notes_t;
    registerPlugins();
}

vsthost::~vsthost() {
    delete moduleMgr;
    delete blockZero;
    delete impl;
    delete midiRealtimeInput;
    delete midiProcessedInput;
}

vsthost::audiostream_properties_t getAudioStreamPropertiesForFormat(sampleformat_t sampleFormat, sampleformat_t sampleFormatExternal, uint32_t tempo100) {
    vsthost::audiostream_properties_t prop;
    prop.microSecsPerBlock = (int64_t)sampleFormat.blockSize * 1000000L / (int64_t)sampleFormat.sampleRate;
    prop.ticksPerBlock     = sampleToTickConvert<double, roundmode::none>(sampleFormat.blockSize,
                                                                      tempo100,
                                                                      sampleFormat.sampleRate);

    prop.blockSizeResampled = DAW::NumSamplesResampled(sampleFormat.blockSize, sampleFormat.sampleRate, sampleFormatExternal.sampleRate);
    prop.numBlocksInternal  = math::max<uint32_t>(1U, sampleFormatExternal.blockSize/prop.blockSizeResampled);
    prop.numBlocksExternal  = (prop.blockSizeResampled + sampleFormatExternal.blockSize - 1)/sampleFormatExternal.blockSize;
    return prop;
}

vsthost::audiostream_properties_t vsthost::getAudioStreamProperties() const {
    return getAudioStreamPropertiesForFormat(m_sampleFormatInternal, m_sampleFormatExternal, prjGlobals.tempo100);
}

void vsthost::setSampleFormat(const sampleformat_t& _sampleFormat) {
    if (this->m_sampleFormatInternal != _sampleFormat) {
        this->m_sampleFormatInternal = _sampleFormat;
        setBlockSize(_sampleFormat.blockSize);
        for (auto* audio : this->allAudioStages) {
            audio->sampleFormat = _sampleFormat;
            audio->input.realloc(_sampleFormat.blockSize);
            audio->output.realloc(_sampleFormat.blockSize);
            audio->outputPost.realloc(_sampleFormat.blockSize);
        }
        for (effectbase* plugin : this->pluginInstances) {
            plugin->setSampleFormat(_sampleFormat);
        }
        for (effectbase* plugin : this->pluginsDeferred) {
            plugin->setSampleFormat(_sampleFormat);
        }
        for (vstplugin* plugin : this->pluginInstancesVST2) {
            plugin->sleep();
            plugin->dispatch(effSetBlockSize, 0, _sampleFormat.blockSize, 0, 0);
            plugin->dispatch(effSetSampleRate, 0, 0, NULL, (float) _sampleFormat.sampleRate);
            plugin->resume();
        }
        for (auto* stage : this->allAudioStages) {
            stage->pluginsChanged();
        }
    }
}

void vsthost::setBlockSize(uint16_t _blockSize) {
    if (!blockZero)
        blockZero = new AudioBlock(numChannels, _blockSize);
    this->blockZero->realloc(_blockSize);
}


inline double PPQ24TickToSample(double midiTickPPQ24, uint32_t bpm100, samplerate_t samplerate, uint32_t blocksize) {
    double seconds = (midiTickPPQ24/(double)(bpm100*24.0)) * 100.0 * 60.0;
    double samplePos = seconds * samplerate;
    return samplePos;
}


//\note VstTimeInfo::samplesToNextClock :
//MIDI Clock Resolution (24 per Quarter Note), can be negative the distance to the next midi clock
//        (24 ppq, pulses per quarter) in samples. unless samplePos falls precicely on a midi clock,
//        this will either be negative such that the previous MIDI clock is addressed,
//        or positive when referencing the following (future) MIDI clock.

void vsthost::updateTime(VstTimeInfo& timeinfo, int32_t samplePos, double dTickPos, playback_state state) const {
    timeinfo.samplePos = samplePos;
    timeinfo.sampleRate = (double) m_sampleFormatInternal.sampleRate;
    timeinfo.nanoSeconds = getTimeMicros() * 1000.0;
    timeinfo.ppqPos = (dTickPos/(double)TICKS_QUARTER);
    timeinfo.tempo = prjGlobals.tempo100/100.0;
    timeinfo.barStartPos = floor(dTickPos / (double) TICKS_BAR) * 4;
    timeinfo.cycleStartPos = (prjGlobals.loopStart/(double)TICKS_QUARTER);
    timeinfo.cycleEndPos = ((prjGlobals.loopStart+prjGlobals.loopLen)/(double)TICKS_QUARTER);
    timeinfo.timeSigNumerator = prjGlobals.signatureNum;
    timeinfo.timeSigDenominator = 1 << prjGlobals.signatureDenom;

    bool loopEnabed = state != playback_state::status_render && prjGlobals.loopEnabled;
    if (!loopEnabed) {
        timeinfo.cycleStartPos = 0;
        timeinfo.cycleEndPos = 0;
    }

    {
        double dPosSeconds = timeinfo.samplePos / timeinfo.sampleRate;
        /* offset in fractions of a second   */
        double dOffsetInSecond = dPosSeconds - floor(dPosSeconds);
        timeinfo.smpteFrameRate = VstSmpteFrameRate::kVstSmpte24fps;
        timeinfo.smpteOffset = (long)(dOffsetInSecond * fSmpteDiv[timeinfo.smpteFrameRate] * 80.L);
    }


    double midiTickPPQ24 = timeinfo.ppqPos*24.0;
    double samplePosMidiTick = PPQ24TickToSample(midiTickPPQ24, prjGlobals.tempo100, m_sampleFormatInternal.sampleRate, m_sampleFormatInternal.blockSize);
    double samplePosPrevMidiTick = PPQ24TickToSample(math::floord(midiTickPPQ24), prjGlobals.tempo100, m_sampleFormatInternal.sampleRate, m_sampleFormatInternal.blockSize);
    double samplePosNextMidiTick = PPQ24TickToSample(math::ceild(midiTickPPQ24), prjGlobals.tempo100, m_sampleFormatInternal.sampleRate, m_sampleFormatInternal.blockSize);

    double samplePosClosestPPQ24Tick = math::absMin(samplePosPrevMidiTick - samplePosMidiTick, samplePosNextMidiTick - samplePosMidiTick);
    //TODO: assingn nearest clock (can be negative), not next aka soonest
    timeinfo.samplesToNextClock = math::rounddS32(samplePosClosestPPQ24Tick);

    {
        bool changed = setFlag(timeinfo.flags, kVstTransportPlaying, DAW::isPlaybackState(state));
        changed |= setFlag(timeinfo.flags, kVstTransportCycleActive, loopEnabed);
        changed |= setFlag(timeinfo.flags, kVstTransportRecording, false);
        setFlag(timeinfo.flags, kVstTransportChanged, changed);
        setFlag(timeinfo.flags, kVstAutomationWriting, false);
        setFlag(timeinfo.flags, kVstAutomationReading, false);
        setFlag(timeinfo.flags, kVstNanosValid, true);
        setFlag(timeinfo.flags, kVstPpqPosValid, true);
        setFlag(timeinfo.flags, kVstTempoValid, true);
        setFlag(timeinfo.flags, kVstBarsValid, true);
        setFlag(timeinfo.flags, kVstCyclePosValid, true); //project.loopEnabled
        setFlag(timeinfo.flags, kVstTimeSigValid, true);
        setFlag(timeinfo.flags, kVstSmpteValid, true);
        setFlag(timeinfo.flags, kVstClockValid, true);
    }

}

void vsthost::sendNotesOff(effectbase* plugin) {
    //TODO: check current thread, check if playthread is locked
    if (plugin && plugin->trackImpl) {
        track_t* tr = plugin->trackImpl->getTrack();
        dbgassert(tr);
        track_impl_t* audio = tr->audio;
        if (audio) {
            audio->sendNotesOff(prjGlobals.tempo100);
        }
    }
}

std::vector<note_t> vsthost::getRealtimeNotes() {
    return this->midiRealtimeInput->m_list;
}

namespace DAW {
bool resolveEffectDefaultConnection(const vsthost* const host, const project_t* const project, const audio_stage_t* const stage, effectbase* const effect, channel_ref_t& out) {
    if (effect == nullptr) {
        if (!stage->effects.empty()) {
            out = DAW::ChannelAudioEffect(stage->effects.back(), stagebuffer_point::OUTPUT_POST);
        } else {
            out = DAW::ChannelStage(stage, stagebuffer_point::INPUT);
        }
    } else {
        int32_t effIdx = indexOfCtr(stage->effects, effect);
        dbgassert(effIdx > -1);
        if (effIdx == 0) {
            out = DAW::ChannelStage(stage, stagebuffer_point::INPUT);
        } else {
            out = DAW::ChannelAudioEffect(stage->effects[effIdx - 1u], stagebuffer_point::OUTPUT_POST);
        }

    }
    return true;
}

bool resolveDefaultConnection(const vsthost* const host, const project_t* const project, track_impl_t* const trImpl, const bool isInput, channel_ref_t& out) {
    if (!isInput && TRACKTYPE_TO_CTR(trImpl->track->type) == TRACK_CTR_MASTER) {
        int32_t idx = 0;
        auto type = AudioIO::getTrackTypeFromNumChannels(trImpl->outputPost.channels);
        String name = "External "+AudioIO::getTrackNameShort(type, idx, stagebuffer_point::OUTPUT_POST);
        out = ChannelAudioInput(idx, 0, name, type);
        return true;
    }
    const track_t* const firstMaster = !project->trackMasterCtr.empty() ? project->trackMasterCtr.front() : nullptr;
    if (!isInput && TRACKTYPE_TO_CTR(trImpl->track->type) == TRACK_CTR_RETURN) {
        if (firstMaster) {
            out = ChannelStage(firstMaster->audio, stagebuffer_point::INPUT);
            return true;
        }
    }
    if (!isInput && TRACKTYPE_TO_CTR(trImpl->track->type) == TRACK_CTR_MIDIAUDIO) {
        const track_t* const dstTrack = trImpl->track->parent ? trImpl->track->parent : firstMaster;
        if (dstTrack) {
            out = ChannelStage(dstTrack->audio, stagebuffer_point::INPUT);
            return true;
        }
    }
    return false;
}

bool resolveAudioChannel(const vsthost* const host, int32_t numChannelsTrack, const channel_ref_t& inputChannel, const AudioBlock* const ptrExternalInputs, track_audio_src& out) {
    if (inputChannel.getType() == channel_input_type::INPUT_EXTERNAL_AUDIO) {
        if (ptrExternalInputs != nullptr) {
            int32_t idx = inputChannel.inputChannelOffset;
            size_t size = math::min<uint32_t>(AudioIO::getNumChannelsFromTrackType(inputChannel.externalInputType), numChannelsTrack);
            if (idx >= 0 && idx+size <= ptrExternalInputs->channels) {
                track_audio_src src;
                for (int i = 0; i < size; i++) {
                    src.channels.push_back(ptrExternalInputs->buf[idx+i]);
                }
                src.sampleFormat = host->m_sampleFormatExternal;
                src.samples = ptrExternalInputs->samples;
                src.latency = 0;
                out = std::move(src);
                return true;
            }
        }
    }
    if (inputChannel.getType() == channel_input_type::INPUT_AUDIOSTAGE) {
        audio_stage_t* stage = host->getAudioStage(inputChannel.stage.stageRef);
        if (stage) {
            /* Calculate audio/midi tracks gain level */
            float fGainTrack = 0.0f;
            if (!dsp_util::getGainLvl(stage->mixer.getParamValue(PARAM_TRACK_GAIN), fGainTrack)) {
                fGainTrack = 0.0f;
            }
            track_audio_src src;
            auto* buff = &stage->input;
            int32_t idx = inputChannel.inputChannelOffset;
            switch (inputChannel.stage.buffer) {
            case stagebuffer_point::INPUT:
                buff = &stage->input;
                src.latency = stage->getInputLatency();
                break;
            case stagebuffer_point::OUTPUT:
                buff = &stage->output;
                src.latency = stage->getOutputLatency();
                break;
            case stagebuffer_point::OUTPUT_POST:
                buff = &stage->outputPost;
                src.latency = stage->getOutputLatency();
                break;
            }
            dbgassert(idx <= (int32_t)buff->channels);
            for (uint32_t i = 0; i < buff->channels; i++) {
                src.channels.push_back(buff->buf[i + idx]);
            }
            src.sampleFormat = stage->sampleFormat;
            src.samples = buff->samples;
            out = std::move(src);
            return true;
        }
    }

    if (inputChannel.getType() == channel_input_type::INPUT_AUDIOSTAGE_EFFECT) {
        audio_stage_t* stage = host->getAudioStage(inputChannel.stage.stageRef);
        if (stage) {
            effectbase* eff = stage->getPluginById(inputChannel.projectGlobalId);
            if (!eff  || !eff->blockOutputs) {
                return false;
            }
            track_audio_src src;
            for (uint32_t i = 0; i < eff->blockOutputs->channels; i++) {
                src.channels.push_back(eff->blockOutputs->buf[i]);
            }
            src.sampleFormat = stage->sampleFormat;
            src.samples = eff->blockOutputs->samples;
            src.latency = 0;
            out = std::move(src);
            return true;
        }
    }
    return false;
};

}

//TODO: get rid of this constant
const int32_t lenTicksInfinite = TICKS_BAR*16;

void vsthost::processMidiRealtimeInput(project_controller_t* ctrl, double posDouble, playback_state state) {
    //TODO: This needs to be done per input and per track
    tick_t tickPosBlockStart = math::ceildS32(posDouble);

    std::vector<MidiIOEvent> msgs = midihost::getInstance()->getInputMessages();
    bool notesProcessed = false;
    if (!msgs.empty()) {
        std::vector<note_t> newNotes;
        for (MidiIOEvent& msg : msgs) {

            int32_t command = MidiMsgStatus(msg.message) & MIDI_CODE_MASK;
//            int32_t chan = MidiMsgStatus(msg.message) & MIDI_CHN_MASK;
            if (command == MIDI_ON_NOTE && MidiMsgData2(msg.message) != 0) {
                note_t note;
                note.setRealtime(true);
                note.setIsHeld(true);
                note.time = tickPosBlockStart;
                note.len = lenTicksInfinite;
                note.pitch = MidiMsgData1(msg.message);
                note.velocity = MidiMsgData2(msg.message);
                newNotes.push_back(note);
            }
        }
        if (!newNotes.empty()) {
            //for (auto& note : newNotes) {
            //    log_printf("%d note TRIG %s %d\n", noteName(note.pitch), note.start());
            //}
            midiRealtimeInput->addAll(newNotes);
        }
        for (MidiIOEvent& msg : msgs) {

            int32_t command = MidiMsgStatus(msg.message) & MIDI_CODE_MASK;
            //int32_t chan = MidiMsgStatus(msg.message) & MIDI_CHN_MASK;
            if ((command == MIDI_ON_NOTE && MidiMsgData2(msg.message) == 0) || command == MIDI_OFF_NOTE) {
                int32_t pitch = MidiMsgData1(msg.message);
                int32_t tickEnd = tickPosBlockStart;
                // kill oldest (first) note
                bool fnd = false;
                for (note_t& noteHeld : midiRealtimeInput->m_list) {
                    if(noteHeld.pitch == pitch) {
                        if (!noteHeld.isHeld()) {
                            //log_printf("%s note was released before, looking for next one\n", noteName(noteHeld.pitch));
                            continue;
                        }
                        if (noteHeld.start() > tickEnd) {
                            //log_printf("%s note starts after this release (tickEnd %d, noteHeld.start() %d)\n",
                            //        noteName(noteHeld.pitch), tickEnd, noteHeld.start());
                            continue;
                        }
                        if (noteHeld.start() == tickEnd) {
                            //log_printf("%s noteHeld.start() == tickEnd %d, adding TICKS_16TH/4\n", noteName(noteHeld.pitch), tickEnd);
                            tickEnd += TICKS_16TH/4;
                        }
                        noteHeld.len = tickEnd - noteHeld.start();
                        noteHeld.setIsHeld(false);
                        assert(noteHeld.len >= 0);
                        fnd = true;
                        notesProcessed = true;
                        //log_printf("%d note KILL %s %d\n", noteName(noteHeld.pitch), noteHeld.start());
                        break;
                    }
                }
                if (!fnd) {
                    log_printf("MIDI_OFF_NOTE note not found %s tickEnd %d\n", noteName(pitch), tickEnd);
                }

            }
        }
        if (newNotes.size() || notesProcessed) {
            midiRealtimeInput->removeDuplicates();
            notesProcessed = true;
        }
    }
    if (midiRealtimeInput->m_list.size()) {
        auto it = midiRealtimeInput->m_list.begin();
        while (it != midiRealtimeInput->m_list.end()) {
            note_t& note = *it;
            if (!note.isHeld() && note.end() < posDouble) {
                notesProcessed = true;
                it = midiRealtimeInput->m_list.erase(it);
            } else {
                it++;
            }
        }
    }
    if (notesProcessed) {
        midiRealtimeInput->updateBounds();
    }
    //if (!midiRealtimeInput->m_list.empty()) {
    //    log_printf("Realtime midi notes %d\n", midiRealtimeInput->m_list.size());
    //}
}
void vsthost::processMidiProcessedOutput(playback_state state, tick_t tickBlockStart, tick_t tickBlockEnd, std::vector<noteevent_t>& noteEventsProcessed) {

    bool notesProcessed = false;
    if (!noteEventsProcessed.empty()) {
        std::vector<note_t> newNotes;
        for (noteevent_t& msg : noteEventsProcessed) {
            if (msg.isNoteOn) {
                note_t note;
                note.setRealtime(true);
                note.setIsHeld(true);
                note.time = tickBlockStart + msg.tickOffsetInBlock;
                note.len = lenTicksInfinite;
                note.pitch = msg.pitch;
                note.velocity = msg.velocity;
                newNotes.push_back(note);
            }
        }
        if (!newNotes.empty()) {
            //for (auto& note : newNotes) {
            //    log_printf("Block %d, note open %d (%s)\n", procPos, note.start(), noteName(note.pitch));
            //}
            midiProcessedInput->addAll(newNotes);
        }
        for (noteevent_t& msg : noteEventsProcessed) {
            if (!msg.isNoteOn) {
                int32_t pitch = msg.pitch;
                int32_t tickEnd = tickBlockStart + msg.tickOffsetInBlock;
                //log_printf("%s@%d Looking for NOTE_ON evt\n", noteName(pitch), tickEnd);
                bool fnd = false;
                for (note_t& noteHeld : midiProcessedInput->m_list) {
                    if(noteHeld.pitch == pitch) {
                        if (!noteHeld.isHeld()) {
                            //log_printf("%s@%d note was released before (@%d), looking for next one\n",
                            //              noteName(noteHeld.pitch), noteHeld.start(), noteHeld.end());
                            continue;
                        }
                        if (noteHeld.start() > tickEnd) {
                            //log_printf("%s@%d note starts after this release\n",
                            //        noteName(noteHeld.pitch), noteHeld.start());
                            continue;
                        }
                        if (noteHeld.start() == tickEnd) {
                            //log_printf("%s noteHeld.start() == tickEnd %d, adding TICKS_16TH/4\n", noteName(noteHeld.pitch), tickEnd);
                            tickEnd += TICKS_16TH/4;
                        }
                        noteHeld.len = tickEnd - noteHeld.start();
                        noteHeld.setIsHeld(false);
                        assert(noteHeld.len >= 0);
                        fnd = true;
                        notesProcessed = true;
                        //log_printf("Block %d, note complete %d END %d (%s)\n", procPos, noteHeld.start(), noteHeld.end(), noteName(noteHeld.pitch));
                        break;
                    }
                }
                if (!fnd) {
                    log_printf("MIDI_OFF_NOTE note not found %s tickEnd %d\n", noteName(pitch), tickEnd);
                }
            }
        }

        if (!newNotes.empty() || notesProcessed) {
            midiProcessedInput->removeDuplicates();
            notesProcessed = true;
        }
    }

    if (notesProcessed) {
        midiProcessedInput->updateBounds();
        if (state == playback_state::status_playback && prjGlobals.recordArmed) {
            updateRecordingClip(tickBlockStart, tickBlockEnd, midiProcessedInput->m_list);
        }
    }

    if (recordingClip && !(state == playback_state::status_playback && prjGlobals.recordArmed)) {
        finishRecordingClip(tickBlockStart, tickBlockEnd, midiProcessedInput->m_list);
    }

    if (!midiProcessedInput->m_list.empty()) {
        auto it = midiProcessedInput->m_list.begin();
        while (it != midiProcessedInput->m_list.end()) {
            note_t& note = *it;
            if (!note.isHeld() && note.end() < tickBlockEnd) {
                notesProcessed = true;
                it = midiProcessedInput->m_list.erase(it);
            } else {
                it++;
            }
        }
    }
}

void vsthost::updateRecordingClip(tick_t tickPosBlockStart, tick_t tickBlockEnd, std::vector<note_t>& m_list) {
    if (recordingClip == nullptr) {
        recordingClip = new clip_t;
        recordingClip->name = "Midi Input - Recorded";
        recordingClip->time = tickPosBlockStart;
        recordingClip->setLen(TICKS_QUARTER);
        recordingClip->loopStart = 0;
        recordingClip->loopLen = TICKS_BAR*4;
    }

    if (recordingClip) {
        if (recordingClip->start() > tickPosBlockStart) {
            recordingClip->time = tickPosBlockStart;
        }
        if (recordingClip->end() < tickBlockEnd) {
            recordingClip->setLen((tickBlockEnd)-recordingClip->start());
        }
        for (auto& note : m_list) {
            if (!note.isHeld()) {
                auto noteCopy = note;
                noteCopy.time -= recordingClip->start();
                noteCopy.setEnabled(true);
                noteCopy.setRealtime(false);
                recordingClip->notes.addSingle(noteCopy);
            }
        }
        clip_t* cloned = recordingClip->clone();
        cloned->setLen(tickPosBlockStart - recordingClip->time);
        cloned->loopEnabled = false;
        cloned->loopLen = ( ( math::max ( 1, cloned->getLen() / (TICKS_BAR*4) ) )   * (TICKS_BAR*4) );
        cloned->notes.updateBounds();
        cloned->setDirty();
        std::swap(recordDataProcessed, cloned);
        delete cloned;
        hasNewRecordedData = true;
    }
}

void vsthost::finishRecordingClip(tick_t tickPosBlockStart, tick_t tickBlockEnd, std::vector<note_t>& m_list) {
    for (auto& note : m_list) {
        note_t noteCopy = note;
        if (noteCopy.time < tickPosBlockStart && noteCopy.isHeld()) {
            noteCopy.len = tickPosBlockStart - noteCopy.time;
            noteCopy.setIsHeld(false);
        }
        if (noteCopy.len > 0 && !noteCopy.isHeld()) {
            noteCopy.time -= recordingClip->start();
            noteCopy.setEnabled(true);
            noteCopy.setRealtime(false);
            recordingClip->notes.addSingle(noteCopy);
        }
    }
    clip_t* cloned = recordingClip->clone();
    tick_t clipLen = tickBlockEnd - recordingClip->time;
    tick_t loopLen = ( ( math::max ( 1, clipLen / (TICKS_BAR*4) ) )   * (TICKS_BAR*4) );

    cloned->loopEnabled = false;
    cloned->setLen(clipLen);
    cloned->loopLen = loopLen;
    cloned->notes.updateBounds();
    cloned->setDirty();
    std::swap(recordDataProcessed, cloned);
    delete cloned;
    hasNewRecordedData = true;
    delete recordingClip;
    recordingClip = nullptr;
}

void vsthost::preExportBegin(project_controller_t* ctrl, export_settings_t& exportSettings) {
    for (auto* trackMaster : ctrl->getTracks().getMasterTracksFlatVecRef()) {
        trackMaster->getStage()->flags |= audiostageflags_t::WRITE_OUTPUT;
    }
}

void vsthost::postExportEnd(project_controller_t* ctrl, export_settings_t& exportSettings) {
    const tick_t tickBegin = exportSettings.exportPos;
    const tick_t tickEnd = tickBegin + exportSettings.exportLen;
    const samplerate_t sr = m_sampleFormatInternal.sampleRate;
    const int32_t tempo100 = prjGlobals.tempo100;
    const samplerate_t sampleBegin = tickToSampleConvert<samplerate_t, roundmode::floor>(tickBegin, tempo100, sr);
    const samplerate_t sampleEnd = tickToSampleConvert<samplerate_t, roundmode::ceil>(tickEnd, tempo100, sr);
    const samplerate_t numSamples = sampleEnd - sampleBegin;

    for (auto* trackMaster : ctrl->getTracks().getMasterTracksFlatVecRef()) {
        if ((trackMaster->getStage()->flags & audiostageflags_t::WRITE_OUTPUT) != audiostageflags_t::NONE) {
            writeTrackSamplesToDisk(exportSettings.exportPath, trackMaster->getStage(), sampleBegin, numSamples);
        }
        trackMaster->getStage()->flags &= ~audiostageflags_t::WRITE_OUTPUT;
    }
}

int64_t vsthost::writeTrackSamplesToDisk(String fOutWave, track_impl_t* trImpl, samplerate_t samplePos, samplerate_t numSamples) {
    if (fOutWave.empty()) {
        dbgassert(0);
        return 0;
    }
    if ((trImpl->flags & audiostageflags_t::WRITE_OUTPUT) == audiostageflags_t::NONE) {
        dbgassert(0);
        return 0;
    }
    if (trImpl->output.channels != this->numChannels || trImpl->output.channels != 2) {
        dbgassert(0);
        return 0;
    }

    log_printf("writeTrackSamplesToDisk %s pos %d len %d\n", StringAsCStr(trImpl->getTrack()->name), samplePos, numSamples);
    const int64_t SPLIT_SAMPLECOUNT = audiotrack_t::GetSplitSampleLength();
    const int64_t samplePosEnd = samplePos + numSamples;
    const int64_t numChannels = trImpl->output.channels;

    trImpl->audioOutput.convertToSamples(this);

    std::vector<audiotrack_split_t*> samples;
    trImpl->audioOutput.visitSamples_NoLock([&samples, SPLIT_SAMPLECOUNT, samplePos, samplePosEnd](std::shared_ptr<audiotrack_split_t>& split) {
        auto* ptrSplit = split.get();
        if (ptrSplit && ptrSplit->samplePos + SPLIT_SAMPLECOUNT >= (int64_t)samplePos && ptrSplit->samplePos < samplePosEnd) {
            samples.push_back(ptrSplit);
        }
    });

    if (samples.empty()) {
        //unexpected
        dbgassert(0);
        return 0;
    }

    std::sort(samples.begin(), samples.end(), [](audiotrack_split_t* lhs, audiotrack_split_t* rhs) {
        return lhs->samplePos < rhs->samplePos;
    });

    drwav_data_format format;
    format.container = drwav_container_riff;    // drwav_container_riff = normal WAV files, drwav_container_w64 = Sony Wave64.
    format.format = DR_WAVE_FORMAT_IEEE_FLOAT;  // Any of the DR_WAVE_FORMAT_* codes.
    format.channels = numChannels;
    format.sampleRate = trImpl->sampleFormat.sampleRate;
    format.bitsPerSample = 32;

    //String nameWaveFileTrack = fOutWave+"_"+std::to_string(trackIndex)+"_"+trackMaster->name+"_f32.wav";
    String nameWaveFileTrack = fOutWave;
    drwav* pWav = drwav_open_file_write(StringAsCStr(nameWaveFileTrack), &format);


    AudioBlock blockFull(1, SPLIT_SAMPLECOUNT*numChannels);
    int64_t samplesWritten = 0;
    int64_t samplesWritten2 = 0;
    int64_t sampleIdx = 0;
    for (audiotrack_split_t* split : samples) {
        auto* sample = split->getSample();
        dbgassert(split->samplePos + SPLIT_SAMPLECOUNT >= samplePos && split->samplePos < samplePosEnd);

        const size_t readBeginOffset = math::clamp<int64_t>((int64_t)samplePos - split->samplePos, 0, SPLIT_SAMPLECOUNT);
        const size_t readEndOffset = math::clamp<int64_t>((int64_t)samplePosEnd - split->samplePos, 0, SPLIT_SAMPLECOUNT);
        const size_t readLen = math::clamp<int64_t>(readEndOffset - readBeginOffset, 0, SPLIT_SAMPLECOUNT);

        dbgassert(sample->nChannels == numChannels);
        dbgassert(sample->nChannels == sample->samples.size());
        dbgassert((int64_t) sample->nSamples == SPLIT_SAMPLECOUNT);
        dbgassert(sample->nSamples == static_cast<int64_t>(sample->samples[0].size()));
        dbgassert(sample->nSamples == static_cast<int64_t>(sample->samples[1].size()));
        dbgassert(blockFull.samples == sample->nSamples*sample->nChannels);

        if (sample->samples.size() >= 2) {
            float* in1 = sample->samples[0].data() + readBeginOffset;
            float* in2 = sample->samples[1].data() + readBeginOffset;
            float* out0 = blockFull.buf[0];
            for (size_t nSample = 0; nSample < readLen; nSample++) {
                *out0++ = *in1++;
                *out0++ = *in2++;
            }
            samplesWritten2 += readLen;
            samplesWritten += drwav_write(pWav, readLen * numChannels, blockFull.buf[0]);
        }
        sampleIdx++;
    }

    log_printf("wrote %lld samples to %s\n", samplesWritten, StringAsCStr(nameWaveFileTrack));
    log_printf("processed %lld splits and %lld samples\n", samples.size(), samplesWritten2);
    drwav_close(pWav);

    return samplesWritten;
}

static int32_t dbgStep = 1;

int32_t vsthost::processRender(project_controller_t* ctrl, int32_t sample, double posDouble) {
    dbgassert(ctrl);
    dbgassert(m_sampleFormatInternal.blockSize > 0);
    dbgassert(m_sampleFormatInternal.sampleRate > 0);
    const bool enableProfiling = (dbgStep%333) != 0;

    project_t* const project = ctrl->getProject();

    auto timeNow_i64 = getTimeMicros();
    if (0 != stats.lastInvocationTime_i64 && enableProfiling) {
        auto timeDelta = timeNow_i64 - stats.lastInvocationTime_i64;
        stats.timings["Block.timeDelta"] = timeDelta;
    }
    stats.lastInvocationTime_i64 = timeNow_i64;


    timerBlock.reset();
    const sampleformat_t& sampleFormat = this->m_sampleFormatInternal;
    const audiostream_properties_t audioProp = getAudioStreamProperties();

    int32_t nBlocksProcessed = 0;

    const playback_state state = playback_state::status_render;

    updateTime(m_sharedTimeInfo, sample, posDouble, state);


    /*
     * Process audio/midi tracks
     */
    auto tracksFlatAll = project->trackList.getAllTracksFlatVec(); //TODO: get rid of copy
    /**
     * process in reverse order: first children, then parents
     */

    if (enableProfiling) {
        timerProfile.reset();
    }

    //TODO: move outside
    /** turn tree structure into linear pointer array with parents followed by their children **/
    std::shared_ptr<DAW::processing_graph_t> processingGraph;
    if (!DAW::buildProcessingGraph(this, project, tracksFlatAll, processingGraph)) {
        log_printf("Failed building track graph\n", 0);
    }

    if (enableProfiling) {
        stats.timings["Block.GraphBuild"] = timerProfile.getTimeReset();
    }

    this->lastTrackGraph = processingGraph->trackGraph;
    this->lastProcessingList= processingGraph;


    int32_t samplePosProcess = sample;
    double tickPosProcess = posDouble;
    AudioBlock blockExtIn(32, sampleFormat.blockSize);
    AudioBlock blockExtOut(32, sampleFormat.blockSize);
    dsp_util::fillBlock(blockExtOut, 0.0f);

    if (enableProfiling) {
        timerProfile.reset();
    }

    nBlocksProcessed += processBlock(ctrl, audioProp, processingGraph.get(), &blockExtIn, &blockExtOut, samplePosProcess, tickPosProcess, state, false, false);
    dbgassert(nBlocksProcessed >= 1);

    if (enableProfiling) {
        stats.timings["Block.Tracks"] = timerProfile.getTimeReset();
    }

    for (auto itAudioStage = processingGraph->nodesFlatOrdered.begin(); itAudioStage != processingGraph->nodesFlatOrdered.end(); itAudioStage++) {
        const DAW::processing_track_node_t* ptrProcessingNode = *itAudioStage;
        const DAW::processing_track_node_t& trackNode = *ptrProcessingNode;
        track_t* const track = trackNode.trackOptional;
        track_impl_t* const trackImpl = track->audio;
        if (trackImpl->mixer.isEnabled()) {
            auto tracDst = trackImpl->outputChannel;
            if (tracDst.type == DAW::channel_input_type::INPUT_DEFAULT) {
                DAW::channel_ref_t tmp;
                if (DAW::resolveDefaultConnection(this, project, trackImpl, false, tmp)) {
                    tracDst = tmp;
                }
            }
            if (DAW::isChannelConnected(tracDst) && tracDst.getType() == DAW::channel_input_type::INPUT_EXTERNAL_AUDIO) {

                // TODO: latency compensate (add external output nodes to graph)
                //int offset = tracDst.inputChannelOffset;

                /* Calculate master tracks gain level */
                float fGainMaster;
                if (dsp_util::getGainLvl(trackImpl->mixer.getParamValue(PARAM_TRACK_GAIN), fGainMaster)) {
                }
                int routedOutputChannelCount = AudioIO::getNumChannelsFromTrackType(tracDst.externalInputType);
                auto trackSubChannelOutput = trackImpl->output.SubChannelsBlock(0, routedOutputChannelCount);
                blockExtOut.SubChannelsBlock(tracDst.inputChannelOffset, numChannels)
                           .addFromOp(&trackSubChannelOutput, AudioBlock::mix_op::ADD, dsp_util::clampReadGain(fGainMaster));

            }
        }
    }
    // blockExtOut now holds master channels outputs

    if (enableProfiling) {
        stats.timings["Block.TrackOutputRouting"] = timerProfile.getTimeReset();
    }

    if (nBlocksProcessed && enableProfiling) {
        dbgassert(nBlocksProcessed >= 1);
        int64_t blockTimeTaken = timerBlock.getTime() / nBlocksProcessed;
        auto curTimeProcess = stats.timeBlock;
        curTimeProcess -= curTimeProcess/NUM_BINS_STATS;
        curTimeProcess += blockTimeTaken/NUM_BINS_STATS;
        stats.timeBlock = curTimeProcess;
        stats.timeBlockRaw = blockTimeTaken;
    }

#if 1
    if (enableProfiling) timerProfile.reset();
    /* Update all track meters */
    for (track_t* track : project->trackList) {
        track_impl_t* trAudio = track->audio;
        if (!trAudio)
            continue;
        float fGainTrack;
        dsp_util::getGainLvl(trAudio->mixer.getParamValue(PARAM_TRACK_GAIN), fGainTrack);
        trAudio->meter.update(&trAudio->output, fGainTrack);
        trAudio->meterInput.update(&trAudio->input, 1.0f);
    }
    if (enableProfiling) {
        stats.timings["Block.UpdateMeters"] = timerProfile.getTimeReset();
    }
#endif
#ifndef NDEBUG
    lastTickEndPos = posDouble + audioProp.ticksPerBlock*nBlocksProcessed;
#endif
#if 1
    double tmSinceStageTick = timerAudioTick.getTimeDoubleReset();
    for (track_t* tr : project->trackList) {
        track_impl_t* trAudio = tr->audio;
        if (trAudio) {
            trAudio->onTick(tmSinceStageTick);
        }
    }
    if (enableProfiling) {
        stats.timings["Block.Tracks.Tick"] = timerProfile.getTimeReset();
    }
#endif
    if (!bypassSampleConversion) {
        int32_t bytesCopied = 0;
        hires_timer_t timerConvert;
        for (track_t* tr : project->trackList) {
            track_impl_t* trAudio = tr->audio;
            if (static_cast<bool>(trAudio->flags & audiostageflags_t::CONVERT_OUTPUT)) {
                bytesCopied += trAudio->audioOutput.convertToSamples(this);
            }
        }
        if (enableProfiling) {
            stats.timings["Block.BufferedAudioConversion"] = timerProfile.getTimeReset();
            stats.timings["Block.BufferedAudioBytesCopied"] = bytesCopied;
        }
    }
    dbgStep++;

    if (nBlocksProcessed) {
        stats.blocksProcessed += nBlocksProcessed;
        stats.samplesProcessed += nBlocksProcessed*sampleFormat.blockSize;

        int32_t tickQuarterStart = static_cast<int32_t>(math::floord((posDouble) / (float) TICKS_QUARTER));
        int32_t tickQuarterEnd = static_cast<int32_t>(math::floord((posDouble + audioProp.ticksPerBlock) / (float) TICKS_QUARTER));
        if (tickQuarterEnd > tickQuarterStart) {
            stats.tickBar += tickQuarterStart - tickQuarterEnd;
        }

        if (enableProfiling) {
            stats.timings["Constants.microSecsPerBlock"] = audioProp.microSecsPerBlock;
            stats.timings["Constants.ticksPerBlock"] = audioProp.ticksPerBlock;
            stats.timings["Constants.blockSizeResampled"] = audioProp.blockSizeResampled;
            stats.timings["Constants.numBlocksExternal"] = audioProp.numBlocksExternal;
            stats.timings["Constants.numBlocksInternal"] = audioProp.numBlocksInternal;
            stats.usage = stats.timeBlock / (float) audioProp.microSecsPerBlock;
            stats.usageRaw = stats.timeBlockRaw / (float) audioProp.microSecsPerBlock;
        }
    }
    return nBlocksProcessed;
}

int32_t vsthost::processPlayback(project_controller_t* ctrl, int32_t sample, double posDouble, playback_state state, bool inLoop, bool isLoopAround) {
    dbgassert(ctrl);
    dbgassert(m_sampleFormatInternal.blockSize > 0);
    dbgassert(m_sampleFormatInternal.sampleRate > 0);
    const bool enableProfiling = (dbgStep%333) != 0;

    project_t* project = ctrl->getProject();

    auto timeNow_i64 = getTimeMicros();
    if (0 != stats.lastInvocationTime_i64 && enableProfiling) {
        auto timeDelta = timeNow_i64 - stats.lastInvocationTime_i64;
        stats.timings["Block.timeDelta"] = timeDelta;
    }
    stats.lastInvocationTime_i64 = timeNow_i64;


    timerBlock.reset();
    const sampleformat_t& sampleFormat = this->m_sampleFormatInternal;
    const audiostream_properties_t audioProp = getAudioStreamProperties();

    std::shared_ptr<resampler_t> resamplerOutput = impl->getResampler(sampleFormat, m_sampleFormatExternal, 0);
    std::shared_ptr<resampler_t> resamplerInput = impl->getResampler(m_sampleFormatExternal, sampleFormat, 1);

    int queueSizeInput = 0;
    int queueSizeOutput = 0;
    auto *stream = audioHost ? audioHost->getStream(0) : nullptr;
    if (stream) {
        queueSizeInput = stream->getInputQueueSize();
        queueSizeOutput = stream->getOutputQueueSize();
    }
    stats.inputQueueLen = queueSizeInput;
    stats.outputQueueLen = queueSizeOutput;

    stats.resamplerInNumBlocks = resamplerInput->numBlocksToPop();
    stats.resamplerInNumSamples = resamplerInput->getNumSamplesOutputBuffer();
    stats.resamplerOutNumBlocks = resamplerOutput->numBlocksToPop();
    stats.resamplerOutNumSamples = resamplerOutput->getNumSamplesOutputBuffer();


    while (queueSizeInput) {
        AudioBuffer* ptrExternalInputs = nullptr;
        if (stream->try_dequeueInput(ptrExternalInputs)) {
            if (enableProfiling) timerProfile.reset();
            resamplerInput->push(*ptrExternalInputs->output);
            if (enableProfiling) stats.timings["Block.ResampleInput"] = timerProfile.getTime();
            //if (queueSizeOutput < 4 && resamplerInput->numBlocksToPop() <= 2) {
            //    //log_printf("enqueue fake input to get ahead\n", 0);
            //    //resamplerInput->push(*ptrExternalInputs->output);
            //} else {
            //
            //    log_printf("enough input for processing: queueSizeOutput %d, blocksToPop %d\n", queueSizeOutput, resamplerInput->numBlocksToPop());
            //}
            ptrExternalInputs->inUse = false;
        }
        queueSizeInput--;
    }


    /*
     * Start processing when the output ring buffer is less than half filled.
     * We also have to wait for the input resampler to have enough data to start processing.
     */
    const bool canProcess = audioHost && queueSizeOutput < RING_BUF_SIZE / 2 && resamplerInput->numBlocksToPop() >= audioProp.numBlocksInternal;

    if (enableProfiling) {
        timerProfile.reset();
        dbgassert(validateIds());
        stats.timings["Block.ValidateIds"] = timerProfile.getTimeReset();
    }

    int32_t nBlocksProcessed = 0;

    if (canProcess) {
        updateTime(m_sharedTimeInfo, sample, posDouble, state);
        processMidiRealtimeInput(ctrl, posDouble, state);
        if (enableProfiling) {
            stats.timings["Block.MidiRealtimeInput"] = timerProfile.getTime();
        }

        /*
         * Process audio/midi tracks
         */
        auto tracksFlatAll = project->trackList.getAllTracksFlatVec(); //TODO: get rid of copy
        /**
         * process in reverse order: first children, then parents
         */

        if (enableProfiling) {
            timerProfile.reset();
        }
        /** turn tree structure into linear pointer array with parents followed by their children **/
        std::shared_ptr<DAW::processing_graph_t> processingGraph;
        if (!DAW::buildProcessingGraph(this, project, tracksFlatAll, processingGraph)) {
            log_printf("Failed building track graph\n", 0);
        }
        if (enableProfiling) {
            stats.timings["Block.GraphBuild"] = timerProfile.getTimeReset();
        }

        this->lastTrackGraph = processingGraph->trackGraph;
        this->lastProcessingList= processingGraph;
        int64_t timeRouting = 0;
        int64_t timeProcessing = 0;
        int64_t timeResampleOutput = 0;


        for (uint32_t i = 0; i < audioProp.numBlocksInternal; i++) {
            int32_t samplePosProcess = sample + sampleFormat.blockSize*i;
            double tickPosProcess = posDouble + audioProp.ticksPerBlock*i;
            int32_t pre = resamplerInput->numBlocksToPop();
            AudioBlock block = resamplerInput->pop();
            int32_t post = resamplerInput->numBlocksToPop();
            dbgassert(post == pre-1);
            AudioBlock blockExtOut(resamplerOutput->numChannels, sampleFormat.blockSize);
            dsp_util::fillBlock(blockExtOut, 0.0f);
            if (enableProfiling) {
                timerProfile.reset();
            }
            nBlocksProcessed += processBlock(ctrl, audioProp, processingGraph.get(), &block, &blockExtOut, samplePosProcess, tickPosProcess, state, inLoop, isLoopAround);

            if (enableProfiling) {
                timeProcessing += timerProfile.getTimeReset();
            }
            for (auto itAudioStage = processingGraph->nodesFlatOrdered.begin(); itAudioStage != processingGraph->nodesFlatOrdered.end(); itAudioStage++) {
                const DAW::processing_track_node_t* ptrProcessingNode = *itAudioStage;
                const DAW::processing_track_node_t& trackNode = *ptrProcessingNode;
                track_t* const track = trackNode.trackOptional;
                track_impl_t* const trackImpl = track->audio;
                if (trackImpl->mixer.isEnabled()) {
                    auto tracDst = trackImpl->outputChannel;
                    if (tracDst.type == DAW::channel_input_type::INPUT_DEFAULT) {
                        DAW::channel_ref_t tmp;
                        if (DAW::resolveDefaultConnection(this, project, trackImpl, false, tmp)) {
                            tracDst = tmp;
                        }
                    }
                    if (DAW::isChannelConnected(tracDst) && tracDst.getType() == DAW::channel_input_type::INPUT_EXTERNAL_AUDIO) {
                        
                        // TODO: latency compensate (add external output nodes to graph)

                        /* Calculate master tracks gain level */
                        float fGainMaster;
                        if (dsp_util::getGainLvl(trackImpl->mixer.getParamValue(PARAM_TRACK_GAIN), fGainMaster)) {
                        }
                        int routedOutputChannelCount = AudioIO::getNumChannelsFromTrackType(tracDst.externalInputType);
                        auto trackSubChannelOutput = trackImpl->output.SubChannelsBlock(0, routedOutputChannelCount);
                        blockExtOut.SubChannelsBlock(tracDst.inputChannelOffset, routedOutputChannelCount).addFromOp(&trackSubChannelOutput, AudioBlock::mix_op::ADD, dsp_util::clampReadGain(fGainMaster));

                    }
                }
            }
            if (enableProfiling) {
                timeRouting += timerProfile.getTimeReset();
            }
            resamplerOutput->push(blockExtOut);
            if (enableProfiling) {
                timeResampleOutput += timerProfile.getTimeReset();
            }
        }
        if (enableProfiling) {
            stats.timings["Block.Tracks"] = timeProcessing/audioProp.numBlocksInternal;
            stats.timings["Block.TrackOutputRouting"] = timeRouting/audioProp.numBlocksInternal;
            stats.timings["Block.ResampleOutput"] = timeResampleOutput/audioProp.numBlocksInternal;
        }
    }


    if (nBlocksProcessed && enableProfiling) {
        int64_t blockTimeTaken = timerBlock.getTime() / nBlocksProcessed;
        auto curTimeProcess = stats.timeBlock;
        curTimeProcess -= curTimeProcess/NUM_BINS_STATS;
        curTimeProcess += blockTimeTaken/NUM_BINS_STATS;
        stats.timeBlock = curTimeProcess;
        stats.timeBlockRaw = blockTimeTaken;
    }

    /*
     * Start draining the resampler until the output ring buffer is 2/3
     * This will ensure that draining happens more frequently than production of blocks.
     * The implications of this have to be analyzed in detail as soon as we are trying
     * to achieve minimal input-to-output latency for live/recording/monitoring scenarios
     */

    if (enableProfiling) timerProfile.reset();
    int32_t nResampledOutputBlocks = resamplerOutput->numBlocksToPop();
    if (nResampledOutputBlocks > 0 && stream->getOutputQueueSize() < RING_BUF_SIZE*2/3) {
        int32_t& writePos = ringbuffer.writePos;
        //TODO: this is incorrect, the resampler should keep track of sample/tick position, but right now these fields are not read on output side
        double blockPosSample = sample;
        double blockPosTick = posDouble;
        int64_t time0 = 0;
        int64_t time1 = 0;
        int64_t time2 = 0;
        while (nResampledOutputBlocks > 0 && stream->getOutputQueueSize() < RING_BUF_SIZE*2/3) {
            if (enableProfiling) timerBlock.reset();
            AudioBlock block = resamplerOutput->pop();
            if (enableProfiling) time0 += timerBlock.getTimeReset();
            AudioBuffer** buffers = ringbuffer.buffers;
            AudioBuffer* const ptrExternalOutputs = buffers[writePos%RING_BUF_SIZE];
            dbgassert(!ptrExternalOutputs->inUse);
            ptrExternalOutputs->submitted = false;
            ptrExternalOutputs->output->realloc(m_sampleFormatExternal.blockSize);
            ptrExternalOutputs->output->copyFrom(&block);
            if (enableProfiling) time1 += timerBlock.getTimeReset();
            ptrExternalOutputs->inUse = true;
            ptrExternalOutputs->submitted = true;
            ptrExternalOutputs->blockPosSample = blockPosSample;
            ptrExternalOutputs->blockPosTick = blockPosTick;
            writePos = (writePos+1) & RING_BUF_MASK;
            stream->enqueue(ptrExternalOutputs);
            nResampledOutputBlocks--;
            if (enableProfiling) time2 += timerBlock.getTimeReset();
        }
        if (enableProfiling) {
            stats.timings["Block.EnqueueOutput.0"] = time0;
            stats.timings["Block.EnqueueOutput.1"] = time1;
            stats.timings["Block.EnqueueOutput.2"] = time2;
        }
    }
    if (enableProfiling) {
        stats.timings["Block.EnqueueOutput"] = timerProfile.getTime();
    }


    if (nBlocksProcessed) {
        if (enableProfiling) timerProfile.reset();
        /* Update all track meters */
        for (track_t* track : project->trackList) {
            track_impl_t* trAudio = track->audio;
            if (!trAudio)
                continue;
            //TODO: do meter updates on the worker threads. (be aware of unconnected tracks not getting processed)
            float fGainTrack;
            dsp_util::getGainLvl(trAudio->mixer.getParamValue(PARAM_TRACK_GAIN), fGainTrack);
            trAudio->meter.update(&trAudio->output, fGainTrack);
            trAudio->meterInput.update(&trAudio->input, 1.0f);
        }
        if (enableProfiling) {
            stats.timings["Block.UpdateMeters"] = timerProfile.getTimeReset();
        }
#ifndef NDEBUG
        lastTickEndPos = posDouble + audioProp.ticksPerBlock*nBlocksProcessed;
#endif
        double tmSinceStageTick = timerAudioTick.getTimeDoubleReset();
        for (track_t* tr : project->trackList) {
            track_impl_t* trAudio = tr->audio;
            if (trAudio) {
                trAudio->onTick(tmSinceStageTick);
            }
        }
        if (enableProfiling) {
            stats.timings["Block.Tracks.Tick"] = timerProfile.getTimeReset();
        }
        if (!bypassSampleConversion) {
            int64_t bytesCopied = 0;
            for (track_t* tr : project->trackList) {
                track_impl_t* trAudio = tr->audio;
                if (static_cast<bool>(trAudio->flags & audiostageflags_t::CONVERT_OUTPUT)) {
                    bytesCopied += trAudio->audioOutput.convertToSamples(this);
                }
            }
            if (enableProfiling) {
                stats.timings["Block.BufferedAudioConversion"] = timerProfile.getTimeReset();
                stats.timings["Block.BufferedAudioBytesCopied"] = bytesCopied;
            }
        }

        dbgStep++;
    }
    if (nBlocksProcessed) {

        stats.blocksProcessed += nBlocksProcessed;
        stats.samplesProcessed += nBlocksProcessed*sampleFormat.blockSize;
        int32_t tickQuarterStart = static_cast<int32_t>(math::floord((posDouble) / (float) TICKS_QUARTER));
        int32_t tickQuarterEnd = static_cast<int32_t>(math::floord((posDouble+audioProp.ticksPerBlock) / (float) TICKS_QUARTER));
        if (tickQuarterEnd > tickQuarterStart) {
            stats.tickBar += tickQuarterStart - tickQuarterEnd;
        }
        if (enableProfiling) {
            // I expect default. In this version FP modes are not modified on any audio thread
            RegisterStatus_SSE_CS sseStatus = getSSEControlStatusRegister();
            stats.timings["SSE"] = sseStatus.registerBits;
            stats.timings["SSE.FlushZeroMode"] = sseStatus.regFlushZeroMode;
            stats.timings["SSE.DenormalsAreZero"] = sseStatus.regDenormalsAreZero;
            stats.timings["SSE.RoundingMode"] = sseStatus.regRoundingMode;
            stats.timings["Constants.microSecsPerBlock"] = audioProp.microSecsPerBlock;
            stats.timings["Constants.ticksPerBlock"] = audioProp.ticksPerBlock;
            stats.timings["Constants.blockSizeResampled"] = audioProp.blockSizeResampled;
            stats.timings["Constants.numBlocksExternal"] = audioProp.numBlocksExternal;
            stats.timings["Constants.numBlocksInternal"] = audioProp.numBlocksInternal;
            stats.usage = stats.timeBlock / (float) audioProp.microSecsPerBlock;
            stats.usageRaw = stats.timeBlockRaw / (float) audioProp.microSecsPerBlock;
        }
    }
    return nBlocksProcessed;
}

int32_t vsthost::processBlockTrack(process_scratch_buf_t& tmp, track_block_processing_task_t& req) /*const*/ {
    const sampleformat_t& sampleFormat = this->m_sampleFormatInternal;
    const double ticksPerBlock = req.audioProp.ticksPerBlock;

    const int32_t samplePosProcess = req.samplePosProcess;
    const DAW::processing_track_node_t& trackNode = *req.trackNode;
    track_t* const track = trackNode.trackOptional;
    track_impl_t* const trackImpl = track->audio;
    const auto playbackState = req.playbackState;
    const double ticksLatency = sampleToTickConvert<double, roundmode::none>(trackNode.inputLatency, prjGlobals.tempo100, sampleFormat.sampleRate);
    const double sampleLatencyCompensated = samplePosProcess - trackNode.inputLatency;
    const double tickLatencyCompensated = req.tickPosProcess - ticksLatency;
    tick_t processingPos = floor(tickLatencyCompensated);
    int32_t tickBlockEnd = floor(tickLatencyCompensated + ticksPerBlock);

    tick_t loopCutStart = -1;
    tick_t loopCutEnd = -1;
    tick_t cursorPos = prjGlobals.cursor.cursorPos;
    if (req.inLoop) {
        loopCutStart = prjGlobals.loopStart;
        loopCutEnd = prjGlobals.loopStart+prjGlobals.loopLen;
    }


    tmp.timer.reset();

    /**
     * Update the per-vst timeinfo structure
     * This applies per-track latency compensation to the time structure.
     *
     * Calling the copy constructor on VstTimeInfo is not thread safe.
     * Plugins that request the time structure in the GUI or other thread
     * may see invalid data.
     * A lock against the vst-master callback should be considered.
     *
     * TODO: Apply latency compensation at plugin level.
     *       This has to be done inside processAudio.
     *
     */
    updateTime(tmp.timeinfo, sampleLatencyCompensated, tickLatencyCompensated, playbackState);
    for (effectbase* plugin : trackImpl->effects) {
        if (plugin->pluginType == PLUGIN_TYPE_VST) {
            auto* ptr = dynamic_cast<vstplugin*>(plugin)->getLocalTimeInfoPtr();
            if (ptr) {
                *ptr = tmp.timeinfo;
            }
        }
    }

    /**
     * Read and apply automation.
     */
    if (DAW::isPlaybackState(playbackState)) {
        std::vector<automatable_t*> targets;
        trackImpl->getAutomatableTrackTargets(targets);
        for (automatable_t* at : targets) {
            at->updateAutomatedParameters(processingPos);
        }
    }

    track->getStage()->procStats.timeTrackApplyAutomation = tmp.timer.getTime();
    dbgassert(tickBlockEnd-processingPos < math::ceildS32(ticksPerBlock+1));
//            if (dbg == 0) {
//                log_printf("process track %s\n", StringAsCStr(track->name));
//                log_printf("process stage 1 %d\n", static_cast<int32_t>(track->audio->stageId));
//                log_printf("process stage 2 %d\n", static_cast<int32_t>(trackNode.stageId));
//            }

    trackImpl->input.realloc(sampleFormat.blockSize);
    trackImpl->output.realloc(sampleFormat.blockSize);

    dsp_util::fillBlock(trackImpl->input, 0.0f);

    int32_t midiProcessFlags = 0;
    switch (playbackState) {
        case playback_state::status_playback:
            midiProcessFlags = MidiFlags::PROCESS_REALTIME|MidiFlags::PROCESS_CLIPS|MidiFlags::PROCESS_ARP;
            break;
        case playback_state::status_render:
            midiProcessFlags = MidiFlags::PROCESS_CLIPS|MidiFlags::PROCESS_ARP;
            break;
        case playback_state::status_stop:
            midiProcessFlags = MidiFlags::PROCESS_REALTIME|MidiFlags::PROCESS_ARP;
            break;
        default:
        case playback_state::status_no_process:
            midiProcessFlags = 0;
            break;
    }
    if (track->type != TRACK_TYPE_MIDI) {
        midiProcessFlags &= ~MidiFlags::PROCESS_ARP;
    }

    tmp.timer.reset();
    trackImpl->sendNotes(playbackState, midiProcessFlags, cursorPos, processingPos, tickBlockEnd, loopCutStart, loopCutEnd, prjGlobals.tempo100, sampleLatencyCompensated, *midiRealtimeInput);
//    if ((trackImpl->flags & audiostageflags_t::RECORD_PROCESSED_MIDI) != audiostageflags_t::NONE) {
    if (isSet(trackImpl->flags, audiostageflags_t::RECORD_PROCESSED_MIDI)) {
        processMidiProcessedOutput(playbackState, processingPos, tickBlockEnd, trackImpl->noteEventsProcessed);
    }

    track->getStage()->procStats.timeTrackProcessMidi = tmp.timer.getTime();

    if (DAW::isPlaybackState(playbackState)) {
        trackImpl->fillAudio(processingPos, tickBlockEnd, loopCutStart, loopCutEnd, prjGlobals.tempo100, sampleLatencyCompensated, trackImpl->input.buf, (int32_t)sampleFormat.blockSize);
    }

    const uint32_t numChannelsTrack = trackImpl->input.channels;

    std::vector<DAW::track_source_t> allSources = trackNode.pulls; // copy
    allSources.insert(allSources.end(), trackNode.pushs.begin(), trackNode.pushs.end()); // copy

#if 1
    struct Func_CheckHasSolo {
        bool operator()(const DAW::track_source_t& src) const {
            return (src.flags & (audiostageflags_t::SOLO|audiostageflags_t::SOLO_PARENT)) != audiostageflags_t::NONE;
        }
    };
    Func_CheckHasSolo funcCheckSolo;
    bool hasSolo = std::any_of(allSources.cbegin(), allSources.cend(), funcCheckSolo);

    tmp.timer.reset();
    for (const DAW::track_source_t& tracksrc : allSources)
    {
        if (hasSolo && !funcCheckSolo(tracksrc))
            continue;
        if (DAW::isChannelConnected(tracksrc.channel)) {
            track_audio_src src;
//                    if (dbg == 0) {
//                         log_printf("track %s has input %s\n", StringAsCStr(track->name), StringAsCStr(tracksrc.channel.name));
//                    }

            if (DAW::resolveAudioChannel(this, numChannelsTrack, tracksrc.channel, req.ptrExternalInputs, src)) {
                /**
                 * Mix routed tracks
                 *
                 * Mix level is fGainInput * src.gain * tracksrc.gain
                 * src.gain:            block-wise automated track gain
                 * tracksrc.gain:        block-wise automated send level, 1.0f for non-sends
                 *
                 * sends are with track gain applied (post-mixer)
                 *
                 */
                /* compensate at input stage */
                /* figure out max latency of all inputs */
                /* delay signal by max_child_input_latency - src_output_latency */
                /* Compensate audio midi track to pre-return latency */
                dbgassert(trackNode.inputLatency >= tracksrc.latency);
                samplerate_t delayToMaxInputLatency = trackNode.inputLatency - tracksrc.latency;

                AudioBlock& tempBlock = tmp.tempBlock;

                AudioBlock srcBlock = src.toAudioBlock();
                DelayLine* delayLine = impl->getDelayLine(tracksrc.trackEdgeId, srcBlock.channels);
                tempBlock.realloc(srcBlock.samples);
                dbgassert(srcBlock.samples == tempBlock.samples);
                dbgassert(srcBlock.channels <= tempBlock.channels);
                dbgassert((delayLine->block.channels == srcBlock.channels) || (2 == delayLine->block.channels && 1 == srcBlock.channels));

                // TODO: one of the delay lines will always be 0 samples delay
                delayAudio(delayLine, &srcBlock, &tempBlock, delayToMaxInputLatency);
                float fGainRaw = 0.0f;

                //TODO: apply per-track latency compensation
                //TODO: apply filtered sample accurate volume automation
                bool bSuccess = DAW::resolveAutomationAtTime(this, tracksrc.gainAutomation, processingPos, &fGainRaw);
                if (bSuccess) {
                    /* Calculate audio/midi tracks gain level */
                    float fGainTrack;
                    if (dsp_util::getGainLvl(fGainRaw, fGainTrack)) {
                        trackImpl->addAudio(tempBlock, fGainTrack);
                    }
                }
            }
        } else {

            if (req.debugLogProcessing) {
                log_printf("track %s has no connected input %s\n", StringAsCStr(trackImpl->inputChannel.name));
            }
        }
    }
    track->getStage()->procStats.timeTrackMixInputs = tmp.timer.getTime();
#endif

    dbgassert(
            vsthost::getInstance()->m_sampleFormatInternal == trackImpl->sampleFormat
            && trackImpl->input.samples == trackImpl->sampleFormat.blockSize
            && trackImpl->output.samples == trackImpl->sampleFormat.blockSize
            && trackImpl->outputPost.samples == trackImpl->sampleFormat.blockSize
            && trackImpl->sampleFormat.blockSize > 0
            && trackImpl->sampleFormat.sampleRate > 0);
    {
        std::shared_ptr<DAW::effect_processing_graph_t> effProcessingGraph;
        {
            if (!DAW::buildEffectProcessingGraph(this, nullptr, trackImpl, effProcessingGraph)) {
                log_printf("Failed building effect graph\n", 0);
            }
            req.effectProcessingGraph = effProcessingGraph;
        }
        /* Processes audio/midi tracks plugin chain */
        processAudio(trackImpl, &trackImpl->input, &trackImpl->output, tickLatencyCompensated, sampleLatencyCompensated, (int32_t)sampleFormat.blockSize, playbackState,
                     req.effectProcessingGraph.get());
    }
    trackImpl->procStats.numBlocksProcessed++;

    
    trackImpl->outputPost.clear();


    DAW::automation_ref_t automationRef;
    // get automationRef
    {
        int32_t paramIdx = PARAM_GAIN;
        auto sendLevelGainVal = trackImpl->mixer.getParamValue(paramIdx);
        automationRef = DAW::AutomationConstant(sendLevelGainVal);
        auto sendLevelAutomation = trackImpl->mixer.getRegisteredConstAutomation(paramIdx);
        if (sendLevelAutomation) {
            automationRef = DAW::AutomationRef(&trackImpl->mixer, paramIdx);
        }
    }
    
    // evaluate automationRef
    float fValAutomated = 0.0f;
    /*bool bSuccess = */DAW::resolveAutomationAtTime(this, automationRef, tickLatencyCompensated, &fValAutomated);
    //if (bSuccess) {
    //    fGainTrack = dsp_util::linScaleToGain(fValAutomated);
    //}

    //TODO: apply filtered sample accurate volume automation
    float fGainTrack;
    if (dsp_util::getGainLvl(fValAutomated, fGainTrack)) {
        trackImpl->outputPost.addFromOp(&trackImpl->output, AudioBlock::mix_op::ADD, dsp_util::clampReadGain(fGainTrack));
    }

    /* Store block in audioOutput memory */
    if (DAW::isPlaybackState(playbackState)) {
        if (static_cast<bool>(trackImpl->flags & audiostageflags_t::WRITE_OUTPUT)) {
            int32_t offset = samplePosProcess - (int32_t)(trackImpl->getOutputLatency());
            if (offset >= 0) {
                String trName = track ? track->name : "?";
                //int32_t blockIdx = samplePosProcess/(int32_t)sampleFormat.blockSize;
                //log_printf("store track %s block %d at sample offset %d (samplepos %d - stage.latencyOutput %u)\n", StringAsCStr(trName), blockIdx, offset, sample, trackImpl->getOutputLatency());
                trackImpl->audioOutput.store(&trackImpl->outputPost, offset);
            } else {
                log_printf("cannot write to negative offset %d (samplepos %d - stage.latencyOutput %d)\n", offset, samplePosProcess, trackImpl->getOutputLatency());
            }
        }

    }
    return 0;
}


/**
 * At least 1 thread will be idle after finishThreadTasks returns
 * @param processFinishedStageIds
 * @param reqFinishWaitStageIds
 * @param isFinalInvocation
 */
void vsthost::finishTreadTasks(std::vector<audiostageid_i32>& processFinishedStageIds,
                               const std::vector<audiostageid_i32>& reqFinishWaitStageIds,
                               bool isFinalInvocation) {
    bool allBusyFlag = true;
    while (allBusyFlag) {
        allBusyFlag = true;
        impl->waitingTasks.clear();
        for (size_t i = 0; i < impl->threadCount; i++) {
            TrackBlockProcessTask& task = impl->tasks[i];
            if (task.isInUse()) {
                auto taskStageId = task.getTask().trackNode->stageId;
                if (!task.isCompleted()) {
                    impl->waitingTasks.push_back(taskStageId);
                    if (isFinalInvocation || STL_CONTAINS(reqFinishWaitStageIds, taskStageId)) {
                        task.wait();
                    } else {
                        continue;
                    }

                }
                dbgassert(task.isCompleted());
                //log_printf("Thread[%d] completed stageId %d\n", i, taskStageId);
                vsthost::track_block_processing_task_t& procTask = task.getTask();
                if (task.isError()) {
                    std::exception_ptr eptr = task.getException();
                    if (eptr != nullptr) {
                        try{
                            std::rethrow_exception(eptr);
                        }
                        catch(const std::exception &ex) {
                            printf("task[%d] had exception: %s\n", (int)i, ex.what());
                        }
                    }
                } else if (!task.isGood()) {
                    /* Logic error. Cannot reach */
                    dbgassert(0);
                }
                processFinishedStageIds.push_back(procTask.trackNode->stageId);
                auto thrdProcStats = thread_stats_process_timings_t{
                                        static_cast<uint32_t>(i),
                                        procTask.trackNode->stageId,
                                        task.stats.timeStart,
                                        task.stats.timeEnd
                                     };
                lastProcessingGraphs[procTask.trackNode->stageId] = procTask.effectProcessingGraph;
                procTask.effectProcessingGraph = nullptr;
                impl->blockThreadStats.push_back(thrdProcStats);
                task.resetTask();
            }
            allBusyFlag = false;
        }
        //allBusyFlag |= std::any_of(reqFinishWaitStageIds.begin(), reqFinishWaitStageIds.end(), [](const audiostageid_i32 stageId) {
        //    return !STL_CONTAINS(processFinishedStageIds, stageId);
        //});
        //if (allBusyFlag) {
        //    seqthreads::threadSleep(500);
        //}
    }
}

int32_t vsthost::processBlock(project_controller_t* ctrl,
                              const audiostream_properties_t& audioProp,
                              const DAW::processing_graph_t* const processingGraph,
                              AudioBlock* const ptrExternalInputs,
                              AudioBlock* const ptrExternalOutputs,
                              int32_t samplePosProcess,
                              double tickPosProcess,
                              playback_state playbackState,
                              bool inLoop,
                              bool isLoopAround)
{
    dbgassert(ctrl);
    project_t* project = ctrl->getProject();
    const sampleformat_t& sampleFormat = this->m_sampleFormatInternal;

    bool debugLogProcessing = false;

#ifndef NDEBUG
    lastState = playbackState;
#endif
    this->impl->resetBlock();

    /*
     * Clear all channels
     */
    for (track_t* track : project->trackList) {
        dbgassert(track->audio);
        track_impl_t* audio = track->audio;
        audio->input.realloc(sampleFormat.blockSize);
        audio->output.realloc(sampleFormat.blockSize);
        audio->outputPost.realloc(sampleFormat.blockSize);
        dsp_util::fillBlock(audio->input, 0.0f);
        dsp_util::fillBlock(audio->output, 0.0f);
        dsp_util::fillBlock(audio->outputPost, 0.0f);
        audio->updateLatency(); // determine max latency so getLatency() is correct
    }

    tick_t loopCutStart = -1;
    tick_t loopCutEnd = -1;
    if (inLoop) {
        loopCutStart = prjGlobals.loopStart;
        loopCutEnd = prjGlobals.loopStart+prjGlobals.loopLen;
    }


    /**
     * Parallelizing processing:
     * In Host initAppWindow:
     * 0. Spin up n WorkerThreads
     *
     *
     * This thread here:
     * 1. iterate over nodesFlatOrdererd:
     * 2.     finish 1 completed task so 1 thread is free
     * 3.     if node has no unprocessed inputs:
     * 4.        push task to free thread
     * 5. after loop wait for all tasks to be finished
     */

    struct Func_CheckUnprocessed {
        std::vector<audiostageid_i32> stagesProcessed; //TODO: use a tree, unsorted search scales badly
        bool operator()(const DAW::track_node_t* trackNode) const {
            return !STL_CONTAINS(stagesProcessed, trackNode->stageId);
        }
    };

    struct stageId_threadIdx_pair {
        audiostageid_i32 stageId;
        uint32_t threadIdx;
    };

    impl->playThreadId = seqthreads::getCurrentThreadId();

    const bool useThreading = this->multithreadedProcessing && impl->threadsRunningCount > 0 && impl->threadCount > 1;

    if (!useThreading) {
        for (auto itAudioStage = processingGraph->nodesFlatOrdered.begin(); itAudioStage != processingGraph->nodesFlatOrdered.end(); itAudioStage++) {
            const DAW::processing_track_node_t* ptrProcessingNode = *itAudioStage;
            const DAW::processing_track_node_t& trackNode = *ptrProcessingNode;

            vsthost::track_block_processing_task_t blockProcTask;
            blockProcTask.trackNode = &trackNode;
            dbgassert(blockProcTask.trackNode==ptrProcessingNode);
            blockProcTask.ptrExternalInputs = ptrExternalInputs;
            blockProcTask.ptrExternalOutputs = ptrExternalOutputs;
            blockProcTask.audioProp = audioProp;
            blockProcTask.samplePosProcess = samplePosProcess;
            blockProcTask.tickPosProcess = tickPosProcess;
            blockProcTask.playbackState = playbackState;
            blockProcTask.inLoop = inLoop;
            blockProcTask.debugLogProcessing = debugLogProcessing;
            auto timeStart = getTimeMicros();
            processBlockTrack(impl->singleThreadedBuf, blockProcTask);
            lastProcessingGraphs[blockProcTask.trackNode->stageId] = blockProcTask.effectProcessingGraph;
            blockProcTask.effectProcessingGraph = nullptr;
            auto timeEnd = getTimeMicros();

            thread_stats_process_timings_t thrdProcStats = {0, blockProcTask.trackNode->stageId, timeStart, timeEnd};
            impl->blockThreadStats.push_back(thrdProcStats);
        }
    } else {
        std::vector<stageId_threadIdx_pair> tasksQueued;

        Func_CheckUnprocessed funcCheckNodeUnprocessed;
        funcCheckNodeUnprocessed.stagesProcessed.reserve(processingGraph->nodesFlatOrdered.size());
        bool outOfOrderProcessing = true;
        auto timeEnd = getTimeMicros();
        int limR = 0;
        for (bool unprocessed=true; unprocessed; unprocessed=outOfOrderProcessing && tasksQueued.size() != processingGraph->nodesFlatOrdered.size()) {
            for (auto itAudioStage = processingGraph->nodesFlatOrdered.begin(); itAudioStage != processingGraph->nodesFlatOrdered.end(); itAudioStage++) {
                const DAW::processing_track_node_t* ptrProcessingNode = *itAudioStage;
                const DAW::processing_track_node_t& trackNode = *ptrProcessingNode;

                bool wasQueued = false;
                for (auto& taskQueued : tasksQueued) {
                    if (taskQueued.stageId == ptrProcessingNode->stageId) {
                        wasQueued = true;
                        break;
                    }
                }
                if (wasQueued) {
                    dbgassert(outOfOrderProcessing);
                    continue;
                }
                /* If all threads are busy wait for a thread to become free
                 * if node has unprocessed inputs wait for threads. This works as long as we process in order.
                 * out of order processing: For processing out of order we skip nodes with unprocessed inputs and loop over nodesFlatOrdered again */

                std::vector<audiostageid_i32> tasksToFinish;
                if (!outOfOrderProcessing) {
                    tasksToFinish = trackNode.dependencies;
                }
                auto timeStart = getTimeMicros();
                finishTreadTasks(funcCheckNodeUnprocessed.stagesProcessed, tasksToFinish, false);
                timeEnd = getTimeMicros();

                /* TODO: A lot of stats entries might be created. Especially when one of the threads goes unresponsive (broken plugin for example)
                 * A forced sleep of this thread might increase latency too much
                 * Until I know a smarter way to handle this I will put a hard limit on the length of the stats vector to avoid OOM situations */
                if (impl->blockThreadStats.size() < 5000) {
                    thread_stats_process_timings_t thrdProcStats = {static_cast<uint32_t>(impl->threadCount), TRACKID_INVALID_I32, timeStart, timeEnd};
                    impl->blockThreadStats.push_back(thrdProcStats);
                } else {
                    limR++;// for setting debugger breakpoint
                }
                bool hasUnprocessedInputs = /*!outOfOrderProcessing || */std::any_of(trackNode.children.cbegin(), trackNode.children.cend(), funcCheckNodeUnprocessed);
                if (!hasUnprocessedInputs) {

                    bool pushd=false;
                    for (size_t i = 0; i < impl->threadCount; i++) {
                        TrackBlockProcessTask& task = impl->tasks[i];
                        if (!task.isInUse()) {

                            vsthost::track_block_processing_task_t blockProcTask;
                            blockProcTask.effectProcessingGraph = nullptr;
                            blockProcTask.trackNode = &trackNode;
                            dbgassert(blockProcTask.trackNode==ptrProcessingNode);
                            blockProcTask.ptrExternalInputs = ptrExternalInputs;
                            blockProcTask.ptrExternalOutputs = ptrExternalOutputs;
                            blockProcTask.audioProp = audioProp;
                            blockProcTask.samplePosProcess = samplePosProcess;
                            blockProcTask.tickPosProcess = tickPosProcess;
                            blockProcTask.playbackState = playbackState;
                            blockProcTask.inLoop = inLoop;
                            blockProcTask.debugLogProcessing = debugLogProcessing;


                            tasksQueued.push_back({ ptrProcessingNode->stageId, static_cast<uint32_t>(i) });
                            task.setTask(blockProcTask);
                            impl->threads[i].pushTask(&task);
                            pushd = true;
                            break;
                        }
                        dbgassert(i+1 != MAX_AUDIOPROCESSING_THREADS);
                    }
                    dbgassert(pushd);
                }
            }
        }
        std::vector<audiostageid_i32> empty;
        finishTreadTasks(funcCheckNodeUnprocessed.stagesProcessed, empty, true);
        bool allProcessed = !std::any_of(processingGraph->nodesFlatOrdered.begin(), processingGraph->nodesFlatOrdered.end(), funcCheckNodeUnprocessed);
         dbgassert(allProcessed);
    }

    /* Profiling/Timings: Accumulate timings */
    int64_t timeProcessingArr[5] = {0};
    track_midiprocess_profiling_t blockMidiStats;
    for (track_t* track : project->trackList) {
        auto& procStats = track->getStage()->procStats;
        auto& procMidiStats = track->getStage()->procMidiStats;
        timeProcessingArr[0] += procStats.timeTrackProcessPluginsRaw;
        timeProcessingArr[1] += procStats.timeTrackMixInputs;
        timeProcessingArr[2] += procStats.timeTrackApplyAutomation;
        timeProcessingArr[3] += procStats.timeTrackProcessMidi;
        blockMidiStats.tm0InputClips += procMidiStats.tm0InputClips;
        blockMidiStats.tm1InputRT += procMidiStats.tm1InputRT;
        blockMidiStats.tm2ProcNotes += procMidiStats.tm2ProcNotes;
        blockMidiStats.tm3RevalidateEnds += procMidiStats.tm3RevalidateEnds;
        blockMidiStats.tm4SortEvents += procMidiStats.tm4SortEvents;
        blockMidiStats.tm5ProcArp += procMidiStats.tm5ProcArp;
        blockMidiStats.tm6WriteVstEvents += procMidiStats.tm6WriteVstEvents;
        blockMidiStats.tm7ProcessOutput += procMidiStats.tm7ProcessOutput;
    }

    int64_t timeTotalProcessPluginsRaw = timeProcessingArr[0];
    stats.timings["Block.Tracks.MixInputs"] = timeProcessingArr[1];
    stats.timings["Block.Tracks.ApplyAutomation"] = timeProcessingArr[2];
    stats.timings["Block.Tracks.ProcessMidi"] = timeProcessingArr[3];
    stats.blockMidiStats = blockMidiStats;
    stats.timeProcessPluginsRaw = timeTotalProcessPluginsRaw;
    auto curTimePluginProcess = stats.timeProcessPlugins;
    curTimePluginProcess -= curTimePluginProcess / NUM_BINS_STATS;
    curTimePluginProcess += timeTotalProcessPluginsRaw / NUM_BINS_STATS;
    stats.timeProcessPlugins = curTimePluginProcess;

    return 1;
}

void vsthost::initThreads() {
    for (auto & thread : impl->threads) {
        thread.setTls(daw_tls::getTls());
    }
    impl->startThreads();
}

void vsthost::onPlaybackJumpFromTo(project_controller_t* ctrl, int32_t fromSamplePos, double fromTickPos, int32_t toSamplePos, double toTickPos) {
    project_t* project = ctrl->getProject();
    for (track_t* track : project->trackList) {
        track->audio->onPlaybackJumpFromTo(fromSamplePos, fromTickPos, toSamplePos, toTickPos);
    }
}

void vsthost::onStartPlayback(project_controller_t* ctrl) {
    lastTickEndPos = 0;
    lastState = playback_state::status_stop;
    project_t* project = ctrl->getProject();
    for (track_t* track : project->trackList) {
        auto trackImpl = track->audio;
        //if (!trackImpl->heldNotes.empty())
        {
            trackImpl->onStartPlayback();
        }
    }
}

void vsthost::onPluginsChanged(audio_stage_t* stage) {
    log_printf("Plugins changed on audio stage %d", static_cast<int32_t>(stage->stageId.stageId));
    dbgassert(validateIds());
}

void vsthost::onStopPlayback(project_controller_t* ctrl) {
    midiRealtimeInput->m_list.clear();
    midiProcessedInput->m_list.clear();

    for (auto stageImpl : allAudioStages) {
        //if (!trackImpl->heldNotes.empty())
        {
            stageImpl->sendNotesOff(prjGlobals.tempo100);
            stageImpl->onStopPlayback();
        }
    }
}

void vsthost::onTrackLayoutChange() {
    impl->resetDelaylines();
}

void vsthost::setOutput(audiohost* audioHost) {
    this->audioHost = audioHost;
    sampleformat_t sampleFormatExternal = this->m_sampleFormatExternal;
    samplerate_t extSampleRate = audioHost && audioHost->lSampleRate > 0 ? audioHost->lSampleRate : sampleFormatExternal.sampleRate;
    uint32_t extBlockSize = audioHost && audioHost->lBlockSize > 0 ? audioHost->lBlockSize : sampleFormatExternal.blockSize;
    sampleFormatExternal = { extSampleRate, extBlockSize, sampleformat_bits_t::FLOAT_32 };
    this->m_sampleFormatExternal        = sampleFormatExternal;
    audiocache::getInstance()->setSamplerate(extSampleRate);
}

bool vsthost::isStreaming() {
    return this->audioHost && this->audioHost->isStreaming();
}

/* Function needs to be re-entrant (thread safe) */
void vsthost::processAudio(audio_stage_t* stage,
                           AudioBlock* input,
                           AudioBlock* output,
                           const double tickLatencyCompensated,
                           int32_t samplePos,
                           int32_t numSamples,
                           playback_state state,
                           const DAW::effect_processing_graph_t* const processingGraph) const
{

    tick_t processingPos = floor(tickLatencyCompensated);
    int count = 0;
    if (!stage->effects.empty()) {
        count += stage->effects.size();
    }

    AudioBlock tempBlock(64, 256);
    hires_timer_t timer;
    int64_t timeTotal = 0;
    if (processingGraph != nullptr) {
        for (auto itAudioStage = processingGraph->nodesFlatOrdered.begin(); itAudioStage != processingGraph->nodesFlatOrdered.end(); itAudioStage++) {
            const DAW::processing_effect_node_t* ptrProcessingNode = *itAudioStage;
            const DAW::processing_effect_node_t& effNode = *ptrProcessingNode;
            AudioBlock* blockIn = nullptr;
            switch (effNode.type) {
            case DAW::track_node_type_t::TRACK:
                dbgassert(0);
                break;
            case DAW::track_node_type_t::AUDIOSTAGE:
                if (effNode.pulls.empty()) {
                    continue;
                }
                //dbgassert(effNode.stageId == TRACKID_DEFAULT_I32);
                blockIn = &effNode.stage->output;
                break;
            case DAW::track_node_type_t::EFFECT:
                dbgassert(effNode.effectOptional);
                dbgassert(effNode.effectOptional->blockInputs);
                blockIn = effNode.effectOptional->blockInputs;
                break;
            }
            const uint32_t numChannelsTrack = blockIn->channels;
            dsp_util::fillBlock(*blockIn, 0.0f);

            effectbase* const effect = effNode.effectOptional;

            std::vector<DAW::effect_source_t> allSources = effNode.pulls; // copy

            struct Func_CheckHasSolo {
                bool operator()(const DAW::track_source_t& src) const {
                    return (src.flags & (audiostageflags_t::SOLO|audiostageflags_t::SOLO_PARENT)) != audiostageflags_t::NONE;
                }
            };
            Func_CheckHasSolo funcCheckSolo;
            bool hasSolo = std::any_of(allSources.cbegin(), allSources.cend(), funcCheckSolo);

            for (const DAW::effect_source_t& tracksrc : allSources) {
                if (hasSolo && !funcCheckSolo(tracksrc))
                    continue;
                if (DAW::isChannelConnected(tracksrc.channel)) {
                    track_audio_src src;
                    //if (dbg == 0) {
                    //    log_printf("track %s has input %s\n", StringAsCStr(track->name), StringAsCStr(tracksrc.channel.name));
                    //}

                    if (DAW::resolveAudioChannel(this, numChannelsTrack, tracksrc.channel, /*ptrExternalInputs*/ nullptr, src)) {
                        /**
                         * Mix routed tracks
                         *
                         * Mix level is fGainInput * src.gain * tracksrc.gain
                         * src.gain:            block-wise automated track gain
                         * tracksrc.gain:        block-wise automated send level, 1.0f for non-sends
                         *
                         * sends are with track gain applied (post-mixer)
                         *
                         */
                        /* compensate at input stage */
                        /* figure out max latency of all inputs */
                        /* delay signal by maxLatency - trackImpl->getLatency() */
                        /* Compensate audio midi track to pre-return latency */
                        dbgassert(effNode.inputLatency >= tracksrc.latency);
                        samplerate_t delayToMaxInputLatency = effNode.inputLatency - tracksrc.latency;

                        AudioBlock srcBlock = src.toAudioBlock();
                        tempBlock.realloc(srcBlock.samples);
                        dbgassert(srcBlock.samples == tempBlock.samples);

                        uint32_t nChannels = math::min(srcBlock.channels, tempBlock.channels);

                        DelayLine* delayLine = stage->getEffectDelayLine(tracksrc.trackEdgeId, nChannels);

                        //dbgassert(srcBlock.channels <= tempBlock.channels);
                        //dbgassert((delayLine->block.channels == srcBlock.channels) ||
                        //          (2 <= delayLine->block.channels && ((delayLine->block.channels % 2) == 0) && 1 == srcBlock.channels));

                        AudioBlock* srcDelayBlocked = &srcBlock;
                        // One of the delay lines will always be 0 samples delay
                        if (delayToMaxInputLatency > 0) {
                            delayAudio(delayLine, &srcBlock, &tempBlock, delayToMaxInputLatency);
                            srcDelayBlocked = &tempBlock;
                        }

                        //TODO: apply filtered sample accurate volume automation
                        float fGainRaw = 0.0f;
                        bool bSuccess = DAW::resolveAutomationAtTime(this, tracksrc.gainAutomation, processingPos, &fGainRaw);
                        if (bSuccess) {
                            /* Calculate audio/midi tracks gain level */
                            float fGainTrack;
                            if (dsp_util::getGainLvl(fGainRaw, fGainTrack)) {
                                //trackImpl->addAudio(tempBlock, src.gain * fGainRaw);
                                blockIn->addFromOp(srcDelayBlocked, AudioBlock::mix_op::ADD, fGainTrack);
                            }
                        }
                    }
                } else {
                    log_printf("effect %s has no connected input %s\n", StringAsCStr(effect->getName()));
                }
            }

            timer.reset();
            AudioBlock* blockPostProcess = nullptr;
            int64_t timePassed = 0;
            if (effect) {
                bool isBypass = effect->isBypass();
                if (isBypass || bypassEffectProcessing) {
                    samplerate_t delay = effect->getPluginLatency();
                    if (delay > 0) {
                        if (!effect->delayLine.get()) {
                            effect->delayLine.reset(new DelayLine(this->numChannels, m_sampleFormatInternal.blockSize));
                        }
                        AudioBlock *blockOut = effect->blockOutputs;
                        delayAudio(effect->delayLine.get(), blockIn, blockOut, delay);
                    } else {
                        effect->blockOutputs->copyFrom(blockIn);
                    }
                    blockPostProcess = effect->blockOutputs;
                } else {
                    effect->process(effect->blockInputs, effect->blockOutputs, tickLatencyCompensated, samplePos, numSamples, state);
                    blockPostProcess = effect->blockOutputs;
                }
                effect->postProcess(blockPostProcess, numSamples, !isBypass);
                timePassed = timer.getTime();
                auto &plugStats = effect->procStats;
                if (plugStats.statsProcStep % STATS_PROCESSING_INTERVAL_STEP == 0) {
                    plugStats.statsProcSamples[(plugStats.statsWriteOffset + 1) % STATS_PROCESSING_MAX_SAMPLES] = timePassed;
                    plugStats.statsWriteOffset++;
                }
                auto curTimeProcess = plugStats.timeTrackProcessPlugins;
                curTimeProcess -= curTimeProcess / NUM_BINS_STATS;
                curTimeProcess += timePassed / NUM_BINS_STATS;
                plugStats.timeTrackProcessPlugins = curTimeProcess;
                plugStats.timeTrackProcessPluginsRaw = timePassed;
            } else {
                timePassed = timer.getTime();
            }
            timeTotal += timePassed;
        }
    }

    auto curTotalTimeProc = stage->procStats.timeTrackProcessPlugins;
    curTotalTimeProc -= curTotalTimeProc/NUM_BINS_STATS;
    curTotalTimeProc += timeTotal/NUM_BINS_STATS;
    stage->procStats.timeTrackProcessPluginsRaw = timeTotal;
    stage->procStats.timeTrackProcessPlugins = curTotalTimeProc;

}

void vsthost::updatePluginWindows() {
    for (auto* plugin : pluginInstancesVST2) {
        //plugin->dispatch(effEditIdle);
        plugin->updateWindow();
    }
}
bool vsthost::onTick() {
    {
        int iDispatched = 0;
        //ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
        for (auto* current : pluginInstancesVST2) {
            if (current->bEditOpen && !current->bInEditIdle) {
                current->bInEditIdle = true;
                current->dispatch(effEditIdle);
                current->bInEditIdle = false;
                if (current->window) {
                    //current->window->captureWindowFrame();
                    current->updateWindow();
                }
                iDispatched++;
            }
            if (current->bWantsEffIdle && !current->bInEditIdle) {
                current->bInEditIdle = true;
                //current->dispatch(effIdle);
                current->bInEditIdle = false;
                iDispatched++;
            }
        }
    }
    checkScanner();
    return false;
}

void vsthost::releaseProjectResources() {
    lastProcessingList = nullptr;
    lastTrackGraph = nullptr;
    //lastProcessingGraphs.clear();
}

void vsthost::unload() {
    dbgassert(!isStreaming()&&"STOP STREAM BEFORE unload()!");
    unloadAllPlugins();
}

void vsthost::destroy() {
    stopScanner();
    freeRingBuffer(ringbuffer);
    dbgassert(hostSlot > -1);
    dbgassert(g_hostslots[hostSlot].g_instance);
    g_hostslots[hostSlot].g_instance = nullptr;
    impl->stopThreads();
}

bool vsthost::assignMasterCallback(vsthost* host)
{
    for (int i = 0; i < NUM_HOST_CB_SLOTS; i++) {
        if (g_hostslots[i].g_instance == nullptr) {
            g_hostslots[i].g_instance = host;
            g_hostslots[i].g_instanceImpl = host->impl;
            host->hostSlot = i;
            if (i == 0) {
                host->masterCallBackSlot = audioMaster1;
            }
            if (i == 1) {
                host->masterCallBackSlot = audioMaster2;
            }
            if (i == 2) {
                host->masterCallBackSlot = audioMaster3;
            }
            if (i == 3) {
                host->masterCallBackSlot = audioMaster4;
            }
            return true;
        }
    }
    dbgassert(0&&"Out of host slots");
    return false;
}

vstplugin* vsthost::getPlugin(AEffect* aeffect) {
    if (aeffect && aeffect->user) {
        return static_cast<vstplugin*>(aeffect->user);
    }
    //for (auto* current : pluginInstancesVST2) {
    //    if (current->handle->aeffect == aeffect)
    //        return current;
    //}
    return nullptr;
}

effectbase* vsthost::getPluginById(int32_t projectGlobalId) const {
    auto it = std::find_if(pluginInstances.begin(), pluginInstances.end(),
        [projectGlobalId] (const effectbase* ptr) {
            return ptr->projectGlobalId == projectGlobalId;
        });
    if (it != pluginInstances.end()) {
        return *it;
    }
    it = std::find_if(pluginsDeferred.begin(), pluginsDeferred.end(),
                      [projectGlobalId](const effectbase* ptr)
                      {
                        if (ptr->projectGlobalId == projectGlobalId)
                            return true;
                        auto plugDeferred = dynamic_cast<const effect_deferred*>(ptr);
                        return plugDeferred->getSnapshotConst().projectGlobalId == projectGlobalId;
                      });
    if (it != pluginsDeferred.end()) {
        return *it;
    }
    return nullptr;
}

void vsthost::unloadTrack(track_t* track) {
    dbgassert(track->audio);
    auto audio = track->audio;
    std::vector<effectbase*> effects = audio->effects; // make a copy before unloading plugins
    for (effectbase* effect : effects) {
        unloadPlugin(effect, FLAG_HOST_UNLOAD_PLUGIN_NO_NOTIFY);
    }
    dbgassert(audio->deferredEffects.empty());
}

void vsthost::removePlugin(effectbase* plugin) {
    audio_stage_t* audioStage = plugin->getTrackLink();
    audioStage->removePlugin(plugin, true);
    audioStage->pluginsChanged();
    if (DawInstance::get()) DawInstance::get()->onPluginsChanged();
    onTrackLayoutChange();
}

void vsthost::unloadPlugin(effectbase* plugin, int flags) {
    bool notifyUp = !(flags & FLAG_HOST_UNLOAD_PLUGIN_NO_NOTIFY);
    if (notifyUp) {
        //TODO: this shouldn't be here!
        if (MainCtrl::get())
            MainCtrl::get()->closeContextMenu();
    }

    plugin->onPreUnload(flags);
    audio_stage_t* audioStage = plugin->getTrackLink();
    if (audioStage) {
        audioStage->removePlugin(plugin, false);
        if (notifyUp) {
            audioStage->pluginsChanged();

        }
    }

    if (notifyUp) {
        plugin->close();
    }
    plugin->unload(this, flags);

    switch (plugin->getModuleType()) {
    case PLUGIN_TYPE_DEFERRED:
        always_assert(removeEntry(pluginsDeferred, plugin));
        break;
    case PLUGIN_TYPE_INTERNAL_EFFECT:
    case PLUGIN_TYPE_VST:
        always_assert(removeEntry(pluginInstancesVST2, plugin));
        always_assert(removeEntry(pluginInstances, plugin));
        break;
    default:
        always_assert(removeEntry(pluginInstancesInternal, plugin));
        always_assert(removeEntry(pluginInstances, plugin));
        break;
    }

    //PopupCtrl::get()->close(); // Make sure context controls do not reference vst
    if (plugin->getModuleType() == PLUGIN_TYPE_VST || plugin->getModuleType() == PLUGIN_TYPE_INTERNAL_EFFECT) {
        vstplugin* vst = dynamic_cast<vstplugin*>(plugin);
        if (vst->internalModuleId <= 0) {
            moduleMgr->releaseModule(vst->handle->hmodule);
        }
    }
    delete plugin;
    if (notifyUp) {
        if (DawInstance::get()) DawInstance::get()->onPluginsChanged();
    }
}

bool vsthost::unloadAllPlugins() {
    dbgassert(pluginInstances.empty());
    dbgassert(pluginInstancesVST2.empty());
    dbgassert(pluginInstancesInternal.empty());
    dbgassert(allAudioStages.empty());
    dbgassert(trackAudioStages.empty());
    //int count = list.size();
    //for (int i = 0; i < count; ++i) {
    //    vstplugin* current = list[i];
    //    if (current->trackImpl) {
    //        current->trackImpl->removePlugin(current, false);
    //    }
    //}
    //for (int i = 0; i < count; ++i) {
    //    vstplugin* current = list[i];
    //    current->close();
    //    list[i] = NULL;
    //    current->unload(this);
    //    moduleMgr->releaseModule(current->handle->hmodule);
    //    delete current;
    //}
    //list.clear();
    return true;
}

void vsthost::getAllInstances(std::vector<effectbase*>& effects) {
    //for (auto* as : allAudioStages) {
    //    effects.insert(effects.end(), as->effects.begin(), as->effects.end());
    //}
    effects = pluginInstances;
}

void vsthost::createAudio(track_t* track) {
    auto audio = new track_impl_t(this,
                                  getNextGlobalAudioStageId(0),
                                  track,
                                  m_sampleFormatInternal.sampleRate,
                                  m_sampleFormatInternal.blockSize,
                                  OUTPUT_CHANNELS);
    allAudioStages.push_back(audio);
    trackAudioStages.push_back(audio);
    track->audio = audio;
}

void vsthost::releaseAudio(track_t* track) {
    auto audioStage = track->audio;
    dbgassert(audioStage);
    dbgassert(audioStage->effects.empty());
    track->audio = nullptr;
    auto it = std::find(allAudioStages.begin(), allAudioStages.end(), audioStage);
    dbgassert(it != allAudioStages.end());
    allAudioStages.erase(it);
    auto it2 = std::find(trackAudioStages.begin(), trackAudioStages.end(), audioStage);
    dbgassert(it2 != trackAudioStages.end());
    trackAudioStages.erase(it2);
    delete audioStage;
}

audio_stage_t* vsthost::createAudioStage() {
    auto audio = new audio_stage_t(this,
                                   getNextGlobalAudioStageId(0),
                                   m_sampleFormatInternal.sampleRate,
                                   m_sampleFormatInternal.blockSize,
                                   OUTPUT_CHANNELS);
    allAudioStages.push_back(audio);
    return audio;
}

void vsthost::releaseAudioStage(audio_stage_t* audioStage) {
    auto it = std::find(allAudioStages.begin(), allAudioStages.end(), audioStage);
    dbgassert(it != allAudioStages.end());
    allAudioStages.erase(it);
}

audio_stage_t* vsthost::getAudioStage(const audio_stage_ref_t& ref) const {
    if (ref.stageId == TRACKID_INVALID_I32)
        return nullptr;
    dbgassert((int32_t)ref.stageId > -1);
    auto it = std::find_if(allAudioStages.begin(), allAudioStages.end(), [ref] (const audio_stage_t* ptr) {
        return audioStageIdMatches(ptr->stageId, ref.stageId);
    });
    //dbgassert(it != allAudioStages.end());
    if (it != allAudioStages.end()) {
        return *it;
    }
    log_printf("null audio stage for %d\n", static_cast<int32_t>(ref.stageId));
    return nullptr;
}

bool vsthost::movePlugins(audio_stage_t* dstTr, audio_stage_t* trp, int32_t src, int32_t dst, int32_t len) {
    ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
    dbgassert(dstTr);
    dbgassert(trp);
    dbgassert(src < (int)trp->effects.size());
    dbgassert(src+len <= (int)trp->effects.size());
    dbgassert(dst-1 <= (int)dstTr->effects.size());
    std::vector<effectbase*> tmpEffects = trp->effects;
    for (int32_t i = 0; i < len; i++) {
        effectbase* tmpPlugin = tmpEffects[src + i];
        trp->removePlugin(tmpPlugin, true);
        dstTr->insertEffect(dst+i, tmpPlugin);
    }
    trp->pluginsChanged();
    dstTr->pluginsChanged();
    onTrackLayoutChange();
    if (DawInstance::get()) DawInstance::get()->onPluginsChanged();
    return true;
}

bool vsthost::moveEffects(audio_stage_t* trp, int32_t src, int32_t dst, int32_t len) {
    ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
    dbgassert(src >= 0 && dst >= 0);
    dbgassert(src != dst);
    //src--;
    //dst--;
    dbgassert((int32_t)trp->effects.size() > src);
    dbgassert((int32_t)trp->effects.size() > dst);
    for (effectbase* effect : trp->effects) {
        dbgassert(effect->getSlot()>=0);
    }

    //shift element
    std::vector<effectbase*> curEffects = trp->effects;
    std::vector<effectbase*> tmpEffects;
    tmpEffects.resize(trp->effects.size());
    auto itIn = curEffects.cbegin();
    auto itOut = tmpEffects.begin();
    int32_t src2 = src;
    int32_t dst2 = dst;
    int32_t end = dst+len;
    for (;itOut!=tmpEffects.cend();) {
        if (curEffects.cbegin()+src == itIn) {
            my_printf("jump input iterator from %d to %d\n", itIn-curEffects.cbegin(), itIn-curEffects.cbegin()+len);
            itIn+=len;
        }
        int srcPos;
        int outPos = itOut-tmpEffects.begin();
        if (dst2 < end && tmpEffects.cbegin()+dst2 == itOut) {
            my_printf("dst2 %d\n", dst2);
            srcPos = src2;
            *itOut++ = curEffects[src2++];
            dst2++;
            my_printf("b writing %d to %d\n", srcPos, outPos);
        } else {
            srcPos = itIn-curEffects.cbegin();
            *itOut++ = *itIn++;
            my_printf("a writing %d to %d\n", srcPos, outPos);
        }
    }
    trp->effects = std::move(tmpEffects);
    int slot = 0;
    for (effectbase* effect : trp->effects) {
        effect->setSlot(slot++);
    }
    onTrackLayoutChange();
    return true;
}

bool vsthost::replacePlugin(audio_stage_t* trp, effectbase* plugin, int32_t dst, effectbase** prevPlugin) {
    bool retVal = trp->replaceEffect(dst, plugin, prevPlugin);
    onTrackLayoutChange();
    return retVal;
}

bool vsthost::insertNewPlugin(audio_stage_t* trp, effectbase* plugin, int32_t dst) {
    //if (plugin->isSynth) {
    //    vstplugin* old = trp->setInstrument(plugin);
    //    if (old) {
    //        unloadPlugin(old);
    //    }
    //} else {
        trp->insertEffect(dst, plugin);
    //}
    return true;
}

bool vsthost::postPluginLoaded(audio_stage_t* trp, effectbase* plugin) {
    trp->pluginsChanged();
    onTrackLayoutChange();
    if (DawInstance::get()) DawInstance::get()->onPluginsChanged();
    return true;
}
int32_t vsthost::getNextGlobalModuleId(int32_t globalId)
{
    if (globalId <= 0) {
        globalId = ++pluginId;
    } else if (globalId < (1 << 16)) {
        globalId += (1 << 16);
    }

    update_maximum(pluginId, globalId);
    return globalId;
}

audio_stage_id_t vsthost::getNextGlobalAudioStageId(int32_t globalId) {
    audio_stage_id_t stageId{};
    audiostageid_i32* stageIds[4] = {&stageId.stageId, &stageId.inputStageId, &stageId.outputStageId, &stageId.outputPostStageId };
    auto startId = globalId;
    if (globalId <= 0) {
        startId = ++audioStageId;
    }
    for (audiostageid_i32* id : stageIds) {
        *id = static_cast<audiostageid_i32>(startId++);
    }
    update_maximum(audioStageId, startId);
    return stageId;
}

void vsthost::updateMaximumStageId() {
    int32_t maximumStageId = 0;
    for (auto* stage : allAudioStages) {
        maximumStageId = math::max<int32_t>(maximumStageId, static_cast<int32_t>(stage->stageId.stageId));
        maximumStageId = math::max<int32_t>(maximumStageId, static_cast<int32_t>(stage->stageId.inputStageId));
        maximumStageId = math::max<int32_t>(maximumStageId, static_cast<int32_t>(stage->stageId.outputStageId));
        maximumStageId = math::max<int32_t>(maximumStageId, static_cast<int32_t>(stage->stageId.outputPostStageId));
    }
    this->audioStageId = maximumStageId;
}

bool vsthost::writeRecordedData(project_t* project) {
    dbgassert(MainCtrl::get());
    if (this->hasNewRecordedData) {
        ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
        this->hasNewRecordedData = false;
        if (recordDataProcessed && recordDataProcessed->notes.m_list.size() && recordDataProcessed->getLen() > 0) {
            clip_t* pClip = nullptr;
            std::swap(recordDataProcessed, pClip);
            track_t* tr = project->trackMidiAudioCtr.front();
            if (tr) {
                //String s = "Recorded notes: ";
                //for (note_t& note : pClip->notes.m_list) {
                //    s += String(noteName(note.pitch)) + ",";
                //}
                //log_printf("%s\n", StringAsCStr(s));
                log_printf("Processing recorded clip with %d notes\n", pClip->notes.m_list.size());
                log_printf("Processing recorded clip. Last note time %d\n", pClip->notes.lastNote.time);
                tick_t tickBegin = pClip->time;
                tick_t tickEnd = pClip->end();
                DawInstance::get()->cutIntersecting(tr, tickBegin, tickEnd);
                pClip->setDirty();
                pClip->notes.updateBounds();
                tr->getMidi().addClip(pClip);
                tr->getMidi().sortClips();
                return true;
            }
        }
    }
    return false;
}

int32_t vsthost::getNextSampleId(int32_t id) {
    if (id <= 0) {
        return ++sampleId;
    } else {
        update_maximum(sampleId, id);
    }
    return id;
}

int32_t vsthost::getPlayThreadId()
{
    return impl->playThreadId;
}

int32_t vsthost::validateIds()
{
    /** check for double usage of stageIds across all audiostages */
    for (auto stage : allAudioStages) {
        audiostageid_i32* stageIds[4] = {&stage->stageId.stageId, &stage->stageId.inputStageId, &stage->stageId.outputStageId,
                                         &stage->stageId.outputPostStageId};
        for (auto* pStageId : stageIds) {
            for (auto* pStageId2 : stageIds) {
                if (pStageId2 == pStageId) {
                    always_assert(static_cast<int32_t>(*pStageId) == static_cast<int32_t>(*pStageId2));
                    continue;
                }
                always_assert(static_cast<int32_t>(*pStageId) != static_cast<int32_t>(*pStageId2));
            }
        }
        for (auto stage2 : allAudioStages) {
            if (stage2 == stage)
                continue;
            audiostageid_i32* stageIds2[4] = {&stage2->stageId.stageId, &stage2->stageId.inputStageId, &stage2->stageId.outputStageId,
                                              &stage2->stageId.outputPostStageId};
            for (auto* pStageId : stageIds) {
                for (auto* pStageId2 : stageIds2) {
                    always_assert(static_cast<int32_t>(*pStageId) != static_cast<int32_t>(*pStageId2));
                }
            }
        }
    }
    /** check for collisions of plugin ids between deferred and normal effect instances */
    for (auto plugin : pluginInstances) {
        auto id = plugin->projectGlobalId;
        for (auto plugin2 : pluginsDeferred) {
            if (plugin == plugin2)
                continue;
            auto id2 = plugin2->projectGlobalId;
            dbgassert(id2 != id);
        }
        for (auto plugin2 : pluginInstances) {
            if (plugin == plugin2)
                continue;
            auto id2 = plugin2->projectGlobalId;
            dbgassert(id2 != id);
        }
    }

    for (auto plugin : pluginsDeferred) {
        auto id = plugin->projectGlobalId;
        for (auto stage : allAudioStages) {
            audiostageid_i32* stageIds[4] = {&stage->stageId.stageId, &stage->stageId.inputStageId, &stage->stageId.outputStageId,
                                             &stage->stageId.outputPostStageId};
            for (auto* pStageId : stageIds) {
                dbgassert(static_cast<int32_t>(*pStageId) != id);
            }
        }
    }

    for (auto plugin : pluginInstances) {
        auto id = plugin->projectGlobalId;
        for (auto stage : allAudioStages) {
            audiostageid_i32* stageIds[4] = {&stage->stageId.stageId, &stage->stageId.inputStageId, &stage->stageId.outputStageId,
                                             &stage->stageId.outputPostStageId};
            for (auto* pStageId : stageIds) {
                dbgassert(static_cast<int32_t>(*pStageId) != id);
            }
        }
    }
    return 1;
}

#ifdef _WIN32
HMODULE safeLoadLib(const char* szLibName);
int32_t loadLib(String filepath, VSTPluginMain_t** out_fn, HMODULE* out_hmodule) {
    if (!FileExists(filepath)) {
        return -2;
    }
    HMODULE hmodule = safeLoadLib(StringAsCStr(filepath));
    if (!hmodule) {
        return -3;
    }

    VSTPluginMain_t* fn = (VSTPluginMain_t*)GetProcAddress(hmodule, "VSTPluginMain");
    if (fn == NULL)
    {
        fn = (VSTPluginMain_t*)GetProcAddress(hmodule, "main");
    }
    if (fn == NULL)
    {
        FreeLibrary(hmodule);
        return -4;
    }
    *out_hmodule = hmodule;
    *out_fn = fn;

    return 0;
}
#endif
#if defined(__APPLE__)

int32_t loadLib(String filepath, VSTPluginMain_t** out_fn, void** out_hmodule);

#endif
#if defined(__linux__)
int32_t loadLib(String filepath, VSTPluginMain_t** out_fn, void** out_hmodule) {
    if (!FileExists(filepath)) {
        return -2;
    }
    void* module = dlopen(StringAsCStr(filepath), RTLD_NOW);
    if (!module) {
        return -3;
    }

    VSTPluginMain_t* fn = (VSTPluginMain_t*)dlsym(module, "VSTPluginMain");
    if (fn == NULL)
    {
        fn = (VSTPluginMain_t*)dlsym(module, "main");
    }
    if (fn == NULL)
    {
        dlclose(module);
        return -4;
    }
    *out_hmodule = module;
    *out_fn = fn;

    return 0;
}
#endif

vstpluginloadres vsthost::loadPlugin(String filepath, int32_t uId, int32_t globalId) {
    dbgassert(masterCallBackSlot);
    String path, name, nameWithoutExt;
    SplitPath(filepath, &path, &nameWithoutExt, nullptr, &name);
    VSTPluginMain_t* fn = nullptr;
    void* moduleHandle = nullptr;
    AEffect* aeffect = nullptr;

    this->impl->vstShellCurrentUniqueId = static_cast<VstInt32>(0);
#ifdef _WIN32

    HMODULE hmodule = nullptr;
    int32_t ret = 0;
    {
        ret = loadLib(filepath, &fn, &hmodule);
        if (ret != 0) {
            return vstpluginloadres(ret, nullptr);
        }
        if (uId != 0) {
            this->impl->vstShellCurrentUniqueId = static_cast<VstInt32>(uId);
        }
        aeffect = fn(masterCallBackSlot);
        if (uId != 0) {
            this->impl->vstShellCurrentUniqueId = static_cast<VstInt32>(0);
        }
        if (!aeffect) {
            FreeLibrary(hmodule);
            return vstpluginloadres(-5, nullptr);
        }
        if (aeffect->magic != kEffectMagic) {
            FreeLibrary(hmodule);
            return vstpluginloadres(-6, nullptr);
        }
        if (uId == 0) {
            // this branch is only reached by the vst scanner application when passing uId == 0
            VstIntPtr vstIntPtr = aeffect->dispatcher(aeffect, effGetPlugCategory, 0, 0, 0, 0);
            VstPlugCategory pluginCategory = static_cast<VstPlugCategory>(vstIntPtr);
            if (pluginCategory == VstPlugCategory::kPlugCategShell) {
                return vstpluginloadres(1, nullptr, new handles_t(nullptr, aeffect, moduleHandle), filepath, nameWithoutExt);
            }
        }

        dbgassert(!aeffect->user);
        moduleHandle = hmodule;
    }

#endif //_WIN32



#if defined(__linux__) || defined(__APPLE__)
    void* hmodule = NULL;
    int32_t ret = loadLib(filepath, &fn, &hmodule);
    if (ret != 0) {
        return vstpluginloadres(ret, NULL);
    }

    aeffect = fn(masterCallBackSlot);
    if (!aeffect) {
        dlclose(hmodule);
        return vstpluginloadres(-5, NULL);
    }
    if (aeffect->magic != kEffectMagic) {
        dlclose(hmodule);
        return vstpluginloadres(-6, NULL);
    }
    moduleHandle = hmodule;
#endif

    globalId = getNextGlobalModuleId(globalId);
    vstplugin* plugin = new vstplugin(new handles_t(nullptr, aeffect, moduleHandle), globalId, path, nameWithoutExt, -1);
    aeffect->user = plugin;
    plugin->handle->localCurrentUniqueId = uId;
    pluginInstancesVST2.push_back(plugin);
    pluginInstances.push_back(plugin);

    plugin->load(this);
    dbgassert(plugin->handle && plugin->handle->aeffect);
    return vstpluginloadres(0, plugin);
};

void vsthost::scanPlugins() {
    if (this->impl->scanningState == 0) {
        try {
            impl->vstscannerProcessThread = std::make_unique<ProcessThread>();
            String nameScannerExe = "daw-vstscanner.exe";
            if (!FileExists(nameScannerExe)) {
                nameScannerExe = "vstscanner-MSVC-debug.exe";
            }
            impl->vstscannerProcessThread->startProcess(nameScannerExe, "-server -auto", "");
            seqthreads::threadSleep(200);
            if (!impl->vstscannerProcessThread->isRunning()) {
                impl->vstscannerProcessThread->checkException();
                log_printf("Failed starting vstscanner", 0);
            } else {
                this->impl->scanningState = 1;
                log_printf("vstscanner is running", 0);
            }
        } catch (std::exception& e) {
            std::cout << "exception: " << e.what() << std::endl;
        } catch (...) {
            std::cout << "Unhandled exception" << std::endl;
        }
    }
}

void vsthost::checkScanner() {
    try {
        static int nCalls = 0;
        if (this->impl->scanningState && impl->vstscannerProcessThread) {
            if (!impl->vstscannerProcessThread->isRunning()) {
                impl->vstscannerProcessThread->joinProcess();
                impl->vstscannerProcessThread.reset();
                DawInstance::get()->getPluginDatabase().reopen();
                this->impl->scanningState = 0;
            } else {
                if (++nCalls >= 10) {
                    nCalls = 0;
                    DawInstance::get()->getPluginDatabase().reopen();
                }
            }

        }
    } catch (std::exception& e) {
        std::cout << "exception: " << e.what() << std::endl;
    } catch (...) {
        std::cout << "Unhandled exception" << std::endl;
    }
}

void vsthost::stopScanner() {
    try {
        if (this->impl->scanningState && impl->vstscannerProcessThread) {
            if (impl->vstscannerProcessThread->isRunning()) {
                impl->vstscannerProcessThread->killProcess();
                this->impl->scanningState = 0;
                if (DawInstance::get()) {
                    DawInstance::get()->getPluginDatabase().reopen();
                }

            }

        }
    } catch (std::exception& e) {
        std::cout << "exception: " << e.what() << std::endl;
    } catch (...) {
        std::cout << "Unhandled exception" << std::endl;
    }
}

bool vsthost::isScanning() {
    return impl->scanningState > 0;
}

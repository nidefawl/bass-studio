#include <atomic>
#include <clap/events.h>
#include <cstddef>
#include <cstdlib>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory.h>
#include <memory>
#include "host/automation/automation.h"
#include "host/clip/clip.h"
#include "host/host.h"
#include "host/audiocache/audiocache.h"
#include "host/daw_channel.h"
#include "host/daw/history.h"
#include "host/host_pluginmanager.h"
#include "host/meter/meter.h"
#include "host/track/track_types.h"
#include "math/seq_math.h"
#include "host/plugin/modules.h"
#include "note.h"
#include "plugin/vst3/vst3plugin.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "str_util.h"
#include "seq_util.h"
#include "seq_time.h"
#include "dsp_util.h"
#include "samplerate.h"

#include "host/project/project.h"
#include "tls.h"
#include "types.h"
#include "util/profiling.h"
#include "util/trace_allocations.hpp"
#include "fileio.h"
#include "host/track/track.h"
#include "basectrl.h"
#include "host/daw/mainctrl.h"
#include "host/plugin/base/base-plugin.h"
#include "host/plugin/vst/vstplugin.h"
#include "host/plugin/clap/clap-plugin.h"
#include "appsettings.h"
#include "logging.h"
#include "audio_config.h"
#include "host/audiobuffer/audioblock.h"
#include "host/audiobuffer/audiobuffer.h"
#include "platform.h"
#include "host/midihost/midi_host.h"
#include "midi-defs.h"
#include "midi-event.h"
#include "assert_dbg.h"
#include "host/track/track_impl.h"
#include "host/project/projectcontroller.h"
#include "thread.h"
#include "threads/threadlock.h"
#include "host/graph/track_graph.h"
#include "host/graph/effect_graph.h"
#include "host/resampler/resampler.h"
#include "threads/workerthread.h"
#include "sse.h"

#ifdef _WIN32
#include <windows.h>
#endif
#if defined(__linux__) || defined(__APPLE__)
#include <emmintrin.h>
#endif

#include <dr_libs/dr_wav.h>
#define THREADSYNC_NONE 0
#define THREADSYNC_SEMAPHORE 1
#define THREADSYNC_ATOMIC 2

#define THREADSYNC THREADSYNC_NONE
#if THREADSYNC == THREADSYNC_SEMAPHORE
#include <semaphore>
#endif

#define DAW_DEBUG_AUDIOGRAPH 0
#define MAX_AUDIOPROCESSING_THREADS 32
#define NUM_AUDIOPROCESSING_THREADS_INITIAL 8

static int32_t dbgStep = 1;


namespace DAW {
bool resolveEffectDefaultConnection(const Host::PluginManager* const host, const project_t* const project, const audio_stage_t* const stage, effectbase* const effect, channel_ref_t& out) {
    if (effect && effect->inputChannelsDesc.empty()) {
        out = DAW::ChannelNone();
        return true;
    }
    int32_t effIdx = 0;
    if (effect) {
        effIdx = indexOfCtr(stage->effects, effect);
    } else {
        effIdx = static_cast<int32_t>(stage->effects.size());
    }
    while (effIdx > 0) {
        auto effectBefore = stage->effects[effIdx - 1];
        if (!effectBefore->outputChannelsDesc.empty()) {
            auto dstChDesc = effect ? effect->inputChannelsDesc.front() : DAW::channel_desc{};
            out = DAW::ChannelAudioEffect(effectBefore, stage_bufferpoint::OUTPUT_POST, effectBefore->outputChannelsDesc.front(), dstChDesc);
            return true;
        }
        effIdx -= 1;
    }
    out = DAW::ChannelStage(stage, stage_bufferpoint::INPUT);
    return true;
}

bool resolveDefaultConnection(const Host::PluginManager* const host, const project_t* const project, track_impl_t* const trImpl, const bool isInput, channel_ref_t& out) {
    if (!isInput && TRACKTYPE_TO_CTR(trImpl->track->type) == TRACK_CTR_MASTER) {
        int32_t idx = 0;
        auto type = AudioIO::getTrackTypeFromNumChannels(trImpl->outputPost.channels);
        String name = "External "+AudioIO::getExternalIOName(type, idx, stage_bufferpoint::OUTPUT_POST);
        out = ChannelAudioInput(idx, 0, name, type);
        return true;
    }
    const track_t* const firstMaster = !project->trackMasterCtr.empty() ? project->trackMasterCtr.front() : nullptr;
    if (!isInput && TRACKTYPE_TO_CTR(trImpl->track->type) == TRACK_CTR_RETURN) {
        if (firstMaster) {
            out = ChannelStage(firstMaster->audio, stage_bufferpoint::INPUT);
            return true;
        }
    }
    if (!isInput && TRACKTYPE_TO_CTR(trImpl->track->type) == TRACK_CTR_MIDIAUDIO) {
        const track_t* const dstTrack = trImpl->track->parent ? trImpl->track->parent : firstMaster;
        if (dstTrack) {
            out = ChannelStage(dstTrack->audio, stage_bufferpoint::INPUT);
            return true;
        }
    }
    return false;
}

bool resolveAudioChannel(const Host::Host* const host, channelnum_t numChannelsTrack, const channel_ref_t& inputChannel, const AudioBlock* const ptrExternalInputs, track_audio_src& out) {
    if (inputChannel.getType() == stage_type::INPUT_EXTERNAL_AUDIO) {
        if (ptrExternalInputs != nullptr) {
            const auto idx = inputChannel.srcChannelOffset;
            const auto size = math::min<channelnum_t>(AudioIO::getNumChannelsFromTrackType(inputChannel.externalInputType), numChannelsTrack);
            if (idx+size <= ptrExternalInputs->channels) {
                out.channels.clear();
                for (channelnum_t ch = 0; ch < size; ++ch) {
                    out.channels.push_back(ptrExternalInputs->buf[idx+ch]);
                }
                out.sampleFormat = host->m_sampleFormatExternal;
                out.samples = ptrExternalInputs->samples;
                return true;
            }
        }
    }
    if (inputChannel.getType() == stage_type::INPUT_AUDIOSTAGE) {
        audio_stage_t* stage = host->getAudioStage(inputChannel.stage.stageRef);
        if (stage) {
            track_audio_src src;
            out.channels.clear();
            auto* buff = &stage->input;
            switch (inputChannel.stage.buffer) {
            case stage_bufferpoint::INPUT:
                buff = &stage->input;
                break;
            case stage_bufferpoint::OUTPUT:
                buff = &stage->output;
                break;
            case stage_bufferpoint::OUTPUT_POST:
                buff = &stage->outputPost;
                break;
            }
            auto numChannels = DAW::AudioIO::getNumChannelsFromTrackType(inputChannel.externalInputType);
            for (uint32_t i = 0; i < numChannels && i + inputChannel.srcChannelOffset < buff->channels; ++i) {
                out.channels.emplace_back(buff->buf[i + inputChannel.srcChannelOffset]);
            }
            out.sampleFormat = stage->sampleFormat;
            out.samples = buff->samples;
            return true;
        }
    }

    if (inputChannel.getType() == stage_type::INPUT_AUDIOSTAGE_EFFECT) {
        audio_stage_t* stage = host->getAudioStage(inputChannel.stage.stageRef);
        if (stage) {
            effectbase* eff = stage->getPluginById(inputChannel.projectGlobalId);
            if (!eff) {
                log_lf(Log::L_WARN, "Effect with id %d is not found on stage\n", inputChannel.projectGlobalId);
                return false;
            }
            if (!eff->blockOutputs) {
                log_lf(Log::L_WARN, "%s Output buffer is null\n", StringAsCStr(eff->getName()));
                return false;
            }
            for (auto& desc : eff->outputChannelsDesc) {
                if (desc.offset == inputChannel.srcChannelOffset) {
                    out.channels.clear();
                    for (auto chIdx = desc.offset; chIdx < desc.offset+desc.count; ++chIdx) {
                        if (chIdx >= eff->blockOutputs->channels) {
                            log_lf(Log::L_WARN, "%s Output buffer has invalid size. Expected %u channels, found %u\n", StringAsCStr(eff->getName()), desc.offset+desc.count, eff->blockOutputs->channels);
                            return false;
                        }
                        out.channels.push_back(eff->blockOutputs->buf[chIdx]);
                    }
                    out.sampleFormat = stage->sampleFormat;
                    out.samples = eff->blockOutputs->samples;
                    return true;
                }
            }
            log_lf(Log::L_WARN, "%s Does not have output with offset %u\n", StringAsCStr(eff->getName()), inputChannel.srcChannelOffset);
        }
    }
    return false;
};

} // namespace DAW

namespace DAW::Host {

void UpdateClapTime(clap_event_transport_t& timeinfo, const sampleformat_t& m_sampleFormatInternal, const project_globals_t& prjGlobals, double samplePos, double dTickPos, playback_state state) {
    timeinfo.header.time = 0;
    double dBarPos = dTickPos / double(TICKS_BAR);
    double dQuarterPos = dTickPos / double(TICKS_QUARTER);
    timeinfo.bar_number = math::floordS32(dBarPos);
    timeinfo.bar_start = math::floordS64(timeinfo.bar_number * 4.0 * CLAP_BEATTIME_FACTOR);
    timeinfo.song_pos_beats = math::floordS64(dQuarterPos * CLAP_BEATTIME_FACTOR);
    timeinfo.loop_start_beats   = math::floordS64((prjGlobals.loopStart / double(TICKS_QUARTER)) * CLAP_BEATTIME_FACTOR);
    timeinfo.loop_end_beats   = math::floordS64(((prjGlobals.loopStart+prjGlobals.loopLen) / double(TICKS_QUARTER)) * CLAP_BEATTIME_FACTOR);
    auto samplePosLoopStart = tickToSampleConvert<double, roundmode::none>(prjGlobals.loopStart, prjGlobals.tempo100, m_sampleFormatInternal.sampleRate);
    auto samplePosLoopEnd = tickToSampleConvert<double, roundmode::none>(prjGlobals.loopStart+prjGlobals.loopLen, prjGlobals.tempo100, m_sampleFormatInternal.sampleRate);
    auto samplePosSongPos = tickToSampleConvert<double, roundmode::none>(dTickPos, prjGlobals.tempo100, m_sampleFormatInternal.sampleRate);
    const double dOneOverSR = 1.0 / m_sampleFormatInternal.sampleRate;
    timeinfo.loop_start_seconds = math::floordS64((samplePosLoopStart * dOneOverSR) * CLAP_SECTIME_FACTOR);
    timeinfo.loop_end_seconds = math::floordS64((samplePosLoopEnd * dOneOverSR) * CLAP_SECTIME_FACTOR);
    timeinfo.song_pos_seconds = math::floordS64((samplePosSongPos * dOneOverSR) * CLAP_SECTIME_FACTOR);

    timeinfo.tempo = prjGlobals.tempo100 / 100.0;
    timeinfo.tempo_inc = 0.0;
    timeinfo.tsig_denom = 1 << prjGlobals.signatureDenom;
    timeinfo.tsig_num = prjGlobals.signatureNum;
    timeinfo.flags = 0;
    timeinfo.flags = CLAP_TRANSPORT_HAS_TEMPO;
    timeinfo.flags |= CLAP_TRANSPORT_HAS_BEATS_TIMELINE;
    timeinfo.flags |= CLAP_TRANSPORT_HAS_SECONDS_TIMELINE;
    timeinfo.flags |= CLAP_TRANSPORT_HAS_TIME_SIGNATURE;
    bool loopEnabed = state != playback_state::status_render && prjGlobals.loopEnabled;
    if (state == playback_state::status_playback || state == playback_state::status_render) {
        timeinfo.flags |= CLAP_TRANSPORT_IS_PLAYING;
    }
    if (loopEnabed) {
        timeinfo.flags |= CLAP_TRANSPORT_IS_LOOP_ACTIVE;
    }
    if (prjGlobals.recordArmed) {
        timeinfo.flags |= CLAP_TRANSPORT_IS_RECORDING;
    }
}

static double PPQ24TickToSample(double midiTickPPQ24, uint32_t bpm100, samplerate_t samplerate, uint32_t blocksize) {
    double seconds = (midiTickPPQ24/(double)(bpm100*24.0)) * 100.0 * 60.0;
    double samplePos = seconds * samplerate;
    return samplePos;
}

void UpdateVst3Time(Steinberg::Vst::ProcessContext& timeinfo, const sampleformat_t& m_sampleFormatInternal, const project_globals_t& prjGlobals, double samplePos, double dTickPos, playback_state state) {
    timeinfo.projectTimeSamples = samplePos;
    timeinfo.continousTimeSamples = samplePos;
    timeinfo.sampleRate = (double) m_sampleFormatInternal.sampleRate;
    timeinfo.systemTime = getTimeMicros() * 1000.0;
    timeinfo.projectTimeMusic = (dTickPos/(double)TICKS_QUARTER);
    timeinfo.tempo = prjGlobals.tempo100/100.0;
    timeinfo.barPositionMusic = floor(dTickPos / (double) TICKS_BAR) * 4;
    timeinfo.cycleStartMusic = (prjGlobals.loopStart/(double)TICKS_QUARTER);
    timeinfo.cycleEndMusic = ((prjGlobals.loopStart+prjGlobals.loopLen)/(double)TICKS_QUARTER);
    timeinfo.timeSigNumerator = static_cast<VstInt32>(prjGlobals.signatureNum);
    timeinfo.timeSigDenominator = 1 << prjGlobals.signatureDenom;

    bool loopEnabed = state != playback_state::status_render && prjGlobals.loopEnabled;
    if (!loopEnabed) {
        timeinfo.cycleStartMusic = 0;
        timeinfo.cycleEndMusic = 0;
    }

    {
        double dPosSeconds = samplePos / timeinfo.sampleRate;
        /* offset in fractions of a second   */
        double dOffsetInSecond = dPosSeconds - floor(dPosSeconds);
        timeinfo.frameRate.framesPerSecond = 24;
        timeinfo.frameRate.flags = 0;
        timeinfo.smpteOffsetSubframes = math::floordS32(dOffsetInSecond * timeinfo.frameRate.framesPerSecond * 80.);
    }


    double midiTickPPQ24 = timeinfo.projectTimeMusic*24.0;
    double samplePosMidiTick = PPQ24TickToSample(midiTickPPQ24, prjGlobals.tempo100, m_sampleFormatInternal.sampleRate, m_sampleFormatInternal.blockSize);
    double samplePosPrevMidiTick = PPQ24TickToSample(math::floord(midiTickPPQ24), prjGlobals.tempo100, m_sampleFormatInternal.sampleRate, m_sampleFormatInternal.blockSize);
    double samplePosNextMidiTick = PPQ24TickToSample(math::ceild(midiTickPPQ24), prjGlobals.tempo100, m_sampleFormatInternal.sampleRate, m_sampleFormatInternal.blockSize);

    double samplePosClosestPPQ24Tick = math::absMin(samplePosPrevMidiTick - samplePosMidiTick, samplePosNextMidiTick - samplePosMidiTick);
    //TODO: assingn nearest clock (can be negative), not next aka soonest
    timeinfo.samplesToNextClock = math::rounddS32(samplePosClosestPPQ24Tick);

    timeinfo.state = 0;
    if (DAW::isPlaybackState(state)) {
        timeinfo.state |= Steinberg::Vst::ProcessContext::kPlaying;
    }
    if (prjGlobals.loopEnabled) {
        timeinfo.state |= Steinberg::Vst::ProcessContext::kCycleActive;
    }
    if (prjGlobals.recordArmed) {
        timeinfo.state |= Steinberg::Vst::ProcessContext::kRecording;
    }
    timeinfo.state |= Steinberg::Vst::ProcessContext::kCycleValid;
    timeinfo.state |= Steinberg::Vst::ProcessContext::kSystemTimeValid;
    timeinfo.state |= Steinberg::Vst::ProcessContext::kContTimeValid;
    timeinfo.state |= Steinberg::Vst::ProcessContext::kProjectTimeMusicValid;
    timeinfo.state |= Steinberg::Vst::ProcessContext::kBarPositionValid;
    timeinfo.state |= Steinberg::Vst::ProcessContext::kTempoValid;
    timeinfo.state |= Steinberg::Vst::ProcessContext::kTimeSigValid;
    timeinfo.state |= Steinberg::Vst::ProcessContext::kSmpteValid;
    timeinfo.state |= Steinberg::Vst::ProcessContext::kClockValid;
}

struct Host::track_block_processing_task_t {
    audiostream_properties_t audioProp;
    project_globals_t projectGlobals;
    /* const */ processing_track_node_t* trackNode = nullptr;
    AudioBlock* ptrExternalInputs = nullptr;
    AudioBlock* ptrExternalOutputs = nullptr;
    double tickPosProcess = 0.0;
    int32_t samplePosProcess = 0;
    playback_state playbackState = playback_state::status_no_process;
    bool inLoop = false;
    int debugLogProcessing = 0;
#if THREADSYNC == THREADSYNC_SEMAPHORE
    std::binary_semaphore* cond;
#elif THREADSYNC == THREADSYNC_ATOMIC
    std::atomic_int_fast8_t* atomicWorkerCount;
#endif
#if DAW_DEBUG_AUDIOGRAPH
    std::shared_ptr<effect_processing_graph_t> effectProcessingGraph;
#endif
};

class TrackBlockProcessTask final : public WorkerThread::ThreadTask {
    process_scratch_buf_t buf;
    Host::track_block_processing_task_t blockProcTask;
    bool inUse = false;
    Host* m_host = nullptr;
public:
    ~TrackBlockProcessTask() {
    }
    struct process_task_stats_t {
        int64_t timeStart = 0;
        int64_t timeEnd = 0;
    };

    void init(Host* _host) {
        this->m_host = _host;
    }

    process_task_stats_t stats;

    bool isInUse() const {
        return inUse;
    }

    void run() override {
        stats.timeStart = getTimeMicros();
        m_host->processGraphNode(buf, blockProcTask);
        stats.timeEnd = getTimeMicros();
    }

    void setTask(Host::track_block_processing_task_t task) {
        reset();
        this->blockProcTask = task;
        inUse = true;
    }

    void resetTask() {
        inUse = false;
    }

    Host::track_block_processing_task_t& getTask() {
        return blockProcTask;
    }
#if THREADSYNC == THREADSYNC_SEMAPHORE
    void notifyCustom() override {
        blockProcTask.cond->release();
    }
#elif THREADSYNC == THREADSYNC_ATOMIC
    void notifyCustom() override {
        auto* atomicCount = blockProcTask.atomicWorkerCount;
        atomicCount->fetch_sub(1, std::memory_order_relaxed);
        atomicCount->notify_all();
    }
#endif
};

/**
 * VST Host implementation internals
 */
class Host::host_impl final : public IDelayLineStorage {
public:
    daw_tls::tlsinstance tls;
    std::array<WorkerThread, MAX_AUDIOPROCESSING_THREADS> threads;
    std::array<TrackBlockProcessTask, MAX_AUDIOPROCESSING_THREADS> tasks;
    std::array<DAW::meter_runningsum, MAX_AUDIO_IO_CHANNELS> meterDataInput;
    std::array<DAW::meter_runningsum, MAX_AUDIO_IO_CHANNELS> meterDataOutput;
    std::shared_ptr<DAW::rmsmeter> meterInput;
    std::shared_ptr<DAW::rmsmeter> meterOutput;
    std::vector<std::shared_ptr<resampler_t>> resamplers;
    std::map<uint32_t, std::shared_ptr<DelayLine>> delayLines;
    std::vector<thread_stats_process_timings_t> blockThreadStats;
    std::vector<thread_stats_process_timings_t> lastBlockThreadStats;
    std::mutex mtx;
    process_scratch_buf_t singleThreadedBuf;
    std::shared_ptr<AudioIO::AudioStream> audioStream;
    std::shared_ptr<processing_graph_t> processingGraph;
    AudioBlock blockInput;
    AudioBlock blockOutput;
    AudioBlock blockExtOut;

    channelnum_t inputChannels = 0;
    channelnum_t outputChannels = 0;
    uint32_t threadsRunningCount = 0;
    uint32_t threadCount = NUM_AUDIOPROCESSING_THREADS_INITIAL;
    uint32_t playThreadId = 0;
#if THREADSYNC == THREADSYNC_SEMAPHORE
    std::binary_semaphore conditionSingleTaskFinished{0};
#elif THREADSYNC == THREADSYNC_ATOMIC
    std::atomic_int_fast8_t atomicWorkerCount{0};
#endif
    int64_t tmLastError = 0;
    explicit host_impl(Host* host) 
    : meterInput(std::make_shared<DAW::rmsmeter>(meterDataInput.data(), meterDataInput.size())),
      meterOutput(std::make_shared<DAW::rmsmeter>(meterDataOutput.data(), meterDataOutput.size()))
    {
        for (TrackBlockProcessTask& task : tasks) {
            task.init(host);
        }
    }
    ~host_impl() {
        stopThreads();
    };

    void resetProjectCache() {
        if (daw_tls::isTlsInitialized()) {
            auto daw = daw_tls::getTls().dawInstance;
            if (daw) {
                dbgassert(daw->getPlayThread()->isLockedOrNotProcessing());
            }
        }
        processingGraph.reset();
        //delayLines.clear();//TODO: this might free a lot of memory and be expensive: profile!
    }

    //TODO: Try avoid this lock
    DelayLine* getProcessingDelayLine(uint32_t id) override {
        std::lock_guard<std::mutex> hold(mtx);
        if (!delayLines.count(id)) {
            delayLines[id] = std::make_shared<DelayLine>();
        }
        return delayLines[id].get();
    }

    std::shared_ptr<resampler_t> getResampler(sampleformat_t in, sampleformat_t out, channelnum_t numChannels, uint32_t idx) {
        auto it = std::find_if(resamplers.begin(), resamplers.end(), [&in,&out,numChannels,idx](std::shared_ptr<resampler_t>& ptr){
            return ptr->in == in && ptr->out == out && ptr->idx == idx && ptr->idx == idx && ptr->numChannels == numChannels;
        });
        if (it == resamplers.end()) {
            oversample_config_t config;
            config.inputSampleRate = in.sampleRate;
            config.outputSampleRate = out.sampleRate;
            config.numChannels = numChannels;
            config.setInputLength(in.blockSize);
            std::shared_ptr<resampler_t> resampler = std::make_shared<resampler_t>(idx, in, out, config);
            resamplers.push_back(resampler);
            return resampler;
        }
        return *it;
    }

    void destroyResamplers() {
        resamplers.clear();
        resamplers.shrink_to_fit();
    }

    void resetResamplers() {
        for (auto& resampler : resamplers) {
            if (resampler) {
                resampler->resetResampler();
            }
        }
    }

    void resetBlock() {
        this->lastBlockThreadStats = this->blockThreadStats;
        this->blockThreadStats.clear();
        for (auto i = threadsRunningCount; i < threadCount && i < MAX_AUDIOPROCESSING_THREADS; i++) {
            threads[i].startThread(StringFormat("AudioProcessingThread %d", i), seqthreads::ThreadType::AudioThread);
            auto task = threads[i].call([]() {
                setSSEFlushDenormals();
            });
            task->wait();
            threadsRunningCount++;
        }
    }

    void startThreads() {
        uint32_t countStarted = 0;
        for (WorkerThread& thread : threads) {
            thread.startThread(StringFormat("AudioProcessingThread %d", countStarted), seqthreads::ThreadType::AudioThread);
            auto task = thread.call([]() {
                setSSEFlushDenormals();
            });
            task->wait();
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
        threadsRunningCount = 0;
    }
};

void Host::getBlockThreadStats(std::vector<thread_stats_process_timings_t>& stats) {
    stats = impl->lastBlockThreadStats;
}

void Host::setThreadCount(uint32_t threadCount) {
    impl->threadCount = math::clamp<uint32_t>(threadCount, 1U, MAX_AUDIOPROCESSING_THREADS);
}

uint32_t Host::getThreadCount() {
    return impl->threadCount;
}

uint32_t Host::getMaxThreadCount() {
    return MAX_AUDIOPROCESSING_THREADS;
}

Host::Host()
    : PluginManager(), impl(new host_impl{this})
{
    allocRingBuffer(ringbuffer, 2);
}

Host::~Host() {
    delete impl;
}

void Host::onTrackLayoutChange() {
    impl->resetProjectCache();
}

Host::audiostream_properties_t getAudioStreamPropertiesForFormat(sampleformat_t sampleFormat, sampleformat_t sampleFormatExternal, int32_t tempo100) {
    Host::audiostream_properties_t prop;
    prop.microSecsPerBlock = (int64_t)sampleFormat.blockSize * 1000000L / (int64_t)sampleFormat.sampleRate;
    prop.ticksPerBlock     = sampleToTickConvert<double, roundmode::none>(sampleFormat.blockSize,
                                                                      tempo100,
                                                                      sampleFormat.sampleRate);

    prop.blockSizeResampled = NumSamplesResampled(sampleFormat.blockSize, sampleFormat.sampleRate, sampleFormatExternal.sampleRate);
    prop.numBlocksInternal  = math::max<uint32_t>(1U, sampleFormatExternal.blockSize/prop.blockSizeResampled);
    prop.numBlocksExternal  = (prop.blockSizeResampled + sampleFormatExternal.blockSize - 1)/sampleFormatExternal.blockSize;
    return prop;
}

const Host::audiostream_properties_t& Host::getAudioStreamProperties() const {
    return this->audioProperties;
}

Host::audiostream_properties_t& Host::updateAudioStreamProperties() {
    this->audioProperties = getAudioStreamPropertiesForFormat(m_sampleFormatInternal, m_sampleFormatExternal, prjGlobals.tempo100);
    return this->audioProperties;
}

void Host::setSampleFormat(const sampleformat_t& _sampleFormat) {
    if (this->m_sampleFormatInternal != _sampleFormat) {
        this->m_sampleFormatInternal = _sampleFormat;
        getHostCallback()->m_sampleFormatInternal = _sampleFormat;
        if (daw_tls::isTlsInitialized()) {
            auto cache = daw_tls::getTls().audioCache;
            if (cache) {
                cache->setSamplerate(m_sampleFormatInternal.sampleRate);
            }
        }
        PluginManager::updateSampleFormat(this->m_sampleFormatInternal);
    }
}

void Host::sendNotesOff(effectbase* plugin) {
    //TODO: check current thread, check if playthread is locked
    if (plugin && plugin->trackImpl) {
        track_t* tr = plugin->trackImpl->getTrack();
        dbgassert(tr);
        track_impl_t* audio = tr->audio;
        if (audio) {
            audio->sendNotesOff();
        }
    }
}

void Host::processMidiRealtimeInput(project_controller_t* ctrl, double dTickPosBlockStart, playback_state state) {
    constexpr bool logProcessedNotes = false;
    // incoming events are shifted 3 blocks into the future
    const auto realtimeMidiDelay = audioProperties.ticksPerBlock * 3;
    // incoming events are kept in buffer for 9 blocks
    const auto realtimeMidiTrackingTime = audioProperties.ticksPerBlock * 9;
    const auto msToTicks = secondsToTicks(1.0 / 1000.0, prjGlobals.tempo100);
    auto& inputDevices = midihost::getInstance()->getDevicesInput();
    const auto midiTimeNow = getMidiTime(nullptr);
    bool notesProcessed = false;
    for (auto& device : inputDevices) {
        std::sort(device.midiMsgs.begin(), device.midiMsgs.end(), [](auto& a, auto& b) {
            return a.timestamp < b.timestamp;
        });
        midi_data_t& midiData = midiRealtimeDeviceInputs[device.deviceName];
        for (auto& msg : device.midiMsgs) {
            auto timeUntilStart = (msg.timestamp - midiTimeNow);
            auto tickEvtDelay = math::rounddS32(dTickPosBlockStart + (timeUntilStart * msToTicks) + realtimeMidiDelay);
            int32_t command = MidiMsgStatus(msg.message) & MIDI_CODE_MASK;
            if (command == MIDI_ON_NOTE && MidiMsgData2(msg.message) != 0) {
                note_miditime_t note;
                note.setRealtime(true);
                note.setIsHeld(true);
                note.time = tickEvtDelay;
                note.len = lenTicksInfinite;
                note.channel = int8_t(msg.message & 0x0F);
                note.pitch = MidiMsgData1(msg.message);
                note.velocity = MidiMsgData2(msg.message);
                note.midiTime = midiTimeNow;
                midiData.notes.m_list.push_back(note);
                notesProcessed = true;
                if (logProcessedNotes) {
                    log_lf(Log::L_DEBUG, "%f note START %s %d\n", dTickPosBlockStart, noteName(note.pitch), note.start());
                }
            } else if ((command == MIDI_ON_NOTE && MidiMsgData2(msg.message) == 0) || command == MIDI_OFF_NOTE) {
                int32_t pitch = MidiMsgData1(msg.message);
                // kill oldest (first) note
                bool fnd = false;
                for (auto& noteHeld : midiData.notes.m_list) {
                    if(noteHeld.pitch == pitch && noteHeld.channel == int8_t(msg.message & 0x0F)) {
                        if (!noteHeld.isHeld()) {
                            continue;
                        }
                        if (noteHeld.start() == tickEvtDelay) {
                            tickEvtDelay += TICKS_16TH/4;
                        }
                        noteHeld.len = math::max<tick_t>((TICKS_16TH >> 4), tickEvtDelay - noteHeld.start());
                        noteHeld.setIsHeld(false);
                        fnd = true;
                        notesProcessed = true;
                        if (logProcessedNotes) {
                            log_printf("%f note KILL %s %d\n", dTickPosBlockStart, noteName(noteHeld.pitch), noteHeld.end());
                        }
                        break;
                    }
                }
                if (!fnd) {
                    log_lf(Log::L_WARN, "MIDI_OFF_NOTE note not found %s tickEvtDelay %d\n", noteName(pitch), tickEvtDelay);
                }

            } else if (command != MIDI_ON_NOTE && command != MIDI_OFF_NOTE) {
                midiData.events.addMidiEvent(tickEvtDelay, msg.message, msg.timestamp);
            }
        }
        device.midiMsgs.clear();
        if (midiData.notes.m_list.size()) {
            auto it = midiData.notes.m_list.begin();
            while (it != midiData.notes.m_list.end()) {
                auto& note = *it;
                if (!note.isHeld() && (midiTimeNow - note.midiTime)*msToTicks > realtimeMidiTrackingTime) {
                    notesProcessed = true;
                    if (logProcessedNotes) {
                        log_lf(Log::L_DEBUG, "%f unheld note remove %s %d\n", dTickPosBlockStart, noteName(note.pitch), note.end());
                    }
                    it = midiData.notes.m_list.erase(it);
                } else {
                    it++;
                }
            }
        }
        if (midiData.events.m_list.size()) {
            auto it = midiData.events.m_list.begin();
            while (it != midiData.events.m_list.end()) {
                auto& evt = *it;
                if ((midiTimeNow - evt.midiTime)*msToTicks > realtimeMidiTrackingTime) {
                    it = midiData.events.m_list.erase(it);
                } else {
                    it++;
                }
            }
        }
        if (notesProcessed) {
            std::sort(midiData.notes.m_list.begin(), midiData.notes.m_list.end());
            // midiData.notes.updateBounds();
        }
        /* if (!midiData.events.m_list.empty()) {
            for (auto& evt : midiData.events.m_list) {
                auto msg = IMidiMsg::FromU32AndTick(evt.message, evt.tick);
                log_lf(Log::L_DEBUG, "Block %f: ctrlEvtsOut %s\n", dTickPosBlockStart, msg.ToString().c_str());
            }
        } */
    }
}

void Host::preExportBegin(project_controller_t* ctrl, export_settings_t& exportSettings) {
    getHostCallback()->isOfflineRendering = true;
    getHostCallback()->m_exportSettings = exportSettings;
}

void Host::postExportEnd(project_controller_t* ctrl, export_settings_t& exportSettings, bool bCancelled) {
    impl->processingGraph = nullptr;
    getHostCallback()->isOfflineRendering = false;
    const tick_t tickBegin = exportSettings.exportPos;
    const tick_t tickEnd = tickBegin + exportSettings.exportLen;
    const samplerate_t sr = m_sampleFormatInternal.sampleRate;
    const int32_t tempo100 = prjGlobals.tempo100;
    const samplerate_t sampleBegin = tickToSampleConvert<samplerate_t, roundmode::floor>(tickBegin, tempo100, sr);
    const samplerate_t sampleEnd = tickToSampleConvert<samplerate_t, roundmode::ceil>(tickEnd, tempo100, sr);
    const samplerate_t numSamples = sampleEnd - sampleBegin;

    for (auto* trackMaster : ctrl->getTracks().getMasterTracksFlatVecRef()) {
        if (!bCancelled && (trackMaster->getStage()->flags & audiostageflags_t::RECORD_OUTPUT) != audiostageflags_t::NONE) {
            String exportPath;
            App::Platform::createUniqueFilename(exportPath, exportSettings.exportPath);
            writeTrackSamplesToDisk(exportPath, trackMaster->getStage(), sampleBegin, numSamples);
        }
        trackMaster->getStage()->flags &= ~audiostageflags_t::RECORD_OUTPUT;
    }
}

void Host::setCustomGraph(const std::shared_ptr<DAW::processing_graph_t>& graph) {
    cacheAudioGraph = true;
    impl->processingGraph = graph;
}

int64_t Host::writeTrackSamplesToDisk(String fOutWave, track_impl_t* trImpl, samplecount_t samplePos, samplecount_t numSamples) {
    if (fOutWave.empty()) {
        dbgassert(0);
        return 0;
    }
    if ((trImpl->flags & audiostageflags_t::RECORD_OUTPUT) == audiostageflags_t::NONE) {
        dbgassert(0);
        return 0;
    }

    log_printf("writeTrackSamplesToDisk %s pos %zd len %zd\n", StringAsCStr(trImpl->getTrack()->name), samplePos, numSamples);
    const samplecount_t SPLIT_SAMPLECOUNT = audiotrack_t::GetSplitSampleLength();
    const samplecount_t samplePosEnd = samplePos + numSamples;
    const channelnum_t numChannels = trImpl->output.channels;

    trImpl->audioOutput.convertToSamples(this);

    std::vector<audiotrack_split_t*> samples;
    trImpl->audioOutput.visitSamples_NoLock([&samples, SPLIT_SAMPLECOUNT, samplePos, samplePosEnd](std::shared_ptr<audiotrack_split_t>& split) {
        auto* ptrSplit = split.get();
        if (ptrSplit && ptrSplit->samplePos + SPLIT_SAMPLECOUNT >= samplePos && ptrSplit->samplePos < samplePosEnd) {
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

    drwav wav;
    if (!drwav_init_file_write_sequential_pcm_frames(&wav, StringAsCStr(fOutWave), &format, numSamples, nullptr)) {
        log_lf(Log::L_WARN, "drwav_init_file_write_sequential_pcm_frames failed\n");
        return 0;
    }

    struct close_wave_file_write {
        drwav* wav;
        ~close_wave_file_write() { drwav_uninit(wav); }
    } closeWaveFile{&wav};


    AudioBlock blockFull(1, SPLIT_SAMPLECOUNT*numChannels);
    samplecount_t samplesWritten = 0;
    samplecount_t samplesWritten2 = 0;
    samplecount_t sampleIdx = 0;
    for (audiotrack_split_t* split : samples) {
        auto* sample = split->getSample();
        dbgassert(split->samplePos + SPLIT_SAMPLECOUNT >= samplePos && split->samplePos < samplePosEnd);

        const auto readBeginOffset = math::clamp<samplecount_t>(samplePos - split->samplePos, 0, SPLIT_SAMPLECOUNT);
        const auto readEndOffset = math::clamp<samplecount_t>(samplePosEnd - split->samplePos, 0, SPLIT_SAMPLECOUNT);
        const auto readLen = math::clamp<samplecount_t>(readEndOffset - readBeginOffset, 0, SPLIT_SAMPLECOUNT);

        dbgassert(sample->nChannels == numChannels);
        dbgassert(sample->nChannels == sample->samples.size());
        dbgassert(sample->nSamples == SPLIT_SAMPLECOUNT);
        dbgassert(sample->nSamples == static_cast<samplecount_t>(sample->samples[0].size()));
        dbgassert(sample->nSamples == static_cast<samplecount_t>(sample->samples[1].size()));
        dbgassert(blockFull.samples == sample->nSamples*sample->nChannels);

        float* out0 = blockFull.buf[0];
        for (samplecount_t nSample = 0; nSample < readLen; ++nSample) {
            for (channelnum_t ch = 0; ch < sample->nChannels; ++ch) {
                *out0++ = sample->samples[ch][readBeginOffset + nSample];
            }
        }
        samplesWritten2 += readLen;
        samplesWritten += samplecount_t(drwav_write_pcm_frames(&wav, readLen, blockFull.buf[0]));
        sampleIdx++;
    }

    log_lf(Log::L_INFO,"Wrote %zd chunks/%zd samples into %s\n", sampleIdx, samplesWritten, StringAsCStr(fOutWave));
    log_lf(Log::L_INFO,"Processed %zu splits and %zd samples\n", samples.size(), samplesWritten2);
    if (samplesWritten != numSamples) {
        log_lf(Log::L_WARN, "Wrote %zd samples, expected %zd!\n", samplesWritten, numSamples);
    }

    return samplesWritten;
}

std::vector<float>& AllocateScratchBuffer(process_scratch_buf_t& tmp, int32_t idx, size_t size) {
    if (tmp.scratchBuffers.empty()) {
        tmp.scratchBuffers.resize(4);
    }
    dbgassert(idx < 4);
    tmp.scratchBuffers[idx].resize(size);
    return tmp.scratchBuffers[idx];
}
AudioBlock& AllocateScratchAudioBuffer(process_scratch_buf_t& tmp, channelnum_t numChannels, samplecount_t numSamples) {
    if (tmp.block.channels < numChannels) {
        tmp.block = AudioBlock(numChannels, numSamples);
    } else {
        tmp.block.realloc(numSamples);
    }
    return tmp.block;
}
AudioBlockBase<double>& AllocateSummingAudioBuffer(process_scratch_buf_t& tmp, channelnum_t numChannels, samplecount_t numSamples) {
    if (tmp.blockSumming.channels < numChannels) {
        tmp.blockSumming = AudioBlockBase<double>(numChannels, numSamples);
    } else {
        tmp.blockSumming.realloc(numSamples);
    }
    return tmp.blockSumming;
}

template<typename FPTypeSrc, typename FPTypeDst>
void MixWithGainAndPanAutomation(const Host* host, process_scratch_buf_t& tmp, AudioBlockBase<FPTypeSrc>* in, AudioBlockBase<FPTypeDst>* out, const automated_param_connection_t& autParGain, const automated_param_connection_t& autParPan, double tickBegin, double tickEnd, playback_state state, float MTR_CEIL, float DBFS_MUTE_POS) {
    dbgassert(autParGain.atl);
    dbgassert(autParPan.atl);
    const auto numSamples = out->samples;
    const auto oneOverCenterGain = 1.0f / DAW::Panning::GetCenterGain();
    auto& bufGain = AllocateScratchBuffer(tmp, 0, numSamples);
    auto& bufPanL = AllocateScratchBuffer(tmp, 1, numSamples);
    auto& bufPanR = AllocateScratchBuffer(tmp, 2, numSamples);
    float* panLR[2] = { bufPanL.data(), bufPanR.data() };
    autParGain.atl->sampleAutomation(host, autParGain.paramIdx, tickBegin, tickEnd, state, numSamples, bufGain.data());
    autParPan.atl->sampleAutomation(host, autParPan.paramIdx, tickBegin, tickEnd, state, numSamples, bufPanL.data());
    for (samplecount_t i = 0; i < numSamples; ++i) {
        dsp_util::getGainLvlWithRange(math::clamp(bufGain[i], 0.0f, 1.0f), MTR_CEIL, DBFS_MUTE_POS, bufGain[i]);
        DAW::Panning::CalculatePanning<DAW::Panning::PanLaw::SIN_4_5DB>(math::clamp(bufPanL[i], 0.0f, 1.0f), &bufPanL[i], &bufPanR[i]);
        bufGain[i] *= oneOverCenterGain;
    }
    DAW::Panning::MultiplyAutomation(in, out, bufGain.data(), panLR); 
}

void MixInputs(const Host* host, const processing_track_node_t& node, process_scratch_buf_t& tmp, IDelayLineStorage* delayLines, AudioBlock* ptrBlockMixDst, const std::vector<DAW::track_source_t>& allSources, channelnum_t numChannelsTrack, samplerate_t trackNodeInputLatency, double processingPos, double tickBlockEnd, playback_state state, AudioBlock* ptrExternalInputs) {
    bool hasSolo = std::any_of(allSources.cbegin(), allSources.cend(), DAW::isTrackSrcSolod);
    const bool is64BitSumming = host->bSummingIn64Bit;
    AudioBlock& tmpBlock = AllocateScratchAudioBuffer(tmp, ptrBlockMixDst->channels, ptrBlockMixDst->samples);
    AudioBlockBase<double>* tmpBlockSumming = nullptr;
    if (is64BitSumming) {
        tmpBlockSumming = &AllocateSummingAudioBuffer(tmp, ptrBlockMixDst->channels, ptrBlockMixDst->samples);
    }
    static DAW_CXX_CONSTINIT thread_local track_audio_src srcTemp{};
    auto& src = srcTemp;
    int32_t numMixed = 0;
    for (const DAW::track_source_t& tracksrc : allSources)
    {
        if (hasSolo && !DAW::isTrackSrcSolod(tracksrc))
            continue;
        if (DAW::isChannelConnected(tracksrc.channel)) {
            if (DAW::resolveAudioChannel(host, numChannelsTrack, tracksrc.channel, ptrExternalInputs, src)) {
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
                dbgassert(trackNodeInputLatency >= tracksrc.latency);
                samplecount_t delayToMaxInputLatency = trackNodeInputLatency - tracksrc.latency;

                AudioBlock srcBlock = src.toAudioBlock();
                DelayLine* delayLine = nullptr;
                if (delayToMaxInputLatency > 0) {
                    delayLine = delayLines->getProcessingDelayLine(tracksrc.trackEdgeId);
                    delayLine->write(&srcBlock, delayToMaxInputLatency);
                }
                auto autParGain = GetParameterModulationFromRouting(host, tracksrc.gainAutomation);
                auto autParPan = GetParameterModulationFromRouting(host, tracksrc.panAutomation);
                channelnum_t dstChannelCount = ptrBlockMixDst->channels;
                if (node.type == DAW::track_node_type_t::EFFECT) {
                    for (auto& desc : node.effectOptional->inputChannelsDesc) {
                        if (desc.offset == tracksrc.channel.dstChannelOffset) {
                            dstChannelCount = desc.count;
                            break;
                        }
                    }
                }
                if (node.type == DAW::track_node_type_t::TRACK) {
                    dstChannelCount = AudioIO::getNumChannelsFromTrackType(AudioIO::getTrackTypeFromNumChannels(ptrBlockMixDst->channels));
                }
                auto mixWithAutomation = [&](auto* blockMixToOffset) {
                    if (autParGain.type <= automation_routing_type::ROUTING_CONSTANT 
                        && (autParPan.type == automation_routing_type::ROUTING_NONE 
                            ||  (autParPan.type == automation_routing_type::ROUTING_CONSTANT && autParPan.atl->getParamValue(autParPan.paramIdx) == 0.5f))) {
                        float fGainTrack = 1.0f;
                        bool bIsNotMuted = true;
                        if (autParGain.type != automation_routing_type::ROUTING_NONE) {
                            bIsNotMuted = dsp_util::getGainLvl(autParGain.atl->getParamValue(autParGain.paramIdx), fGainTrack);
                        }
                        /* Fast path */
                        if (bIsNotMuted) {
                            if (delayToMaxInputLatency > 0) {
                                dbgassert(delayLine);
                                blockMixToOffset->addFromDelayLineOp(delayLine, delayToMaxInputLatency, mix_op::ADD, fGainTrack);
                            } else {
                                blockMixToOffset->addFromOp(&srcBlock, mix_op::ADD, fGainTrack);
                            }
                        }
                    } else {
                        AudioBlock* inputBlock = &srcBlock;
                        if (delayToMaxInputLatency > 0) {
                            tmpBlock.clear();
                            tmpBlock.addFromDelayLineOp(delayLine, delayToMaxInputLatency, mix_op::ADD, 1.0);
                            inputBlock = &tmpBlock;
                        }
                        MixWithGainAndPanAutomation(host, tmp, inputBlock, blockMixToOffset, autParGain, autParPan, processingPos, tickBlockEnd, state, dsp_util::MTR_CEIL, dsp_util::DBFS_MUTE_POS);
                    }
                };
                if (is64BitSumming) {
                    if (numMixed == 0 && tmpBlockSumming) {
                        tmpBlockSumming->clear();
                    }
                    auto blockMixToOffset = tmpBlockSumming->SubChannelsBlock(tracksrc.channel.dstChannelOffset, dstChannelCount);
                    mixWithAutomation(&blockMixToOffset);
                } else {
                    auto blockMixToOffset = ptrBlockMixDst->SubChannelsBlock(tracksrc.channel.dstChannelOffset, dstChannelCount);
                    mixWithAutomation(&blockMixToOffset);
                }
                numMixed++;
            }
        }
    }
    if (is64BitSumming && numMixed > 0 && tmpBlockSumming) {
        ptrBlockMixDst->copyFrom(tmpBlockSumming);
    }
}

int32_t Host::processRender(project_controller_t* ctrl, int32_t sample, double posDouble) {
    dbgassert(ctrl);
    dbgassert(m_sampleFormatInternal.blockSize > 0);
    dbgassert(m_sampleFormatInternal.sampleRate > 0);
    const bool enableProfiling = (dbgStep%333) != 0;
    // AudioBlock::BeginTrace();

    project_t* const project = ctrl->getProject();

    auto timeNow_i64 = getTimeMicros();
    if (0 != stats.lastInvocationTime_i64 && enableProfiling) {
        auto timeDelta = timeNow_i64 - stats.lastInvocationTime_i64;
        stats.timings["Block.timeDelta"] = timeDelta;
    }
    stats.lastInvocationTime_i64 = timeNow_i64;



    if (enableProfiling) {
        timerProfile.reset();
    }

    /** Build the audio graph **/
    if (!cacheAudioGraph || !impl->processingGraph) {
        bool bWasValid = impl->processingGraph != nullptr;
        if (!DAW::buildProcessingGraph(this, project, project->trackList.getAllTracksFlatVecRef(), impl->processingGraph)) {
            impl->processingGraph = nullptr;
            if (bWasValid)
                log_lf(Log::L_ERROR, "Failed building track graph\n");
            return 0;
        }
    }

    if (enableProfiling) {
        stats.timings["Block.GraphBuild"] = timerProfile.getTimeReset();
    }

    auto& processingGraph = impl->processingGraph;
#if DAW_DEBUG_AUDIOGRAPH
    this->lastTrackGraph = processingGraph->trackGraph;
    this->lastProcessingList= processingGraph;
#endif

    timerBlock.reset();
    const sampleformat_t& sampleFormat = this->m_sampleFormatInternal;
    const audiostream_properties_t audioProp = updateAudioStreamProperties();

    int32_t nBlocksProcessed = 0;

    const playback_state state = playback_state::status_render;

    PluginManager::onBeforeBlock(ctrl->getGlobals(), sample, posDouble, state);
    PluginManager::UpdateVstTime(getHostCallback()->m_vstTimeInfo, m_sampleFormatInternal, ctrl->getGlobals(), sample, posDouble, state);

    int32_t samplePosProcess = sample;
    double tickPosProcess = posDouble;
    
    if (impl->blockInput.channels != impl->inputChannels || impl->blockInput.samples != sampleFormat.blockSize) {
        impl->blockInput = AudioBlock(impl->inputChannels, sampleFormat.blockSize);
    }
    if (impl->blockOutput.channels != impl->outputChannels || impl->blockOutput.samples != sampleFormat.blockSize) {
        impl->blockOutput = AudioBlock(impl->outputChannels, sampleFormat.blockSize);
    }
    dsp_util::fillBlock(impl->blockInput, 0.0f);
    dsp_util::fillBlock(impl->blockOutput, 0.0f);

    if (enableProfiling) {
        timerProfile.reset();
    }

    nBlocksProcessed += processGraph(ctrl, audioProp, processingGraph.get(), &impl->blockInput, &impl->blockOutput, samplePosProcess, tickPosProcess, state, false);
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
            if (tracDst.type == DAW::stage_type::INPUT_DEFAULT) {
                DAW::channel_ref_t tmp;
                if (DAW::resolveDefaultConnection(this, project, trackImpl, false, tmp)) {
                    tracDst = tmp;
                }
            }
            if (DAW::isChannelConnected(tracDst) && tracDst.getType() == DAW::stage_type::INPUT_EXTERNAL_AUDIO) {
                // TODO: latency compensate (add external output nodes to graph)
                /* Calculate master tracks gain level */
                //TODO: use MixInputs
                float fGainMaster;
                if (dsp_util::getGainLvl(trackImpl->mixer.getParam(PARAM_TRACK_GAIN)->getValue(), fGainMaster)) {
                }
                auto routedOutputChannelCount = DAW::AudioIO::getNumChannelsFromTrackType(tracDst.externalInputType);
                auto trackSubChannelOutput = trackImpl->output.SubChannelsBlock(0, routedOutputChannelCount);
                impl->blockOutput.SubChannelsBlock(tracDst.srcChannelOffset, routedOutputChannelCount)
                        .addFromOp(&trackSubChannelOutput, mix_op::ADD, dsp_util::clampReadGain(fGainMaster));

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
    // for (track_t* track : project->trackList) {
    //     track_impl_t* trAudio = track->audio;
    //     if (!trAudio)
    //         continue;
    //     float fGainTrack;
    //     dsp_util::getGainLvl(trAudio->mixer.getParamValue(PARAM_TRACK_GAIN), fGainTrack);
    //     trAudio->meter.update(&trAudio->output, fGainTrack);
    //     trAudio->meterInput.update(&trAudio->input, 1.0f);
    // }
    if (enableProfiling) {
        stats.timings["Block.UpdateMeters"] = timerProfile.getTimeReset();
    }
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

    //AudioBlock::EndTrace();

    if (!bypassSampleConversion) {
        samplecount_t samplesCopied = 0;
        hires_timer_t timerConvert;
        for (track_t* tr : project->trackList) {
            track_impl_t* trAudio = tr->audio;
            if (static_cast<bool>(trAudio->flags & audiostageflags_t::CONVERT_OUTPUT)) {
                samplesCopied += trAudio->audioOutput.convertToSamples(this);
            }
        }
        if (enableProfiling) {
            stats.timings["Block.RecordedSampleConversionPost"] = timerProfile.getTimeReset();
            stats.timings["Block.RecordedSamples"] = samplesCopied;
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

int32_t Host::processPlayback(project_controller_t* ctrl, int32_t sample, double posDouble, playback_state state, bool inLoop) {
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
    const audiostream_properties_t audioProp = updateAudioStreamProperties();

    std::shared_ptr<resampler_t> resamplerOutput = impl->getResampler(sampleFormat, m_sampleFormatExternal, impl->outputChannels, 0);
    std::shared_ptr<resampler_t> resamplerInput = impl->getResampler(m_sampleFormatExternal, sampleFormat, impl->inputChannels, 1);

    int queueSizeInput = 0;
    int queueSizeOutput = 0;
    auto const stream = impl->audioStream;
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
    stats.resamplerDelayInput = resamplerInput->getResamplerDelay();
    stats.resamplerDelayOutput = resamplerOutput->getResamplerDelay();

    while (queueSizeInput) {
        AudioBuffer* ptrExternalInputs = nullptr;
        if (stream && stream->try_dequeueInput(ptrExternalInputs)) {
            if (enableProfiling) timerProfile.reset();
            resamplerInput->push(*ptrExternalInputs->output, ptrExternalInputs->time);
            if (enableProfiling) stats.timings["Block.ResampleInput"] = timerProfile.getTime();
            ptrExternalInputs->inUse = false;
        }
        queueSizeInput--;
    }


    /*
     * Start processing when the output ring buffer is less than half filled.
     * We also have to wait for the input resampler to have enough data to start processing.
     */
    const bool canProcess = queueSizeOutput < RING_BUF_SIZE / 2 && resamplerInput->numBlocksToPop() >= audioProp.numBlocksInternal;

    if (enableProfiling) timerProfile.reset();

    stats.timings["Block.ValidateIds"] = timerProfile.getTimeReset();

    int32_t nBlocksProcessed = 0;

    if (canProcess) {
        PluginManager::onBeforeBlock(ctrl->getGlobals(), sample, posDouble, state);
        PluginManager::UpdateVstTime(getHostCallback()->m_vstTimeInfo, m_sampleFormatInternal, ctrl->getGlobals(), sample, posDouble, state);
        processMidiRealtimeInput(ctrl, posDouble, state);
        if (enableProfiling) {
            stats.timings["Block.MidiRealtimeInput"] = timerProfile.getTime();
        }

        if (enableProfiling) {
            timerProfile.reset();
        }

        /** Build the audio graph **/
        if (!cacheAudioGraph || !impl->processingGraph) 
        {
            bool bWasValid = impl->processingGraph != nullptr;
            if (!DAW::buildProcessingGraph(this, project, project->trackList.getAllTracksFlatVecRef(), impl->processingGraph)) {
                impl->processingGraph = nullptr;
                if (bWasValid)
                    log_lf(Log::L_ERROR, "Failed building track graph\n");
            }
        }

        if (enableProfiling) {
            stats.timings["Block.GraphBuild"] = timerProfile.getTimeReset();
        }

#if DAW_DEBUG_AUDIOGRAPH
        this->lastTrackGraph = processingGraph->trackGraph;
        this->lastProcessingList= processingGraph;
#endif
    }

    if (canProcess && impl->processingGraph) {
#if ENABLE_ALLOCATION_TRACKING != 0
        DebugAlloc::beginTrace();
#endif
        int64_t timeRouting = 0;
        int64_t timeProcessing = 0;
        int64_t timeResampleOutput = 0;
        auto processingGraph = impl->processingGraph; // we need to make a copy
        // auto ticksLatency = sampleToTickConvert<double, roundmode::none>(processingGraph->trackGraph->maxLatencySamples, prjGlobals.tempo100, m_sampleFormatInternal.sampleRate);
        const auto samplesLatencyOutput = resamplerOutput->getResamplerDelay() + (stream ? stream->getStreamOutputLatency() : 0);
        const auto ticksLatencyOutput = sampleToTickConvert<double, roundmode::none>(samplesLatencyOutput, prjGlobals.tempo100, m_sampleFormatExternal.sampleRate);
        const auto ticksLatencyInput = sampleToTickConvert<double, roundmode::none>(processingGraph->trackGraph->maxLatencySamples, prjGlobals.tempo100, m_sampleFormatInternal.sampleRate);
        for (uint32_t i = 0; i < audioProp.numBlocksInternal; i++) {
            int32_t samplePosProcess = sample + sampleFormat.blockSize*i; //TODO: inspect precision
            double tickPosProcess = posDouble + audioProp.ticksPerBlock*i;
            AudioBufferTimeInfo bufferTimeInfo{ };
            resamplerInput->pop(bufferTimeInfo, impl->blockInput);
            impl->meterInput->update(&impl->blockInput, 1.0f);
            impl->meterInput->onTick(impl->blockInput.samples / double(m_sampleFormatInternal.sampleRate));


            //TODO: avoid allocation
            if (impl->blockExtOut.channels != resamplerOutput->numChannels || impl->blockExtOut.samples != sampleFormat.blockSize) {
                impl->blockExtOut = AudioBlock(resamplerOutput->numChannels, sampleFormat.blockSize);
            }
            dsp_util::fillBlock(impl->blockExtOut, 0.0f);
            if (enableProfiling) {
                timerProfile.reset();
            }
            nBlocksProcessed += processGraph(ctrl, audioProp, processingGraph.get(), &impl->blockInput, &impl->blockExtOut, samplePosProcess, tickPosProcess, state, inLoop);

            if (enableProfiling) {
                timeProcessing += timerProfile.getTimeReset();
            }
            auto& allSources = processingGraph->trackGraph->externalOutputRouting;

            processing_track_node_t trackNode{};
            trackNode.type = track_node_type_t::TRACK;
            trackNode.inputLatency = processingGraph->trackGraph->maxLatencySamples;
            MixInputs(this, trackNode, impl->singleThreadedBuf, this->impl, &impl->blockExtOut, allSources, impl->blockExtOut.channels, trackNode.inputLatency, tickPosProcess - ticksLatencyInput, tickPosProcess - ticksLatencyInput + audioProp.ticksPerBlock, state, &impl->blockInput);
            if (enableProfiling) {
                timeRouting += timerProfile.getTimeReset();
            }
            impl->meterOutput->update(&impl->blockExtOut, 1.0f);
            impl->meterOutput->onTick(impl->blockExtOut.samples / double(m_sampleFormatInternal.sampleRate));
            bufferTimeInfo.outputTickPos = tickPosProcess - (ticksLatencyInput + ticksLatencyOutput);
            bufferTimeInfo.bResyncPos = this->bResyncOutputTime;
            this->bResyncOutputTime = false;
            resamplerOutput->push(impl->blockExtOut, bufferTimeInfo);
            if (enableProfiling) {
                timeResampleOutput += timerProfile.getTimeReset();
            }
        }
        if (enableProfiling) {
            stats.timings["Block.Tracks"] = timeProcessing/audioProp.numBlocksInternal;
            stats.timings["Block.TrackOutputRouting"] = timeRouting/audioProp.numBlocksInternal;
            stats.timings["Block.ResampleOutput"] = timeResampleOutput/audioProp.numBlocksInternal;
        }
#if ENABLE_ALLOCATION_TRACKING != 0
        static DebugAlloc::AllocStats lastAllocStats;
        lastAllocStats = DebugAlloc::endTrace();
        static int ticks = 0;
        if (++ticks > 50) {
            ticks = 0;
            String stats = lastAllocStats.toString();
            log_lf(Log::L_DEBUG, "AllocStats: %s\n", StringAsCStr(stats));
        }
#endif
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
    if (nResampledOutputBlocks > 0 && stream && stream->getOutputQueueSize() < RING_BUF_SIZE*2/3) {
        auto& writePos = ringbuffer.writePos;
        //TODO: this is incorrect, the resampler should keep track of sample/tick position, but right now these fields are not read on output side
        int64_t time0 = 0;
        int64_t time1 = 0;
        int64_t time2 = 0;
        while (nResampledOutputBlocks > 0 && stream->getOutputQueueSize() < RING_BUF_SIZE*2/3) {
            if (enableProfiling) timerBlock.reset();
            AudioBufferTimeInfo bufferTimeInfo{ };
            resamplerOutput->pop(bufferTimeInfo, impl->blockOutput);
            if (enableProfiling) time0 += timerBlock.getTimeReset();
            AudioBuffer** buffers = ringbuffer.buffers;
            AudioBuffer* const ptrExternalOutputs = buffers[writePos%RING_BUF_SIZE];
            dbgassert(!ptrExternalOutputs->inUse);
            ptrExternalOutputs->output->realloc(m_sampleFormatExternal.blockSize);
            ptrExternalOutputs->output->copyFrom(&impl->blockOutput);
            if (enableProfiling) time1 += timerBlock.getTimeReset();
            ptrExternalOutputs->inUse = true;
            ptrExternalOutputs->time = bufferTimeInfo;
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
            trAudio->meter.update(&trAudio->outputPost, 1.0f);
            trAudio->meterInput.update(&trAudio->input, 1.0f);
        }
        if (enableProfiling) {
            stats.timings["Block.UpdateMeters"] = timerProfile.getTimeReset();
        }

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
            samplecount_t samplesCopied = 0;
            for (track_t* tr : project->trackList) {
                track_impl_t* trAudio = tr->audio;
                if (static_cast<bool>(trAudio->flags & audiostageflags_t::CONVERT_OUTPUT)) {
                    samplesCopied += trAudio->audioOutput.convertToSamples(this);
                }
            }
            if (enableProfiling) {
                stats.timings["Block.BufferedAudioConversion"] = timerProfile.getTimeReset();
                stats.timings["Block.BufferedAudioSamplesCopied"] = samplesCopied;
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

int32_t Host::processGraphNode(process_scratch_buf_t& tmp, track_block_processing_task_t& req) /*const*/ {
    const sampleformat_t& sampleFormat = this->m_sampleFormatInternal;
    const double ticksPerBlock = req.audioProp.ticksPerBlock;

    const int32_t samplePosProcess = req.samplePosProcess;
    const DAW::processing_track_node_t& trackNode = *req.trackNode;
    track_t* const track = trackNode.trackOptional;
    track_impl_t* const trackImpl = track->audio;
    const auto playbackState = req.playbackState;
    const double ticksLatency = sampleToTickConvert<double, roundmode::none>(trackNode.inputLatency, prjGlobals.tempo100, sampleFormat.sampleRate);
    const samplecount_t sampleLatencyCompensated = samplePosProcess - trackNode.inputLatency;
    const double tickLatencyCompensated = req.tickPosProcess - ticksLatency;
    tick_t processingPos = math::floordS32(tickLatencyCompensated);
    tick_t tickBlockEnd = math::floordS32(tickLatencyCompensated + ticksPerBlock);

    tick_t loopCutStart = -1;
    tick_t loopCutEnd = -1;
    tick_t cursorPos = prjGlobals.cursor.cursorPos;
    if (req.inLoop) {
        loopCutStart = prjGlobals.loopStart;
        loopCutEnd = prjGlobals.loopStart+prjGlobals.loopLen;
    }

    bool bIsRecording = prjGlobals.recordArmed && isSet(trackImpl->flags, audiostageflags_t::RECORD_ARMED);
    if (!bIsRecording && isSet(trackImpl->flags, audiostageflags_t::RECORD_FORCE)) {
        auto renderRange = getHostCallback()->m_exportSettings;
        auto begin = tickLatencyCompensated;
        auto end = tickLatencyCompensated + ticksPerBlock;
        bIsRecording = end > renderRange.exportPos && begin <= renderRange.exportPos + renderRange.exportLen;
    }


    tmp.timer.reset();

    /**
     * Apply input stage latency time info to all effects.
     * This is not per plugin compensated.
     * The correct time will be assinged in vstplugin::process.
     * This is here as a backup measure, in case vstplugin::process is not called.
     */
    
    PluginManager::UpdateVstTime(tmp.timeinfo, m_sampleFormatInternal, req.projectGlobals, sampleLatencyCompensated, tickLatencyCompensated, playbackState);
    for (effectbase* plugin : trackImpl->effects) {
        if (plugin->getModuleType() == MODULE_TYPE_VST2) {
            auto* ptr = dynamic_cast<vstplugin*>(plugin)->getLocalTimeInfoPtr();
            if (ptr) {
                *ptr = tmp.timeinfo;
            }
        }
    }

    /**
     * Read and apply automation.
     */
    trackImpl->updateAutomatableTargets(this, tickLatencyCompensated, playbackState);

    track->getStage()->procStats.timeTrackApplyAutomation = tmp.timer.getTime();
    dbgassert(tickBlockEnd-processingPos < math::ceildS32(ticksPerBlock+1));

    trackImpl->input.realloc(sampleFormat.blockSize);
    trackImpl->output.realloc(sampleFormat.blockSize);

    dsp_util::fillBlock(trackImpl->input, 0.0f);

    int32_t midiProcessFlags = 0;
    switch (playbackState) {
        case playback_state::status_playback:
            if (bIsRecording) {
                midiProcessFlags = MidiFlags::PROCESS_REALTIME|MidiFlags::PROCESS_ARP;
            } else {
                midiProcessFlags = MidiFlags::PROCESS_REALTIME|MidiFlags::PROCESS_CLIPS|MidiFlags::PROCESS_ARP;
            }
            break;
        case playback_state::status_render:
            if (bIsRecording) {
                midiProcessFlags = MidiFlags::PROCESS_ARP;
            } else {
                midiProcessFlags = MidiFlags::PROCESS_CLIPS|MidiFlags::PROCESS_ARP;
            }
            break;
        case playback_state::status_stop:
            midiProcessFlags = MidiFlags::PROCESS_REALTIME|MidiFlags::PROCESS_ARP;
            break;
        default:
        case playback_state::status_no_process:
            midiProcessFlags = 0;
            break;
    }
    // if (track->type != TRACK_TYPE_MIDI) {
    //     midiProcessFlags &= ~MidiFlags::PROCESS_ARP;
    // }

    tmp.timer.reset();
    trackImpl->processMidiInput(playbackState, midiProcessFlags, cursorPos, processingPos, tickBlockEnd, loopCutStart, loopCutEnd, prjGlobals, trackNode.inputLatency, midiRealtimeDeviceInputs);

    trackImpl->procStats.timeTrackProcessMidi = tmp.timer.getTime();

    const auto numChannelsTrack = trackImpl->input.channels;

    auto& allSources = tmp.trackSources;
    allSources.clear();
    allSources.insert(allSources.end(), trackNode.pulls.cbegin(), trackNode.pulls.cend());
    allSources.insert(allSources.end(), trackNode.pushs.cbegin(), trackNode.pushs.cend());
    tmp.timer.reset();
    MixInputs(this, trackNode, tmp, this->impl, &trackImpl->input, allSources, numChannelsTrack, trackNode.inputLatency, tickLatencyCompensated, tickLatencyCompensated + ticksPerBlock, playbackState, req.ptrExternalInputs);
    trackImpl->procStats.timeTrackMixInputs = tmp.timer.getTime();

    /* Store block in audioInput memory */
    if (DAW::isPlaybackState(playbackState)) {
        if (track->type == TRACK_TYPE_AUDIO && bIsRecording) {
            if (sampleLatencyCompensated >= 0) {
                auto firstPair = trackImpl->input.SubChannelsBlock(0, 2);
                trackImpl->audioInput.store(&firstPair, sampleLatencyCompensated);
                trackImpl->audioInput.convertToSamples(this);
                // trackImpl->recorder.samplesRecorded += trackImpl->audioInput.convertToSamples(this);
            } else {
                log_printf("cannot write to negative offset %zd (samplepos %d - stage.latencyOutput %zd)\n", sampleLatencyCompensated, samplePosProcess, trackImpl->getInputLatency());
            }
        }
    }


    /* Update currently recording clip */
    trackImpl->recorder.update(playbackState, sampleLatencyCompensated, sampleLatencyCompensated + sampleFormat.blockSize, processingPos, tickBlockEnd, track->type, bIsRecording);

    trackImpl->procStats.timeTrackRecordPre = tmp.timer.getTime();

    /* Read audio clips and add them */
    if (DAW::isPlaybackState(playbackState)) {
        AudioBlock& tmpBlock = AllocateScratchAudioBuffer(tmp, trackImpl->input.channels, trackImpl->input.samples);
        tmpBlock.clear();
        trackImpl->fillAudio(processingPos, tickBlockEnd, loopCutStart, loopCutEnd, prjGlobals, sampleLatencyCompensated, tmpBlock.samples, tmpBlock);
        trackImpl->input.addFromOp(&tmpBlock, mix_op::ADD, 1.0f);
    }
    trackImpl->procStats.timeTrackFillAudioClips = tmp.timer.getTime();

    dbgassert(
            this->m_sampleFormatInternal == trackImpl->sampleFormat
            && trackImpl->input.samples == trackImpl->sampleFormat.blockSize
            && trackImpl->output.samples == trackImpl->sampleFormat.blockSize
            && trackImpl->outputPost.samples == trackImpl->sampleFormat.blockSize
            && trackImpl->sampleFormat.blockSize > 0
            && trackImpl->sampleFormat.sampleRate > 0);

    {
        auto& effProcessingGraph = trackImpl->processingGraph;
        if (!cacheAudioGraph || !effProcessingGraph)
        {
            bool bWasValid = effProcessingGraph != nullptr;
            if (!DAW::buildEffectProcessingGraph(this, nullptr, trackImpl, effProcessingGraph)) {
                effProcessingGraph = nullptr;
                if (bWasValid)
                    log_lf(Log::L_ERROR, "Failed building effect graph\n");
            }
#if DAW_DEBUG_AUDIOGRAPH
            req.effectProcessingGraph = effProcessingGraph;
#endif
        }
        if (effProcessingGraph) {
            /* Processes audio/midi tracks plugin chain */
            processAudio(tmp, trackImpl, &trackImpl->input, &trackImpl->output, req.projectGlobals, tickLatencyCompensated, sampleLatencyCompensated, (int32_t)sampleFormat.blockSize, playbackState,
                        effProcessingGraph.get(), trackImpl);
            trackImpl->procStats.numBlocksProcessed++;
        }
    }
    trackImpl->procStats.timeTrackProcessAudio = tmp.timer.getTime();


    trackImpl->outputPost.clear();


    /* Mix outputPos, applies track gain and panning */
    {
        // auto postStageSamplePos = samplePosProcess - (trackNode.inputLatency + trackNode.internalLatency);
        // auto postStageSamplePosFloored = math::floordS64(postStageSamplePos);
        const double postStageTicksLatency = sampleToTickConvert<double, roundmode::none>(trackNode.inputLatency + trackNode.internalLatency, prjGlobals.tempo100, sampleFormat.sampleRate);
        tick_t postStageTickLatencyCompensated = math::floordS32(req.tickPosProcess - postStageTicksLatency);
        tick_t postStageTickEnd = math::floordS32(postStageTickLatencyCompensated + ticksPerBlock);

        // float fGainTrackLin = trackImpl->mixer.getParam(PARAM_GAIN)->getValue();
        auto autParGain = GetParameterModulationFromRouting(this, GetRoutingFromDestinationParam(&trackImpl->mixer, PARAM_GAIN));
        auto autParPan = GetParameterModulationFromRouting(this, GetRoutingFromDestinationParam(&trackImpl->mixer, PARAM_PAN));
        if (autParGain.type <= automation_routing_type::ROUTING_CONSTANT 
            && (autParPan.type <= automation_routing_type::ROUTING_NONE 
                ||  (autParPan.type <= automation_routing_type::ROUTING_CONSTANT && autParPan.atl->getParamValue(autParPan.paramIdx) == 0.5f))) {
            float fGainTrack = 1.0f;
            bool bIsNotMuted = true;
            if (autParGain.type != automation_routing_type::ROUTING_NONE) {
                bIsNotMuted = dsp_util::getGainLvl(autParGain.atl->getParamValue(autParGain.paramIdx), fGainTrack);
            }
            /* Fast path */
            if (bIsNotMuted) {
                trackImpl->outputPost.addFromOp(&trackImpl->output, mix_op::ADD, fGainTrack);
            }
        } else {
            MixWithGainAndPanAutomation(this, tmp, &trackImpl->output, &trackImpl->outputPost, autParGain, autParPan, postStageTickLatencyCompensated, postStageTickEnd, playbackState, dsp_util::MTR_CEIL, dsp_util::DBFS_MUTE_POS);
        }
    }

    /* Store block in audioOutput memory */
    if (DAW::isPlaybackState(playbackState)) {
        auto postStageSamplePos = samplePosProcess - (trackNode.inputLatency + trackNode.internalLatency);
        if (static_cast<bool>(trackImpl->flags & audiostageflags_t::RECORD_OUTPUT)) {
            tmp.timer.reset();
            if (postStageSamplePos >= 0) {
                trackImpl->audioOutput.store(&trackImpl->outputPost, postStageSamplePos);
            } else {
                log_printf("cannot write to negative offset %zd (samplepos %zd - stage.latencyOutput %zd)\n", postStageSamplePos, postStageSamplePos, trackImpl->getOutputLatency());
            }
            trackImpl->procStats.timeTrackRecordPost = tmp.timer.getTime();
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
uint32_t Host::finishTreadTasks(uint32_t tasksRunning, bool wait) {
    uint32_t finishedTasks = 0;
    // int64_t numspin = 0;
    while (true) {
        // numspin++;
        for (size_t i = 0; i < impl->threadCount; i++) {
            TrackBlockProcessTask& task = impl->tasks[i];
            if (task.isInUse()) {
                if (!task.isCompleted()) {
                    // if (!wait) {
                        continue;
                    // }
                    // task.wait();
                }
                finishedTasks++;
                dbgassert(task.isCompleted());
                Host::track_block_processing_task_t& procTask = task.getTask();
                //log_printf("Thread[%d] completed stageId %d\n", i, procTask.trackNode->stageId);
                if (task.isError()) {
                    std::exception_ptr eptr = task.getException();
                    if (eptr != nullptr) {
                        try {
                            std::rethrow_exception(eptr);
                        }
                        catch(const std::exception &ex) {
                            log_lf(Log::L_ERROR, "task[%d] had exception: %s\n", (int)i, ex.what());
                        }
                    }
                } else {
                    dbgassert(task.isGood());
                }
#if DAW_DEBUG_AUDIOGRAPH
                lastProcessingGraphs[procTask.trackNode->stageId] = procTask.effectProcessingGraph;
                procTask.effectProcessingGraph = nullptr;
#endif
                impl->blockThreadStats.emplace_back(static_cast<uint32_t>(i),
                                        procTask.trackNode->stageId,
                                        task.stats.timeStart,
                                        task.stats.timeEnd);
                procTask.trackNode->state = DAW::processing_track_node_state_t::PROCESSED;
                task.resetTask();
                // if (!wait)
                    // break;
            }
        }
        if (!wait || finishedTasks)
            break;

#if THREADSYNC == THREADSYNC_SEMAPHORE
        impl->conditionSingleTaskFinished.acquire();
#elif THREADSYNC == THREADSYNC_ATOMIC
        int_fast8_t maxWorkCount = static_cast<int_fast8_t>(tasksRunning);
        // auto countPre = impl->atomicWorkerCount.load();
#if 0
        auto const atomicImpl = reinterpret_cast<std::__cxx_atomic_impl<int_fast8_t>*>(&impl->atomicWorkerCount);
        auto const atomicMonitoryImpl = __libcpp_atomic_monitor(atomicImpl);
        if(std::__cxx_nonatomic_compare_equal(__cxx_atomic_load(atomicImpl, std::memory_order_relaxed), maxWorkCount))
            __libcpp_atomic_wait(atomicImpl, atomicMonitoryImpl);
#else
        impl->atomicWorkerCount.wait(maxWorkCount, std::memory_order_relaxed);
#endif
        tasksRunning--;
        // auto coountPost = impl->atomicWorkerCount.load();
        // static int invoc = 0;
        // if (invoc%500==0) {
        //     log_lf(Log::L_FATAL, "impl->atomicWorkerCount pre  %d\n", countPre);
        //     log_lf(Log::L_FATAL, "impl->atomicWorkerCount post %d\n", coountPost);
        // }
        // invoc++;
#else
        _mm_pause();
        _mm_pause();
        _mm_pause();
        _mm_pause();
#endif
    }
    // static int invoc = 0;
    // if (invoc++%800==0) {
    //     log_lf(Log::L_FATAL, "numspin  %zd\n", numspin);
    // }
    return finishedTasks;
}

int32_t Host::processGraph(project_controller_t* ctrl,
                              const audiostream_properties_t& audioProp,
                              DAW::processing_graph_t* const processingGraph,
                              AudioBlock* const ptrExternalInputs,
                              AudioBlock* const ptrExternalOutputs,
                              int32_t samplePosProcess,
                              double tickPosProcess,
                              playback_state playbackState,
                              bool inLoop)
{
    dbgassert(ctrl);
    project_t* const project = ctrl->getProject();

    /*
     * Clear all channels
     */
#if 0
    const sampleformat_t& sampleFormat = this->m_sampleFormatInternal;
    for (track_t* track : project->trackList) {
        dbgassert(track->audio);
        track_impl_t* audio = track->audio;
        audio->input.realloc(sampleFormat.blockSize);
        audio->output.realloc(sampleFormat.blockSize);
        audio->outputPost.realloc(sampleFormat.blockSize);
        dsp_util::fillBlock(audio->input, 0.0f);
        dsp_util::fillBlock(audio->output, 0.0f);
        dsp_util::fillBlock(audio->outputPost, 0.0f);
    }
#endif

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

    struct stageId_threadIdx_pair {
        audiostageid_i32 stageId;
        uint32_t threadIdx;
    };

    for (auto* node : processingGraph->nodesFlatOrdered) {
        node->state = DAW::processing_track_node_state_t::UNPROCESSED;
    }

    constexpr bool debugLogProcessing = false;

    this->impl->resetBlock();

    impl->playThreadId = seqthreads::getCurrentThreadId();

    const bool useThreading = this->multithreadedProcessing && impl->threadsRunningCount > 0 && impl->threadCount > 1;

    if (!useThreading) {
        for (auto itAudioStage = processingGraph->nodesFlatOrdered.begin(); itAudioStage != processingGraph->nodesFlatOrdered.end(); itAudioStage++) {
            auto const ptrProcessingNode = *itAudioStage;
            Host::track_block_processing_task_t blockProcTask;
            blockProcTask.audioProp = audioProp;
            blockProcTask.projectGlobals = ctrl->getGlobals();
            blockProcTask.trackNode = ptrProcessingNode;
            blockProcTask.ptrExternalInputs = ptrExternalInputs;
            blockProcTask.ptrExternalOutputs = ptrExternalOutputs;
            blockProcTask.tickPosProcess = tickPosProcess;
            blockProcTask.samplePosProcess = samplePosProcess;
            blockProcTask.playbackState = playbackState;
            blockProcTask.inLoop = inLoop;
            blockProcTask.debugLogProcessing = debugLogProcessing;
            auto timeStart = getTimeMicros();
            processGraphNode(impl->singleThreadedBuf, blockProcTask);
#if DAW_DEBUG_AUDIOGRAPH
            lastProcessingGraphs[blockProcTask.trackNode->stageId] = blockProcTask.effectProcessingGraph;
            blockProcTask.effectProcessingGraph = nullptr;
#endif
            auto timeEnd = getTimeMicros();

            thread_stats_process_timings_t thrdProcStats = {0, blockProcTask.trackNode->stageId, timeStart, timeEnd};
            impl->blockThreadStats.push_back(thrdProcStats);
        }
    } else {

        const auto funcCheckNodeUnprocessed=[](const DAW::track_node_t* trackNode) {
            return static_cast<const DAW::processing_track_node_t*>(trackNode)->state != DAW::processing_track_node_state_t::PROCESSED;
        };
        auto itBegin = processingGraph->nodesFlatOrdered.begin();
        const auto itEnd = processingGraph->nodesFlatOrdered.end();
        const auto graphSize = processingGraph->nodesFlatOrdered.size();
        size_t numNodesQueued = 0;
        uint32_t tasksRunning = 0;
        // int64_t numouter = 0;
        // int64_t numinner = 0;
#if THREADSYNC == THREADSYNC_SEMAPHORE
        impl->conditionSingleTaskFinished.acquire();
#elif THREADSYNC == THREADSYNC_ATOMIC
        impl->atomicWorkerCount.store(0);
#else
#endif
        while (numNodesQueued != graphSize) {
            int32_t numQueuedIteration = 0;
            for (auto itAudioStage = itBegin; itAudioStage != itEnd; ++itAudioStage) {
                // numinner++;
                DAW::processing_track_node_t& trackNode = **itAudioStage;

                if (trackNode.state != DAW::processing_track_node_state_t::UNPROCESSED) {
                    continue;
                }

                /* skip nodes with unprocessed inputs and loop over nodesFlatOrdered again */
                bool hasUnprocessedInputs = std::any_of(trackNode.children.cbegin(), trackNode.children.cend(), funcCheckNodeUnprocessed);
                if (!hasUnprocessedInputs) {
                    for (size_t i = 0; i < impl->threadCount; i++) {
                        TrackBlockProcessTask& task = impl->tasks[i];
                        if (!task.isInUse()) {
                            trackNode.state = DAW::processing_track_node_state_t::PROCESSING;
                            Host::track_block_processing_task_t blockProcTask;
                            blockProcTask.audioProp = audioProp;
                            blockProcTask.projectGlobals = ctrl->getGlobals();
                            blockProcTask.trackNode = &trackNode;
                            blockProcTask.ptrExternalInputs = ptrExternalInputs;
                            blockProcTask.ptrExternalOutputs = ptrExternalOutputs;
                            blockProcTask.tickPosProcess = tickPosProcess;
                            blockProcTask.samplePosProcess = samplePosProcess;
                            blockProcTask.playbackState = playbackState;
                            blockProcTask.inLoop = inLoop;
                            blockProcTask.debugLogProcessing = debugLogProcessing;
#if THREADSYNC == THREADSYNC_SEMAPHORE
                            blockProcTask.cond = &impl->conditionSingleTaskFinished;
#elif THREADSYNC == THREADSYNC_ATOMIC
                            impl->atomicWorkerCount.fetch_add(1, std::memory_order_relaxed);
                            blockProcTask.atomicWorkerCount = &impl->atomicWorkerCount;
#endif
                            task.setTask(blockProcTask);
                            impl->threads[i].pushTask(&task);
                            tasksRunning++;
                            numNodesQueued++;
                            numQueuedIteration++;
                            if (itAudioStage == itBegin) {
                                itBegin++;
                            }
                            break;
                        }
                    }
                    if (tasksRunning == impl->threadCount) {
                        break;
                    }
                }
            }
            if (tasksRunning > 0) {
                bool waitForTask = !numQueuedIteration || tasksRunning == impl->threadCount;
                tasksRunning -= finishTreadTasks(tasksRunning, waitForTask);
                dbgassert(tasksRunning != impl->threadCount);
            }
        }
        while (tasksRunning > 0) {
            tasksRunning -= finishTreadTasks(tasksRunning, true);
        }
        dbgassert(tasksRunning == 0);
        dbgassert(!std::any_of(processingGraph->nodesFlatOrdered.begin(), processingGraph->nodesFlatOrdered.end(), funcCheckNodeUnprocessed));
        // static int invoc = 0;
        // if (invoc++%100==0) {
        //     log_lf(Log::L_FATAL, "numouter %zd\n", numouter);
        //     log_lf(Log::L_FATAL, "numinner %zd\n", numinner);
        // }
    }

    /* Profiling/Timings: Accumulate timings */
    int64_t timeProcessingArr[8] = {0};
    track_midiprocess_profiling_t blockMidiStats;
    for (track_t* track : project->trackList) {
        auto& procStats = track->getStage()->procStats;
        auto& procMidiStats = track->getStage()->procMidiStats;
        int k = 0;
        timeProcessingArr[k++] += procStats.timeTrackProcessPluginsRaw;
        timeProcessingArr[k++] += procStats.timeTrackApplyAutomation;
        timeProcessingArr[k++] += procStats.timeTrackProcessMidi;
        timeProcessingArr[k++] += procStats.timeTrackMixInputs;
        timeProcessingArr[k++] += procStats.timeTrackRecordPre;
        timeProcessingArr[k++] += procStats.timeTrackFillAudioClips;
        timeProcessingArr[k++] += procStats.timeTrackProcessAudio;
        timeProcessingArr[k++] += procStats.timeTrackRecordPost;
        blockMidiStats.tm0InputClips += procMidiStats.tm0InputClips;
        blockMidiStats.tm1InputRT += procMidiStats.tm1InputRT;
        blockMidiStats.tm2ProcNotes += procMidiStats.tm2ProcNotes;
        blockMidiStats.tm3RevalidateEnds += procMidiStats.tm3RevalidateEnds;
        blockMidiStats.tm4SortEvents += procMidiStats.tm4SortEvents;
        blockMidiStats.tm5ProcArp += procMidiStats.tm5ProcArp;
        blockMidiStats.tm6UpdateOutputPost += procMidiStats.tm6UpdateOutputPost;
        blockMidiStats.tm7ValidateMidi += procMidiStats.tm7ValidateMidi;
    }
    int k = 0;
    int64_t timeTotalProcessPluginsRaw = timeProcessingArr[k++];
    stats.timings["Block.Tracks.0_ApplyAutomation"] = timeProcessingArr[k++];
    stats.timings["Block.Tracks.1_ProcessMidi"] = timeProcessingArr[k++];
    stats.timings["Block.Tracks.2_MixInputs"] = timeProcessingArr[k++];
    stats.timings["Block.Tracks.3_RecordAudioPre"] = timeProcessingArr[k++];
    stats.timings["Block.Tracks.4_FillAudioClips"] = timeProcessingArr[k++];
    stats.timings["Block.Tracks.5_ProcessAudio"] = timeProcessingArr[k++];
    stats.timings["Block.Tracks.6_RecordAudioPost"] = timeProcessingArr[k++];
    stats.blockMidiStats = blockMidiStats;
    stats.timeProcessPluginsRaw = timeTotalProcessPluginsRaw;
    auto curTimePluginProcess = stats.timeProcessPlugins;
    curTimePluginProcess -= curTimePluginProcess / NUM_BINS_STATS;
    curTimePluginProcess += timeTotalProcessPluginsRaw / NUM_BINS_STATS;
    stats.timeProcessPlugins = curTimePluginProcess;

    return 1;
}

void Host::initThreads() {
    for (auto & thread : impl->threads) {
        thread.setRealtimePriority(true);
        thread.setTls(daw_tls::getTls());
    }
    impl->startThreads();
}

void Host::onPlaybackJumpFromTo(project_controller_t* ctrl, int32_t fromSamplePos, double fromTickPos, int32_t toSamplePos, double toTickPos) {
    visitAudioStageInstances([&](auto* stage) {
        stage->onPlaybackJumpFromTo(fromSamplePos, fromTickPos, toSamplePos, toTickPos);
    });
}

void Host::onStartPlayback(project_controller_t* ctrl) {
    bResyncOutputTime = true;
    project_t* project = ctrl->getProject();
    for (track_t* track : project->trackList) {
        auto trackImpl = track->audio;
        //if (!trackImpl->heldNotes.empty())
        {
            trackImpl->onStartPlayback();
        }
    }
    if (impl->audioStream) {
        impl->audioStream->onPlayback();
    }
}

void Host::onStopPlayback(project_controller_t* ctrl) {
    getHostCallback()->isOfflineRendering = false;
    for (auto device : midiRealtimeDeviceInputs) {
        device.second.notes.m_list.clear();
    }
    visitAudioStageInstances([](auto stageImpl) {
        //if (!trackImpl->heldNotes.empty())
        {
            stageImpl->sendNotesOff();
            stageImpl->onStopPlayback();
        }
    });
}

void Host::resetResamplers() {
    impl->resetResamplers();
}

void Host::setOutput(std::shared_ptr<DAW::AudioIO::AudioStream> stream) {
    impl->audioStream = std::move(stream);
    if (impl->audioStream) {
        const auto numInputChannels = math::max<channelnum_t>(impl->audioStream->getNumInputChannels(), impl->inputChannels);
        const auto numOutputChannels = math::max<channelnum_t>(impl->audioStream->getNumOutputChannels(), impl->outputChannels);

        if (numInputChannels != impl->inputChannels || numOutputChannels != impl->outputChannels) {
            impl->destroyResamplers();
        } else {
            impl->resetResamplers();
        }
        if (numOutputChannels != impl->outputChannels) {
            freeRingBuffer(ringbuffer);
            allocRingBuffer(ringbuffer, numOutputChannels);
        }
        
        impl->inputChannels = numInputChannels;
        impl->outputChannels = numOutputChannels;
    }
    auto sampleFormatExternal = this->m_sampleFormatExternal;
    auto extSampleRate = impl->audioStream ? impl->audioStream->getSampleRate() : sampleFormatExternal.sampleRate;
    auto extBlockSize = impl->audioStream ? impl->audioStream->getBlockSize() : sampleFormatExternal.blockSize;
    sampleFormatExternal = { extSampleRate, extBlockSize, sampleformat_bits_t::FLOAT_32 };
    this->m_sampleFormatExternal        = sampleFormatExternal;
}

bool Host::isStreaming() {
    //watch out for race condition here
    return impl->audioStream && impl->audioStream->isActive();
}

/* Function needs to be re-entrant (thread safe) */
void Host::processAudio(process_scratch_buf_t& tmp, 
                           audio_stage_t* stage,
                           AudioBlock* input,
                           AudioBlock* output,
                           const project_globals_t& globals,
                           const double tickStageLatencyCompensated,
                           const samplecount_t sampleStageLatencyCompensated,
                           int32_t numSamples,
                           playback_state playbackState,
                           const DAW::effect_processing_graph_t* const processingGraph,
                           IDelayLineStorage* delayLines) const
{
   
    hires_timer_t timer;
    int64_t timeTotal = 0;
    if (processingGraph != nullptr) {
        //TODO: move to scratch_buf
        for (auto itAudioStage = processingGraph->nodesFlatOrdered.begin(); itAudioStage != processingGraph->nodesFlatOrdered.end(); itAudioStage++) {
            const DAW::processing_effect_node_t* ptrProcessingNode = *itAudioStage;
            const DAW::processing_effect_node_t& effNode = *ptrProcessingNode;

            const double ticksLatency = sampleToTickConvert<double, roundmode::none>(effNode.inputLatency, globals.tempo100, stage->sampleFormat.sampleRate);
            const samplecount_t sampleLatencyCompensated = sampleStageLatencyCompensated - effNode.inputLatency;
            const double tickLatencyCompensated = tickStageLatencyCompensated - ticksLatency;
            tick_t processingPosLatencyCompensate = math::floordS32(tickLatencyCompensated);

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
            const auto numChannelsTrack = blockIn->channels;
            dsp_util::fillBlock(*blockIn, 0.0f);

            effectbase* const effect = effNode.effectOptional;

            timer.reset();
            MixInputs(this, effNode, tmp, delayLines, blockIn, effNode.pulls, numChannelsTrack, effNode.inputLatency, tickLatencyCompensated, tickLatencyCompensated + getAudioStreamProperties().ticksPerBlock, playbackState, nullptr);
            AudioBlock* blockPostProcess = nullptr;
            int64_t timePassed = 0;
            if (effect) {
                bool isBypass = effect->isBypass();
                //TODO: this should be done in the vstplugin::process function
                if (effect->getModuleType() == MODULE_TYPE_VST2) {
                    VstTimeInfo timeinfo{};
                    PluginManager::UpdateVstTime(timeinfo, m_sampleFormatInternal, globals, sampleLatencyCompensated, tickLatencyCompensated, playbackState);
                    auto* ptr = static_cast<vstplugin*>(effect)->getLocalTimeInfoPtr();
                    if (ptr) {
                        *ptr = timeinfo;
                    }
                }
                if (effect->getModuleType() == MODULE_TYPE_CLAP) {
                    clap_event_transport_t transport{};
                    UpdateClapTime(transport, m_sampleFormatInternal, globals, sampleLatencyCompensated, tickLatencyCompensated, playbackState);
                    auto& pluginLocalTransport = static_cast<clapplugin*>(effect)->getTransport();
                    pluginLocalTransport = transport;
                }
                if (effect->getModuleType() == MODULE_TYPE_VST3) {
                    Steinberg::Vst::ProcessContext procContext{};
                    UpdateVst3Time(procContext, m_sampleFormatInternal, globals, sampleLatencyCompensated, tickLatencyCompensated, playbackState);
                    auto& pluginLocalContext = static_cast<vst3plugin*>(effect)->getVst3ProcessContext();
                    pluginLocalContext = procContext;
                }
                effect->updateAutomatedParameters(this, tickLatencyCompensated, playbackState);
                if (isBypass || bypassEffectProcessing) {
                    auto delay = effect->getPluginLatency();
                    if (delay > 0) {
                        AudioBlock *blockOut = effect->blockOutputs;
                        if (!effect->delayLine) {
                            effect->delayLine = std::make_unique<DelayLine>();
                        }
                        blockOut->clear();
                        delayAudio(effect->delayLine.get(), blockIn, blockOut, delay);
                    } else {
                        if (effect->blockOutputs->channels > blockIn->channels) {
                            effect->blockOutputs->clear();
                        }
                        effect->blockOutputs->copyFrom(blockIn);
                    }
                    blockPostProcess = effect->blockOutputs;
                } else {
                    tmp.ctrlEventsTemp.clear();
                    tmp.noteEventsTemp.clear();
                    effect->getTrackLink()->getNotesDelayed(processingPosLatencyCompensate, math::floordS32(tickLatencyCompensated + audioProperties.ticksPerBlock), audioProperties.ticksPerBlock, tmp.noteEventsTemp, tmp.ctrlEventsTemp, true, -1, -1);
                    effect->getTrackLink()->sendMidiToEffect(tmp.noteEventsTemp, tmp.ctrlEventsTemp, processingPosLatencyCompensate, prjGlobals.tempo100, effect);
                    effect->process(this, effect->blockInputs, effect->blockOutputs, tickLatencyCompensated, sampleLatencyCompensated, numSamples, playbackState);
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
                stats_processing_timings_t::MixStats(effect->procStatsAvg, effect->procStats, 0.1);
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

void Host::onTick() {
    PluginManager::onTick();
    if (isStreaming()
            && this->impl->audioStream
            && this->impl->audioStream->getNumCallbacks()
            && getTimeMillis() - this->impl->tmLastError > 30000) {
        auto timings = impl->audioStream->getStreamTimings();
        this->impl->tmLastError = getTimeMillis();
        double msec = timings.tmDeltaCbAvg / 1000.0;
        auto sr = impl->audioStream->getSampleRate();
        auto bs = impl->audioStream->getBlockSize();
        auto expectedTimeDelta = double(bs) / double(sr) * 1000.0;
        double maxError = 0.15;
        if (msec > expectedTimeDelta + maxError) {
            log_lf(Log::L_WARN, "audioCallback: time delta too large: %.5f ms avg, %.5f ms min, %.5f ms max (expected %f ms)\n", msec, timings.tmDeltaCbMin / 1000.0, timings.tmDeltaCbMax / 1000.0, expectedTimeDelta);
        } else if (msec < expectedTimeDelta - maxError) {
            log_lf(Log::L_WARN, "audioCallback: time delta too small: %.5f ms avg, %.5f ms min, %.5f ms max (expected %f ms)\n", msec, timings.tmDeltaCbMin / 1000.0, timings.tmDeltaCbMax / 1000.0, expectedTimeDelta);
        }
    }
}

void Host::unload() {
#if DAW_DEBUG_AUDIOGRAPH
    lastProcessingList = nullptr;
    lastTrackGraph = nullptr;
    //lastProcessingGraphs.clear();
#endif
    PluginManager::unload();
}

void Host::destroy() {
    dbgassert(!isStreaming() && "Stream is not stopped");
    PluginManager::destroy();
    freeRingBuffer(ringbuffer);
    impl->stopThreads();
}

void Host::setTls(daw_tls::tlsinstance& tls) {
    PluginManager::setTls(tls);
    this->impl->tls = tls;
    if (tls.dawInstance) {
        this->prjGlobals = tls.dawInstance->getGlobals();
    }
}

bool Host::writeRecordedData(project_controller_t* ctrl) {
    bool bHasNewData = false;
    auto cache = getTls().audioCache;
    auto daw = getTls().dawInstance;
    visitTrackAudioStageInstances([&](auto* track) {
        bHasNewData |= track->recorder.writeRecordedData(ctrl, track, cache, daw);
    });
    return bHasNewData;
}

int32_t Host::getPlayThreadId()
{
    return impl->playThreadId;
}


void FillAudioBlockFromClips(audiocache* cache, const project_globals_t& prjGlobals, const std::vector<clip_t*>& clips, const sampleformat_t& sf, samplecount_t samplePosBegin, AudioBlock& out) {
    for (clip_t* clip : clips) {
        if (!clip->enabled)
            continue;
        audiofile_t* audio = cache->getDerivedSample(clip->audio);
        if (!audio)
            continue;
        audiosample_t* sample = audio->sample.get();
        if (sample->samples.empty())
            continue;
        if (!assert_expr(sample->samples.size() == sample->nChannels))
            continue;
        if (!assert_expr(samplecount_t(sample->samples[0].size()) >= sample->nSamples))
            continue;
        dbgassert(sample->sampleRate == sf.sampleRate);
        /* positive if in the future, negative if in the past */
        samplecount_t clipSampleBegin = tickToSampleConvert<samplecount_t, roundmode::floor>(clip->start(), prjGlobals.tempo100, sf.sampleRate) - samplePosBegin;
        /* positive if in the future, negative if in the past */
        samplecount_t clipSampleEnd   = tickToSampleConvert<samplecount_t, roundmode::floor>(clip->end(), prjGlobals.tempo100, sf.sampleRate) - samplePosBegin;
        if (clipSampleBegin >= out.samples) {
            continue;
        }
        if (clipSampleEnd <= 0) {
            continue;
        }

        // 0 if clip starts before samplePosBegin, otherwise clipSampleBegin
        samplecount_t sampleReadBegin = math::max<samplecount_t>(0, clipSampleBegin);
        auto readSamples = math::min(clipSampleEnd, out.samples)  - sampleReadBegin;

        if (readSamples < 1) {
            continue;
        }


        sample_fades_ref_t fades[2] = {
            clip->getSampleFadeIn(prjGlobals.tempo100, sf.sampleRate), 
            clip->getSampleFadeOut(prjGlobals.tempo100, sf.sampleRate)
        };
        /* fixed readoffset into backing sample */
        samplecount_t clipSampleOffset = tickToSampleConvert<samplecount_t, roundmode::floor>(clip->offsetStart, prjGlobals.tempo100, sample->sampleRate);
        samplecount_t preLoopLen       = 0;
        samplecount_t loopStart        = 0;
        samplecount_t loopEnd          = 0;

        if (clip->isLoopEnabled()) {
            loopStart = tickToSampleConvert<samplecount_t, roundmode::floor>(clip->loopStart, prjGlobals.tempo100, sample->sampleRate);
            loopEnd   = tickToSampleConvert<samplecount_t, roundmode::floor>(clip->loopStart + clip->loopLen, prjGlobals.tempo100, sample->sampleRate);
            if (clip->offsetStart < clip->loopStart) {
                preLoopLen = tickToSampleConvert<samplecount_t, roundmode::floor>(clip->loopStart - clip->offsetStart, prjGlobals.tempo100, sample->sampleRate);
            }
        }

        auto numChannels = math::max(sample->nChannels, out.channels);
        DAW::AudioClipFadeLoopProcessor clipSample{fades[0], fades[1], sample, clipSampleOffset, preLoopLen, loopStart, loopEnd};
        for (auto ch = channelnum_t(0); ch < numChannels; ++ch) {
            auto srcChannel = sample->nChannels == 1 ? 0 : ch;
            auto* dst = (ch >= out.channels) ? out.buf[out.channels - 1] : out.buf[ch];
            samplecount_t start = sampleReadBegin;
            auto end = start + readSamples;
            for (auto s = start; s < end; ++s) {
                auto dstOffset = s;
                dbgassert(dstOffset >= 0 && dstOffset < out.samples);
                if (!assert_expr(dstOffset >= 0)) {
                    continue;
                }
                if (!assert_expr(dstOffset < out.samples)) {
                    break;
                }
                auto srcOffset = 0 + s - clipSampleBegin;
                dst[dstOffset] = clipSample.get(srcChannel, srcOffset);
            }
        }
    }
}
void Host::onAudioStageChanged(audio_stage_t* stage) {
    DAW::validateEffectRoutings(this, stage);
    auto daw = this->impl->tls.dawInstance;
    if (daw) {
        daw->onAudioStageChanged(stage);
    }
}

std::shared_ptr<DAW::rmsmeter> Host::getMeterInput() {
    return impl->meterInput;
}

std::shared_ptr<DAW::rmsmeter> Host::getMeterOutput() {
    return impl->meterOutput;
}

samplecount_t Host::getLatency() const {
    if (impl->processingGraph) {
        return impl->processingGraph->trackGraph->maxLatencySamples;
    }
    return 0;
}
samplecount_t Host::getOutputQueueSamples() const {
    if (impl->audioStream) {
        return impl->audioStream->getOutputQueueSize() * m_sampleFormatExternal.blockSize;
    }
    return 0;
}
double Host::getOutputTickPos() {
    if (impl->audioStream) {
        auto stream = impl->audioStream.get();
        auto tickPosSync = stream->getPlaybackBeginTickPos();
        auto secondsSinceBegin = stream->getPlaybackTimeSeconds();
        auto sampleSinceBegin = secondsToSamplesConvert<double, roundmode::floor>(secondsSinceBegin, m_sampleFormatExternal.sampleRate);
        auto tickPos = tickPosSync + sampleToTickConvert<double, roundmode::floor>(sampleSinceBegin, prjGlobals.tempo100, m_sampleFormatExternal.sampleRate);
        return tickPos;
    }
    return 0;
}

}// namespace DAW::Host

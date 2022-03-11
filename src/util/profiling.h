#pragma once
#include <cstdint>
#include <map>
#include "str_util.h"

#define STATS_PROCESSING_MAX_SAMPLES 1024
#define STATS_PROCESSING_INTERVAL_STEP 16
struct track_midiprocess_profiling_t {
    int64_t tm0InputClips     = 0;
    int64_t tm1InputRT        = 0;
    int64_t tm2ProcNotes      = 0;
    int64_t tm3RevalidateEnds = 0;
    int64_t tm4SortEvents     = 0;
    int64_t tm5ProcArp        = 0;
    int64_t tm6WriteVstEvents = 0;
    int64_t tm7ProcessOutput  = 0;
};
struct stats_processing_timings_t {
    int64_t statsProcSamples[STATS_PROCESSING_MAX_SAMPLES] = {};

    int64_t timeTrackProcessPluginsRaw = 0;
    int64_t timeTrackProcessPlugins    = 0;
    int64_t timeTrackApplyAutomation   = 0;
    int64_t timeTrackMixInputs         = 0;
    int64_t timeTrackProcessMidi       = 0;
    int32_t statsProcStep              = 0;
    int64_t statsWriteOffset           = 0;
    int64_t numBlocksProcessed         = 0;
};

#define NUM_BINS_STATS 16
struct host_stats_reducted_t {
    double usage;
    int64_t timeProcess;
    int64_t timeProcessRaw;
    int64_t timePerBlock_usec;
};
struct host_stats_t {
    int32_t tickBar = 0;
    int32_t samplesProcessed;
    int32_t blocksProcessed;
    int64_t timeProcessPluginsRaw;
    int64_t timeProcessPlugins;
    int64_t timeBlockRaw;
    int64_t timeBlock;
    std::map<String, int64_t> timings;
    track_midiprocess_profiling_t blockMidiStats;
    float usage;
    float usageRaw;
    int32_t inputBufferUnderuns    = 0;
    int32_t inputQueueLen          = 0;
    int32_t outputQueueLen         = 0;
    int32_t resamplerInNumBlocks   = 0;
    int32_t resamplerInNumSamples  = 0;
    int32_t resamplerOutNumBlocks  = 0;
    int32_t resamplerOutNumSamples = 0;
    int64_t lastInvocationTime_i64 = 0;
};

struct render_clip_cache_stats_t {
    int64_t timeRender;
    int64_t clipsCached;
    int64_t sizeCacheAllocatedMemBytes;
};
struct alignas(64) prof_stats_applicaton_t {
    int64_t tickTimerDelay       = 0;
    int64_t tickTimerDuration    = 0;
    int64_t timeRefreshAll       = 0;
    int64_t timeSwapBuffersAll   = 0;
    int64_t numMessagesProcessed = 0;
    int64_t numMessagesWmPaint   = 0;
    int64_t numRedrawReq         = 0;
};
struct alignas(64) prof_stats_render_t {
    int64_t timePrerender           = 0;
    int64_t timeRenderEditor        = 0;
    int64_t timeRenderTrackControls = 0;
    int64_t timeUpdateWaveforms     = 0;
    int64_t clipsRendered           = 0;
    int64_t notesRendered           = 0;
    int64_t numWaveFormsRendered    = 0;
    int64_t playThreadLockCount     = 0;
};
struct alignas(64) prof_stats_window_t {
    int64_t timeRender              = 0;
    int64_t timeSwapBuffers         = 0;
    int64_t timePrerender           = 0;
};
struct vst_opcode_stats_t {
    int32_t tmMillis      = 0;
    int32_t numDispatches = 0;
};
namespace Profiling {
    template<typename T>
    void profilingRegisterEntry(void* instance, const String& name);
    template<typename T>
    void profilingCommitStats(void* instance, int frameNumber, T& stats);
}// namespace Profiling

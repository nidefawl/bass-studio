#pragma once
#include <stdint.h>
#include <map>
#include "str_util.h"

#define STATS_PROCESSING_MAX_SAMPLES 1024
#define STATS_PROCESSING_INTERVAL_STEP 16
struct stats_processing_timings_t {
	int64_t timeProcessRaw = 0;
	int64_t timeProcess = 0;
	int64_t timeUpdateParameters = 0;
	int64_t timeGetNotesInRange = 0;
	int64_t timeMixInputs = 0;
	int64_t timeSendNotes = 0;
	int64_t statsProcSamples[STATS_PROCESSING_MAX_SAMPLES];
	int32_t statsProcStep = 0;
	int64_t statsWriteOffset=0;
	int64_t numBlocksProcessed=0;
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
	int64_t timeProcessRaw;
	int64_t timeProcess;
	std::map<String, int64_t> timings;
	double usage;
	double usageRaw;
	int32_t inputBufferUnderuns = 0;
	int32_t lastInvocationTime_i64 = 0;
	int32_t inputQueueLen = 0;
	int32_t outputQueueLen = 0;
	int32_t resamplerInNumBlocks = 0;
	int32_t resamplerInNumSamples = 0;
	int32_t resamplerOutNumBlocks = 0;
	int32_t resamplerOutNumSamples = 0;

};

struct render_clip_cache_stats_t {
	int64_t timeRender;
	int64_t clipsCached;
	int64_t sizeCacheAllocatedMemBytes;
};
struct render_stats_t {
	float fps;
	int64_t timeRender;
	int64_t clipsRendered;
	int64_t notesRendered;
	int64_t timeRenderEditor;
	int64_t playThreadLockCount;
	bool enableCache = true;
};

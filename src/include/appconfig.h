#pragma once
#include "types.h"
#include <map>
#include "str_util.h"
#include "util/profiling.h"

struct appsysteminfo {
    String glRenderer;
    String glVendor;
    String glVersion;
};
struct appruntime {
    render_clip_cache_stats_t renderClipCacheStats{};
    prof_stats_render_t renderStats{};
    prof_stats_render_t prevRenderStats{};
    appsysteminfo systeminfo{};
    bool enableCache                  = true;
    bool disableWaveformUpdates       = false;
    bool enableClipRendererDebugLayer = false;
    bool printWindowFps = false;
};

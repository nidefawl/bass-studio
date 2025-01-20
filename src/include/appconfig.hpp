#pragma once
#include "gui/gui.hpp"
#include "saferef.hpp"
#include "types.hpp"
#include <map>
#include <vector>
#include "str_util.hpp"
#include "util/profiling.hpp"

struct appsysteminfo {
    String glRenderer;
    String glVendor;
    String glVersion;
};
struct dawruntime {
    bool enableCache                  = true;
    bool disableWaveformUpdates       = false;
    bool enableClipRendererDebugLayer = false;
    bool copyAutomation = true;
};
struct appruntime final : public dawruntime {
    SafeRefStorage<guibase> safeRefs;
    render_clip_cache_stats_t renderClipCacheStats{};
    prof_stats_render_t renderStats{};
    prof_stats_render_t prevRenderStats{};
    appsysteminfo systeminfo{};
    bool printWindowFps = false;
    bool enableAudioIO = true;
};

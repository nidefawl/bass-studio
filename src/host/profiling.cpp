#include "profiling.h"
#include "str_util.h"
#include "profiling_impl.h"

namespace ProfilingImpl {

    template<typename T>
    struct profiling_impl_t {
        profiled_instances<T> regWindowProfStats;
        void commit(void* ptr, int frameNumber, T& stats) {
            static_assert(sizeof(T) % 64 == 0, "Type must provide 64 byte size");
            static_assert(alignof(T) % 64 == 0, "Type must provide 64 byte alignment");
            static_assert((sizeof(T) >> 3) % 8 == 0, "Stride expected to be multiple of 8");
            for (auto& entry : regWindowProfStats) {
                if (entry.instancePtr == ptr) {
                    entry.stats[entry.writeIdx] = stats;
                    if (entry.writeIdx + 1 >= PROFILING_MAX_LEN) {
                        entry.writeIdx = 0;
                        entry.loopCount++;
                    } else
                        entry.writeIdx++;
                }
            }
        }
        void registerInstance(void* ptr, const String& name) {
            regWindowProfStats.resize(regWindowProfStats.size() + 1);
            profiling_entry_t<T>& last = regWindowProfStats.back();
            memset(last.stats.data(), 0, sizeof(T) * last.stats.size());
            last.name        = name;
            last.instancePtr = ptr;
        }
    };

    profiling_impl_t<render_stats_t> renderStats;
    profiling_impl_t<application_stats_t> appStats;

    template<>
    void profilingGetData(profiling_data_t<render_stats_t>* out) {
        static const profiling_channel_descs channels = { {
            { "ctrl::render", "us", offsetof(render_stats_t, timeRender) },
            { "ctrl::swapbuffer", "us", offsetof(render_stats_t, timeSwapBuffers) },
            { "ctrl::prerender", "us", offsetof(render_stats_t, timePrerender) },
            { "tm editor::render", "us", offsetof(render_stats_t, timeRenderEditor) },
            { "tm track_controls::render", "us", offsetof(render_stats_t, timeRenderTrackControls) },
            { "tm waveforms::update", "us", offsetof(render_stats_t, timeUpdateWaveforms) },
            { "# waveforms updates", " ", offsetof(render_stats_t, numWaveFormsRendered) },
            { "# notes rendered", " ", offsetof(render_stats_t, notesRendered) },
            { "# clips rendered", " ", offsetof(render_stats_t, clipsRendered) }
        } };
        out->instanceList = &renderStats.regWindowProfStats;
        out->channelDesc = &channels;
    }

    template<>
    void profilingGetData(profiling_data_t<application_stats_t>* out) {
        static const profiling_channel_descs channels = { {
            { "Render All duration", "us", offsetof(application_stats_t, timeRefreshAll) },
            { "TickTimer delay", "us", offsetof(application_stats_t, tickTimerDelay) },
            { "TickTimer duration", "us", offsetof(application_stats_t, tickTimerDuration) },
            { "#window msgs", "msgs", offsetof(application_stats_t, numMessagesProcessed) },
            { "#WM_PAINT msgs", "msgs", offsetof(application_stats_t, numMessagesWmPaint) },
            { "#redraw req", "req", offsetof(application_stats_t, numRedrawReq) },
        } };
        out->instanceList = &appStats.regWindowProfStats;
        out->channelDesc = &channels;
    }
}// namespace ProfilingImpl
namespace Profiling {
    template<>
    void profilingRegisterEntry<render_stats_t>(void* entry, const String& name) {
        ProfilingImpl::renderStats.registerInstance(entry, name);
    }
    template<>
    void profilingCommitStats(void* entry, int frameNumber, render_stats_t& stats) {
        ProfilingImpl::renderStats.commit(entry, frameNumber, stats);
    }
    template<>
    void profilingRegisterEntry<application_stats_t>(void* entry, const String& name) {
        ProfilingImpl::appStats.registerInstance(entry, name);
    }
    template<>
    void profilingCommitStats(void* entry, int frameNumber, application_stats_t& stats) {
        ProfilingImpl::appStats.commit(entry, frameNumber, stats);
    }
}// namespace Profiling

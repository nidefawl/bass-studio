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
                    entry.frameNum++;
                }
            }
        }
        void registerInstance(void* ptr, const String& name) {
            regWindowProfStats.resize(regWindowProfStats.size() + 1);
            profiling_entry_t<T>& last = regWindowProfStats.back();
            memset(last.stats.data(), 0, sizeof(T) * last.stats.size());
            last.name        = name;
            last.instancePtr = ptr;

            // enforce template instantiation
            T dummy{};
            ProfilingImpl::profilingGetRecentFrame(ptr, &dummy);
        }
    };

    profiling_impl_t<prof_stats_window_t> windowStats;
    profiling_impl_t<prof_stats_render_t> renderStats;
    profiling_impl_t<prof_stats_applicaton_t> appStats;
    // constexpr auto sizeof_windowStats = sizeof(profiling_entry_t<prof_stats_window_t>);
    // constexpr auto sizeof_renderStats = sizeof(profiling_entry_t<prof_stats_render_t>);
    // constexpr auto sizeof_appStats = sizeof(profiling_entry_t<prof_stats_applicaton_t>);
    template<>
    void profilingGetData(profiling_data_t<prof_stats_window_t>* out) {
        static const profiling_channel_descs channels = { {
            { "ctrl->prerender", "us", offsetof(prof_stats_window_t, timePrerender) },
            { "ctrl->render", "us", offsetof(prof_stats_window_t, timeRender) },
            { "glSwapBuffer", "us", offsetof(prof_stats_window_t, timeSwapBuffers) }
        } };
        out->instanceList = &windowStats.regWindowProfStats;
        out->channelDesc = &channels;
    }

    template<>
    void profilingGetData(profiling_data_t<prof_stats_render_t>* out) {
        static const profiling_channel_descs channels = { {
            { "containers[]->prerender", "us", offsetof(prof_stats_render_t, timePrerender) },
            { "editor->render", "us", offsetof(prof_stats_render_t, timeRenderEditor) },
            { "track_controls->render", "us", offsetof(prof_stats_render_t, timeRenderTrackControls) },
            { "waveforms->update", "us", offsetof(prof_stats_render_t, timeUpdateWaveforms) },
            { "# waveforms updates", " ", offsetof(prof_stats_render_t, numWaveFormsRendered) },
            { "# notes rendered", " ", offsetof(prof_stats_render_t, notesRendered) },
            { "# clips rendered", " ", offsetof(prof_stats_render_t, clipsRendered) }
        } };
        out->instanceList = &renderStats.regWindowProfStats;
        out->channelDesc = &channels;
    }

    template<>
    void profilingGetData(profiling_data_t<prof_stats_applicaton_t>* out) {
        static const profiling_channel_descs channels = { {
            { "Render All duration", "us", offsetof(prof_stats_applicaton_t, timeRefreshAll) },
            { "TickTimer delay", "us", offsetof(prof_stats_applicaton_t, tickTimerDelay) },
            { "TickTimer duration", "us", offsetof(prof_stats_applicaton_t, tickTimerDuration) },
            { "#window msgs", "msgs", offsetof(prof_stats_applicaton_t, numMessagesProcessed) },
            { "#WM_PAINT msgs", "msgs", offsetof(prof_stats_applicaton_t, numMessagesWmPaint) },
            { "#redraw req", "req", offsetof(prof_stats_applicaton_t, numRedrawReq) },
        } };
        out->instanceList = &appStats.regWindowProfStats;
        out->channelDesc = &channels;
    }
    template<typename T>
    bool profilingGetRecentFrame(void* instance, T* out) {
        profiling_data_t<T> data{};
        profilingGetData(&data);
        for (profiling_entry_t<T>& entry : *data.instanceList) {
            if (entry.instancePtr == instance) {
                auto prevFramIdx = entry.writeIdx;
                if (entry.writeIdx == 0) {
                    prevFramIdx = PROFILING_MAX_LEN - 1;
                } else {
                    --prevFramIdx;
                }
                *out = entry.stats[prevFramIdx];
                return true;
            }
        }
        return false;
    }
}// namespace ProfilingImpl
namespace Profiling {
    template<>
    void profilingRegisterEntry<prof_stats_render_t>(void* instance, const String& name) {
        ProfilingImpl::renderStats.registerInstance(instance, name);
    }
    template<>
    void profilingCommitStats(void* instance, int frameNumber, prof_stats_render_t& stats) {
        ProfilingImpl::renderStats.commit(instance, frameNumber, stats);
    }
    template<>
    void profilingRegisterEntry<prof_stats_applicaton_t>(void* instance, const String& name) {
        ProfilingImpl::appStats.registerInstance(instance, name);
    }
    template<>
    void profilingCommitStats(void* instance, int frameNumber, prof_stats_applicaton_t& stats) {
        ProfilingImpl::appStats.commit(instance, frameNumber, stats);
    }
    template<>
    void profilingRegisterEntry<prof_stats_window_t>(void* instance, const String& name) {
        ProfilingImpl::windowStats.registerInstance(instance, name);
    }
    template<>
    void profilingCommitStats(void* instance, int frameNumber, prof_stats_window_t& stats) {
        ProfilingImpl::windowStats.commit(instance, frameNumber, stats);
    }
}// namespace Profiling

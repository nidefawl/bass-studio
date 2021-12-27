#include "profiling.h"
#include "str_util.h"
#include "profiling_impl.h"

namespace ProfilingImpl {

    template<typename T>
    struct profiling_impl_t {
        profiled_instances<T> regWindowProfStats;
        void commit(void* ptr, int frameNumber, T& stats) {
            for (auto& entry : regWindowProfStats) {
                if (entry.instancePtr == ptr) {
                    frame_stats<T>* lastEntry = &entry.stats[entry.writeIdx];
                    lastEntry->frameNumber    = frameNumber;
                    lastEntry->stats          = stats;
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

            last.name        = name;
            last.instancePtr = ptr;
        }
    };

    profiling_impl_t<render_stats_t> renderStats;
    profiling_impl_t<application_stats_t> appStats;

    template<>
    void profilingGetData(profiled_instances<render_stats_t>** out) {
        *out = &(renderStats.regWindowProfStats);
    }

    template<>
    void profilingGetData(profiled_instances<application_stats_t>** out) {
        *out = &(appStats.regWindowProfStats);
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

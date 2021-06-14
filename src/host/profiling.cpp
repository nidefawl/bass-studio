#include "profiling.h"
#include "str_util.h"
#include "assert_dbg.h"
#include "profiling_impl.h"
#include <stdint.h>
#include <array>
#include <vector>

namespace ProfilingImpl {


	struct profiling_impl_t {
		profiled_instances<frame_render_stats> regWindowRenderStats;
		void commit(void* ptr, int frameNumber, render_stats_t& frameRenderStats) {
			for (auto& entry : regWindowRenderStats) {
				if (entry.instancePtr == ptr)  {
					frame_render_stats* lastEntry = &entry.stats[entry.writeIdx];
					lastEntry->frameNumber = frameNumber;
					lastEntry->renderStats = frameRenderStats;
					if (entry.writeIdx + 1 >= PROFILING_MAX_LEN) {
						entry.writeIdx = 0;
						entry.loopCount++;
					} else entry.writeIdx++;
				}
			}
		}
		void registerInstance(void* ptr, String name) {
			regWindowRenderStats.push_back({ptr, name});
		}
	};
	profiling_impl_t impl;
	void profilingGetDataRenderStats(profiled_instances<frame_render_stats>** out) {
		*out = &(impl.regWindowRenderStats);
	}
}
namespace Profiling {
	void profilingRegisterWindow(void* window, String name) {
		ProfilingImpl::impl.registerInstance(window, name);
	}
	void profilingCommitStats(void* window, int frameNumber, render_stats_t& stats) {
		ProfilingImpl::impl.commit(window, frameNumber, stats);
	}
}

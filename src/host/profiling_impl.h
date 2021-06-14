#include "profiling.h"
#include "str_util.h"
#include "assert_dbg.h"
#include <stdint.h>
#include <array>
#include <vector>

#define PROFILING_MAX_LEN 1024
namespace ProfilingImpl {
	template<typename T>
	struct profiling_entry_t {
		void* instancePtr;
		String name;
		std::array<T, PROFILING_MAX_LEN> stats;
		int writeIdx = 0;
		int loopCount = 0;
	};

    template <typename T>
    using profiled_instances = std::vector<ProfilingImpl::profiling_entry_t<T>>;
	void profilingGetDataRenderStats(profiled_instances<frame_render_stats>** out);
}

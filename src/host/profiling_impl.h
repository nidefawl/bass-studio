#include "profiling.h"
#include "str_util.h"
#include "assert_dbg.h"
#include <array>
#include <vector>

#define PROFILING_MAX_LEN 1024
namespace ProfilingImpl {
	template <typename T>
	struct frame_stats {
		int frameNumber;
		T stats;
	};
	template<typename T>
	struct profiling_entry_t {
		void* instancePtr{};
		String name;
		std::array<frame_stats<T>, PROFILING_MAX_LEN> stats;
		int writeIdx = 0;
		int loopCount = 0;
	};

    template <typename T>
    using profiled_instances = std::vector<ProfilingImpl::profiling_entry_t<T>>;
    template <typename T>
	void profilingGetData(profiled_instances<T>** out);
}

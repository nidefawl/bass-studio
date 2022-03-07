#pragma once
#include "profiling.h"
#include "str_util.h"
#include "assert_dbg.h"
#include <array>
#include <vector>

#define PROFILING_MAX_LEN 1024
namespace ProfilingImpl {
    template<typename T>
    struct profiling_entry_t {
        void* instancePtr{};
        String name;
        std::array<T, PROFILING_MAX_LEN> stats;
        size_t writeIdx  = 0;
        size_t loopCount = 0;
    };
    struct profiledata_channel_desc_t {
        String name;
        String unit;
        size_t offsetStMember;
    };
    template<typename T>
    using profiled_instances = std::vector<profiling_entry_t<T>>;

    using profiling_channel_descs = std::vector<profiledata_channel_desc_t>;

    template<typename T>
    struct profiling_data_t {
        const profiling_channel_descs* channelDesc{};
        profiled_instances<T>* instanceList{};
    };

    template<typename T>
    void profilingGetData(profiling_data_t<T>* out);
}// namespace ProfilingImpl

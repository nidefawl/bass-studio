#pragma once
#include "util/profiling.h"
#include "str_util.h"
#include "assert_dbg.h"
#include <array>
#include <vector>

#define PROFILING_MAX_LEN 512
namespace ProfilingImpl {
    template<typename T>
    struct profiling_entry_t {
        std::array<T, PROFILING_MAX_LEN> stats;
        void* instancePtr{};
        size_t writeIdx  = 0;
        int64_t frameNum = -1;
        size_t loopCount = 0;
        String name;
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

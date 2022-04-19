#pragma once
#include <vector>
#include <algorithm>
#include "types.h"
#include <unordered_map>
#include "platform.h"
#include "logging.h"
#include "assert_dbg.h"


#define RECORD_ALLOC_STACKTRACES 0

namespace DebugAlloc {
    struct AllocInfo;
    template<typename T>
    class Tracker;

    template<typename T>
    Tracker<T>* getTracker();


    template<typename T>
    void throwUntrackked(Tracker<T>& t, T* g) {
        log_printf("object with allocId %zd was not tracked\n", g->allocId);
        dbgassert(0);
    }


    struct AllocInfo {
        int64_t allocId;
        std::vector<String> stacktrace;
    };

    template<typename T>
    void printLeaked(int64_t allocCount, const std::vector<T*>& allocList, const std::unordered_map<T*, AllocInfo>& mapAllocInfo) {
        log_lf(Log::L_DEBUG, "allocations: %lld\n", allocCount);
    #if RECORD_ALLOC_STACKTRACES
        for (auto tStar : allocList) {
            auto& allocInfo = mapAllocInfo.at(tStar);
            log_lf(Log::L_DEBUG, "leaked %012zX %zd allocated from:\n", reinterpret_cast<int64_t>(tStar), allocInfo.allocId);// add debug info to clip instance (track/time )
            for (auto& s : allocInfo.stacktrace) {
                log_lf(Log::L_DEBUG, "  %s\n", StringAsCStr(s));
            }
        }
    #endif
    }

    template<typename T>
    class Tracker {
    public:
        int64_t allocId    = 0;
        int64_t allocCount = 0;
        std::vector<T*> allocList;
        std::unordered_map<T*, AllocInfo> mapAllocInfo;
        int64_t objConstructor(T* ref) {
            const auto refAllocId = allocId++;
#if RECORD_ALLOC_STACKTRACES
            AllocInfo info;
            info.allocId = refAllocId;
            getStackTrace(info.stacktrace);
            mapAllocInfo[ref] = std::move(info);
#endif
            allocCount++;
            allocList.push_back(ref);
            return refAllocId;
        }
        void objDestructor(T* ref) {
            auto it = std::find(allocList.begin(), allocList.end(), ref);
            if (it != allocList.end()) {
                allocList.erase(it);
                allocCount--;
#if RECORD_ALLOC_STACKTRACES
                mapAllocInfo.erase(ref);
#endif
                return;
            }
            throwUntrackked(*this, ref);
        }
        void onExit() {
            printLeaked(allocCount, allocList, mapAllocInfo);
            allocList.clear();
            mapAllocInfo.clear();
        }
    };
}// namespace DebugAlloc

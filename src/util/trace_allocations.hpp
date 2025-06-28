#pragma once
#include "str_util.hpp"
#include "types.hpp"

#define ENABLE_ALLOCATION_TRACING 1
#define ENABLE_ALLOCATION_TRACING_AUDIO_THREAD (ENABLE_ALLOCATION_TRACING && 1) 
#define ENABLE_ALLOCATION_TRACING_MAIN_THREAD (ENABLE_ALLOCATION_TRACING && 0)

#if ENABLE_ALLOCATION_TRACING != 0
#endif
namespace DebugAlloc {

struct OperatorNewStats {
#if ENABLE_ALLOCATION_TRACING != 0
    int64_t numAllocations;
    size_t bytesAllocated;
#endif
};

struct AllocStats {
    OperatorNewStats malloc = {};
    OperatorNewStats single = {};
    OperatorNewStats array = {};
    String toString() const {
#if ENABLE_ALLOCATION_TRACING != 0
        return StringFormat("malloc: %zd allocations, %zu bytes, new: %zd allocations, %zu bytes, new[]: %zd allocations, %zu bytes",
                            malloc.numAllocations,
                            malloc.bytesAllocated,
                            single.numAllocations,
                            single.bytesAllocated,
                            array.numAllocations,
                            array.bytesAllocated);
#else
        return "Allocation tracking disabled";
#endif
    }
};
struct AllocTraceState {
    bool recordStats      = false;
    bool enableAllocPrint = false;
};
void beginTrace();
AllocStats endTrace();

} // namespace DebugAlloc
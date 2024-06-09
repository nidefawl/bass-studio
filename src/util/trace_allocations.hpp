#pragma once
#include "str_util.h"
#include "types.h"
#define ENABLE_ALLOCATION_TRACKING 0

namespace DebugAlloc {

struct OperatorNewStats {
#if ENABLE_ALLOCATION_TRACKING != 0
    int64_t numAllocations;
    size_t bytesAllocated;
#endif
};

struct AllocStats {
    OperatorNewStats single = {};
    OperatorNewStats array = {};
    String toString() const {
#if ENABLE_ALLOCATION_TRACKING != 0
        return StringFormat("new: %zd allocations, %zu bytes, new[]: %zd allocations, %zu bytes", 
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
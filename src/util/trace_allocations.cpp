#include "trace_allocations.hpp"

#if ENABLE_ALLOCATION_TRACKING == 0
namespace DebugAlloc {
void beginTrace() {
}
AllocStats endTrace() {
    return {};
}
} // namespace DebugAlloc
#else
#include "compiler.h"
#include "logging.h"
#include <new>
#include <cstdlib>

using std::size_t;
using std::int64_t;

namespace DebugAlloc {

thread_local DAW_CXX_CONSTINIT AllocStats allocStats;
thread_local DAW_CXX_CONSTINIT AllocTraceState allocTraceState;

void beginTrace() {
    allocStats = {};
    allocTraceState.recordStats = true;
}
AllocStats endTrace() {
    allocTraceState.recordStats = false;
    return allocStats;
}
} // namespace DebugAlloc

void* operator new(size_t size) noexcept(false) {
    void* p = malloc(size);
    if (DebugAlloc::allocTraceState.recordStats) {
        ++DebugAlloc::allocStats.single.numAllocations;
        DebugAlloc::allocStats.single.bytesAllocated+=size;
    }
    return p;
}

void* operator new[](size_t size) noexcept(false) {
    void* p = malloc(size);
    if (DebugAlloc::allocTraceState.recordStats) {
        ++DebugAlloc::allocStats.single.numAllocations;
        DebugAlloc::allocStats.single.bytesAllocated+=size;
    }
    return p;
}

void operator delete(void* ptr) noexcept {
    free(ptr);
}

void operator delete[](void* ptr) noexcept {
    free(ptr);
}

#endif

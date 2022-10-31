#define ENABLE_ALLOCATION_TRACKING 0

#if ENABLE_ALLOCATION_TRACKING

#include "logging.h"
#include <new>
#include <cstdlib>
#include <cstdio>
#include "types.h"

using std::printf;
using std::size_t;
using std::int64_t;
using std::malloc;
using std::free;
namespace DebugAlloc {
    struct OperatorNewStats {
        int64_t numAllocations;
        size_t bytesAllocated;
    };

namespace {
    bool recordStats      = false;
    OperatorNewStats allocStats = {};
    OperatorNewStats allocArrayStats = {};
}
    bool enableAllocPrint = false;

    void beginTrace() {
        allocStats = {};
        allocArrayStats = {};
        recordStats = true;
    }
    void endTrace() {
        recordStats = false;
        log_lf(Log::L_INFO, "Single  %zd allocations, %zu bytes\n", allocStats.numAllocations, allocStats.bytesAllocated);
        log_lf(Log::L_INFO, "Array   %zd allocations, %zu bytes\n", allocArrayStats.numAllocations, allocArrayStats.bytesAllocated);
    }
}

void* operator new(size_t size) noexcept(false) {
    void* p = malloc(size);
    if (DebugAlloc::recordStats) {
        ++DebugAlloc::allocStats.numAllocations;
        DebugAlloc::allocStats.bytesAllocated+=size;
    }
    return p;
}

void* operator new[](size_t size) noexcept(false) {
    void* p = malloc(size);
    if (DebugAlloc::recordStats) {
        ++DebugAlloc::allocArrayStats.numAllocations;
        DebugAlloc::allocArrayStats.bytesAllocated+=size;
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

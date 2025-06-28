#include "trace_allocations.hpp"

#if ENABLE_ALLOCATION_TRACING == 0
namespace DebugAlloc {
void beginTrace() {
}
AllocStats endTrace() {
    return {};
}
} // namespace DebugAlloc
#else
#include "compiler.hpp"
#include <new>
#include <cstdlib>
#ifdef __linux__
#include <dlfcn.h>
#endif

using std::size_t;
using std::int64_t;
extern "C" {
#ifdef __linux__
// Store original malloc/free to avoid infinite recursion
static void* (*original_malloc)(size_t) = nullptr;
static void (*original_free)(void*) = nullptr;
static bool initialized = false;

// Initialize original function pointers lazily
static void ensure_initialized() {
    if (!initialized) {
        original_malloc = (void*(*)(size_t))dlsym(RTLD_NEXT, "malloc");
        original_free = (void(*)(void*))dlsym(RTLD_NEXT, "free");
        initialized = true;
    }
}
#endif
} // extern "C"

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

#ifdef __linux__
// Override malloc
extern "C" void* malloc(size_t size) {
    ensure_initialized();
    void* p = original_malloc(size);
    if (DebugAlloc::allocTraceState.recordStats) {
        ++DebugAlloc::allocStats.malloc.numAllocations;
        DebugAlloc::allocStats.malloc.bytesAllocated += size;
    }
    return p;
}

// Override free
extern "C" void free(void* ptr) {
    ensure_initialized();
    original_free(ptr);
}
#endif

void* operator new(size_t size) noexcept(false) {
#ifdef __linux__
    void* p = original_malloc(size);
#else
    void* p = malloc(size);
#endif
    if (DebugAlloc::allocTraceState.recordStats) {
        ++DebugAlloc::allocStats.single.numAllocations;
        DebugAlloc::allocStats.single.bytesAllocated+=size;
    }
    return p;
}

void* operator new[](size_t size) noexcept(false) {
#ifdef __linux__
    void* p = original_malloc(size);
#else
    void* p = malloc(size);
#endif
    if (DebugAlloc::allocTraceState.recordStats) {
        ++DebugAlloc::allocStats.array.numAllocations;
        DebugAlloc::allocStats.array.bytesAllocated+=size;
    }
    return p;
}

void operator delete(void* ptr) noexcept {
#ifdef __linux__
    original_free(ptr);
#else
    free(ptr);
#endif
}

void operator delete[](void* ptr) noexcept {
#ifdef __linux__
    original_free(ptr);
#else
    free(ptr);
#endif
}

#endif

#pragma once
#include <cstddef>
#include <memory.h>
#include "assert_dbg.h"
#include "logging.h"

inline void* aligned_malloc(size_t size, size_t align) {
    void* result;
#if defined(_MSC_VER) || defined(__MINGW32__)
    result = _aligned_malloc(size, align);
#else
    if (posix_memalign(&result, align, size)) result = 0;
#endif
    return result;
}

inline void aligned_free(void* ptr) {
#if defined(_MSC_VER) || defined(__MINGW32__)
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

inline void handleFailedAllocation(int allocId, size_t allocSize) {
    log_printf("Failed allocation of size %d at %d\n", allocSize, allocId);
    dbgassert(0);
}

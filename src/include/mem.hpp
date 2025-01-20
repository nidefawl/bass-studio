#pragma once
#include <cstddef>

namespace DAW {
void* aligned_malloc(size_t size, size_t align);
void aligned_free(void* ptr);
void handleFailedAllocation(int allocId, size_t allocSize);
}

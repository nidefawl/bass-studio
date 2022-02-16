#define ENABLE_ALLOCATION_TRACKING 0

#if ENABLE_ALLOCATION_TRACKING

#include <new>
#include <iostream>
#include <cstdlib>
bool enableAllocPrint = false;
void* operator new(size_t size) noexcept(false) {
    void* p = std::malloc(size);
    if (enableAllocPrint)
        printf("new   %p %zu\n", p, size);
    return p;
}

void* operator new[](size_t size) noexcept(false) {
    void* p = std::malloc(size);
    if (enableAllocPrint)
        printf("new[] %p %zu\n", p, size);
    return p;
}

void operator delete(void* ptr) noexcept {
    if (enableAllocPrint)
        printf("del   %p\n", ptr);
    std::free(ptr);
}

void operator delete[](void* ptr) noexcept {
    if (enableAllocPrint)
        printf("del[] %p\n", ptr);
    std::free(ptr);
}

#endif

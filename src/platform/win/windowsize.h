#pragma once
#ifdef _WIN32
#include <windows.h>
struct windowsize {
    bool valid;
    WINDOWPLACEMENT p{};
    windowsize() {
        p.length = sizeof(WINDOWPLACEMENT);
        valid = false;
    }
};
#endif

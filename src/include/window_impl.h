#pragma once
#include <memory>
#include <nanovg_min.h>
#include <functional>

void AppWndProc_disableBlockReentrant();
void AppWndProc_enableBlockReentrant();

class window_abstract_t {
public:
    window_abstract_t() = default;
    virtual ~window_abstract_t() = default;
    virtual int init(NVGcontext*) = 0;
    virtual int render(NVGcontext*, int, int, float) = 0;
    virtual int destroy(NVGcontext*) = 0;
};

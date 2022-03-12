#pragma once
#include <memory>
#include <nanovg_min.h>
#include <functional>

void AppWndProc_disableBlockReentrant();
void AppWndProc_enableBlockReentrant();

struct window_draw_fn {
    std::function<int(NVGcontext*, int, int, float)> drawCallback;
};

struct window_init_fn {
    std::function<int(NVGcontext*)> initCallback;
};

class window_abstract_t {
public:
    window_abstract_t() = default;
    virtual ~window_abstract_t() = default;
    virtual int init(NVGcontext*) = 0;
    virtual int render(NVGcontext*, int, int, float) = 0;
    virtual int destroy(NVGcontext*) = 0;
};

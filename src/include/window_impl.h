#pragma once
#include <nanovg_min.h>
#include <functional>

void AppWndProc_disableBlockReentrant();
void AppWndProc_enableBlockReentrant();

struct window_draw_fn {
    std::function<int(NVGcontext*, int, int, float)> drawCallback;
};

struct window_init_fn {
    std::function<void(NVGcontext*)> initCallback;
};

#pragma once
#include <nanovg_min.h>
#include <functional>

struct window_draw_fn {
	std::function<void(NVGcontext*,int,int,float)> drawCallback;
};

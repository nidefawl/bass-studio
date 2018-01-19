#pragma once
#include <algorithm>
#include <cmath>
#define CLAMP_I(x, min, max) (x > max ? max : x < min ? min : x)
#define CLAMP_F(x) (x > 1.f ? 1.f : x < 0.f ? 0.f : x)
#ifndef M_PI
#define M_PI           3.14159265358979323846  /* pi */
#endif
#define FLOAT_PI           3.141592653f
using std::max;
using std::min;
using std::fmod;
using std::ceil;
using std::floor;
using std::abs;

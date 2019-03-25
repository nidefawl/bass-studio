#pragma once
#include <algorithm>
#include <cmath>
#define CLAMP_I(x, min, max) (x > max ? max : x < min ? min : x)
#define CLAMP_F(x) (x > 1.f ? 1.f : x < 0.f ? 0.f : x)
#ifndef M_PI
#define M_PI           3.14159265358979323846  /* pi */
#endif
#define FLOAT_PI           3.141592653f
namespace math {
	static float const F_MIN = 1E-12F;
	template<typename T>
	inline T max(T a, T b) {
		return a < b ? a : b;
	}
	template<typename T>
	inline T min(T a, T b) {
		return a < b ? a : b;
	}
	inline float powf(float a, float b) {
		return std::pow(a, b);
	}
	template<typename T>
	inline T abs(T a) {
		return a < 0 ? -a : a;
	}
	template<typename T>
	inline T floor(T a) {
		return std::floor(a);
	}
	//using fmod = std::fmod;
	//using ceil = std::ceil;
	//using floor = std::floor;
	//using abs = std::abs;
}

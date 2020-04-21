#pragma once
#include <algorithm>
#include <cmath>
#include <assert.h>
#include <limits>
#include "vec.h"
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
		return a > b ? a : b;
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
	template<typename T>
	inline T round(T a) {
		return std::round(a);
	}
	template<typename T>
	inline T ceil(T a) {
		return std::round(a);
	}
	template <typename T, typename U>
	inline bool CheckFitsTypeRange(const U value) {
		return value >= std::numeric_limits<T>::min()  && value <= std::numeric_limits<T>::max() ;
	}
	template<typename T>
	inline int64_t ceilCast(T a) {
		T val = std::ceil(a);
		assert(CheckFitsTypeRange<int64_t>(a));
		return val;
	}
	template<typename T>
	inline int64_t floorCast(T a) {
		T val = std::floor(a);
		assert(CheckFitsTypeRange<int64_t>(a));
		return val;
	}
	template<typename T>
	inline int32_t floorF32toS32(T a) {
		T val = std::floor(a);
		assert(CheckFitsTypeRange<int32_t>(val));
		return static_cast<int32_t>(val);
	}
	template<typename T>
	inline T clamp(T a, T tmin, T tmax) {
		return a < tmin ? tmin : a > tmax ? tmax : a;
	}
	//using fmod = std::fmod;
	//using ceil = std::ceil;
	//using floor = std::floor;
	//using abs = std::abs;

	inline ivec2 maxvec2(const ivec2& a, const ivec2& b) {
		return {math::max(a.x, b.x), math::max(a.y, b.y)};
	}
	inline ivec2 absvec2(const ivec2 a) {
		return {math::abs(a.x), math::abs(a.y)};
	}
}

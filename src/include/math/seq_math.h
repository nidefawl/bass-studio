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
    template <typename T>
    inline T absMax(T a, T b)
    {
        return abs(a) > abs(b) ? a : b;
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
		//TODO: this implicitly casts T=int32 to U=float, changing the max from 2147483647 to 2147483648
		// The easy way to fix this is C++20 http://cpp.arh.pub.ro/cpp/utility/in_range
		return value >= std::numeric_limits<T>::min()  && value <= std::numeric_limits<T>::max() ;
	}
	template<typename T>
	inline int64_t ceilCast(T a) {
		T val = std::ceil(a);
		return val;
	}
	template<typename T>
	inline int64_t floorCast(T a) {
		T val = std::floor(a);
		return val;
	}
	template<typename T>
	inline int32_t floorF32toS32(T a) {
		T val = std::floor(a);
		assert(a >= -2147483648.0 && a <= 2147483520.0);
		return static_cast<int32_t>(val);
	}
	template<typename T>
	inline uint32_t floorF32toU32(T a) {
		T val = std::floor(a);
		assert(a >= 0 && a <= 4294967040.0);
		return static_cast<uint32_t>(val);
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
	inline vec2 maxvec2f(const vec2& a, const vec2& b) {
		return {math::max(a.x, b.x), math::max(a.y, b.y)};
	}
	inline ivec2 absvec2(const ivec2 a) {
		return {math::abs(a.x), math::abs(a.y)};
	}
	inline float distvec2(const ivec2 a, const ivec2 b) {
		auto vLen = vec2(b - a);
		return glm::length(vLen);
	}
	inline float distancePointLine(const ivec2 pt, const ivec2 a, const ivec2 b) {
		vec2 v = b - a;
		float lenSq = glm::dot(v, v);
		if (lenSq < 1E-4F) {
			return glm::distance(vec2(pt), vec2(a));
		}
		float t = math::max(0.0f, math::min(1.0f, glm::dot(vec2(pt - a), v) / lenSq));
		const vec2 p = vec2(a) + t * v;
		return glm::distance(vec2(pt), p);
	}
}

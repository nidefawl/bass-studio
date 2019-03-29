#pragma once
#include <glm/glm.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

using mat4x4 = glm::mat4x4;

template<typename T>
inline float const* mat_ptr(T const& m)
{
	return &(m[0].x);
}

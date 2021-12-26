#pragma once
#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <vector>

using vec2  = glm::vec2;
using vec4  = glm::vec4;
using ivec2 = glm::ivec2;
using ivec4 = glm::ivec4;

template<typename T>
inline float const* vec_ptr(T const& m) {
    return &(m.x);
}

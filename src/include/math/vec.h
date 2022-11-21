#pragma once
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

using vec2  = glm::vec2;
using vec4  = glm::vec4;
using ivec2 = glm::ivec2;
using ivec4 = glm::ivec4;

using vec2_unaligned = glm::vec<2, float, glm::qualifier::packed_highp>;
using vec4_unaligned = glm::vec<4, float, glm::qualifier::packed_highp>;
using vec2_aligned = glm::vec<2, float, glm::qualifier::aligned_highp>;
using vec4_aligned = glm::vec<4, float, glm::qualifier::aligned_highp>;
static_assert(sizeof(vec2_unaligned) == 2 * sizeof(float), "vec2_unaligned is not packed as expected");
static_assert(sizeof(vec2_aligned) == 2 * sizeof(float), "vec2_aligned is not packed as expected");
static_assert(sizeof(vec4_unaligned) == 4 * sizeof(float), "vec4_unaligned is not packed as expected");
static_assert(sizeof(vec4_aligned) == 4 * sizeof(float), "vec4_aligned is not packed as expected");
static_assert(alignof(vec2_unaligned) == 4, "vec2_unaligned is not aligned as expected");
static_assert(alignof(vec4_unaligned) == 4, "vec4_unaligned is not aligned as expected");
static_assert(alignof(vec2_aligned) == 8, "vec2_aligned is not aligned as expected");
static_assert(alignof(vec4_aligned) == 16, "vec4_aligned is not aligned as expected");
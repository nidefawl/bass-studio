#pragma once
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

using vec2  = glm::vec2;
using vec3  = glm::vec3;
using vec4  = glm::vec4;
using ivec2 = glm::ivec2;
using ivec3 = glm::ivec3;
using ivec4 = glm::ivec4;

static_assert(GLM_CONFIG_SIMD == GLM_ENABLE, "GLM_CONFIG_SIMD == GLM_ENABLE");

using vec2_unaligned = glm::vec<2, float, glm::qualifier::packed_highp>;
using vec3_unaligned = glm::vec<3, float, glm::qualifier::packed_highp>;
using vec4_unaligned = glm::vec<4, float, glm::qualifier::packed_highp>;
using vec2_aligned = glm::vec<2, float, glm::qualifier::aligned_highp>;
using vec3_aligned = glm::vec<3, float, glm::qualifier::aligned_highp>;
using vec4_aligned = glm::vec<4, float, glm::qualifier::aligned_highp>;
static_assert(sizeof(vec2_unaligned) == 2 * sizeof(float), "vec2_unaligned is not packed as expected");
static_assert(sizeof(vec2_aligned) == 2 * sizeof(float), "vec2_aligned is not packed as expected");
static_assert(sizeof(vec3_unaligned) == 3 * sizeof(float), "vec3_unaligned is not packed as expected");
static_assert(sizeof(vec3_aligned) == 4 * sizeof(float), "vec3_aligned is not packed as expected");
static_assert(sizeof(vec4_unaligned) == 4 * sizeof(float), "vec4_unaligned is not packed as expected");
static_assert(sizeof(vec4_aligned) == 4 * sizeof(float), "vec4_aligned is not packed as expected");
static_assert(alignof(vec2_unaligned) == 4, "vec2_unaligned is not aligned as expected");
static_assert(alignof(vec3_unaligned) == 4, "vec3_unaligned is not aligned as expected");
static_assert(alignof(vec4_unaligned) == 4, "vec4_unaligned is not aligned as expected");
static_assert(alignof(vec2_aligned) == 8, "vec2_aligned is not aligned as expected");
static_assert(alignof(vec3_aligned) == 16, "vec3_aligned is not aligned as expected");
static_assert(alignof(vec4_aligned) == 16, "vec4_aligned is not aligned as expected");
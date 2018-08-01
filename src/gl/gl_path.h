#include <glm/glm.hpp>
#include <vector>
#include <stdint.h>

#include "gl_vbo.h"
#include "gl_attr.h"

#ifndef GL_FLOAT
#define GL_FLOAT 0x1406
#endif
using glm::vec2;
using glm::vec4;
using vec2list = std::vector<vec2>;

struct BakeGLPath {
	DrawVBO vbo;
	int numPaths = 0;
	uint32_t uniforms_texture = 0;
};
namespace LineJoin {
	enum {
		miter, round, bevel
	};
}
namespace LineCaps {
	enum {
		none, round, triangle_in, tirangle_out, square, butt
	};
}
#pragma pack(push, 1)
struct vert {
	vec2 pos;
	vec2 seg;
	vec2 angles;
	vec2 tangent0;
	vec2 tangent1;
	vec2 tex;
	float index;
	float pad[7];
};
#define ATTR_STRIDE ((2+2+2+2+2+2+8)*sizeof(float))
struct Uniforms {
	vec4 color{1.f,1.f,1.f,1.f};
	vec2 translate{0.f, 0.f};
	float scale{1.0f};
	float rotate{0.0f};
	float linewidth{1.0f};
	float antialias{1.0f};
	vec2 linecaps  { LineCaps::none, LineCaps::none};
	float linejoin { LineJoin::round };
	float miter_limit { 10.0f };
	float length { 0 };
	float dash_phase { 0 };
	float dash_period { 1e20f };
	float dash_index { 0 };
	vec2 dash_caps { LineCaps::none, LineCaps::none };
	float closed {0};
};
#pragma pack(pop)
class GLPathRenderer {
	std::vector<VertexAttr> attributes {
		{"a_position", 2, GL_FLOAT},
		{"a_segment", 2, GL_FLOAT},
		{"a_angles", 2, GL_FLOAT},
		{"a_tangents", 4, GL_FLOAT},
		{"a_texcoord", 2, GL_FLOAT},
		{"a_index", 1, GL_FLOAT},
	};
public:
	const int countUniforms = 32;
	const int sizeUniforms = countUniforms*4;
	uint32_t program2dLines;
	uint32_t u_dash_atlas;
	uint32_t u_model;
	uint32_t u_view;
	uint32_t u_projection;
	uint32_t u_uniforms;
	uint32_t u_uniforms_shape;
public:
	int init();
	void destroy();
	void bakePaths(std::vector<vec2list> paths, Uniforms pathOpt, BakeGLPath& out);
};

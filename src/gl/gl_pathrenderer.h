#pragma once
#include <vector>
#include "types.h"

#include "math/vec.h"
#include "math/mat.h"
#include "gl_vbo.h"
#include "gl_attr.h"
#include "hires_timer.h"

#ifndef GL_FLOAT
#define GL_FLOAT 0x1406
#endif
using vec2list = std::vector<vec2>;

namespace LineJoin {
    enum {
        miter,
        round,
        bevel
    };
}
namespace LineCaps {
    enum {
        none,
        round,
        triangle_in,
        tirangle_out,
        square,
        butt
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
    float pad[3];
    auto fields() const {
        return std::tie(pos, seg, angles, tangent0, tangent1, tex, index);
    }
    bool operator==(const vert& other) const {
        return fields() == other.fields();
    }
    bool operator!=(const vert& other) const {
        return operator==(other);
    }
};

#define ATTR_STRIDE ((2 + 2 + 2 + 2 + 2 + 2 + 1 + 3) * sizeof(float))
struct Uniforms {
    vec4 color{1.f, 1.f, 1.f, 1.f};
    vec2 translate{0.f, 0.f};
    float scale{1.0f};
    float rotate{0.0f};
    float linewidth{1.0f};
    float antialias{1.0f};
    vec2 linecaps{LineCaps::none, LineCaps::none};
    float linejoin{LineJoin::round};
    float miter_limit{10.0f};
    float length{0};
    float dash_phase{0};
    float dash_period{1e20f};
    float dash_index{0};
    vec2 dash_caps{LineCaps::none, LineCaps::none};
    float closed{0};
};
#pragma pack(pop)
struct BakeGLPath {
    DrawVBO vbo;
    int32_t numPaths = 0;
    uint32_t uniforms_texture = 0;
    float lineWidth = 1.0f;
    Uniforms bakeOpts;
};
struct vbuf {
    std::vector<float> v;
    std::vector<uint32_t> i;
};
enum class pathrenderer_type_e : int32_t {
    DASHLINES = 0,
    POLYLINE2D,
    PAR_BASIC,
    PAR_ADVANCED
};
class IPathRenderer {
public:
    uint32_t program2dLines;
    virtual ~IPathRenderer()= default;
    virtual int init()     = 0;
    virtual void destroy() = 0;

    virtual void bakePaths(std::vector<vec2list> paths, Uniforms pathOpt, BakeGLPath& out) = 0;

    virtual void render(BakeGLPath& out, const mat4x4& matProj, const mat4x4& matView, const mat4x4& matModel) = 0;
};
class GLPathRendererDashLines : public IPathRenderer {
    std::vector<VertexAttr> attributes{
        {"a_position", 2, GL_FLOAT},
        {"a_segment", 2, GL_FLOAT},
        {"a_angles", 2, GL_FLOAT},
        {"a_tangents", 4, GL_FLOAT},
        {"a_texcoord", 2, GL_FLOAT},
        {"a_index", 1, GL_FLOAT},
    };

public:
    const uint32_t countUniforms = 32;
    const uint32_t sizeUniforms  = countUniforms * 4;
    int32_t u_dash_atlas;
    int32_t u_model;
    int32_t u_view;
    int32_t u_projection;
    int32_t u_uniforms;
    int32_t u_uniforms_shape;
public:
    int init() override;
    void destroy() override;
    void bakePaths(std::vector<vec2list> paths, Uniforms pathOpt, BakeGLPath& out) override;
    void render(BakeGLPath& bakedPath, const mat4x4& matProj, const mat4x4& matView, const mat4x4& matModel) override;
};

class GLPathRendererPolyline2d : public IPathRenderer {
    std::vector<VertexAttr> attributes{
            {"a_position", 2, GL_FLOAT},
    };

public:
    int32_t u_mvp;
    int32_t u_color;

public:
    int init() override;
    void destroy() override;
    void bakePaths(std::vector<vec2list> paths, Uniforms pathOpt, BakeGLPath& out) override;
    void render(BakeGLPath& bakedPath, const mat4x4& matProj, const mat4x4& matView, const mat4x4& matModel) override;
};

class GLPathRendererParBasic : public IPathRenderer {
    std::vector<VertexAttr> attributes{
            {"a_vertex", 4, GL_FLOAT}
    };
    std::vector<float> tmpBuffer;

    int32_t u_mvp;
    int32_t u_linewidth;

public:
    int init() override;
    void destroy() override;
    void bakePaths(std::vector<vec2list> paths, Uniforms pathOpt, BakeGLPath& out) override;
    void render(BakeGLPath& bakedPath, const mat4x4& matProj, const mat4x4& matView, const mat4x4& matModel) override;
};


class GLPathRendererParAdvanced : public IPathRenderer {
    std::vector<VertexAttr> attributes{
            {"a_vertex", 4, GL_FLOAT}
    };
    std::vector<float> tmpBuffer;

    int32_t u_mvp;
    int32_t u_color;
    int32_t u_linewidth;

public:
    int init() override;
    void destroy() override;
    void bakePaths(std::vector<vec2list> paths, Uniforms pathOpt, BakeGLPath& out) override;
    void render(BakeGLPath& bakedPath, const mat4x4& matProj, const mat4x4& matView, const mat4x4& matModel) override;
};

#include "glheaders.h"
#include <cmath>
#include <vector>
#include <algorithm>
#include <glm/gtc/type_ptr.hpp>

#include "math/seq_math.h"
#include "seq_util.h"
#include "math/vec.h"
#include "math/mat.h"
#include "str_util.h"
#include "fileio.h"
#include "audiocache.h"

#include "gl_pathrenderer.h"
#include "gl_util.h"
#include "gl_attr.h"
#include "gl_vbo.h"
#include "gl_tess2d.h"
#include "hires_timer.h"
#include "assert_dbg.h"
#include <par/par_streamlines.h>

namespace GLPathRendererParBasicSrc {
    constexpr const char* srcShaderVertex   = R"END(
#version 150 core

uniform mat4 u_mvp;
in vec4 a_vertex;
out vec2 uv;
void main() {
  gl_Position = u_mvp * vec4(a_vertex.xy, 1.0-a_vertex.z*2.0, 1.0);
  uv = a_vertex.zw;
}
)END";
    constexpr const char* srcShaderFragment = R"END(
#version 150 core

uniform float u_linewidth;
in vec2 uv;
out vec4 frag_color;
void main() {
  float a = min(1.0, (1.0 - abs(uv.y)) * u_linewidth * 0.5);
  frag_color = vec4(vec3(1.0), a);
}
)END";
}// namespace
int GLPathRendererParBasic::init() {
    GLuint vertex_shader = compileShader(GL_VERTEX_SHADER, GLPathRendererParBasicSrc::srcShaderVertex);
    if (!vertex_shader) {
        return 1;
    }
    GLuint fragment_shader = compileShader(GL_FRAGMENT_SHADER, GLPathRendererParBasicSrc::srcShaderFragment);
    if (!fragment_shader) {
        return 1;
    }
    GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    String log = getLog(1, program);
    if (getStatus(program, GL_LINK_STATUS) != 1) {
        checkGLError("getStatus");
        printf("Link error: %s\n", StringAsCStr(log));
        return 1;
    }
    if (!log.empty()) {
        printf("Link log: %s\n", StringAsCStr(log));
    }
    checkGLError("linkProgram");
    glUseProgram(program);
    u_mvp       = glGetUniformLocation(program, "u_mvp");
    u_linewidth = glGetUniformLocation(program, "u_linewidth");

    for (VertexAttr& attr : attributes) {
        attr.bindingPt   = glGetAttribLocation(program, attr.name);
        checkGLError("glGetAttribLocation");
    }

    program2dLines = program;
    return 0;
}
void GLPathRendererParBasic::destroy() {
    glDeleteProgram(program2dLines);
}
void GLPathRendererParBasic::bakePaths(const std::vector<path_t>& paths, BakeGLPath& out) {
    dbgassert(!paths.empty());
    out.bakeOpts = paths.at(0).pathOpts;

    int idx = 0;
    std::vector<uint16_t> spineLengths;
    std::vector<vec2> bufFinal;
    for (auto& path : paths) {
        auto& list = path.pathVecs;
        if (list.size() > 1) {
            bufFinal.insert(bufFinal.end(), list.begin(), list.end());
            dbgassert(FitsTypeRange<uint16_t>(list.size()));
            spineLengths.push_back(static_cast<uint16_t>(list.size()));
            idx++;
        }
    }
    parsl_spine_list spinelist{};
    spinelist.num_vertices  = bufFinal.size();
    spinelist.vertices      = reinterpret_cast<parsl_position*>(bufFinal.data());
    spinelist.num_spines    = spineLengths.size();
    spinelist.spine_lengths = spineLengths.data();

    // float miterLimit = 2.0f;
    parsl_config config{};
    config.flags |= PARSL_FLAG_ANNOTATIONS;
    config.thickness       = out.bakeOpts.linewidth*2.0f;
    config.miter_limit     = 0.0f;
    parsl_context* context = nullptr;
    context                = parsl_create_context(config);
    parsl_mesh* mesh       = parsl_mesh_from_lines(context, spinelist);
    int nPaths             = idx;
#ifdef NO_OPENGL
    return;
#endif
    tmpBuffer.resize(4ULL * mesh->num_vertices);
    for (uint32_t i = 0; i < mesh->num_vertices; ++i) {
        size_t index = i * 4ULL;
        tmpBuffer[index++] = mesh->positions[i].x;
        tmpBuffer[index++] = mesh->positions[i].y;
        tmpBuffer[index++] = mesh->annotations[i].u_along_curve;
        tmpBuffer[index++] = mesh->annotations[i].v_across_curve;
    }

    bool newBuffer = false;
    DrawVBO& vbo   = out.vbo;
    if (vbo.vaoId == 0) {
        glGenVertexArrays(1, &vbo.vaoId);
        vbo.genBuffers();
        newBuffer = true;
    }
    glBindVertexArray(vbo.vaoId);
    vbo.uploadBuffer(GL_ARRAY_BUFFER, tmpBuffer.data(), sizeof(float)*tmpBuffer.size());
    vbo.uploadBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->triangle_indices, sizeof(uint32_t) * mesh->num_triangles * 3);
    vbo.nIndices = static_cast<int32_t>(mesh->num_triangles * 3);

    parsl_destroy_context(context);
    if (newBuffer) {
        bindVertexAttributes(attributes);
    }


    glBindVertexArray(0);

    out.numPaths  = nPaths;
    out.lineWidth = out.bakeOpts.linewidth;
}

void GLPathRendererParBasic::render(BakeGLPath& bakedPath, const mat4x4& matProj, const mat4x4& matView, const mat4x4& matModel) {
    glUseProgram(program2dLines);

    dbgassert(bakedPath.vbo.vaoId > 0);
    glBindVertexArray(bakedPath.vbo.vaoId);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bakedPath.vbo.vboIdxId);

    const mat4x4 mvp = matProj * (matView * matModel);
    glUniformMatrix4fv(u_mvp, 1, GL_FALSE, value_ptr(mvp));
    glUniform1f(u_linewidth, bakedPath.lineWidth);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bakedPath.vbo.vboIdxId);
    glDrawElements(GL_TRIANGLES, bakedPath.vbo.nIndices, GL_UNSIGNED_INT, nullptr);
}

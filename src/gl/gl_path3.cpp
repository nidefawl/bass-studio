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

#include "gl_path.h"
#include "gl_util.h"
#include "gl_attr.h"
#include "gl_vbo.h"
#include "gl_tess2d.h"
#include "hires_timer.h"
#include "assert_dbg.h"
#if BUILD_VSTHOST
#define PAR_STREAMLINES_IMPLEMENTATION
#endif
#include <par/par_streamlines.h>

using vec2list = std::vector<vec2>;
namespace {
    String srcShaderVertex   = R"END(
#version 150 core

uniform mat4 u_mvp;
in vec2 a_position;
in vec4 a_annotation;
out vec4 vannotation;
void main() {
  gl_Position = u_mvp * vec4(a_position.xy, 0.0, 1.0);
  vannotation = a_annotation;
}
)END";
    String srcShaderFragment = R"END(
#version 150 core

uniform vec4 u_color;
uniform float u_linewidth;
in vec4 vannotation;
out vec4 frag_color;
void main() {
  float L = vannotation.w;
  float a = min(1.0, (1.0 - abs(L)) * u_linewidth);
  frag_color = vec4(u_color.rgb, u_color.a * a);
}
)END";
}// namespace
int GLPathRendererSimple2::init() {
    GLuint vertex_shader, fragment_shader;
    vertex_shader = compileShader(GL_VERTEX_SHADER, srcShaderVertex);
    if (!vertex_shader) {
        return 1;
    }
    fragment_shader = compileShader(GL_FRAGMENT_SHADER, srcShaderFragment);
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
    } else if (!log.empty()) {
        printf("Link log: %s\n", StringAsCStr(log));
    }
    checkGLError("linkProgram");
    glUseProgram(program);
    u_mvp       = glGetUniformLocation(program, "u_mvp");
    u_color     = glGetUniformLocation(program, "u_color");
    u_linewidth = glGetUniformLocation(program, "u_linewidth");
    //
    //for (int i = 0; i < attributes.size(); i++) {
    //attributes[i].bindingPt = glGetAttribLocation(program, attributes[i].name);
    //}
    for (int i = 0; i < (int) attributes.size(); i++) {
        VertexAttr& attr = attributes[i];
        attr.bindingPt   = glGetAttribLocation(program, attr.name);
        checkGLError("glGetAttribLocation");
        //printf("%s %d\n", attributes[i].name, attr.bindingPt);
    }

    program2dLines = program;
    return 0;
}
void GLPathRendererSimple2::destroy() {
    glDeleteProgram(program2dLines);
}
void GLPathRendererSimple2::bakePaths(std::vector<vec2list> paths, Uniforms pathOpt, BakeGLPath& out) {


    int idx = 0;
    std::vector<uint16_t> spineLengths;
    std::vector<vec2> bufFinal;
    for (vec2list& list : paths) {
        if (list.size() > 1) {
            bufFinal.insert(bufFinal.end(), list.begin(), list.end());
            dbgassert(FitsTypeRange<uint16_t>(list.size()));
            spineLengths.push_back(static_cast<uint16_t>(list.size()));
            idx++;
        }
    }
    parsl_spine_list spinelist{};
    spinelist.num_vertices  = bufFinal.size();
    spinelist.vertices      = (parsl_position*) bufFinal.data();
    spinelist.num_spines    = spineLengths.size();
    spinelist.spine_lengths = spineLengths.data();

    float miterLimit = 2.0f;
    parsl_config config{};
    config.flags |= PARSL_FLAG_ANNOTATIONS;
    config.thickness       = pathOpt.linewidth;
    config.miter_limit     = config.thickness * miterLimit;
    parsl_context* context = nullptr;
    context                = parsl_create_context(config);
    parsl_mesh* mesh       = parsl_mesh_from_lines(context, spinelist);
    int nPaths             = idx;
#ifdef NO_OPENGL
    return;
#endif
    dbgassert((sizeof(parsl_position) + sizeof(parsl_annotation)) == 6 * sizeof(float));
    tmpBuffer.resize((sizeof(parsl_position) + sizeof(parsl_annotation)) * mesh->num_vertices);
    for (uint32_t i = 0; i < mesh->num_vertices; ++i) {
        int32_t index      = i * 6 + 0;
        tmpBuffer[index++] = mesh->positions[i].x;
        tmpBuffer[index++] = mesh->positions[i].y;
        tmpBuffer[index++] = mesh->annotations[i].spine_to_edge_x;
        tmpBuffer[index++] = mesh->annotations[i].spine_to_edge_y;
        tmpBuffer[index++] = mesh->annotations[i].u_along_curve;
        tmpBuffer[index++] = mesh->annotations[i].v_across_curve;
    }

    //
    //for (float f : bufFinal.v) {
    //dbgassert(!std::isnan(f) && !std::isinf(f));
    //}
    bool newBuffer = false;
    DrawVBO& vbo   = out.vbo;
    if (vbo.vaoId == 0) {
        glGenVertexArrays(1, &vbo.vaoId);
        glGenBuffers(1, &vbo.vboVertId);
        glGenBuffers(1, &vbo.vboIdxId);
        newBuffer = true;
    }
    glBindVertexArray(vbo.vaoId);
    vbo.uploadBuffer(GL_ARRAY_BUFFER, tmpBuffer.data(), tmpBuffer.size());
    vbo.uploadBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->triangle_indices, sizeof(uint32_t) * mesh->num_triangles * 3);
    vbo.nIndices = mesh->num_triangles * 3;

    parsl_destroy_context(context);
    if (newBuffer) {
        bindVertexAttributes(attributes);
    }


    glBindVertexArray(0);

    out.numPaths  = nPaths;
    out.lineWidth = pathOpt.linewidth;
}

void GLPathRendererSimple2::render(BakeGLPath& bakedPath, const mat4x4& matProj, const mat4x4& matView, const mat4x4& matModel) {
    mat4x4 mvp = matProj * (matView * matModel);
    glUniformMatrix4fv(u_mvp, 1, GL_FALSE, value_ptr(mvp));
    glUniform4f(u_color, 1, 1, 1, 1);
    glUniform1f(u_linewidth, bakedPath.lineWidth);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bakedPath.vbo.vboIdxId);
    glDrawElements(GL_TRIANGLES, bakedPath.vbo.nIndices, GL_UNSIGNED_INT, NULL);
}

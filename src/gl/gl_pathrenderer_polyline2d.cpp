#include "glheaders.h"
#include <cmath>
#include <vector>

#include "math/seq_math.h"
#include "math/vec.h"
#include "math/mat.h"
#include "str_util.h"
#include "fileio.h"
#include "host/audiocache/audiocache.h"

#include "gl_pathrenderer.h"
#include "gl_util.h"
#include "gl_attr.h"
#include "gl_vbo.h"
#include "gl_tess2d.h"
#include "hires_timer.h"
#include "assert_dbg.h"
#include <polyline/Polyline2D.h>
#include <algorithm>
#include <glm/gtc/type_ptr.hpp>

int GLPathRendererPolyline2d::init() {
    String srcVertex;
    String srcFragment;
    int64_t ret = ReadFileText("polyline2d.vsh", srcVertex);
    if (ret <= 0) {
        return 1;
    }
    ret = ReadFileText("polyline2d.fsh", srcFragment);
    if (ret <= 0) {
        return 1;
    }

    GLuint vertex_shader, fragment_shader;
    vertex_shader = compileShader(GL_VERTEX_SHADER, srcVertex);
    if (!vertex_shader) {
        return 1;
    }
    fragment_shader = compileShader(GL_FRAGMENT_SHADER, srcFragment);
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
        log_lf(Log::L_ERROR, "Link error: %s\n", StringAsCStr(log));
        return 1;
    } else if (!log.empty()) {
        log_lf(Log::L_WARN, "Link log: %s\n", StringAsCStr(log));
    }
    checkGLError("linkProgram");
    glUseProgram(program);
    u_mvp   = glGetUniformLocation(program, "u_mvp");
    u_color = glGetUniformLocation(program, "u_color");
    //
    //for (int i = 0; i < attributes.size(); i++) {
    //attributes[i].bindingPt = glGetAttribLocation(program, attributes[i].name);
    //}
    for (int i = 0; i < (int) attributes.size(); i++) {
        VertexAttr& attr = attributes[i];
        attr.bindingPt   = glGetAttribLocation(program, attr.name);
        checkGLError("glGetAttribLocation");
        //log_lf(Log::L_TRACE "%s %d\n", attributes[i].name, attr.bindingPt);
    }

    program2dLines = program;
    return 0;
}
void GLPathRendererPolyline2d::destroy() {
    glDeleteProgram(program2dLines);
}
void GLPathRendererPolyline2d::bakePaths(const std::vector<path_t>& paths, BakeGLPath& out) {
    dbgassert(!paths.empty());
    out.bakeOpts = paths.at(0).pathOpts;

    using namespace crushedpixel;
    float thickness = out.bakeOpts.linewidth;
    int idx         = 0;
    std::vector<vec2> bufFinal;
    for (auto& path : paths) {
        auto& list = path.pathVecs;
        if (list.size() > 1) {
            /*auto len = */Polyline2D::create(bufFinal, list, thickness,
                                          Polyline2D::JointStyle::ROUND,
                                          Polyline2D::EndCapStyle::BUTT);
            idx++;
        }
    }
    int nPaths = idx;
#ifdef NO_OPENGL
    return;
#endif

    //
    //for (float f : bufFinal.v) {
    //dbgassert(!std::isnan(f) && !std::isinf(f));
    //}
    bool newBuffer = false;
    DrawVBO& vbo   = out.vbo;
    if (vbo.vaoId == 0) {
        glGenVertexArrays(1, &vbo.vaoId);
        vbo.genBuffers();
        newBuffer = true;
    }
    glBindVertexArray(vbo.vaoId);
    vbo.uploadBuffer(GL_ARRAY_BUFFER, bufFinal.data(), sizeof(vec2) * bufFinal.size());

    if (newBuffer) {
        bindVertexAttributes(attributes);
    }


    glBindVertexArray(0);

    out.numPaths     = nPaths;
    out.vbo.nIndices = static_cast<int32_t>(bufFinal.size());
}

void GLPathRendererPolyline2d::render(BakeGLPath& bakedPath, const mat4x4& matProj, const mat4x4& matView, const mat4x4& matModel) {
    glUseProgram(program2dLines);

    dbgassert(bakedPath.vbo.vaoId > 0);
    glBindVertexArray(bakedPath.vbo.vaoId);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bakedPath.vbo.vboIdxId);

    const mat4x4 mvp = matProj * (matView * matModel);
    const auto& bakeOpt = bakedPath.bakeOpts;
    glDisable(GL_CULL_FACE);
    glUniformMatrix4fv(u_mvp, 1, GL_FALSE, value_ptr(mvp));
    glUniform4f(u_color, 1, 1, 1, 1);
    glUniform4f(u_color, bakeOpt.color.r, bakeOpt.color.g, bakeOpt.color.b, bakeOpt.color.a);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glDrawArrays(GL_TRIANGLES, 0, bakedPath.vbo.nIndices);
    glEnable(GL_CULL_FACE);

}

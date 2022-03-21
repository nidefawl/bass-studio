#include "glheaders.h"
#include <cmath>
#include <vector>
#include <algorithm>
#include <array>
#include <glm/gtc/type_ptr.hpp>


#include "math/seq_math.h"
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
#include "logging.h"
#include "assert_dbg.h"


void buildIndices(uint32_t numVertices, uint32_t offset, std::vector<uint32_t>& _out) {
    // static int quadIdx[] = { 0, 1, 2, 2, 1, 3 };
    // static int quadIdx[] = { 2, 1, 0, 3, 1, 2 };
    static std::array<uint32_t, 6> quadIndices { 0, 1, 2, 1, 2, 3 };
    for (uint32_t i = 0; i < numVertices; i++) {
        for (uint32_t idx : quadIndices)
            _out.push_back(offset + idx + i * 4);
    }
}

static inline float fast_atan2(float y, float x) {
    static const auto c1 = static_cast<float>(M_PI / 4.0);
    static const auto c2 = static_cast<float>(M_PI * 3.0 / 4.0);
    if (y == 0 && x == 0)
        return 0;
    float abs_y = fabsf(y);
    float angle;
    if (x >= 0)
        angle = c1 - c1 * ((x - abs_y) / (x + abs_y));
    else
        angle = c2 - c1 * ((x + abs_y) / (abs_y - x));
    if (y < 0)
        return -angle;
    return angle;
}

float packVertexData2(vec2list& verticesIn, std::vector<vert>& outVdata, int index = 0, bool closed = false) {

    vec2list vertices = verticesIn;
    float dist        = glm::distance(vertices.front(), vertices.back());
    if (closed && dist > 1e-10) {
        vertices.push_back(verticesIn.front());
    }

    auto n = vertices.size();
    std::vector<vert> vdata(n);
    memset(vdata.data(), 0, vdata.size() * sizeof(vert));
    vec2list T(n - 1);
    std::vector<float> N(n - 1);
    size_t idx = 0;
    for (vec2& v : vertices) {
        vert& vd = vdata[idx++];
        vd.pos   = v;
        vd.index = index;
    }
    for (size_t i = 1; i < n; i++) {
        T[i - 1] = vertices[i] - vertices[i - 1];
        N[i - 1] = glm::length(T[i - 1]);
        //    printf("T[%d] = %f %f\n", i-1, T[i-1].x, T[i-1].y);
    }
    for (size_t i = 1; i < n; i++) {
        vdata[i].tangent0     = T[i - 1];
        vdata[i - 1].tangent1 = T[i - 1];
    }
    if (closed) {
        vdata[0].tangent0     = T[n - 2];
        vdata[n - 1].tangent1 = T[0];
    } else {
        vdata[0].tangent0     = T[0];
        vdata[n - 1].tangent1 = T[n - 2];
    }
    std::vector<float> atans(n);
    for (size_t i = 0; i < n; i++) {
        vert& p  = vdata[i];
        float x  = p.tangent0.x * p.tangent1.y - p.tangent0.y * p.tangent1.x;
        float y  = p.tangent0.x * p.tangent1.x + p.tangent0.y * p.tangent1.y;
        atans[i] = fast_atan2(x, y);
    }
    for (size_t i = 0; i < n - 1; i++) {
        vdata[i].angles.x = atans[i];
        vdata[i].angles.y = atans[i + 1];
    }
    float fLength = 0;
    for (size_t i = 0; i < n - 1; i++) {
        fLength += N[i];
        vdata[i + 1].seg.x = fLength;
        vdata[i].seg.y     = fLength;
    }

    std::vector<vert> vdata2;
    vdata2.resize(n * 2 - 2);
    vdata2[0] = vdata[0];
    for (size_t i = 1, j = 1; i < n - 1; i++, j += 2) {
        vert p        = vdata[i];
        p.seg         = vdata[i - 1].seg;
        p.angles      = vdata[i - 1].angles;
        vdata2[j + 0] = p;
        vdata2[j + 1] = vdata[i];
    }
    vert p   = vdata[n - 1];
    p.seg    = vdata[n - 2].seg;
    p.angles = vdata[n - 2].angles;
    dbgassert(vdata2.size() == n * 2 - 2);
    vdata2[n * 2 - 3] = p;

    n = vdata2.size();
    for (size_t i = 0; i < n; i += 2) {
        vdata2[i].tex     = vec2(-1);
        vdata2[i + 1].tex = vec2(1);
    }
    outVdata.resize(n * 2);
    for (size_t i = 0; i < n; ++i) {
        outVdata[i * 2 + 0]       = vdata2[i];
        outVdata[i * 2 + 1]       = vdata2[i];
        outVdata[i * 2].tex.y     = -1;
        outVdata[i * 2 + 1].tex.y = 1;
    }

    return fLength;
}
bool readShaderSrc(const String& filename, String& out) {
    out         = "";
    int64_t ret = ReadFileText(filename, out);
    if (ret <= 0) {
        log_printf("%s: Failed reading file", StringAsCStr(filename));
        return false;
    }
    if (out.empty()) {
        log_printf("%s: File is empty", StringAsCStr(filename));
        return false;
    }
    return true;
}
int GLPathRendererDashLines::init() {

    String srcVertex;
    String srcFragment;

    if (!readShaderSrc("dash-lines-2D.vsh", srcVertex)) {
        return 1;
    }
    if (!readShaderSrc("dash-lines-2D.fsh", srcFragment)) {
        return 1;
    }

    GLuint vertex_shader, fragment_shader;
    vertex_shader = compileShader(GL_VERTEX_SHADER, srcVertex);
    if (!vertex_shader) {
        return 2;
    }
    fragment_shader = compileShader(GL_FRAGMENT_SHADER, srcFragment);
    if (!fragment_shader) {
        return 2;
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
        return 3;
    }
    if (!log.empty()) {
        printf("Link log: %s\n", StringAsCStr(log));
    }
    checkGLError("linkProgram");
    glUseProgram(program);
    u_dash_atlas     = glGetUniformLocation(program, "u_dash_atlas");
    u_model          = glGetUniformLocation(program, "u_model");
    u_view           = glGetUniformLocation(program, "u_view");
    u_projection     = glGetUniformLocation(program, "u_projection");
    u_uniforms       = glGetUniformLocation(program, "u_uniforms");
    u_uniforms_shape = glGetUniformLocation(program, "u_uniforms_shape");

    for (VertexAttr& attr : attributes) {
        attr.bindingPt   = glGetAttribLocation(program, attr.name);
        checkGLError("glGetAttribLocation");
    }

    glUniform1i(u_uniforms, 0);
    glUniform1i(u_dash_atlas, 1);
    checkGLError("glUniform1i");
    program2dLines = program;
    return 0;
}
void GLPathRendererDashLines::destroy() {
    glDeleteProgram(program2dLines);
}
void GLPathRendererDashLines::bakePaths(std::vector<vec2list> paths, Uniforms pathOpt, BakeGLPath& out) {
    std::vector<vert> outVdata;
    vbuf bufFinal;
    std::vector<float> bufUniforms;
    dbgassert((int) sizeof(Uniforms) <= sizeUniforms);
    bufUniforms.resize(paths.size() * sizeUniforms);
    bufFinal.i.clear();
    const int sizeFloatsVert = sizeof(vert) / sizeof(float);
    size_t flBufUniformsPos  = 0;
    size_t flBufVertsPos     = 0;
    int packed               = 0;
    int idx                  = 0;
    for (vec2list& list : paths) {
        if (list.size() > 1) {
            float len = packVertexData2(list, outVdata, idx);
            size_t flBufPos    = flBufVertsPos * sizeFloatsVert;
            size_t flBakedSize = outVdata.size() * sizeFloatsVert;
            bufFinal.v.resize(flBufPos + flBakedSize);
            memcpy(bufFinal.v.data() + flBufPos, outVdata.data(), flBakedSize * sizeof(float));
            buildIndices(outVdata.size() / 4, flBufVertsPos, bufFinal.i);
            Uniforms uniforms = pathOpt;
            uniforms.length   = len;
            memcpy(bufUniforms.data() + flBufUniformsPos, &uniforms, sizeof(Uniforms));
            flBufUniformsPos += sizeUniforms;
            flBufVertsPos += outVdata.size();
            packed++;
            outVdata.clear();
            idx++;
        }
    }
    size_t nPaths = 0;
    if (flBufVertsPos == 0) {
        bufFinal.v.resize(0);
        bufFinal.i.resize(0);
        bufUniforms.resize(0);
        nPaths = 0;
    } else {
        nPaths = paths.size();
    }
#ifdef NO_OPENGL
    return;
#endif

    bool newBuffer = false;
    DrawVBO& vbo   = out.vbo;
    if (vbo.vaoId == 0) {
        glGenVertexArrays(1, &vbo.vaoId);
        glGenBuffers(1, &vbo.vboVertId);
        glGenBuffers(1, &vbo.vboIdxId);
        newBuffer = true;
    }
    glBindVertexArray(vbo.vaoId);
    vbo.uploadBuffer(GL_ARRAY_BUFFER, bufFinal.v.data(), sizeof(float) * bufFinal.v.size());
    vbo.uploadBuffer(GL_ELEMENT_ARRAY_BUFFER, bufFinal.i.data(), sizeof(uint32_t) * bufFinal.i.size());

    if (newBuffer) {
        bindVertexAttributes(attributes, ATTR_STRIDE);
    }

    auto texSize = static_cast<int32_t>(bufUniforms.size() / 4);
    glActiveTexture(GL_TEXTURE0);
    if (out.uniforms_texture && nPaths * countUniforms != texSize) {
        log_printf("tex shape changed %d %d\n", out.numPaths * countUniforms, texSize);
        glDeleteTextures(1, &out.uniforms_texture);
        out.uniforms_texture = 0;
    }
    if (out.uniforms_texture == 0)
        glGenTextures(1, &out.uniforms_texture);
    glBindTexture(GL_TEXTURE_2D, out.uniforms_texture);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, texSize, 1, 0, GL_RGBA, GL_FLOAT, bufUniforms.data());
    checkGLError("glTexImage2D");

    glBindVertexArray(0);

    out.numPaths     = nPaths;
    out.vbo.nIndices = bufFinal.i.size();
}

void GLPathRendererDashLines::render(BakeGLPath& bakedPath, const mat4x4& matProj, const mat4x4& matView, const mat4x4& matModel) {
    glDisable(GL_CULL_FACE);

    glUniformMatrix4fv(u_projection, 1, GL_FALSE, value_ptr(matProj));
    glUniformMatrix4fv(u_view, 1, GL_FALSE, value_ptr(matView));
    glUniformMatrix4fv(u_model, 1, GL_FALSE, value_ptr(matModel));
    glUniform3f(u_uniforms_shape, 1, bakedPath.numPaths * countUniforms, countUniforms);

    glBindTexture(GL_TEXTURE_2D, bakedPath.uniforms_texture);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bakedPath.vbo.vboIdxId);
    glBindTexture(GL_TEXTURE_2D, bakedPath.uniforms_texture);
    glDrawElements(GL_TRIANGLES, bakedPath.vbo.nIndices, GL_UNSIGNED_INT, nullptr);
    glEnable(GL_CULL_FACE);
}

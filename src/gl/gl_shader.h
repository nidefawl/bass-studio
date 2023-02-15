#pragma once
#include <memory>
#include "glheaders.h"
#include "types.h"
#include "logging.h"
#include "str_util.h"
#include "gl/gl_util.h"
#include "gl/gl_attr.h"
#include "gl/gl_vbo.h"

struct glshader_src {
    int32_t stage;
    String filepath;
    String source;
};
struct glshader_srcloader {
    std::vector<glshader_src> sources;
    bool addStageSrc(int32_t type, const char* fname);
    bool setStageSrc(int32_t type, const String& fname, const String& strSrc);
    bool reload();
};

int32_t prependGLSL(String& s, const String& src);
int32_t buildShaderProgram(const std::vector<glshader_src>& srcList);
template<typename T>
int compileShaderCombo(T* owner, const char* fnameVsh, const char* fnameFsh) {
    auto glSourceLoader = std::make_unique<glshader_srcloader>();
    if (!glSourceLoader->addStageSrc(GL_VERTEX_SHADER, fnameVsh))
        return -2;
    if (!glSourceLoader->addStageSrc(GL_FRAGMENT_SHADER, fnameFsh))
        return -2;
    owner->preprocessSources(glSourceLoader->sources);
    return buildShaderProgram(glSourceLoader->sources);
}

struct gl_shader_program_base_t : public OpenGLResource {
    GLint program = 0;
    std::vector<VertexAttr> attributes;
    ~gl_shader_program_base_t() override {
        destroy();
    };
    void destroy() override {
        if (program && makeContextCurrent()) {
            glDeleteProgram(program);
            checkGLError("glDeleteProgram");
        }
    }
    int setAttributeLocations() {
        for (VertexAttr& attr : attributes) {
            attr.bindingPt   = glGetAttribLocation(program, attr.name);
            if (checkGLError("glGetAttribLocation"))
                return -1;
            if (attr.bindingPt < 0) {
                log_lf(Log::L_WARN, "%s %d\n", attr.name, attr.bindingPt);
                log_lf(Log::L_WARN, "Missing attribLocation %s\n", attr.name);
                return 1;
            }
        }
        return 0;
    }

    /*
     * bindBuffer helper method
     *
     * Only needs to be called once per vbo and vao when using single DrawVBO instance
     * Note that GL_ELEMENT_ARRAY_BUFFER is not state of vao!
     */
    void bindBuffer(const DrawVBO& vbo) {
        dbgassert(vbo.vaoId > 0);
        dbgassert(vbo.vboVertId > 0);
        dbgassert(vbo.vboIdxId > 0);
        glBindVertexArray(vbo.vaoId);
        glBindBuffer(GL_ARRAY_BUFFER, vbo.vboVertId);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo.vboIdxId);
        bindVertexAttributes(attributes);
    }
};
struct gl_shader_pipeline : gl_shader_program_base_t {
    DrawVBO vbo;
    GLint u_mvp      = 0;
    GLint u_tex0     = 0;
    GLint u_tex1     = 0;
    GLint u_time     = 0;
    GLint u_viewport = 0;
    int bindAttributes() {
        u_mvp      = glGetUniformLocation(program, "u_mvp");
        u_tex0     = glGetUniformLocation(program, "tex0");
        u_tex1     = glGetUniformLocation(program, "tex1");
        u_time     = glGetUniformLocation(program, "u_time");
        u_viewport = glGetUniformLocation(program, "u_viewport");
        setAttributeLocations();
        if (u_tex0 >= 0)
            glUniform1i(u_tex0, 0);
        if (u_tex1 >= 0)
            glUniform1i(u_tex1, 1);
        glBindFragDataLocation(program, 0, "out_Color");
        if (checkGLError("glBindFragDataLocation"))
            return -1;
        return 0;
    };
};

#include "glheaders.h"
#include <vector>
#include <nanovg.h>
#include <nanovg_gl.h>
#include <memory>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "math/seq_math.h"
#include "math/vec.h"
#include "math/mat.h"

#include "platform.h"
#include "gui/gui.h"
#include "guishaderview.h"
#include "gl/gl_framebuffer.h"
#include "gl/gl_shader.h"
#include "gl/gl_tess2d.h"
#include "gl/gl_util.h"
#include "rand.h"

struct testshader : gl_shader_pipeline {
    ivec2 lastBufSize = { -1, -1 };
    GLuint texture    = 0;
    testshader() {
        attributes = {
            { "a_position", 2, GL_FLOAT },
            { "a_texcoord", 2, GL_FLOAT }
        };
    }
    ~testshader() {
        //TODO: we should check if gl context is still present
        // or redesign shader resource management strategy so we don't have to do this here in the destructor
        if (texture)
            glDeleteTextures(1, &texture);
    }
    void setUniforms(int32_t w, int32_t h, float fTime) {
        if (u_viewport >= 0)
            glUniform2f(u_viewport, w, h);
        if (u_time >= 0)
            glUniform1f(u_time, fTime);
    }
    template<typename T>
    int load(T* srcParser) {
        const char* fnameVsh = "test.vsh";
        const char* fnameFsh = "test.fsh";
        int newprogram       = compileShaderCombo(srcParser, fnameVsh, fnameFsh);
        if (newprogram < 0) {
            dbgassert(newprogram != -2);
            return -1;
        }
        program = newprogram;
        glUseProgram(program);
        if (bindAttributes()) {
            return -1;
        }
        glGenVertexArrays(1, &vbo.vaoId);
        glBindVertexArray(vbo.vaoId);
        vbo.genBuffers();
        glBindBuffer(GL_ARRAY_BUFFER, vbo.vboVertId);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo.vboIdxId);
        bindVertexAttributes(attributes);
        glBindVertexArray(0);

        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
        checkGLError("glTexImage2D");
        return 0;
    };
};
struct gui_shaderview_impl_t {
    NVGLUframebuffer* fb = nullptr;
    std::shared_ptr<testshader> pipeTestShader;
    int64_t initTime = 0;
};
gui_shaderview::gui_shaderview() : guictr_base(), impl(new gui_shaderview_impl_t) {
    ctrType = CTR_TYPE_SHADERVIEW;
    setBackgroundRendered(false);
}
gui_shaderview::~gui_shaderview() {
    delete impl;
}
void gui_shaderview::prerender(NVGcontext* vg) {
    if (!impl->pipeTestShader) {
        impl->pipeTestShader = std::make_shared<testshader>();
        struct shader_src_parser_noise {
            void preprocessSources(std::vector<glshader_src>& srcList) {
            }
        };
        shader_src_parser_noise parser;
        impl->pipeTestShader->load(&parser);
    }
    int w    = math::min(size.x, size.y);
    int h    = w;
    impl->fb = nvgluCreateTempFramebuffer(vg, w, h, NVG_IMAGE_PREMULTIPLIED);
    nvgluBindFramebuffer(impl->fb);
    glViewport(0, 0, w, h);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    auto pipeline = impl->pipeTestShader.get();
    auto& vbo     = pipeline->vbo;
    glUseProgram(pipeline->program);
    glm::mat4x4 matProj = glm::ortho(0.f, (float) w, (float) h, 0.f, 1.0f, -1.0f);
    pipeline->setUniforms(w, h, getTimeMillisF());
    glUniformMatrix4fv(pipeline->u_mvp, 1, GL_FALSE, mat_ptr(matProj));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, pipeline->texture);

    glBindVertexArray(vbo.vaoId);
    if (pipeline->lastBufSize.x != w || pipeline->lastBufSize.y != h) {
        pipeline->lastBufSize = { w, h };
        tess2d tess(0);
        tess2d::fullscreenQuad(tess, w, h);
        tess2d::uploadVBO(tess, vbo);
    }
    glDrawElements(GL_TRIANGLES, vbo.nIndices, GL_UNSIGNED_INT, NULL);

    nvgluBindFramebuffer(nullptr);
}
void gui_shaderview::onTick(AppCtrl* appctrl) {
    if (impl->pipeTestShader) {
        auto now = getTimeMillis();
        if (now - impl->initTime > 1000) {
            impl->initTime = now;
            seq_rand rnd;
            rnd.rng_seed(now);
            const int texW = math::min(size.x, size.y);
            //const int texW = 128;
            std::vector<float> texData;
            texData.resize(texW * texW);
//            for (int x = 0; x < texW; x++) {
//                float fx = x / (texW - 1.0f);
//                for (int y = 0; y < texW; y++) {
//                    float fy              = y / (texW - 1.0f);
//                    texData[y * texW + x] = fx * fy;
//                }
//            }
            for (int x = 0; x < texW; x++) {
                for (int y = 0; y < texW; y++) {
                    int32_t r             = rnd.rng_rand();
                    texData[y * texW + x] = (r & 0xFFFF) / (float) 0xFFFF;
                }
            }
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, impl->pipeTestShader->texture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, texW, texW, 0, GL_RED, GL_FLOAT, texData.data());
        }
    }
}
void gui_shaderview::render(NVGcontext* vg) {
    if (isBackgroundRendered()) {
        renderWidgetBorder(vg, getStateFlags());
    }
    if (!setScissorTransform(vg)) {
        return;
    }

    //dbgassert(impl->fb);
    if (impl->fb) {
        int w        = math::min(size.x, size.y);
        int h        = w;
        NVGpaint img = nvgImagePattern(vg, 0, 0, w, h, 0, impl->fb->image, 1);
        nvgTranslate(vg, size.x - w, 0);
        nvgBeginPath(vg);
        nvgRect(vg, 0, 0, w, h);
        nvgFillPaint(vg, img);
        nvgFill(vg);
    }
}

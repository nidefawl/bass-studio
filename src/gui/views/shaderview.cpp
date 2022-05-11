#include "glheaders.h"
#include <vector>
#include <nanovg.h>
#include <nanovg_gl.h>
#include <memory>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "math/seq_math.h"
#include "math/vec.h"
#include "math/mat.h"

#include "platform.h"
#include "gui/gui.h"
#include "shaderview.h"
#include "gl/gl_framebuffer.h"
#include "gl/gl_shader.h"
#include "gl/gl_tess2d.h"
#include "gl/gl_util.h"
#include "rand.h"

struct testshader : gl_shader_pipeline {
    bool isValid = false;
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
        if (checkGLError("glGenVertexArrays and genBuffers"))
            return -1;

        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
        if (checkGLError("glGenTextures"))
            return -1;
        isValid = true;
        return 0;
    }
};
class gui_shaderview_impl_t {
    NVGLUframebuffer* fb = nullptr;
    std::shared_ptr<testshader> pipeTestShader;
    int64_t initTime = 0;
    std::vector<float> texData;
    bool hasTextureUpdate = false;
    int texWidth = 0;

public:
    void prerender(NVGcontext* vg, ivec2 size) {
        if (!pipeTestShader) {
            pipeTestShader = std::make_shared<testshader>();
            struct shader_src_parser_noise {
                void preprocessSources(std::vector<glshader_src>& srcList) {
                }
            };
            shader_src_parser_noise parser;
            pipeTestShader->load(&parser);
        }
        if (!pipeTestShader->isValid)
            return;
        if (hasTextureUpdate) {
            hasTextureUpdate = false;
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, pipeTestShader->texture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, texWidth, texWidth, 0, GL_RED, GL_FLOAT, texData.data());
        }
        int w    = math::min(size.x, size.y);
        int h    = w;
        fb = nvgluCreateTempFramebuffer(vg, w, h, NVG_IMAGE_PREMULTIPLIED);
        nvgluBindFramebuffer(fb);
        glViewport(0, 0, w, h);
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        auto pipeline = pipeTestShader.get();
        auto& vbo     = pipeline->vbo;
        glUseProgram(pipeline->program);
        mat4x4 matProj = glm::ortho(0.f, (float) w, (float) h, 0.f, 1.0f, -1.0f);
        pipeline->setUniforms(w, h, getTimeMillisF());
        glUniformMatrix4fv(pipeline->u_mvp, 1, GL_FALSE, value_ptr(matProj));
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, pipeline->texture);

        glBindVertexArray(vbo.vaoId);
        if (pipeline->lastBufSize.x != w || pipeline->lastBufSize.y != h) {
            pipeline->lastBufSize = { w, h };
            tess2d tess(0);
            tess2d::fullscreenQuad(tess, w, h);
            tess2d::uploadVBO(tess, vbo);
        }
        glDrawElements(GL_TRIANGLES, vbo.nIndices, GL_UNSIGNED_INT, nullptr);

        nvgluBindFramebuffer(nullptr);
    }
    void onTick(ivec2 size) {
        if (!pipeTestShader || !pipeTestShader->isValid) {
            return;
        }
        auto now = getTimeMillis();
        if (now - initTime > 1000) {
            initTime = now;
            seq_rand rnd;
            rnd.rng_seed(now);
            const auto texW = math::min(size.x, size.y);
            texData.resize(texW * texW);
            for (int x = 0; x < texW; x++) {
                for (int y = 0; y < texW; y++) {
                    int32_t r = rnd.rng_rand();
                    texData[y * texW + x] = (r & 0xFFFF) / (float) 0xFFFF;
                }
            }
            texWidth = texW;
            hasTextureUpdate = true;
        }
    }
    int getNvgImageId() {
        return fb ? fb->image : -1;
    }
};
gui_shaderview::gui_shaderview() : guictr_base(), impl(new gui_shaderview_impl_t) {
    ctrType = CTR_TYPE_SHADERVIEW;
    setBackgroundRendered(false);
}
gui_shaderview::~gui_shaderview() {
    delete impl;
}
void gui_shaderview::prerender(NVGcontext* vg) {
    impl->prerender(vg, size);
}
void gui_shaderview::onTick(AppCtrl* appctrl) {
    impl->onTick(size);
}
void gui_shaderview::render(NVGcontext* vg) {
    if (isBackgroundRendered()) {
        renderWidgetBorder(vg, getStateFlags());
    }
    int nvgImageId = impl->getNvgImageId();
    if (nvgImageId <= 0) {
        return;
    }
    if (!setScissorTransform(vg)) {
        return;
    }
    int w = math::min(size.x, size.y);
    int h = w;
    NVGpaint img = nvgImagePattern(vg, 0, 0, w, h, 0, nvgImageId, 1);
    nvgTranslate(vg, size.x - w, 0);
    nvgBeginPath(vg);
    nvgRect(vg, 0, 0, w, h);
    nvgFillPaint(vg, img);
    nvgFill(vg);
}

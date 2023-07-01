#pragma once
#include "glheaders.h"
#include "types.h"
#include "logging.h"
#include "str_util.h"
#include "fileio.h"

#include "gl/gl_util.h"
#include "gl/gl_vbo.h"
#include "gl/gl_attr.h"
#include "gl/gl_tess2d.h"
#include "gl/gl_framebuffer.h"
#include "gl/gl_shader.h"
#include "gl/builtin_shaders.h"

#include "math/vec.h"
#include "math/mat.h"
#include <glm/gtc/type_ptr.hpp>

enum class blur_num_passes : int32_t {
    PASSES_2 = 0,
    PASSES_3,
    PASSES_5,
    PASSES_7,
    PASSES_10
};
namespace blur_pass_parameters {
    std::vector<int32_t> BLUR_2PASS{0, 0};
    std::vector<int32_t> BLUR_3PASS{0, 1, 1};
    std::vector<int32_t> BLUR_5PASS{0, 1, 2, 2, 3};
    std::vector<int32_t> BLUR_7PASS{0, 1, 2, 3, 4, 4, 5};
    std::vector<int32_t> BLUR_10PASS{0, 1, 2, 3, 4, 5, 7, 8, 9, 10};
    inline std::vector<int32_t>& getPassConstants(blur_num_passes n) {
        switch (n) {
            case blur_num_passes::PASSES_2:
                return BLUR_2PASS;
            case blur_num_passes::PASSES_3:
                return BLUR_3PASS;
            case blur_num_passes::PASSES_5:
                return BLUR_5PASS;
            case blur_num_passes::PASSES_7:
                return BLUR_7PASS;
            case blur_num_passes::PASSES_10:
                return BLUR_10PASS;
        }
        return BLUR_2PASS;
    }
}// namespace blur_pass_parameters
struct blur_tex_shader : gl_shader_pipeline {
public:
    struct rendercontext_t {
        int w = 0;
        int h = 0;
        GLuint inputTexture  = 0;
        GLuint outputTexture = 0;
        double timeAbs = 0.0;
    };
    blur_tex_shader() {
        attributes = {
            {"a_position", 2, GL_FLOAT},
            {"a_texcoord", 2, GL_FLOAT}
        };
    }
    ~blur_tex_shader() {
        if (isGLContextPresent()) {
            for (auto& fb: framebuffers) {
                fb->destroy();
            }
        }
        framebuffers.clear();
    }

public:
    template<typename T>
    int load(T* srcParser) {
        storeGlContext();
        const char* fnameVsh = "textured_fullscreen.vsh";
        const char* fnameFsh = "blur.fsh";
        int newprogram       = compileShaderCombo(srcParser, fnameVsh, fnameFsh);
        if (newprogram < 0) {
            newprogram = compileBuiltinShader(srcParser, TEXTURED_FULLSCREEN_GLSL_VERT, BLUR_GLSL_FRAG);
        }
        if (newprogram < 0) {
            log_lf(Log::L_WARN, "failed loading shaders: %s %s\n", fnameVsh, fnameFsh);
            return -1;
        }
        program = newprogram;
        glUseProgram(program);
        u_blurPassProp = glGetUniformLocation(program, "blurPassProp");
        if (bindAttributes()) {
            log_printf("bind attributes failed\n");
            return -1;
        }
        glGenVertexArrays(1, &vbo.vaoId);
        glUniform1i(u_tex0, 0);
        return 0;
    }


    void render(rendercontext_t* ctxt, blur_num_passes passes, float pixelScale) {
        if (framebuffers.empty()) {
            ctxt->outputTexture = ctxt->inputTexture;
            return;
        }
        const auto w = ctxt->w;
        const auto h = ctxt->h;
        glUseProgram(program);
        setCommonUniforms(ctxt);
        mat4x4 matProj = glm::ortho(0.f, (float) w, (float) h, 0.f, -1.0f, 1.0f);
        glUniformMatrix4fv(u_mvp, 1, GL_FALSE, value_ptr(matProj));
        glViewport(0, 0, wd, hd);
        glActiveTexture(GL_TEXTURE0);

        glBindVertexArray(vbo.vaoId);
        if (this->w != w || this->h != h) {
            tess2d tess(0);
            tess2d::fullscreenQuad(tess, w, h);
            tess2d::uploadVBO(tess, vbo);
            checkGLError("uploadVBO");
            bindVertexAttributes(attributes);
        }
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo.vboIdxId);
        
        auto input = ctxt->inputTexture;
        FrameBuffer* bufferTarget = framebuffers[0].get();

        std::vector<int32_t>& kawaseKernPasses = blur_pass_parameters::getPassConstants(passes);
        for (size_t p = 0; p < kawaseKernPasses.size(); p++) {
            glUniform3f(u_blurPassProp, pixelScale / (float) w, pixelScale / (float) h, kawaseKernPasses[p]);
            bufferTarget->bind();
            bufferTarget->clearFrameBuffer();
            glBindTexture(GL_TEXTURE_2D, input);
            glDrawElements(GL_TRIANGLES, vbo.nIndices, GL_UNSIGNED_INT, nullptr);
            input        = bufferTarget->colorAttTextures[0];
            bufferTarget = framebuffers[(p + 1) % 2].get();
        }
        checkGLError("glDrawElements");
        ctxt->outputTexture = input;
        FrameBuffer::unbindFramebuffer();
        glViewport(0, 0, w, h);
        glBindVertexArray(0);
    }
    void setupFramebuffers(int w, int h, int downsample) {
        for (auto& fb: framebuffers) {
            fb->destroy();
        }
        framebuffers.clear();
        downsampledResolution(w, h, downsample, wd, hd);
        for (int i = 0; i < 2; i++) {
            auto fb = std::make_shared<FrameBuffer>(wd, hd, GL_RGBA16F, false, vec4(0.0f));
            fb->setup();
            framebuffers.push_back(fb);
        }
    }

private:
    void setCommonUniforms(rendercontext_t* ctxt) {
        if (u_viewport >= 0)
            glUniform2f(u_viewport, ctxt->w, ctxt->h);
        if (u_time >= 0)
            glUniform1f(u_time, static_cast<float>(ctxt->timeAbs));
    }

private:
    int w = 0;
    int h = 0;
    int wd = 0;
    int hd = 0;
    std::vector<std::shared_ptr<FrameBuffer>> framebuffers;
    DrawVBO vbo;
    GLint u_blurPassProp    = 0;
};

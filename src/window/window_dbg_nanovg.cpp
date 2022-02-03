#include "glheaders.h"
#include <memory>
#include <nanovg.h>
#include <nanovg_gl.h>
#include <nanovg_gl_utils.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "math/vec.h"
#include "math/mat.h"
#include "fileio.h"
#include "str_util.h"
#include "../gl/gl_util.h"
#include "../gl/gl_attr.h"
#include "../gl/gl_vbo.h"
#include "../gl/gl_tess2d.h"
#include "renderresources.h"
#include "color_util.h"
#include "rand.h"
#include "platform.h"

namespace windowdebug_dbgnanovg {

    GLuint program2dTexture;
    GLint u_mvp;
    GLint u_tex0;

    float wTexPreview = 1024;
    std::vector<VertexAttr> attributes{
        { "in_position", 2, GL_FLOAT },
        { "in_texcoord", 2, GL_FLOAT },
    };
    DrawVBO vbo;
    int loadShader() {
        String srcVertex;
        String srcFragment;
        int64_t ret = ReadFileText("textured.vsh", srcVertex);
        if (ret <= 0) {
            printf("Cannot read file shader.vert\n");
            return 1;
        }
        ret = ReadFileText("textured.fsh", srcFragment);
        if (ret <= 0) {
            printf("Cannot read file shader.frag\n");
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
        glBindFragDataLocation(program, 0, "out_Color");
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
        u_mvp  = glGetUniformLocation(program, "mvp");
        u_tex0 = glGetUniformLocation(program, "tex0");
        for (auto & attribute : attributes) {
            attribute.bindingPt = glGetAttribLocation(program, attribute.name);
        }
        checkGLError("glGetAttribLocation");
        program2dTexture = program;
        return 0;
    }


    using ImgData = std::shared_ptr<uint8_t>;

    void setColor(uint8_t* b, uint32_t i) {
        b[0] = i & 0xFF;
        i    = i >> 8;
        b[1] = i & 0xFF;
        i    = i >> 8;
        b[2] = i & 0xFF;
        i    = i >> 8;
        b[3] = i & 0xFF;
    }
    ImgData createQuadTexture(int w) {
        int size = w * w * 4;
        std::shared_ptr<uint8_t> imageData(new uint8_t[size], std::default_delete<uint8_t[]>());

        uint8_t* dataBuf = imageData.get();
        for (int x = 0; x < w; x++) {
            for (int y = 0; y < w; y++) {
                int idx   = (x * w + y) * 4;
                int scale = 16;
                int ix    = x % scale;
                setColor(dataBuf + idx, ix < 10 ? 0xffffffff : 0x00ffffff);
            }
        }
        return imageData;
    }
    RenderResources::NvgImageTexture imgQuad;
}// namespace windowdebug_dbgnanovg

using namespace windowdebug_dbgnanovg;

int initDebugWindowNanoVG(NVGcontext* vg) {
    glBindVertexArray(0);
    int ret = loadShader();
    if (ret)
        return ret;
    tess2d tess;
    tess.add(wTexPreview, 0.0f, 1, 1);
    tess.add(0.0f, 0.0f, 0, 1);
    tess.add(0.0f, wTexPreview, 0, 0);
    tess.add(wTexPreview, wTexPreview, 1, 0);
    checkGLError("uploadVBO");
    glGenVertexArrays(1, &vbo.vaoId);
    glBindVertexArray(vbo.vaoId);
    tess2d::uploadVBO(tess, vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo.vboVertId);
    bindVertexAttributes(attributes);
    glBindVertexArray(0);
    checkGLError("initDebugWindow");

    {
        int texSize   = 64;
        ImgData dataB = createQuadTexture(texSize);
        int32_t nvgid = nvgCreateImageRGBA(vg, texSize, texSize, NVG_IMAGE_REPEATX | NVG_IMAGE_REPEATY | NVG_IMAGE_NEAREST, (const unsigned char*) dataB.get());
        nvgImageSize(vg, nvgid, &imgQuad.width, &imgQuad.height);
        imgQuad.perContextId[vg] = nvgid;
    }
    return 0;
}

void drawDebugWindowNanoVG(NVGcontext* vg, int winW, int winH, float pxratio) {


    glActiveTexture(GL_TEXTURE0);
    glUseProgram(program2dTexture);
    glUniform1i(u_tex0, 0);
    glBindVertexArray(vbo.vaoId);

    // note that we have to call the next 2 lines every frame when not on OpenGL 3.0 or higher contexts.
    // OpenGL documentation does not mention this directly
    glBindBuffer(GL_ARRAY_BUFFER, vbo.vboVertId);
    bindVertexAttributes(attributes);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo.vboIdxId);
    float x           = 0.0f;
    float y           = 0.0f;
    int gltexture     = 0;
    glm::mat4 matProj = glm::ortho(0.f, (float) winW, (float) winH, 0.f, 1.0f, -1.0f);
    glm::mat4 mvp     = matProj * glm::translate(glm::mat4(1.0), glm::vec3(x, y, 0));
    glUniformMatrix4fv(u_mvp, 1, GL_FALSE, value_ptr(mvp));
    glBindTexture(GL_TEXTURE_2D, gltexture);
    //glDrawElements( GL_TRIANGLES, vbo.nIndices, GL_UNSIGNED_INT, nullptr);

    glBindVertexArray(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glStencilMask(~0U);
    glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    nvgBeginFrame(vg, winW, winH, pxratio);
    ivec2 pos  = { 10, 20 };
    ivec2 size = { 300, 40 };
#define STEP_MAX 3

    float steps[STEP_MAX] = { 1.0f, 1.0f / 32.0f, 1.0f / 128.0f };

    nvgSave(vg);
    nvgTranslate(vg, pos.x, pos.y);
    nvgShapeAntiAlias(vg, 1);
    nvgBeginPath(vg);
    nvgRect(vg, 0, 0, size.x, size.y);
    nvgFillColor(vg, rgbToNvg(0xFFFFFF));
    nvgFill(vg);
    nvgRestore(vg);

    pos.y += size.y + 10;
    nvgSave(vg);
    //nvgTranslate(vg, pos.x, pos.y);
    nvgShapeAntiAlias(vg, 0);
    nvgBeginPath(vg);
    nvgRect(vg, pos.x, pos.y, size.x, size.y);
    nvgFillColor(vg, rgbToNvg(0xFFFFFF));
    nvgFill(vg);
    nvgRestore(vg);
    pos.y += size.y + 10;

    RenderResources::NvgImageTexture& image = RenderResources::imgIcons[ICON_DAW_EXE];

    int32_t extImg        = 2;
    const int32_t iconW   = math::min(size.x, size.y);
    const int32_t renderW = iconW + extImg * 2;
    NVGpaint paintIcon    = nvgImagePattern(vg, -extImg, -extImg, iconW + extImg * 2, iconW + extImg * 2, 0, image.perContextId[vg], 1.0f);

    nvgSave(vg);
    nvgTranslate(vg, pos.x, pos.y);
    nvgShapeAntiAlias(vg, 1);

    nvgBeginPath(vg);
    nvgRect(vg, -extImg, -extImg, iconW + extImg * 2, iconW + extImg * 2);
    nvgFillPaint(vg, paintIcon);
    nvgFill(vg);
    pos.y += size.y + 10;

    nvgRestore(vg);
    nvgSave(vg);
    nvgTranslate(vg, pos.x, pos.y);
    nvgShapeAntiAlias(vg, 0);
    nvgBeginPath(vg);
    nvgRect(vg, -extImg, -extImg, iconW + extImg * 2, iconW + extImg * 2);
    nvgFillPaint(vg, paintIcon);
    nvgFill(vg);
    pos.y += size.y + 10;
    nvgRestore(vg);
    nvgShapeAntiAlias(vg, 1);

    for (int pass = 0; pass < 2; ++pass) {
        nvgSave(vg);
        nvgTranslate(vg, pos.x, pos.y);
        nvgShapeAntiAlias(vg, pass);
        x        = 0;
        y        = 0;
        float h  = size.y * 4;
        int dir  = 0;
        float x1 = x;
        float y1 = y;
        float y2 = y + h / 2.0f;
        float y3 = y + h;
        float x3 = dir == 1 ? x - h / 1.41f : x + h / 1.41f;
        nvgBeginPath(vg);
        //    nvgRect(vg, x1, y1, h, h);
        nvgMoveTo(vg, x1, y1 + pass * 20);
        nvgLineTo(vg, x3, y2 + pass * 20);
        nvgLineTo(vg, x1, y3 + pass * 20);
        nvgClosePath(vg);
        nvgFillColor(vg, rgbToNvg(0xFFFF00FF));
        nvgSetShapeExtents(vg, x1, y1 + pass * 20, h, h);
        nvgFill(vg);
        float strokeWidth = 2.0f;

        if (strokeWidth > 0) {
            nvgStrokeColor(vg, rgbToNvg(0xFFFFFF00));
            nvgStrokeWidth(vg, strokeWidth);
            nvgStroke(vg);
        }
        pos.y += h + 10;
        nvgRestore(vg);
    }
    for (int pass = 0; pass < 2; ++pass) {

        nvgSave(vg);
        nvgTranslate(vg, pos.x + pass * (size.x + 20), pos.y);
        nvgShapeAntiAlias(vg, pass);
        float x1 = 0;
        float y1 = 0;
        float x2 = size.x;
        float y2 = size.y;
        //nvgBeginPath(vg);
        //nvgMoveTo(vg, x2, y1);
        //nvgLineTo(vg, x1, y1);
        //nvgLineTo(vg, x1, y2);
        //nvgLineTo(vg, x2, y2);
        //nvgClosePath(vg);
        //nvgFillColor(vg, rgbToNvg(0xFFFF00FF));
        //nvgFill(vg);
        //y1 += size.y + 10;
        //y2 += size.y + 10;
        nvgBeginPath(vg);
        nvgMoveTo(vg, x2, y1);
        nvgLineTo(vg, x1, y1);
        nvgLineTo(vg, x1, y2);
        nvgClosePath(vg);
        nvgFillColor(vg, rgbToNvg(0xFFFF00FF));
        nvgSetShapeExtents(vg, x1, y1, size.x, size.y);
        nvgFill(vg);
        y1 += size.y + 10;
        y2 += size.y + 10;
        nvgBeginPath(vg);
        nvgMoveTo(vg, x1, y1);
        nvgLineTo(vg, x2, y1 + (y2 - y1) / 2.0);
        nvgLineTo(vg, x1, y2);
        nvgClosePath(vg);
        nvgFillColor(vg, rgbToNvg(0xFFFF00FF));
        nvgSetShapeExtents(vg, x1, y1, size.x, size.y);
        nvgFill(vg);
        y1 += size.y + 10;
        y2 += size.y + 10;
        nvgRestore(vg);
    }

    //NVGpaint paintQuad = nvgImagePattern(vg, 0, 0, imgQuad.width, imgQuad.height, 0, imageId, 1.0f);
    //nvgSetPaintColor(vg, &paintQuad, rgbToNvg(0xFFFF00FF));
    NVGpaint paint;
    memset(&paint, 0, sizeof(paint));
    paint.image      = -1;
    paint.innerColor = rgbToNvg(0xFF00FF);
    paint.outerColor = rgbToNvg(0xFF00FF);
    paint.customPar  = 1234;
    //NVGpaint paintQuad = nvgBoxGradient(vg, 0, 0, 100, 10, 5.0f, 0.5f, rgbToNvg(0xFFFF00FF), rgbToNvg(0xFF121212));
    //nvgSetPaintColor(vg, &paintQuad, rgbToNvg(0xFFFF00FF));

    {
        ivec2 qSize(100, 10);
        seq_rand rand;
        for (int i = 0; i < 40; i++) {
            //ivec2 qSize(10 + rand.rng_rand(7) * 20, 10 + rand.rng_rand(7) * 20);
            nvgBatchedRect(vg, rand.rng_rand(1000), rand.rng_rand(1000), qSize.x, qSize.y);
        }
        nvgFillPaint(vg, paint);
        nvgBatchedRender(vg);
    }
    {
        ivec2 qSize(20);
        seq_rand rand;
        for (int i = 0; i < 40; i++) {
            //ivec2 qSize(10 + rand.rng_rand(7) * 20, 10 + rand.rng_rand(7) * 20);
            nvgBatchedRect(vg, rand.rng_rand(1000), rand.rng_rand(1000), qSize.x, qSize.y);
        }
        paint.renderType = 5;
        nvgFillPaint(vg, paint);
        nvgBatchedRender(vg);
    }


    nvgEndFrame(vg);
}

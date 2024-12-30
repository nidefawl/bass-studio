#include "glheaders.h"
#include <memory>
#include <nanovg.h>
#include <array>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "gui/gui.h"
#include "logging.h"
#include "math/seq_math.h"
#include "platform.h"
#include "str_util.h"
#include "gl/gl_util.h"
#include "gl/gl_attr.h"
#include "gl/gl_vbo.h"
#include "gl/gl_tess2d.h"
#include "gl/builtin_shaders.h"
#include "renderresources.h"
#include "color_util.h"
#include "rand.h"
#include "guifonts.h"
#include "theme.h"
#include "window_impl.h"

namespace windowdebug_dbgnanovg {

class window_impl final : public window_abstract_t {
    using ImgData = std::shared_ptr<uint8_t>;
    RenderResources::NvgImageTexture imgQuad;
    UIFont::font_instance instance{"jbmononf.ttf", -1};
    GLuint program2dTexture{};
    GLint u_mvp{};
    GLint u_tex0{};
    float wTexPreview = 1024;
    std::vector<VertexAttr> attributes{
        { "in_position", 2, GL_FLOAT },
        { "in_texcoord", 2, GL_FLOAT },
    };
    DrawVBO vbo;
    guitheme_t theme;
    textlabel_dynamic_t m_textLabelParamValue;

    int loadShader() {
        String srcVertex;
        String srcFragment;

        int64_t ret = ReadFileText("textured.vsh", srcVertex);
        if (ret <= 0) {
            log_lf(Log::L_ERROR, "Cannot read file textured.vsh\n");
            srcVertex = TEXTURED_GLSL_VERT;
        }

        ret = ReadFileText("textured.fsh", srcFragment);
        if (ret <= 0) {
            log_lf(Log::L_ERROR, "Cannot read file textured.fsh\n");
            srcFragment = TEXTURED_GLSL_FRAG;
        }

        GLuint vertex_shader = compileShader(GL_VERTEX_SHADER, srcVertex);
        if (!vertex_shader) {
            return 1;
        }
        GLuint fragment_shader = compileShader(GL_FRAGMENT_SHADER, srcFragment);
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
            log_lf(Log::L_ERROR, "Link error: %s\n", StringAsCStr(log));
            return 1;
        } else if (!log.empty()) {
            log_lf(Log::L_WARN, "Link log: %s\n", StringAsCStr(log));
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
                float fx = x / float(w-1);
                float fy = y / float(w-1);
                float brightness = fx * fy;
                auto rgb_u32 = vec3ToRgbU32(vec3(brightness));
                setColor(dataBuf + idx, 0xFF000000 | rgb_u32);
            }
        }
        //
        return imageData;
    }
public:
    window_impl() = default;

    int init(NVGcontext* vg) override {
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

    int render(NVGcontext* vg, int winW, int winH, float pxratio) override {


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
        
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        nvgBeginFrame(vg, winW, winH, pxratio);
        theme.bindFont(vg, UIFont::FONT_DEFAULT);
        ivec2 pos  = { 10, 20 };
        ivec2 size = { 300, 40 };

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
            int h    = size.y * 4;
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
            nvgLineTo(vg, x2, y1 + (y2 - y1) / 2.0f);
            nvgLineTo(vg, x1, y2);
            nvgClosePath(vg);
            nvgFillColor(vg, rgbToNvg(0xFFFF00FF));
            nvgSetShapeExtents(vg, x1, y1, size.x, size.y);
            nvgFill(vg);
            y1 += size.y + 10;
            y2 += size.y + 10;
            nvgRestore(vg);
        }

        NVGpaint texRepeatPattern = nvgImagePattern(vg, 0, 0, imgQuad.width, imgQuad.height, 0, imgQuad.perContextId[vg], 1.0f);
        {
            ivec2 qSize(100, 100);
            seq_rand rand;
            rand.rng_seed(0xABCD);
            for (int i = 0; i < 6; i++) {
                ivec2 qPos = {rand.rng_rand(1000), rand.rng_rand(1000)};
                nvgTranslate(vg, qPos.x, qPos.y);
                nvgBeginPath(vg);
                nvgRect(vg, 0, 0, qSize.x, qSize.y);
                nvgFillPaint(vg, texRepeatPattern);
                nvgFill(vg);
                nvgTranslate(vg, -qPos.x, -qPos.y);
            }
        }

        NVGpaint paint;
        memset(&paint, 0, sizeof(paint));
        paint.image      = -1;
        paint.innerColor = rgbToNvg(0x00FFFF);
        paint.outerColor = rgbToNvg(0x00FF00);
        {
            ivec2 qSize(20);
            seq_rand rand;
            rand.rng_seed(0xABCD);
            for (int i = 0; i < 6; i++) {
                nvgBatchedRect(vg, rand.rng_rand(1000), rand.rng_rand(1000), qSize.x, qSize.y);
            }
            paint.renderType = 5;
            nvgFillPaint(vg, paint);
            nvgBatchedRender(vg);
        }

        {
            std::array strings = {
                "Hello World",
                "Debug The Fontrendering",
                "123"
            };
            auto idx = 1;//math::floorfS32(getTimeMillisF() / 1000.0f) % strings.size();
            auto szStr = strings[idx];
            auto titlePos = vec2(winW, winH) / 2.0f;
            auto steppedScaleI = 1;//math::floorfS32(getTimeMillisF() / 777.0f) % 12;
            // log_printf("%zd %d\n",idx, steppedScaleI);
            steppedScaleI++;
            auto steppedScaleF = (steppedScaleI / 32.0f) * 4.0f;
            m_textLabelParamValue.size = vec2{ 200, 40 } * (steppedScaleF);
            m_textLabelParamValue.pos = titlePos - m_textLabelParamValue.size / 2.0f;
            m_textLabelParamValue.fontSize = math::clamp(math::floorfS32(m_textLabelParamValue.size.y * 0.8f), 4, 200);
            m_textLabelParamValue.alignment = NVG_ALIGN_MIDDLE | NVG_ALIGN_CENTER;

            nvgSave(vg);
            nvgBeginPath(vg);
            nvgRect(vg, m_textLabelParamValue.pos.x, m_textLabelParamValue.pos.y, m_textLabelParamValue.size.x, m_textLabelParamValue.size.y);
            nvgFillColor(vg, rgbaToNvg(0x3f0000FF));
            nvgFill(vg);
            m_textLabelParamValue.render(vg, &theme, szStr, rgbaToNvg(0xFFFFFFFF));
            nvgRestore(vg);

        }

        nvgEndFrame(vg);
        return 1;
    }
    int destroy(NVGcontext*) override {
        return 0;
    }
    void tick() override {
        m_textLabelParamValue.adjustWidth();
    }
};

}// namespace windowdebug_dbgnanovg

std::shared_ptr<window_abstract_t> getWindowDebugNanoVG() {
    return std::make_shared<windowdebug_dbgnanovg::window_impl>();
}
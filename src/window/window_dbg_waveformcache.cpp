#include "glheaders.h"
#include <nanovg.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "math/vec.h"
#include "math/mat.h"
#include "fileio.h"
#include "str_util.h"
#include "gl/gl_util.h"
#include "gl/gl_attr.h"
#include "gl/gl_vbo.h"
#include "gl/gl_tess2d.h"
#include "gl/gl_shader.h"
#include "gl/builtin_shaders.h"
#include "wave/waveform_render_impl.h"
#include "color_util.h"
#include "window_impl.h"
#include "host/daw/mainctrl.h"

namespace windowdebug_waveformcache {

class window_impl final : public window_abstract_t {
    GLuint program2dTexture;
    GLint u_mvp;
    GLint u_tex0;

    std::vector<VertexAttr> attributes{
        { "in_position", 2, GL_FLOAT },
        { "in_texcoord", 2, GL_FLOAT },
    };

    DrawVBO vbo;
    int loadShader() {
        String srcVertex = TEXTURED_GLSL_VERT;
        String srcFragment = TEXTURED_GLSL_FRAG;
        // int64_t ret = ReadFileText("textured.vsh", srcVertex);
        // if (ret <= 0) {
        //     log_lf(Log::L_ERROR, "Cannot read file textured.vsh\n");
        //     return 1;
        // }
        // ret = ReadFileText("textured.fsh", srcFragment);
        // if (ret <= 0) {
        //     log_lf(Log::L_ERROR, "Cannot read file textured.fsh\n");
        //     return 1;
        // }

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
            log_lf(Log::L_ERROR, "Link error: %s\n", StringAsCStr(log));
            return 1;
        } else if (!log.empty()) {
            log_lf(Log::L_WARN, "Link log: %s\n", StringAsCStr(log));
        }
        checkGLError("linkProgram");
        glUseProgram(program);
        u_mvp  = glGetUniformLocation(program, "u_mvp");
        u_tex0 = glGetUniformLocation(program, "tex0");
        for (auto & attribute : attributes) {
            attribute.bindingPt = glGetAttribLocation(program, attribute.name);
        }
        checkGLError("glGetAttribLocation");
        program2dTexture = program;
        return 0;
    }
public:
    window_impl() {
    }

    ~window_impl() {
    }

    int init(NVGcontext* vg) {
        glBindVertexArray(0);
        int ret = loadShader();
        if (ret)
            return ret;

        float wTexPreview = 256;
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
        return 0;
    }


    int render(NVGcontext* ctx, int winW, int winH, float pxratio) {
        std::vector<TextureAtlas> rendered;
        auto const ctrl = MainCtrl::get();
        if (ctrl) {
            auto const wfrender = ctrl->getWaveformRenderer();
            if (wfrender) {
                wfrender->getRenderedTextures(rendered);
            }
        }

        float x = 0;
        float y = 0;


        float wTexPreview = 1024;
        int xCols, yCols;
        for (; wTexPreview > 1;) {
            xCols = (winW + wTexPreview - 1) / wTexPreview;
            yCols = (winH + wTexPreview - 1) / wTexPreview;
            if (xCols * yCols >= CtrSize(rendered)) {
                break;
            }
            wTexPreview /= 2;
        }

        const auto gr = 15/256.f;
        glClearColor(gr, gr, gr, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        tess2d tess;
        tess.add(wTexPreview, 0.0f, 1, 1);
        tess.add(0.0f, 0.0f, 0, 1);
        tess.add(0.0f, wTexPreview, 0, 0);
        tess.add(wTexPreview, wTexPreview, 1, 0);
        tess2d::uploadVBO(tess, vbo);

        glActiveTexture(GL_TEXTURE0);
        glUseProgram(program2dTexture);
        glUniform1i(u_tex0, 0);
        glBindVertexArray(vbo.vaoId);

        // note that we have to call the next 2 lines every frame when not on OpenGL 3.0 or higher contexts.
        // OpenGL documentation does not mention this directly
        glBindBuffer(GL_ARRAY_BUFFER, vbo.vboVertId);
        bindVertexAttributes(attributes);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo.vboIdxId);

        int nrendered = 0;
        for (TextureAtlas& e : rendered) {
            int n = e.glTexture;
            if (n > 0 && !e.entries.empty()) {
                glm::mat4 matProj = glm::ortho(0.f, (float) winW, (float) winH, 0.f, 1.0f, -1.0f);
                glm::mat4 mvp     = matProj * glm::translate(glm::mat4(1.0), glm::vec3(x, y, 0));
                glUniformMatrix4fv(u_mvp, 1, GL_FALSE, value_ptr(mvp));
                glBindTexture(GL_TEXTURE_2D, n);
                glDrawElements(GL_TRIANGLES, vbo.nIndices, GL_UNSIGNED_INT, nullptr);
                nrendered++;
                x += wTexPreview + 8;
                if (x >= 1024) {
                    x = 0;
                    y += wTexPreview + 8;
                }
            }
        }
        glBindVertexArray(0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glClearColor(0, 0, 0, 0);
        glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        nvgBeginFrame(ctx, winW, winH, pxratio);

        //nvgBeginPath(ctx);
        //nvgRect(ctx, 0, 0, winW, winH);
        //nvgFillColor(ctx, rgbToNvg(0x33ff33));
        //nvgFill(ctx);
        x           = 0;
        y           = 0;
        nrendered   = 1;
        float scale = (float) wTexPreview / (float) FBO_WIDTH;
        for (TextureAtlas& _atlas : rendered) {
            int n = _atlas.glTexture;
            if (n > 0 && !_atlas.entries.empty()) {
                for (TextureAtlasEntry& _entry : _atlas.entries) {
                    nvgBeginPath(ctx);
                    auto entryColor = rgbToNvg(col(nrendered));
                    nvgRect(ctx, x + _entry.pos.x * scale, y + _entry.pos.y * scale, _entry.size.x * scale, _entry.size.y * scale);
                    if (!_entry.inuse) {
                        entryColor = rgbToNvg(0xffff0000);
                    }
                    nvgStrokeColor(ctx, entryColor);
                    nvgStrokeWidth(ctx, 2.0f);
                    nvgStroke(ctx);
                }
                x += wTexPreview + 8;
                if (x >= 1024) {
                    x = 0;
                    y += wTexPreview + 8;
                }
                nrendered++;
            }
        }
        nvgEndFrame(ctx);
        return 1;
    }

    int destroy(NVGcontext*) {
        return 0;
    }
};

}// namespace windowdebug_waveformcache



std::shared_ptr<window_abstract_t> getWindowDebugWaveformCache() {
    return std::make_shared<windowdebug_waveformcache::window_impl>();
}
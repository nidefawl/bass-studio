#include "glheaders.h"
#include <vector>
#include <nanovg.h>
#include <nanovg_gl.h>
#include <memory>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "host/daw/mainctrl.h"
#include "host/host.h"
#include "math/seq_math.h"
#include "math/vec.h"
#include "math/mat.h"

#include "note.h"
#include "platform.h"
#include "gui/gui.h"
#include "shaderview.h"
#include "gl/gl_framebuffer.h"
#include "gl/gl_shader.h"
#include "gl/gl_tess2d.h"
#include "gl/gl_util.h"
#include "rand.h"
#include "basectrl.h"
#include "renderresources.h"
#include "seq_time.h"
#include "str_util.h"
#include "util/presetmanager.h"

struct testshader final : gl_shader_pipeline {
    const String fnameVsh;
    const String fnameFsh;
    bool isValid = false;
    ivec2 lastBufSize = { -1, -1 };
    GLuint texture    = 0;
    GLint u_mouse = 0;
    testshader(const String& nameVsh, const String& nameFsh) : fnameVsh(nameVsh), fnameFsh(nameFsh) {
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
    void setMouse(vec4 mouse) {
        if (u_mouse >= 0)
            glUniform4f(u_mouse, mouse.x, mouse.y, mouse.z, mouse.w);
    }
    int bindAttributes() {
        u_mouse = glGetUniformLocation(program, "u_mouse");
        int n = gl_shader_pipeline::bindAttributes();
        setAttributeLocations();
        if (u_mouse >= 0) {
            glUniform4f(u_mouse, 0, 0, 0, 0);
            checkGLError("glUniform4f u_mouse");
        }
        return n;
    };
    template<typename T>
    int load(T* srcParser) {
        storeGlContext();
        int newprogram = compileShaderCombo(srcParser, fnameVsh.c_str(), fnameFsh.c_str(), -1);
        if (newprogram < 0) {
            log_lf(Log::L_WARN, "failed loading shaders: %s %s\n", fnameVsh.c_str(), fnameFsh.c_str());
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
    friend class gui_shaderview;
    NVGLUframebuffer* fb = nullptr;
    std::shared_ptr<testshader> pipeTestShader;
    int64_t initTime = 0;
    std::vector<float> texData;
    bool hasTextureUpdate = false;
    ivec2 textureSize = { 0, 0 };
    seq_rand rnd;
    PresetManager presetManager;
    PresetManager::Preset currentPreset = {
        .name = "", .path = "", .isFavorite = false
    };
public:
    gui_shaderview_impl_t() {
        auto now = getTimeMillis();
        rnd.rng_seed(static_cast<uint64_t>(now));
        presetManager.setFileExtension("fsh");
        presetManager.load(App::Platform::toResourcePath("shaders"));
        presetManager.load(App::Platform::toUserdataPath("shaders"));
        auto& presets = presetManager.getPresets();
        if (!presets.empty()) {
            currentPreset = presets[0];
            String defName = "neonlove";
            auto it = std::find_if(presets.begin(), presets.end(), [defName](const PresetManager::Preset& p) {
                return StringContainsCI(p.name, defName) > -1;
            });
            if (it != presets.end()) {
                currentPreset = *it;
            }
        }
    }
    void setPreset(const PresetManager::Preset& preset) {
        currentPreset = preset;
        if (pipeTestShader) {
            pipeTestShader.reset();
        }
    }
    void prerender(NVGcontext* vg, ivec2 size, vec2 mouse, float fTime) {
        if (!pipeTestShader) {
            if (currentPreset.path.empty())
                return;
            String vsh = App::Platform::toResourcePath("shaders/fullscreen_quad.vsh");
            pipeTestShader = std::make_shared<testshader>(vsh, currentPreset.path);
            struct shader_src_parser_noise {
                void preprocessSources(std::vector<glshader_src>& srcList) {
                    for (auto& src : srcList) {
                        if (src.stage == GL_FRAGMENT_SHADER) {
                            prependGLSL(src.source,
R"(#version 150 core
uniform sampler2D tex0;
uniform float u_time;
uniform vec2 u_viewport;
uniform vec4 u_mouse;
// uniform vec3      iResolution;           // viewport resolution (in pixels)
// uniform float     iTime;                 // shader playback time (in seconds)
// uniform float     iTimeDelta;            // render time (in seconds)
// uniform float     iFrameRate;            // shader frame rate
// uniform int       iFrame;                // shader playback frame
// uniform float     iChannelTime[4];       // channel playback time (in seconds)
// uniform vec3      iChannelResolution[4]; // channel resolution (in pixels)
// uniform vec4      iMouse;                // mouse pixel coords. xy: current (if MLB down), zw: click
// uniform samplerXX iChannel0..3;          // input channel. XX = 2D/Cube
// uniform vec4      iDate;                 // (year, month, day, time in seconds)
// uniform float     iSampleRate;           // sound sample rate (i.e., 44100)

in vec2 pass_texcoord;
out vec4 out_Color;
#define iTime u_time*0.001
#define iResolution u_viewport.xy
#define iMouse u_mouse
#define iChannel0 tex0
)");
                            src.source +=
R"(
void main(void) {
    vec4 color = vec4(0.0, 1.0, 0.0, 1.0);
    mainImage(color, pass_texcoord * u_viewport.xy);
    out_Color = vec4(color.rgb, 1.0);
})";
                            // printf("%s\n", src.source.c_str());
                            // fflush(stdout);
                        }
                    }
                }
            };
            shader_src_parser_noise parser;
            int ret = pipeTestShader->load(&parser);
            if (ret < 0) {
                log_lf(Log::L_WARN, "failed loading shader %s\n", currentPreset.path.c_str());
                return;
            }
        }
        if (!pipeTestShader->isValid)
            return;
        if (hasTextureUpdate) {
            hasTextureUpdate = false;
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, pipeTestShader->texture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, textureSize.x, textureSize.y, 0, GL_RED, GL_FLOAT, texData.data());
        }
        // int w    = math::min(size.x, size.y);
        // int h    = w;
        int w = size.x;
        int h = size.y;
        fb = nvgluCreateTempFramebuffer(vg, w, h, NVG_IMAGE_PREMULTIPLIED);
        nvgluBindFramebuffer(fb);
        glViewport(0, 0, w, h);
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        auto pipeline = pipeTestShader.get();
        auto& vbo     = pipeline->vbo;
        glUseProgram(pipeline->program);
        mat4x4 matProj = glm::ortho(0.f, (float) w, (float) h, 0.f, 1.0f, -1.0f);
        pipeline->setUniforms(w, h, fTime);
        bool bIsInside = mouse.x >= 0 && mouse.x < w && mouse.y >= 0 && mouse.y < h;
        pipeline->setMouse({mouse.x, mouse.y, bIsInside ? 1.f : 0.f, bIsInside ? 1.f : 0.f});
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
            // const auto texW = math::min(size.x, size.y);
            textureSize = {size.x, size.y};
            texData.resize(textureSize.x * textureSize.y);
            for (int y = 0; y < textureSize.y; y++) {
                for (int x = 0; x < textureSize.x; x++) {
                    texData[y * textureSize.x + x] = static_cast<float>(rnd.rng_double());
                }
            }
            hasTextureUpdate = true;
        }
    }
    int getNvgImageId() {
        return fb ? fb->image : -1;
    }
};
class guictr_shader_preset_control final : public guictr_base {
    friend class gui_shaderview;
    guidropdown_select_preset selectPreset;
    guibutton buttonReload;
public:
    guictr_shader_preset_control() {
        padding = 0;
        margin = 0;
        buttonReload.drawFn   = drawTextureSymbol;
        buttonReload.drawParm = ICON_SAVE;
        buttonReload.setText("Reload");
        add(&selectPreset);
        add(&buttonReload);
    }
    ~guictr_shader_preset_control() {
        removeGuis();
    }
    void layout() override {
        auto cs = getSizeContent();
        vec2 sizePadded = vec2(cs.x - 2, cs.y);
        float btnWidth = 0.3f;
        buttonReload.size = vec2(sizePadded.x * btnWidth, sizePadded.y);
        if (buttonReload.size.x < 40) {
            buttonReload.size.x = buttonReload.size.y;
            buttonReload.setText("");
        } else {
            buttonReload.setText("Reload");
        }
        float presetWidth = sizePadded.x - buttonReload.size.x - 2;
        selectPreset.size = vec2(presetWidth, sizePadded.y);
        selectPreset.pos = vec2(0, 0);
        buttonReload.pos = vec2(selectPreset.right() + 2, 0);
        guictr_base::layout();
    }
};
gui_shaderview::gui_shaderview() : guictr_base(), impl(new gui_shaderview_impl_t), presetControl(new guictr_shader_preset_control{}) {
    setGuiType(gui_type::CTR_TYPE_SHADERVIEW);
    setBackgroundRendered(false);
    add(presetControl);
    presetControl->selectPreset.setPresetManager(impl->presetManager);
    presetControl->selectPreset.setCallback([this](const PresetManager::Preset& path) {
        impl->setPreset(path);
        presetControl->selectPreset.setString(impl->currentPreset.name);
    });
    presetControl->selectPreset.setString(impl->currentPreset.name);
}
gui_shaderview::~gui_shaderview() {
    remove(presetControl);
    delete presetControl;
    delete impl;
}

void gui_shaderview::prerender(NVGcontext* vg) {
    auto m = parentCtrl->m_mousePos;
    toControlsObjectSpace(m, this);
    float fTime = getTimeMillisF();
    if (dawCtrl) {
        auto state = dawCtrl->getDaw()->getPlayThread()->getState();
        if (state == playback_state::status_stop) {
            // fTime = dawCtrl->getDaw()->getProjectGlobals().idleTickPos / float(TICKS_QUARTER);
            // fTime *= 1000.0f;
            fTime = 0;
        } else if (state != playback_state::status_no_process) {
            // fTime = dawCtrl->getDaw()->getProjectGlobals().playbackPos / float(TICKS_QUARTER);
            // fTime *= 1000.0f;
                
            auto const daw = dawCtrl->getDaw();
            auto const host = daw->getHost();
            fTime = host->getOutputTickPos() * 1000.0f / float(TICKS_QUARTER);
            auto& prjGlobals = daw->getProjectGlobals();
            fTime += secondsToTicks(0.002, prjGlobals.tempo100);
            if (fTime < 0) {
                fTime = -30000;
            }
        }
    }
    impl->prerender(vg, size, m, fTime);
}
void gui_shaderview::onTick(AppCtrl* appctrl) {
    impl->onTick(size);
}
void gui_shaderview::render(NVGcontext* vg) {
    if (isBackgroundRendered()) {
        renderWidgetBorder(vg, getStateFlags());
    }
    if (!setScissorTransform(vg)) {
        return;
    }
    int nvgImageId = impl->getNvgImageId();
    if (nvgImageId > 0) {
        // int w = math::min(size.x, size.y);
        // int h = w;
        int w = size.x;
        int h = size.y;
        NVGpaint img = nvgImagePattern(vg, 0, 0, w, h, 0, nvgImageId, 1);
        // nvgTranslate(vg, size.x - w, 0);
        nvgBeginPath(vg);
        nvgRect(vg, 0, 0, w, h);
        nvgFillPaint(vg, img);
        nvgFill(vg);
    }
    for (auto* gui : guis) {
        if (!gui->isVisible())
            continue;
        nvgSave(vg);
        gui->render(vg);
        nvgRestore(vg);
    }
}
void gui_shaderview::layout() {
    auto cs = getSizeContent();
    presetControl->pos = {0,0};
    presetControl->size = cs;
    auto controlsHeight = math::clamp<int32_t>(size.y/8, 12, inputHeight);
    if (controlsHeight < inputHeight*2/3) {
        presetControl->size.y = 0;
        presetControl->setVisible(false);
    } else {
        presetControl->size.y = controlsHeight;
        presetControl->setVisible(true);
    }
    guictr_base::layout();
}

void gui_shaderview::buttonClicked(guibase* button) {
    if (button == &presetControl->buttonReload) {
        impl->setPreset(impl->currentPreset);
    }
}


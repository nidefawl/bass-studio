#include "pymachine.h"
#include "gui/container/container_layout_types.h"
#include "host/plugin/modules.h"
#include "plugins/latency/latency-plugin.h"
#include "plugins/visualizer/visualizer-plugin.h"
#include "host/daw/mainctrl.h"
#include "host/host.h"
#include <cmath>
#include <cstddef>
#include <glm/trigonometric.hpp>
#include <memory>
#include <muParser.h>
#include <memory>
#include <optional>
#include "assert_dbg.h"
#include "event.h"
#include "glheaders.h"
#include "gui/gui.h"
#include "guicolors.h"
#include "math/simd_math.h"
#include "seq_util.h"
#include "samplerate.h"
#include "types.h"
#include "tls.h"
#include "platform.h"
#include "str_util.h"
#include "logging.h"
#include "fileio.h"
#include "basectrl.h"
#include "appsettings.h"
#include "renderresources.h"
#include "gui/container/container.h"
#include "gui/controls/knob.h"
#include "gui/controls/colorpick.h"
#include "gui/controls/inputfield.h"
#include "gui/controls/list.h"
#include "gui/dropdown/dropdown_generic.h"
#include "gl/gl_pathrenderer.h"
#include "gl/gl_util.h"
#include "gl/gl_tess2d.h"
#include "gl/gl_framebuffer.h"
#include "gl/gl_shader.h"
#include "gl/gl_util.h"
#include "gl/builtin_shaders.h"
#include "math/mat.h"
#include "math/vec.h"
#include "math/seq_math.h"
#include "host/audio_analyzer.h"
#include "host/audiohost/audio_host.h"
#include "gl/fx/blur.h"

#include <algorithm>
#include <utility>
#include <vector>
#include <array>
#include <iostream>
#include <fstream>
#include <sstream>
#include <type_traits>
#include <spline.h>
#include <nanovg.h>
#include <nanovg_gl.h>
#include <splines/generic_b_spline.h>
#include <splines/uniform_cubic_bspline.h>
#include <splines/uniform_cr_spline.h>
#include <splines/natural_spline.h>
#include <cereal/cereal.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/polymorphic.hpp>
#include <cereal/types/map.hpp>
#include <cereal_optional_nvp/cereal_optional_nvp.hpp>
#include <wave/waveform_generate.h>
#include <tinysplinecxx.h>
#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/fast_exponential.hpp>
#include <glm/gtx/fast_trigonometry.hpp>

template<typename T>
float lengthSquared(T a) { return glm::length2(a); }

#ifdef USE_PYTHON

const char* moduleNamePathGen = "visualizer_pathgen";

namespace py = pybind11;
using namespace py::literals;

template<typename TResult, typename... Args>
TResult callPyFunction(const char* moduleName, const char* fnName, Args&&... args) {
    py::module moduleBindings = py::module::import("path_bindings");
    py::module moduleImpl     = py::module::import(moduleName);
    py::object result         = moduleImpl.attr(fnName)(std::forward<Args>(args)...);
    return result.cast<TResult>();
}

void getPyFunctionList(const char* moduleName, std::vector<String>& fnList) {
    py::module moduleImpl = py::module::import(moduleName);
    for (auto& thing : moduleImpl.attr("__dict__")) {
        String string = py::str(thing);
        if (StrStartsWith(string, "pathgen_fl32_")) {
            fnList.push_back(string);
        }
    }
    for (auto& thing : moduleImpl.attr("__dict__")) {
        String string = py::str(thing);
        if (StrStartsWith(string, "pathGen_")) {
            fnList.push_back(string);
        }
    }
}
#endif

#if __cplusplus >= 201703L
static_assert(sizeof(float) == (sizeof(tsReal)));
static_assert(sizeof(vec2) == (sizeof(tsReal) * 2));
#endif
using floating_t = float;

struct tex_shader : gl_shader_pipeline {
    GLint u_fade      = 0;
    ivec2 lastBufSize = { -1, -1 };
    const char* fnameVsh;
    const char* fnameFsh;
    const char* builtinSrcVsh;
    const char* builtinSrcFsh;
    tex_shader(const char* _fnameVsh, const char* _fnameFsh, const char* _builtinSrcVsh, const char* _builtinSrcFsh)
        : fnameVsh(_fnameVsh),
          fnameFsh(_fnameFsh),
          builtinSrcVsh(_builtinSrcVsh),
          builtinSrcFsh(_builtinSrcFsh) {
        attributes = {
            { "a_position", 2, GL_FLOAT },
            { "a_texcoord", 2, GL_FLOAT }
        };
    }
    ~tex_shader() = default;
    void setUniforms(int32_t w, int32_t h, float fTime, float fFade) {
        if (u_viewport >= 0)
            glUniform2f(u_viewport, w, h);
        if (u_time >= 0)
            glUniform1f(u_time, fTime);
        if (u_fade >= 0)
            glUniform1f(u_fade, fFade);
    }
    template<typename T>
    int load(T* srcParser) {
        storeGlContext();
        checkGLError("pre compileShaderCombo");
        int newprogram = compileShaderCombo(srcParser, fnameVsh, fnameFsh);
        if (newprogram < 0) {
            newprogram = compileBuiltinShader(srcParser, builtinSrcVsh, builtinSrcFsh);
        }
        if (newprogram < 0) {
            log_lf(Log::L_WARN, "failed loading shaders: %s %s\n", fnameVsh, fnameFsh);
            dbgassert(newprogram != -2);
            return -1;
        }
        program = newprogram;
        glUseProgram(program);
        u_fade = glGetUniformLocation(program, "u_fade");
        if (bindAttributes()) {
            return -1;
        }
        dbgassert(attributes[0].elements == 2);
        dbgassert(attributes[0].bindingPt == 0);
        dbgassert(attributes[1].elements == 2);
        // 1 or -1: vertex attribute a_texcoord might get optimized out
        dbgassert(attributes[1].bindingPt == 1 || attributes[1].bindingPt == -1);
        // glGenVertexArrays(1, &vbo.vaoId);
        // glBindVertexArray(vbo.vaoId);
        // vbo.genBuffers();
        // glBindBuffer(GL_ARRAY_BUFFER, vbo.vboVertId);
        // glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo.vboIdxId);
        // bindVertexAttributes(attributes);
        // glBindVertexArray(0);

        return 0;
    };
};

#define OP_CLEAR 0
#define OP_GEN_SINE 1
#define OP_RAND_EACH 2
#define OP_RAND_ALL 3
#define OP_GEN_CIRCLE 4
#define OP_MULTIPLY_ALL 5
template<typename T>
gui_numberinput_field_generic<T>* createParamInput(String _label, glm::vec<2, T> _minmax, T* ptr) {
    auto inputCtrl     = new gui_numberinput_field_generic<T>(ptr);
    inputCtrl->fnClamp = [_minmax](auto n) {
        return math::max(_minmax.x, math::min((_minmax.y), n));
    };
    inputCtrl->setLabel(_label);

    if constexpr ( std::is_same_v<double, T> ) {
        inputCtrl->setStringFormat("%f");
    }
    if constexpr ( std::is_same_v<float, T> ) {
        inputCtrl->setStringFormat("%f");
    }
    
    return inputCtrl;
}
namespace lineplot {
    enum PathMode {
        PLOT_MATH_EXPRESSION = 0,
        PLOT_PYTHON_EXPRESSION,
        PLOT_SIN,
        PATH_SCRIPT,
        PATH_FREE,
        FFT_SPECTRUM,
        AUDIO_WAVEFORM,
        SIMD_COS_TEST,
        SIMD_ENV_TEST,
        NUM_PATH_MODES,
    };
    struct settings_preset_t {
        String name;
        String expression;
        String pythonFunc;
        int32_t nRenderer           = 2;
        PathMode mode               = FFT_SPECTRUM;
        int32_t nSplineType         = 1;
        int32_t degree              = 4;
        int32_t blendMode           = 0;
        int32_t nBlurDownsample     = 0;
        int32_t nBlurPasses         = 0;
        int32_t nOversample         = 1; // 1 or 2
        int32_t nSamples            = 300;
        int32_t lineWidth           = 38;
        int32_t randRange           = 100;
        float fFadeOutStrength      = 0.988905f;
        float fAntialias            = 1.0f;
        float fBloomStrength        = 1.162104f;
        uint32_t rgba               = 0x6B26195A;
        float fGain                 = 1.0f;
        float fCycleColor           = 0.0f;
        int32_t nBands              = 32;
        int32_t freqMin             = 20;
        int32_t freqMax             = 22000;
        float fBlurFeedbackStrength = 0.1f;
    };
    struct settings_t {
        int32_t currentPresetIdx = 0;
        std::vector<settings_preset_t> presets;
    };


    using std::ifstream;
    using std::ofstream;

    template<class Archive>
    void save(Archive& ar, const settings_preset_t& preset, const std::uint32_t version) {
        using cereal::make_nvp;
        using cereal::make_optional_nvp;
        ar(make_nvp("name", preset.name),
           make_nvp("samples", preset.nSamples),
           make_nvp("mode", preset.mode),
           make_nvp("rgba", preset.rgba),
           make_nvp("linewidth", preset.lineWidth),
           make_nvp("degree", preset.degree),
           make_nvp("randrange", preset.randRange),
           make_nvp("fadeoutstrength", preset.fFadeOutStrength),
           make_nvp("antialias", preset.fAntialias),
           make_nvp("bloomstrength", preset.fBloomStrength),
           make_nvp("cyclecolor", preset.fCycleColor),
           make_nvp("gain", preset.fGain),
           make_nvp("numfftbands", preset.nBands),
           make_nvp("freqMin", preset.freqMin),
           make_nvp("freqMax", preset.freqMax),
           make_nvp("blurfeedback", preset.fBlurFeedbackStrength),
           make_nvp("blendmode", preset.blendMode),
           make_nvp("expression", preset.expression),
           make_nvp("nRenderer", preset.nRenderer),
           make_nvp("nSplineType", preset.nSplineType),
           make_nvp("nBlurDownsample", preset.nBlurDownsample),
           make_nvp("nBlurPasses", preset.nBlurPasses),
           make_nvp("nOversample", preset.nOversample),
           make_nvp("pythonFunc", preset.pythonFunc) 
        );

    }

    template<class Archive>
    void load(Archive& ar, settings_preset_t& preset, const std::uint32_t version) {
        using cereal::make_nvp;
        using cereal::make_optional_nvp;
        ar(make_nvp("name", preset.name),
           make_nvp("samples", preset.nSamples),
           make_nvp("mode", preset.mode),
           make_nvp("rgba", preset.rgba),
           make_nvp("linewidth", preset.lineWidth),
           make_nvp("degree", preset.degree),
           make_nvp("randrange", preset.randRange),
           make_nvp("fadeoutstrength", preset.fFadeOutStrength),
           make_nvp("antialias", preset.fAntialias),
           make_nvp("bloomstrength", preset.fBloomStrength),
           make_nvp("cyclecolor", preset.fCycleColor),
           make_nvp("gain", preset.fGain),
           make_nvp("numfftbands", preset.nBands),
           make_nvp("freqMin", preset.freqMin),
           make_nvp("freqMax", preset.freqMax),
           make_nvp("blurfeedback", preset.fBlurFeedbackStrength),
           make_nvp("blendmode", preset.blendMode));
        if (version > 0) {
            ar(make_nvp("expression", preset.expression));
        }
        if (version > 1) {
            ar(make_nvp("nRenderer", preset.nRenderer),
               make_nvp("nSplineType", preset.nSplineType),
               make_nvp("nBlurDownsample", preset.nBlurDownsample),
               make_nvp("nBlurPasses", preset.nBlurPasses),
               make_nvp("nOversample", preset.nOversample));
        }
        if (version > 2) {
            ar(make_nvp("pythonFunc", preset.pythonFunc));
        }
    }

    template<class Archive>
    void serialize(Archive& ar, settings_t& settings) {
        using cereal::make_nvp;
        ar(make_nvp("currentPresetIdx", settings.currentPresetIdx),
           make_nvp("presets", settings.presets));
    }
    settings_t loadSettings(const String& filename) {
        Stringstream ss;
        ifstream file(filename, ifstream::in);
        if (file) {
            ss << file.rdbuf();
            std::streampos length = file.tellg();
            if (length > 10) {
                cereal::JSONInputArchive ar(ss);
                settings_t newSettings;
                ar(newSettings);
                return newSettings;
            }
        }
        throw std::runtime_error("Failed reading config");
    }
    void saveSettings(const settings_t& newSettings, const String& filename) {
        ofstream file;
        file.exceptions(~ofstream::goodbit);
        file.open(filename, ofstream::out);
        cereal::JSONOutputArchive ar(file);
        ar(newSettings);
    }


#define FILENAME_PRESETS "data/presets.json"
    class guictr_presets : public guictr_base {
    public:
        guibutton buttonAdd;
        guibutton buttonRemove;
        guibutton buttonSave;
        gui_textfield textFieldName;
        gui_list list;
        guictr_presets() : guictr_base() {
            add(&textFieldName);
            add(&buttonAdd);
            add(&buttonRemove);
            add(&buttonSave);
            add(&list);
            list.setBackgroundRendered(false);
            list.padding = 0;
            textFieldName.setLabel("Enter name");
            buttonAdd.setText("Add");
            buttonRemove.setText("Delete");
            buttonSave.setText("Save");
            setLabel("Preset");
            setBackgroundRendered(true);
            setFlag(FLG_RENDER_LABEL, true);
        }
        ~guictr_presets() override {
            removeGuis();
        }
        void layout() override {
            int32_t h          = 24;
            auto cs            = getSizeContent();
            buttonAdd.size     = { cs.x * 1 / 3, h };
            buttonSave.size    = buttonAdd.size;
            textFieldName.pos  = { 0, 0 };
            textFieldName.size = { cs.x - buttonAdd.size.x, h };
            buttonAdd.pos      = { textFieldName.right(), 0 };
            buttonRemove.pos   = { 0, textFieldName.bottom() };
            buttonRemove.size  = { textFieldName.size.x, h };
            buttonSave.pos     = { cs.x - buttonSave.size.x, h };
            list.pos           = { 0, buttonRemove.bottom() };
            list.size          = { cs.x, cs.y - list.pos.y };
            for (auto gui : guis) {
                gui->layout();
            }
        }
        void buttonClicked(guibase* button) override {
            if (parent) parent->buttonClicked(button);
        }


        class gui_linetess_preset_list_entry : public gui_list_entry {
        public:
            const String presetName;
            const int32_t presetIdx;
            gui_linetess_preset_list_entry(const settings_preset_t& _preset, const int32_t _presetIdx) : gui_list_entry(), presetName(_preset.name), presetIdx(_presetIdx) {
                icon = ICON_FILE;
            }
            String getText() override {
                return presetName;
            }
            void dragMoveOn(guibase* target, ivec2 mousepos) override {
            }
            void dragReleaseOn(guibase* target, ivec2 mousepos) override {
            }
        };
        void setPresets(const std::vector<settings_preset_t>& presets) {
            std::vector<gui_list_entry*> entries;
            entries.reserve(presets.size());
            int32_t presetIdx = 0;
            for (const settings_preset_t& preset : presets) {
                entries.push_back(new gui_linetess_preset_list_entry(preset, presetIdx++));
            }
            list.setList(entries);
        }
    };
    class guictr_functions : public guictr_base {
    public:
        guibutton buttonReload;
        gui_list list;
        guictr_functions() : guictr_base() {
            add(&buttonReload);
            add(&list);
            list.setBackgroundRendered(false);
            list.padding = 0;
            buttonReload.setText("Reload");
            setLabel("Python function");
            setBackgroundRendered(true);
            setFlag(FLG_RENDER_LABEL, true);
        }
        ~guictr_functions() override {
            removeGuis();
        }
        void layout() override {
            int32_t h         = 24;
            auto cs           = getSizeContent();
            buttonReload.size = { cs.x, h };
            list.pos          = { 0, buttonReload.bottom() };
            list.size         = { cs.x, cs.y - list.pos.y };
            for (auto gui : guis) {
                gui->layout();
            }
        }
        void buttonClicked(guibase* button) override {
            if (parent) parent->buttonClicked(button);
        }


        class guictr_functions_entry : public gui_list_entry {
        public:
            const String functionName;
            const int32_t presetIdx;
            String displayName;
            guictr_functions_entry(const String& _functionName, const int32_t _presetIdx) : gui_list_entry(), functionName(_functionName), presetIdx(_presetIdx) {
                icon        = ICON_FILE;
                displayName = _functionName.substr(8);
            }
            String getText() override {
                return displayName;
            }
            void dragMoveOn(guibase* target, ivec2 mousepos) override {
            }
            void dragReleaseOn(guibase* target, ivec2 mousepos) override {
            }
        };
        void setFunctions(const std::vector<String>& functions) {
            std::vector<gui_list_entry*> entries;
            entries.reserve(functions.size());
            int32_t presetIdx = 0;
            for (const String& preset : functions) {
                entries.push_back(new guictr_functions_entry(preset, presetIdx++));
            }
            list.setList(entries);
        }
    };
#define WAVEFORM_BUFFER_SIZE (1024ULL * 64ULL)
    class guictr_audiovis : public guictr_base {
        static constexpr int INSET_OUTER = 8;
        static constexpr int INSET_INNER = 24;
        DawInstance* daw;
        std::unique_ptr<audioanaylzer> audioAnalyzer;
        audiohost* optionalAudioHost{}; // only set when running in standalone mode 
        sampleformat_t curSampleFormat{0, 0, sampleformat_bits_t::FLOAT_32};
        audio_spectrum spectrum;
        overlap_buffer_t<WAVEFORM_BUFFER_SIZE, audio_spectrum::NUM_CHANNELS> buffer;
        std::array<std::array<float, WAVEFORM_BUFFER_SIZE>, audio_spectrum::NUM_CHANNELS> bufferProcessed{};
        int32_t waveformWindowOffset  = 0;
        int32_t waveformWindowOffset2 = 0;

        settings_preset_t curPreset{};
        bool bHasPresetToLoad = false;
        settings_preset_t presetToLoad{};

        gui_color_pick colorPick;
        guidropdown_generic<String> dropdownRenderer;
        guidropdown_generic<String> dropdownMode;
        guidropdown_generic<String> dropdownSplineType;
        guidropdown_generic<String> dropdownBlendMode;
        guidropdown_generic<String> dropdownEdit;
        guidropdown_generic<int32_t> dropdownBlurDownsample;
        guidropdown_generic<int32_t> dropdownBlurPasses;
        gui_textfield textFieldExpression;
        guictr_presets presetBrowser;

        std::vector<String> fnList;
        guictr_functions functionBrowser;

        std::vector<gui_numberinput_field_base*> controlsParameters;
        String exprError;

        // temporary framebuffer, owned by nanovg context, cleared every frame
        NVGLUframebuffer* nvgFramebuffer = nullptr;
        GLPathRendererDashLines rendererDashLines;
        GLPathRendererPolyline2d rendererPolyline2d;
        GLPathRendererParAdvanced rendererParAdv;
        std::array<IPathRenderer*, 3> pathRenderers{ &rendererDashLines, &rendererPolyline2d, &rendererParAdv };
        // int curRendererType = 0;//2;
        IPathRenderer* curRenderer;
        std::array<BakeGLPath, 32> bakedPaths;
        std::shared_ptr<tex_shader> pipeShaderDarken;
        std::shared_ptr<blur_tex_shader> pipeFbBlur;
        std::shared_ptr<tex_shader> pipeShaderBloom;
        std::shared_ptr<tex_shader> pipeShaderBlit;
        std::shared_ptr<tex_shader> pipeShaderBlitFinal;
        std::shared_ptr<DrawVBO> vboFullscreenQuad;

        // continuously rendered to framebuffer
        std::array<std::shared_ptr<FrameBuffer>, 2> framebuffers;
        ivec2 vboSize{ 0, 0 };
        ivec2 lastCs{ 0, 0 };

        struct axis_label_t {
            vec2 pos;
            String label;
        };
        std::vector<vec2> flatCtrlPtsRender;
        std::vector<std::vector<vec2>> vecVecCtrlPtsRender;
        std::vector<axis_label_t> vecAxisLabels;
        std::vector<std::vector<float>> vecVecCtrlPtsRaw;


        bool bIsIdle = true;
        bool bEarlyInit = false;
        bool bInit = false;
        uint32_t curPath         = 0;
        uint32_t nextPathIdx     = 0;
        int ptDraggedIdx         = -1;
        bool bReinitBlurFB       = true;
        int iNextInitRenderer    = -1;
        bool bPause              = false;
        int nFrame               = 0;
        float fCtrAlpha          = 1.0f;
        int clearFrame           = 1;
        int triggerReloadShaders = 0;
        float phase01            = 0.0f;
        float phaseCont          = 0.0f;
        int32_t tick             = 0;
        int64_t timeLastBlock    = 0;

        // String pythonFunction    = "pathGen_circleFFT1024StepsStereoBig";
        int32_t currentPresetIdx = 2220;
        std::vector<settings_preset_t> presets;
        seq_rand rng;
        std::vector<vec2> ctrlPts;
        ivec2 sizeView{ 0, 0 };
        ivec2 sizeViewInner{ 0, 0 };
        vec2 aspectView{ 0.0f, 0.0f };
        void setSplineType(int _splineType) {
            curPreset.nSplineType = _splineType;
            onModeChanged();
        }
        void setMode(PathMode _mode) {
            curPreset.mode = _mode;
            onModeChanged();
        }
        void onModeChanged() {
            ctrlPts.clear();
            flatCtrlPtsRender.clear();
            exprError.clear();
            PathMode mode = curPreset.mode;
            textFieldExpression.setVisible(mode <= PLOT_PYTHON_EXPRESSION);
            // dropdownSplineMode.setVisible(mode == PATH_SCRIPT || mode == PATH_FREE || mode == FFT_SPECTRUM || mode == AUDIO_WAVEFORM);
            dropdownSplineType.setVisible(true);
            dropdownEdit.setVisible(mode == PATH_FREE);
            functionBrowser.setVisible(mode == PATH_SCRIPT);
            initControls();
            layout();
        }
        bool hasControlHandles() {
            return curPreset.mode == PATH_FREE;
        }
        struct src_parser {
            void preprocessSources(std::vector<glshader_src>& srcList) {
            }
        };
        void resizeFramebuffers() {
            vboSize = { 0, 0 };
        }
        void reinitPathRenderer() {
            for (BakeGLPath& path : bakedPaths) {
                if (path.uniforms_texture != 0) {
                    glDeleteTextures(1, &path.uniforms_texture);
                }
                path.uniforms_texture = 0;
                path.vbo.destroy();
            }
        }

    public:
        void reloadShaders() {
            src_parser parser;
            pipeShaderDarken = std::make_shared<tex_shader>("textured_fullscreen.vsh", "darken.fsh", TEXTURED_FULLSCREEN_GLSL_VERT, DARKEN_GLSL_FRAG);
            always_assert(!pipeShaderDarken->load(&parser));
            struct src_parser_blit_final {
                void preprocessSources(std::vector<glshader_src>& srcList) {
                    for (glshader_src& src : srcList) {
                        if (src.stage == GL_FRAGMENT_SHADER && 0) {
                            src.source = R"END(
#version 150 core

uniform sampler2D tex0;

in vec2 pass_texcoord;
out vec4 out_Color;

vec3 sampleShifted(sampler2D tex, vec2 tc, vec2 offset, vec2 offset1, vec2 offset2) {
    vec3 s1 = texture(tex, tc+offset, 0).rgb;
    vec3 s2 = texture(tex, tc+offset1, 0).rgb;
    vec3 s3 = texture(tex, tc+offset2, 0).rgb;
    return vec3(s1.r, s2.g, s3.b);
}
void main(void) {
    vec3 c = texture(tex0, pass_texcoord).rgb;
    //c = max(vec3(0.0), vec3(1.0)-c);
    c = pow(c, vec3(0.7));

    /*vec2 texDim = textureSize(tex0, 0);
    float step = 1.0f/texDim.x;
    vec3 sampleA = sampleShifted(tex0, pass_texcoord.st, vec2(step*2, 0), vec2(0, step*0), vec2(step*-2, 0));
    out_Color = vec4(sampleA, 1.0);*/  
    out_Color = vec4(c, 1.0);
}
)END";
                        }
                    }
                }
            };
            src_parser_blit_final parserBlitF;
            pipeShaderBlitFinal = std::make_shared<tex_shader>("textured_fullscreen.vsh", "blit_final.fsh", TEXTURED_FULLSCREEN_GLSL_VERT, BLIT_FINAL_GLSL_FRAG);
            always_assert(!pipeShaderBlitFinal->load(&parserBlitF));
            struct src_parser_blit {
                void preprocessSources(std::vector<glshader_src>& srcList) {
                    for (glshader_src& src : srcList) {
                        if (src.stage == GL_FRAGMENT_SHADER) {
                            src.source = R"END(
#version 150 core

uniform sampler2D tex0;

in vec2 pass_texcoord;
out vec4 out_Color;

void main(void) {
    vec4 c = texture(tex0, pass_texcoord);
     out_Color = c;
}
)END";
                        }
                    }
                }
            };
            src_parser_blit parserBlit;
            pipeShaderBlit = std::make_shared<tex_shader>("textured_fullscreen.vsh", "blit_texture.fsh", TEXTURED_FULLSCREEN_GLSL_VERT, BLIT_TEXTURE_GLSL_FRAG);
            always_assert(!pipeShaderBlit->load(&parserBlit));

            struct src_parser_bloom {
                void preprocessSources(std::vector<glshader_src>& srcList) {
                    for (glshader_src& src : srcList) {
                        if (src.stage == GL_FRAGMENT_SHADER) {
                            src.source = R"END(
#version 150 core

uniform float u_fade;
uniform sampler2D tex0;

in vec2 pass_texcoord;
out vec4 out_Color;

void main(void) {
     vec4 texCol = texture(tex0, pass_texcoord);
   texCol.rgb *= vec3(1.01, 1.0, 0.99);
     out_Color = tanh(texCol*u_fade*0.9)*1.0/0.9;
}
)END";
                        }
                    }
                }
            };
            src_parser_bloom parserBloom;
            pipeShaderBloom = std::make_shared<tex_shader>("textured_fullscreen.vsh", "blit_texture.fsh", TEXTURED_FULLSCREEN_GLSL_VERT, BLIT_TEXTURE_GLSL_FRAG);
            always_assert(!pipeShaderBloom->load(&parserBloom));
            src_parser parserFbBlur;
            always_assert(!pipeFbBlur->load(&parserFbBlur));
        }
        explicit guictr_audiovis(DawInstance* daw) 
            : guictr_base(), daw(daw), curRenderer(pathRenderers[curPreset.nRenderer])
        {
            setGuiType(gui_type::CTR_TYPE_AUDIO_VISUALIZER);
            for (int32_t i = 0; i < audio_spectrum::NUM_CHANNELS; i++) {
                auto& buf = bufferProcessed[i];
                memset(buf.data(), 0, sizeof(float) * buf.size());
            }
            setCanMouseHit(true);
            setBackgroundRendered(true);
            add(&colorPick);
            add(&dropdownMode);
            add(&dropdownRenderer);
            add(&dropdownSplineType);
            add(&dropdownBlendMode);
            add(&dropdownEdit);
            add(&textFieldExpression);
            add(&presetBrowser);
            add(&functionBrowser);
            add(&dropdownBlurDownsample);
            add(&dropdownBlurPasses);
        }

        ~guictr_audiovis() override {
            removeGuis();
            if (optionalAudioHost) {
                optionalAudioHost->stopAudio();
                delete optionalAudioHost;
            }
            for (auto* input : controlsParameters) {
                delete input;
            }
        }

        String storeContainerData() override {
            Stringstream sstream;
            try {
                std::vector<settings_preset_t> presets;
                presets.emplace_back(this->curPreset);
                settings_t settings1{ this->currentPresetIdx, presets };
                cereal::JSONOutputArchive ar(sstream);
                ar(cereal::make_nvp("visualizerPreset", settings1));
                sstream.flush();
            } catch (std::exception& e) {
                log_lf(Log::L_ERROR, "Exception: %s\n", e.what());
                return "";
            }
            return sstream.str();
        }

        void loadContainerData(const String& data) override {
            try {
                settings_t settings1;
                Stringstream sstream(data);
                cereal::JSONInputArchive ar(sstream);
                ar(cereal::make_nvp("visualizerPreset", settings1));
                if (settings1.currentPresetIdx >= 0 && settings1.presets.size() > 0) {
                    this->bHasPresetToLoad = true;
                    this->presetToLoad = settings1.presets[0];
                }
                return;
            } catch (std::exception& e) {
                log_lf(Log::L_ERROR, "Exception: %s\n", e.what());
            }
        }

        guictxtmenu_base* getTooltip(AppCtrl* appctrl) override {
            return nullptr;
        }

        void onRemove() override {
            guictr_base::onRemove();
            if (optionalAudioHost) {
                optionalAudioHost->stopAudio();
                // delete optionalAudioHost;
                // optionalAudioHost = nullptr;
            } else {
            }
            bIsIdle = true;
        }
        void earlyInit() {
            checkGLError("pre earlyInit");
            auto pathVisualizerPresets = App::Platform::toUserdataPath("presets/Visualizer");
            CreateDirectoryIfNotExists(pathVisualizerPresets);
            auto pathPythonModule = App::Platform::toUserdataPath("presets/Visualizer/visualizer_pathgen.py");
            if (!FileExists(pathPythonModule)) {
                std::vector<uint8_t> out;
                ReadFileVector(App::Platform::toDefaultSettingFilesPath("visualizer_pathgen.py"), out);
                WriteFileVector(pathPythonModule, out);
            }

#ifdef USE_PYTHON
            DAW::InitPythonInterpreter();
#endif
            reloadPythonModule();
            
            auto path = App::Platform::toUserdataPath("presets/Visualizer/presets.json");
            try {
                settings_t settings1 = loadSettings(path);
                presets              = settings1.presets;
                currentPresetIdx     = math::clamp<int32_t>(settings1.currentPresetIdx, 0, static_cast<int32_t>(presets.size()) - 1);
                presetBrowser.setPresets(presets);
                if (!presets.empty() && currentPresetIdx >= 0 && !bHasPresetToLoad) {
                    bHasPresetToLoad = true;
                    presetToLoad     = presets.at(currentPresetIdx);
                }
            } catch (std::exception& e) {
                try {
                    settings_t settings1{ this->currentPresetIdx, this->presets };
                    CreateDirectoryIfNotExists(App::Platform::toUserdataPath("presets/Visualizer"));
                    saveSettings(settings1, path);
                } catch (std::exception& e2) {
                    log_lf(Log::L_ERROR, "Exception: %s\n", e2.what());
                }
            }
        }

        void init(const sampleformat_t& sampleFormat) {
            pipeFbBlur        = std::make_shared<blur_tex_shader>();
            vboFullscreenQuad = std::make_shared<DrawVBO>();
            vboFullscreenQuad->genBuffers();
            resizeFramebuffers();
            glGenVertexArrays(1, &vboFullscreenQuad->vaoId);
            reloadShaders();
            // textFieldExpression.setValue("sin(x*3.1416*2)");
            // textFieldExpression.setValue("-(0.1+999.9*(1.0-(x^0.05)))/1000.0");
            textFieldExpression.setValue("x");
            textFieldExpression.setInputActivates(true);
            textFieldExpression.setReturnCommits(true);
            textFieldExpression.setLabel("Math Expression");
            auto fnValidateFunction = ([this](const String& value) {
                {
                    curPreset.expression = value;
                }
                // if (parent) {
                //     parent->buttonClicked(this);
                // }
                return true;
            });
            textFieldExpression.setChangeCallback(fnValidateFunction);
            textFieldExpression.setEndEditCallback(fnValidateFunction);
            for (auto renderer : pathRenderers) {
                always_assert(!renderer->init());
            }
            std::vector<String> strDropDownRenderer;
            strDropDownRenderer.emplace_back("ADV");
            strDropDownRenderer.emplace_back("POLYLINE2D");
            strDropDownRenderer.emplace_back("PAR");
            dropdownRenderer.setOptions(strDropDownRenderer);
            dropdownRenderer.fnOptionSelected = [this](int n, const String& option) {
                iNextInitRenderer = n;
                return option;
            };
            dropdownRenderer.setLabel("Renderer");
            dropdownRenderer.setSelectedIndex(iNextInitRenderer >= 0 ? iNextInitRenderer : curPreset.nRenderer);

            std::vector<String> strDropDownOptions;
            strDropDownOptions.emplace_back("Math Expression");
            strDropDownOptions.emplace_back("Python Expression");
            strDropDownOptions.emplace_back("Sin");
            strDropDownOptions.emplace_back("Python Script");
            strDropDownOptions.emplace_back("Free");
            strDropDownOptions.emplace_back("FFT Spectrum");
            strDropDownOptions.emplace_back("Audio Waveform");
            strDropDownOptions.emplace_back("SIMD Cosine");
            strDropDownOptions.emplace_back("SIMD Envelope");
            dbgassert(strDropDownOptions.size() == PathMode::NUM_PATH_MODES);
            dropdownMode.setOptions(strDropDownOptions);
            dropdownMode.fnOptionSelected = [this](int n, const String& option) {
                setMode(static_cast<PathMode>(n));
                return option;
            };
            dropdownMode.setLabel("Mode");
            dropdownMode.setSelectedIndex(curPreset.mode);

            std::vector<String> strSplineOption;
            strSplineOption.emplace_back("No Spline");
            strSplineOption.emplace_back("GenericBSpline");
            strSplineOption.emplace_back("LoopingGenericBSpline");
            strSplineOption.emplace_back("UniformCubicBSpline");
            strSplineOption.emplace_back("UniformCRSpline");
            strSplineOption.emplace_back("NaturalSpline");
            strSplineOption.emplace_back("B-Spline (tinyspline)");
            dropdownSplineType.setOptions(strSplineOption);
            dropdownSplineType.fnOptionSelected = [this](int n, const String& option) {
                setSplineType(n);
                return option;
            };
            dropdownSplineType.setSelectedIndex(curPreset.nSplineType);
            dropdownSplineType.setLabel("Spline Type");
            std::vector<String> strDropDownOptions2;
            strDropDownOptions2.emplace_back("Clear");
            strDropDownOptions2.emplace_back("Generate Sine");
            strDropDownOptions2.emplace_back("Randomize (Each)");
            strDropDownOptions2.emplace_back("Offset (Random)");
            strDropDownOptions2.emplace_back("Generate Circle");
            strDropDownOptions2.emplace_back("Scale (Random)");
            dropdownEdit.setOptions(strDropDownOptions2);
            dropdownEdit.fnOptionSelected = [this](int n, const String& option) {
                modifyPointOperation(ctrlPts, n);
                return "Generate Points";
            };
            dropdownEdit.setLabel("Modify Points");
            std::vector<String> strBlendModeOptions;
            strBlendModeOptions.emplace_back("Additive");
            strBlendModeOptions.emplace_back("Alpha");
            dropdownBlendMode.setOptions(strBlendModeOptions);
            dropdownBlendMode.fnOptionSelected = [this, strBlendModeOptions](int n, const String& option) {
                this->curPreset.blendMode = n;
                return strBlendModeOptions[n];
            };
            dropdownBlendMode.setLabel("Blend Mode");
            dropdownBlendMode.setSelectedIndex(static_cast<int32_t>(this->curPreset.blendMode % strBlendModeOptions.size()));

            std::vector<int32_t> vecBlurDownsampleOptions;
            vecBlurDownsampleOptions.reserve(8);
            int selIdx = 0;
            for (int i = 0; i < 8; i++) {
                vecBlurDownsampleOptions.emplace_back(1 << i);
                if (curPreset.nBlurDownsample == (1 << i)) {
                    selIdx = i;
                }
            }
            dropdownBlurDownsample.setOptions(vecBlurDownsampleOptions);
            dropdownBlurDownsample.fnOptionSelected = [this](int n, const int32_t& option) {
                this->curPreset.nBlurDownsample = n;
                this->bReinitBlurFB            = true;
                return std::to_string(option);
            };
            dropdownBlurDownsample.setLabel("Blur Downsample");
            dropdownBlurDownsample.setSelectedIndex(selIdx);


            std::vector<int32_t> vecBlurPassesOption;
            vecBlurPassesOption.push_back(2);
            vecBlurPassesOption.push_back(3);
            vecBlurPassesOption.push_back(5);
            vecBlurPassesOption.push_back(7);
            vecBlurPassesOption.push_back(10);
            auto blurPassesToString = [](auto idx) -> String {
                switch (static_cast<int32_t>(idx)) {
                    case 0:
                        return "2";
                    case 1:
                        return "3";
                    case 2:
                        return "5";
                    case 3:
                        return "7";
                    case 4:
                        return "10";
                    default:
                        return "OUTOFBOUNDS";
                }
            };
            dropdownBlurPasses.setOptions(vecBlurPassesOption);
            dropdownBlurPasses.fnOptionSelected = [this, blurPassesToString](int n, const int32_t& option) {
                this->curPreset.nBlurPasses = n;
                return blurPassesToString(this->curPreset.nBlurPasses);
            };
            dropdownBlurPasses.setLabel("Blur #Passes");
            dropdownBlurPasses.setSelectedIndex(this->curPreset.nBlurPasses);

            ctrlPts.emplace_back(0.1f, 0.1f);
            ctrlPts.emplace_back(0.3f, 0.8f);
            ctrlPts.emplace_back(0.7f, 0.3f);
            ctrlPts.emplace_back(0.9f, 0.5f);
            modifyPointOperation(ctrlPts, OP_CLEAR);
            modifyPointOperation(ctrlPts, OP_GEN_SINE);

            if (optionalAudioHost)
            {
                auto& tls      = daw_tls::getTls();
                tls.audioHost  = optionalAudioHost;

                app_iosettings& ioSettings = tls.settings->iosettings;

#ifdef _WIN32
                ioSettings.blocksize   = 512;
                ioSettings.samplerate  = 0;
                ioSettings.device_api  = "Windows WASAPI";
                app_ioaudioconfig& cnf = ioSettings.getConfig(ioSettings.device_api);
                cnf.device_api         = ioSettings.device_api;
                cnf.deviceNameInput    = "loopback";
                cnf.deviceNameOutput   = "default";
#endif
#ifdef __linux__
                ioSettings.blocksize   = 512;
                ioSettings.samplerate  = 0;
                ioSettings.device_api  = "ALSA";
                app_ioaudioconfig& cnf = ioSettings.getConfig(ioSettings.device_api);
                cnf.device_api         = ioSettings.device_api;
                cnf.deviceNameInput    = "default";
                cnf.deviceNameOutput   = "default";
#endif
                if (!optionalAudioHost->startAudio(ioSettings)) {
                    throw appexception("Failed starting audio stream");
                }
                const auto stream = optionalAudioHost->getStream(0);
                const auto sampleRate = stream->getSampleRate();
                const auto blockSize = stream->getBlockSize();
                audioAnalyzer = std::make_unique<audioanaylzer>();
                audioAnalyzer->init(blockSize, sampleRate);
                spectrum = *audioAnalyzer->analyzerLf;
                this->curSampleFormat = {sampleRate, blockSize, sampleformat_bits_t::FLOAT_32};
            } else if (daw) {
                auto sf = daw->getHost()->getSampleFormatInternal();
                audioAnalyzer = std::make_unique<audioanaylzer>();
                audioAnalyzer->init(sampleFormat.blockSize, sampleFormat.sampleRate);
                spectrum = *audioAnalyzer->analyzerLf;
                this->curSampleFormat = sf;
            }
            bool didSetPreset = false;
            if (bHasPresetToLoad) {
                setPreset(presetToLoad);
                bHasPresetToLoad = false;
                didSetPreset     = true;
            }
            if (!didSetPreset) {
                settings_preset_t pr{};
                pr.fBloomStrength   = 0;
                pr.fGain            = 1;
                pr.fFadeOutStrength = 0;
                pr.lineWidth        = 60;
                pr.blendMode        = 1;
                setPreset(pr);
            }
            onModeChanged();
        }

    private:
        void setPythonFunction(const String& fnName) {
            this->curPreset.pythonFunc = fnName;
        }
        void setPreset(const settings_preset_t& preset) {
            log_printf("Preset color: %08x\n", preset.rgba);
            const settings_preset_t curPresetCopy = this->curPreset;
            if (preset.nRenderer != this->curPreset.nRenderer)
                iNextInitRenderer = preset.nRenderer;
            if (preset.nBlurDownsample != this->curPreset.nBlurDownsample)
                bReinitBlurFB = true;
            
            this->curPreset = preset;
            
            if (fnList.empty() || std::find(fnList.begin(), fnList.end(), this->curPreset.pythonFunc) == fnList.end()) {
                this->curPreset.pythonFunc = "pathGen_multiShapes";
            }
            if (!fnList.empty() && std::find(fnList.begin(), fnList.end(), this->curPreset.pythonFunc) == fnList.end()) {
                this->curPreset.pythonFunc = fnList[0];
            }
            
            this->textFieldExpression.setValue(preset.expression);
            restoreRetainedParameters(curPresetCopy);
            onModeChanged();
            setColorSelector();
            audioAnalyzer->analyzerHf->setNumBands(curPreset.nBands);
            audioAnalyzer->analyzerLf->setNumBands(curPreset.nBands);
            audioAnalyzer->analyzerHf->setFreqRange(curPreset.freqMin, curPreset.freqMax);
            audioAnalyzer->analyzerLf->setFreqRange(curPreset.freqMin, curPreset.freqMax);
            audioAnalyzer->analyzerHf->updateBands();
            audioAnalyzer->analyzerLf->updateBands();
            spectrum = *audioAnalyzer->analyzerLf;
        }
        void restoreRetainedParameters(const settings_preset_t& preset) {
        }
        void initControls() {
            for (auto* input : controlsParameters) {
                remove(input);
                delete input;
            }
            controlsParameters.clear();
            PathMode mode = curPreset.mode;
            auto addInput = [this](auto* ptr) {
                controlsParameters.push_back(ptr);
                add(ptr);
                return ptr;
            };
            dropdownMode.setSelectedIndex(static_cast<int32_t>(curPreset.mode));
            dropdownBlendMode.setSelectedIndex(curPreset.blendMode);
            dropdownBlurPasses.setSelectedIndex(curPreset.nBlurPasses);
            dropdownBlurDownsample.setSelectedIndex(curPreset.nBlurDownsample);
            dropdownRenderer.setSelectedIndex(iNextInitRenderer >= 0 ? iNextInitRenderer : curPreset.nRenderer);
            dropdownSplineType.setSelectedIndex(curPreset.nSplineType);
            addInput(createParamInput<int32_t>("Oversample", ivec2(1, 2), &curPreset.nOversample));
            if (curPreset.nSplineType > 0 && curPreset.nSplineType < 4) {
                addInput(createParamInput<int32_t>("Degree", ivec2(1, 8), &curPreset.degree));
            }
            addInput(createParamInput<int32_t>("NSamples", ivec2(3, 1 << 16), &curPreset.nSamples));
            addInput(createParamInput<float>("Antialias", vec2(0, 512), &curPreset.fAntialias));
            addInput(createParamInput<int32_t>("Line Width", ivec2(1, 4000), &curPreset.lineWidth));
            addInput(createParamInput<float>("Fade Out", vec2(0.0f, 1.0f), &curPreset.fFadeOutStrength));
            addInput(createParamInput<float>("Bloom", vec2(-12.0f, 12.0f), &curPreset.fBloomStrength));
            addInput(createParamInput<float>("Blur Feedback", vec2(-12.0f, 12.0f), &curPreset.fBlurFeedbackStrength));
            if (mode == PATH_FREE) {
                addInput(createParamInput<int32_t>("Randomness", ivec2(0, 1 << 20), &curPreset.randRange));
            }
            addInput(createParamInput<float>("Cycle Color", vec2(-100.0f, 100.0f), &curPreset.fCycleColor));
            if (mode == FFT_SPECTRUM || mode == AUDIO_WAVEFORM || mode == PATH_SCRIPT) {
                addInput(createParamInput<float>("Gain", vec2(0.0f, dsp_util::fromdBFS(24.0f)), &curPreset.fGain));
                gui_numberinput_field_generic<int32_t>* ctrlFreqMin = addInput(createParamInput<int32_t>("Min Freq", ivec2(20, 100000), &curPreset.freqMin));
                gui_numberinput_field_generic<int32_t>* ctrlFreqMax = addInput(createParamInput<int32_t>("Max Freq", ivec2(20, 100000), &curPreset.freqMax));
                ctrlFreqMin->fnValueEditChanged = ctrlFreqMax->fnValueEditChanged = [&audio = audioAnalyzer, pCurPreset = &curPreset](gui_numberinput_field_base*, int32_t v) {
                    if (audio->analyzerLf) {
                        audio->analyzerLf->setFreqRange(pCurPreset->freqMin, pCurPreset->freqMax);
                        audio->analyzerHf->setFreqRange(pCurPreset->freqMin, pCurPreset->freqMax);
                        audio->analyzerLf->updateBands();
                        audio->analyzerHf->updateBands();
                    }
                };
            }
            if (mode == FFT_SPECTRUM || mode == PATH_SCRIPT) {
                gui_numberinput_field_generic<int32_t>* ctrlNumBands = addInput(createParamInput<int32_t>("# Bands", ivec2(4, 1024), &curPreset.nBands));
                ctrlNumBands->fnValueEditChanged = [this](gui_numberinput_field_base*, int32_t v) {
                    if (audioAnalyzer->analyzerLf) {
                        audioAnalyzer->analyzerLf->setNumBands(v);
                        audioAnalyzer->analyzerHf->setNumBands(v);
                        audioAnalyzer->analyzerLf->updateBands();
                        audioAnalyzer->analyzerHf->updateBands();
                        spectrum = *audioAnalyzer->analyzerLf;
                    }
                };
            }
        }
        float fRand() {
            auto mask          = 0xFFFFFF;
            uint32_t rnd32Bits = rng.rng_rand();
            return ((rnd32Bits & mask) / (float) mask) * 2.0f - 1.0f;
        }
        static float smoothstep(float t) {
            return t * t * (3.0f - 2.0f * t);
        }
        void modifyPointOperation(std::vector<vec2>& ctrlPts, int op) {

            switch (op) {
                case OP_CLEAR: {
                    ctrlPts.clear();
                    break;
                }
                case OP_GEN_SINE: {
                    ctrlPts.clear();
                    int iSteps  = 32;
                    float fStep = 1.0f / (iSteps - 1);
                    for (int i = 0; i < iSteps; i++) {
                        ctrlPts.emplace_back(fStep * i * 2.0f - 1.0f, sin(M_PI * 2.0f * fStep * i));
                    }
                    break;
                }
                case OP_GEN_CIRCLE: {
                    ctrlPts.clear();
                    int iSteps  = 32;
                    float fStep = 1.0f / (iSteps - 1);
                    float cX    = 0.0f;
                    float cY    = 0.0f;
                    for (int i = 0; i < iSteps; i++) {
                        float x = sin(fStep * i * M_PI * 2.0f) + cX;
                        float y = cos(fStep * i * M_PI * 2.0f) + cY;
                        // ctrlPts.emplace_back(fStep*i, 0.5+0.5*sin(M_PI*2.0f*fStep*i));
                        ctrlPts.emplace_back(x, y);
                    }
                    break;
                }
                case OP_RAND_EACH: {
                    float fRandRng = curPreset.randRange / 2000000.0f;
                    for (auto& pt : ctrlPts) {
                        float scale = 1.0;//smoothstep(idx/(float)(len-1));
                        pt.x += fRand() * fRandRng * scale;
                        pt.y += fRand() * fRandRng * scale;
                    }
                    break;
                }
                case OP_RAND_ALL: {
                    int len        = ctrlPts.size();
                    int idx        = 0;
                    float fRandRng = curPreset.randRange / 4000000.0f;
                    vec2 offset{ fRand() * fRandRng, fRand() * fRandRng };
                    for (auto& pt : ctrlPts) {
                        float scale = smoothstep(idx / (float) (len - 1));
                        pt += offset * scale;
                        idx++;
                    }
                    break;
                }
                case OP_MULTIPLY_ALL: {
                    float fRandRng = curPreset.randRange / 4000000.0f;
                    float scale    = 1.0f + (fRand() * 2.0f - 1.0f) * fRandRng;
                    for (auto& pt : ctrlPts) {
                        pt *= scale;
                    }
                    break;
                }
                default:
                    break;
            }
        }
        static GLboolean getGlState(int glenum) {
            GLboolean b = false;
            glGetBooleanv(glenum, &b);
            return b;
        }
        static void preGLState() {
            checkGLError("waveformrender::render start");
            dbgassert(getGlState(GL_CULL_FACE));
            dbgassert(getGlState(GL_BLEND));
            dbgassert(!getGlState(GL_DEPTH_TEST));
            dbgassert(!getGlState(GL_SCISSOR_TEST));
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, 0);
            checkGLError("fb prerender");
        }
        template<typename T>
        void sampleFillVec(std::vector<vec2>& v, T& evalFn) {
            const auto steps   = math::max(2, curPreset.nSamples / math::max(1, static_cast<int32_t>(vecVecCtrlPtsRender.size())));
            const float fStepX = 1.0f / (steps - 1);
            for (int i = 0; i < steps; i++) {
                v.push_back(vec2{ evalFn(i * fStepX) });
            }
        }

        void makeVecSplineLib(const std::vector<vec2>& ctrlPts, std::vector<vec2>& v, int option) {
            if (ctrlPts.size() < 2)
                return;
            const auto sampleSplineLibType = [this](auto& spline, std::vector<vec2>& v) {
                const double maxT = spline.getMaxT();
                auto evalAt       = [&spline, maxT](float f) {
                    return spline.getPosition(f * maxT);
                };
                sampleFillVec(v, evalAt);
            };
            switch (option) {
                case 0: {
                    auto deg = math::min<int32_t>(ctrlPts.size() - 1, curPreset.degree);
                    GenericBSpline<vec2, floating_t> spline(ctrlPts, deg);
                    sampleSplineLibType(spline, v);
                } break;
                case 1: {
                    auto deg = math::min<int32_t>(ctrlPts.size() - 1, curPreset.degree);
                    LoopingGenericBSpline<vec2, floating_t> spline(ctrlPts, deg);
                    sampleSplineLibType(spline, v);
                } break;
                case 2: {
                    if (ctrlPts.size() < 4)
                        return;
                    UniformCubicBSpline<vec2, floating_t> spline(ctrlPts);
                    sampleSplineLibType(spline, v);
                } break;
                case 3: {
                    if (ctrlPts.size() < 4)
                        return;
                    UniformCRSpline<vec2, floating_t> spline(ctrlPts);
                    sampleSplineLibType(spline, v);
                } break;
                case 4:
                    if (ctrlPts.size() > 2) {
                        NaturalSpline<vec2, floating_t> spline(ctrlPts);
                        sampleSplineLibType(spline, v);
                    }
                    break;
            }
        }

        void makeVecTinySpline(const std::vector<vec2>& ctrlPts, std::vector<vec2>& v) {
            if (ctrlPts.empty())
                return;
            int32_t dim     = 2;
            auto deg        = math::min<int32_t>(ctrlPts.size() - 1, curPreset.degree);
            auto splineType = TS_CLAMPED;
            // if (ts_bspline_new(nCtrlp, dim, deg, type, &spline, &status))
            tsBSpline spline;
            tsStatus status;
            tsError err = ts_bspline_new(ctrlPts.size(), dim, deg, splineType, &spline, &status);
            if (err) log_printf("ts_bspline_new err %d: %s\n", err, status.message);
            dbgassert(err == 0);


            dbgassert(ctrlPts.size() * dim == ts_bspline_len_control_points(&spline));


            const auto data = reinterpret_cast<const tsReal*>(ctrlPts.data());
            err             = ts_bspline_set_control_points(&spline, data, &status);
            if (err) log_printf("ts_bspline_new err %d: %s\n", err, status.message);
            dbgassert(err == 0);


            auto evalAt = [pSpline = &spline](float f) -> vec2 {
                tsStatus status;
                tsError err;
                tsReal* result;
                tsDeBoorNet net;
                err = ts_bspline_eval(pSpline, f, &net, &status);
                if (err) log_printf("ts_bspline_new err %d: %s\n", err, status.message);

                err = ts_deboornet_result(&net, &result, &status);
                if (err) log_printf("ts_bspline_new err %d: %s\n", err, status.message);
                dbgassert(err == 0);
                vec2 vec;
                vec[0] = result[0];
                vec[1] = result[1];
                free(result);
                ts_deboornet_free(&net);
                return vec;
            };
            sampleFillVec(v, evalAt);
            ts_bspline_free(&spline);
        }
        void makeVecSin(const std::vector<vec2>& ctrlPts, std::vector<vec2>& v) {
            auto evalAt = [t = phaseCont](float f) {
                return vec2(f * 2.0f - 1.0f, sinf(M_PI * 2.0f * f + t));
            };
            sampleFillVec(v, evalAt);
        }
        void evaluateMuParserExpression(const std::vector<vec2>& ctrlPts, std::vector<vec2>& v, const String& expression) {
            try
            {
                double var_x = 0.0;
                double var_t = phaseCont;
                double var_p = phase01;
                mu::Parser p;
                p.DefineVar("x", &var_x); 
                p.DefineVar("p", &var_p); 
                p.DefineVar("t", &var_t);
                p.DefineConst("pi", mu::MathImpl<mu::value_type>::CONST_PI); 
                p.DefineConst("e", mu::MathImpl<mu::value_type>::CONST_E);
                p.SetExpr(expression);
                auto evalAt = [&](float f) {
                    var_x = f * 2.0 - 1.0;
                    double result = p.Eval();
                    return vec2(var_x, result);
                };
                sampleFillVec(v, evalAt);
                exprError = "";
            }
            catch (mu::Parser::exception_type &e)
            {
                String cur = exprError;
                exprError  = e.GetMsg();
                if (exprError != cur) {
                    log_printf("Failed evaluating expression %s: %s\n", StringAsCStr(expression), StringAsCStr(exprError));
                }
            }
        }
        void evaluateExpression(const std::vector<vec2>& ctrlPts, std::vector<vec2>& v, const String& expression) {
            try {
#ifdef USE_PYTHON
                const char* szExpr = StringAsCStr(expression);
                auto evalAt        = [szExpr, t = phaseCont](float f) {
                    double result = 0.0;
                    if (DAW::IsPythonInitialized()) {
                        result = PyMachine::pyEvalExpression(szExpr, f, t);
                    }
                    return vec2(f * 2.0f - 1.0f, result);
                };
#else
                auto evalAt = [](float f) {
                    return vec2(f * 2.0f - 1.0f, 0.0f);
                };
#endif
                sampleFillVec(v, evalAt);
                exprError = "";
            } catch (std::exception& e) {
                String cur = exprError;
                exprError  = e.what();
                if (exprError != cur) {
                    log_printf("Failed evaluating expression %s: %s\n", StringAsCStr(expression), StringAsCStr(exprError));
                }
            }
        }

        /** 
        * XMScalarCosFast: Fast cosine interpolation
        *  @param y in [-pi,pi]
        *  @return in [-1,1]
        */
        inline float XMScalarCosFast
        (
            float y
        )
        {
            using math::simd::XM_PI;
            using math::simd::XM_PIDIV2;
            // Map y to [-pi/2,pi/2] with cos(y) = sign*cos(x).
            float sign;
            if (y > XM_PIDIV2)
            {
                y = XM_PI - y;
                sign = -1.0f;
            }
            else if (y < -XM_PIDIV2)
            {
                y = -XM_PI - y;
                sign = -1.0f;
            }
            else
            {
                sign = +1.0f;
            }

            // 10-degree minimax approximation
            float y2 = y*y;
            float p = ( ( ( ( -2.6051615e-07f * y2 + 2.4760495e-05f ) * y2 - 0.0013888378f ) * y2 + 0.041666638f ) * y2 - 0.5f ) * y2 + 1.0f;
            return sign*p;
        }
        BakeGLPath& getPath(std::vector<std::vector<vec2>>& vecVecCtrlPts, Uniforms bakeOpt, int oversample) {
            if (bPause) {
                return this->bakedPaths[this->curPath];
            }

            const auto colorAxis = vec4(0.7f, 0.7f, 0.7f, 1.0f);
            const auto colorGrid = vec4(0.3f, 0.3, 0.3f, 1.0f);
            bool hasAxis = curPreset.mode >= SIMD_COS_TEST || 
                            curPreset.mode == PATH_FREE || 
                            curPreset.mode == PLOT_PYTHON_EXPRESSION || 
                            curPreset.mode == PLOT_MATH_EXPRESSION;
            std::vector<path_t> paths;
            if (hasAxis) {
                vecAxisLabels.clear();
                if (vecVecCtrlPts.size() < 2+4+1) {
                    vecVecCtrlPts.resize(2+4+1);
                }
            }
            int32_t pathIndex = 0;
            float fOversample = oversample;
            auto optColor = bakeOpt.color;
            for (auto& ctrlPts : vecVecCtrlPts) {
                bakeOpt.color = optColor;
                path_t path;
                std::vector<vec2>& vecSampled = path.pathVecs;
                if (hasAxis && pathIndex < 2+4) {
                    vecSampled.resize(2);
                    switch (pathIndex) {
                        case 0: // x axis
                            vecSampled[0] = vec2{-1.0f, 0.0f};
                            vecSampled[1] = vec2{1.0f, 0.0f};
                            bakeOpt.color = colorAxis;
                            vecAxisLabels.push_back(axis_label_t{{-1.05f, 0.0f}, "-1"});
                            vecAxisLabels.push_back(axis_label_t{{ 1.05f, 0.0f}, "+1"});
                            break;
                        case 1: // y axis
                            vecSampled[0] = vec2{0.0f, -1.0f};
                            vecSampled[1] = vec2{0.0f, 1.0f};
                            bakeOpt.color = colorAxis;
                            vecAxisLabels.push_back(axis_label_t{{0.0f, -1.05f}, "-1"});
                            vecAxisLabels.push_back(axis_label_t{{0.0f,  1.05f}, "+1"});
                            break;
                        case 2: // y + 1
                            vecSampled[0] = vec2{-1.0f, 1.0f};
                            vecSampled[1] = vec2{1.0f, 1.0f};
                            bakeOpt.color = colorGrid;
                            break;
                        case 3: // x + 1
                            vecSampled[0] = vec2{1.0f, -1.0f};
                            vecSampled[1] = vec2{1.0f, 1.0f};
                            bakeOpt.color = colorGrid;
                            break;
                        case 4: // y - 1
                            vecSampled[0] = vec2{-1.0f, -1.0f};
                            vecSampled[1] = vec2{1.0f, -1.0f};
                            bakeOpt.color = colorGrid;
                            break;
                        case 5: // x - 1
                            vecSampled[0] = vec2{-1.0f, -1.0f};
                            vecSampled[1] = vec2{-1.0f, 1.0f};
                            bakeOpt.color = colorGrid;
                            break;
                    }
                } else {
                    switch (curPreset.mode) {
                        case SIMD_ENV_TEST: {
                            auto stepsSetting = math::max(2, curPreset.nSamples / math::max(1, static_cast<int32_t>(vecVecCtrlPtsRender.size())));
                            static constexpr size_t SIMD_WIDTH = 8;
                            // round steps up to nearest multiple of SIMD_WIDTH
                            const auto steps = (stepsSetting + SIMD_WIDTH - 1) / SIMD_WIDTH * SIMD_WIDTH;
                            const float fStepX = 1.0f / (steps - 1);
                            using FPType = float;
                            std::optional<std::vector<FPType>> optHeapData;
                            FPType heapArrIn[32]{};
                            FPType heapArrOut[32]{};
                            FPType* pDataIn = &heapArrIn[0];
                            FPType* pDataOut = &heapArrOut[0];
                            if (steps > 32) {
                                optHeapData = std::vector<FPType>(static_cast<size_t>(steps*2));
                                pDataIn = optHeapData.value().data();
                                pDataOut = pDataIn + steps;
                                dbgassert(optHeapData->size() == steps*2);
                            }
                            const auto activeGraph = (getTimeMillis()/1000) % 2;
                            const bool isBlueActive = activeGraph % 2 == 0;
                            for (size_t i = 0; i < steps; i++) {
                                pDataIn[i] = (i * fStepX * 2.0f - 1.0f);
                            }

                            bakeOpt.color = vec4(0.0f, 0.0f, 0.0f, 1.0f);
                            bakeOpt.color[1+isBlueActive*1] = 1.0f;
#ifdef PROFILE_PATHGEN
                            hires_timer_t timer;
#endif
                            switch (activeGraph) {
                                default:
                                case 0: {
                                    for (size_t i = 0; i < steps; i++) {
                                        auto vals = math::clamp<FPType>(*pDataIn++, 1.0E-12, 1.0);
                                        vals = log(vals) * 0.1;
                                        vals = exp(vals);
                                        vals = cos(vals * M_PI);
                                        *pDataOut++ = 1000.0 - 999.9 * (.5 - .5 * vals);
                                    }
                                    break;
                                }
                                case 1: {
                                    static constexpr size_t SIMD_WIDTH = 8;
                                    for (size_t i = 0; i < steps; i+=SIMD_WIDTH) {
                                        FPType envParamVals[SIMD_WIDTH]{};
                                        for (size_t j = 0; j < SIMD_WIDTH; j++) {
                                            envParamVals[j] = math::clamp<FPType>(*pDataIn++, 1.0E-12f, 1.0f);
                                        }
                                        using Vec4D = glm::vec<4, FPType, glm::packed_highp>;
                                        auto sse8Float = reinterpret_cast<__m256*>(&envParamVals[0]);
                                        *sse8Float = math::simd::log_v8f(*sse8Float);
                                        auto pIn = &envParamVals[0];
                                        __m128 m;
                                        for (size_t j = 0; j < SIMD_WIDTH; j += 4) {
                                            Vec4D& valsRef = *reinterpret_cast<Vec4D*>(&pIn[0]);
                                            // Vec4D& vals = *reinterpret_cast<Vec4D*>(&pIn[0]);
                                            *reinterpret_cast<vec4*>(&m) = valsRef * 0.1f;
                                            // auto sse4Float = reinterpret_cast<__m128*>(&vals);
                                            m = math::simd::exp_v4f(m);
                                            // auto valsCosd = glm::fastCos(vals * M_PIf);
                                            auto valsCosd = pDataOut;
                                            auto floatPtr = reinterpret_cast<float*>(&m);
                                            math::simd::cos_test<float, 4>(floatPtr, valsCosd);
                                            for (size_t k = 0; k < 4; k++) {
                                                // pDataOut[k] = 1000.0f - 999.9f * (.5f - .5f * valsCosd[k]);
                                                pDataOut[k] = 1000.0f - 999.9f * (.5f - .5f * valsCosd[k]);
                                            }
                                            pIn += 4;
                                            pDataOut += 4;
                                        }
                                    }
                                    break;
                                }
                            }
#ifdef PROFILE_PATHGEN
                            log_printf("%zd took %zd\n", activeGraph, timer.getTime());
#endif
                            pDataOut -= steps;
                            vecSampled.resize(steps);
                            for (size_t i = 0; i < steps; i++) {
                                vecSampled[i] = vec2((i * fStepX) * 2.0f - 1.0f, pDataOut[i] / 1000.0);
                            }
                            break;
                        }
                        case SIMD_COS_TEST: {
                            auto stepsSetting = math::max(2, curPreset.nSamples / math::max(1, static_cast<int32_t>(vecVecCtrlPtsRender.size())));
                            static constexpr size_t SIMD_WIDTH = 8;
                            // round steps up to nearest multiple of SIMD_WIDTH
                            const auto steps = (stepsSetting + SIMD_WIDTH - 1) / SIMD_WIDTH * SIMD_WIDTH;
                            const float fStepX = 1.0f / (steps - 1);
                            using FPType = float;
                            std::optional<std::vector<FPType>> optHeapData;
                            FPType heapArrIn[32]{};
                            FPType heapArrOut[32]{};
                            FPType* pDataIn = &heapArrIn[0];
                            FPType* pDataOut = &heapArrOut[0];
                            if (steps > 32) {
                                optHeapData = std::vector<FPType>(static_cast<size_t>(steps*2));
                                pDataIn = optHeapData.value().data();
                                pDataOut = pDataIn + steps;
                                dbgassert(optHeapData->size() == steps*2);
                            }
                            const auto activeGraph = (getTimeMillis()/1000) % 4;
                            const bool isBlueActive = activeGraph % 2 == 0;
                            const FPType range = FPType(M_PI * 1.0f);
                            for (size_t i = 0; i < steps; i++) {
                                pDataIn[i] = (i * fStepX * 2.0f - 1.0f) * range;
                            }

                            bakeOpt.color = vec4(0.0f, 0.0f, 0.0f, 1.0f);
                            bakeOpt.color[1+isBlueActive*1] = 1.0f;
                            hires_timer_t timer;
                            switch (activeGraph) {
                                case 0: {
                                    for (size_t i = 0; i < steps / SIMD_WIDTH; i++) {
                                        // v4sf* pIn = reinterpret_cast<v4sf*>(pDataIn);
                                        // v4sf result = cos_ps(*pIn);
                                        // std::memcpy(pDataOut, &result, sizeof(FPType) * 4);
                                        math::simd::cos<FPType, SIMD_WIDTH>(pDataIn, pDataOut);
                                        pDataIn += SIMD_WIDTH;
                                        pDataOut += SIMD_WIDTH;
                                    }
                                    break;
                                }
                                case 1: {
                                    for (size_t i = 0; i < steps / SIMD_WIDTH; i++) {
                                        // math::simd::sin_test<FPType, SIMD_WIDTH>(pDataIn, pDataOut);
                                        math::simd::cos_test<FPType, SIMD_WIDTH>(pDataIn, pDataOut);
                                        pDataIn += SIMD_WIDTH;
                                        pDataOut += SIMD_WIDTH;
                                    }
                                    break;
                                }
                                case 2: {
                                    for (size_t i = 0; i < steps; i++) {
                                        *pDataOut++ = std::sin(*pDataIn++);
                                    }
                                    break;
                                }
                                case 3: {
                                    for (size_t i = 0; i < steps / SIMD_WIDTH; i++) {
                                        // math::simd::sin_test<FPType, SIMD_WIDTH>(pDataIn, pDataOut);
                                        math::simd::sin_test<FPType, SIMD_WIDTH>(pDataIn, pDataOut);
                                        pDataIn += SIMD_WIDTH;
                                        pDataOut += SIMD_WIDTH;
                                    }
                                    break;
                                }
                            }
                            
                            log_printf("%zd took %zd\n", activeGraph, timer.getTime());
                            pDataOut -= steps;
                            vecSampled.resize(steps);
                            for (size_t i = 0; i < steps; i++) {
                                vecSampled[i] = vec2((i * fStepX) * 2.0f - 1.0f, pDataOut[i]);
                            }
                            break;
                        }
                        case PLOT_MATH_EXPRESSION:
                            evaluateMuParserExpression(ctrlPts, vecSampled, textFieldExpression.getEditValue());
                            break;
                        case PLOT_PYTHON_EXPRESSION:
                            evaluateExpression(ctrlPts, vecSampled, textFieldExpression.getEditValue());
                            break;
                        case PLOT_SIN:
                            makeVecSin(ctrlPts, vecSampled);
                            break;
                        default:
                        case AUDIO_WAVEFORM:
                        case FFT_SPECTRUM:
                        case PATH_SCRIPT:
                        case PATH_FREE:
                            switch (curPreset.nSplineType) {
                                case 0: {
                                    /* if (pathIndex < 2) {
                                        bakeOpt.color = vec4(1.0f, 1.0f, 1.0f, 0.5f);
                                    } else {
                                        auto color = dbgcolorsArray[pathIndex % dbgcolorsArraySize];
                                        bakeOpt.color = {color.r, color.g, color.b, 0.8f};
                                    } */
                                    vecSampled = ctrlPts;
                                    break;
                                }
                                case 6:
                                    makeVecTinySpline(ctrlPts, vecSampled);
                                    break;
                                default:
                                    makeVecSplineLib(ctrlPts, vecSampled, curPreset.nSplineType-1);
                                    break;
                            }
                            break;
                    }
                }
                for (auto& pt : vecSampled) {
                    auto scaledPt = toScaledPt(pt);//(pt * aspectView) + 0.5f;
                    pt = (vec2(sizeViewInner) * scaledPt + vec2(INSET_INNER)) * fOversample;
                }
                path.pathOpts = bakeOpt;
                paths.push_back((path));
                pathIndex++;
            }
            for (axis_label_t& label : vecAxisLabels) {
                auto scaledPt = toScaledPt(label.pos);//(pt * aspectView) + 0.5f;
                label.pos = (vec2(sizeViewInner) * scaledPt + vec2(INSET_INNER+INSET_OUTER));
            }

            curPath = nextPathIdx;
            BakeGLPath& bakedPath = this->bakedPaths[this->nextPathIdx++];
            if (this->nextPathIdx >= this->bakedPaths.size()) {
                this->nextPathIdx = 0;
            }
            curRenderer->bakePaths(paths, bakedPath);
            return bakedPath;
        }

    public:
        void onTick(AppCtrl* ctrl) override {
            guictr_base::onTick(ctrl);
            bool bIsHovered = ctrl->mouseInside;
            // also check if mouse is inside the rect
            if (bIsHovered) {
                ivec2 mPosRel = toControlsObjectSpace(ctrl->m_mousePos, this);
                if (!(getStateFlags() & FLG_HVRD) && (mPosRel.x < 0 || mPosRel.y < 0 || mPosRel.x > size.x || mPosRel.y > size.y)) {
                    bIsHovered = false;
                }
            }
            fCtrAlpha = math::clamp(fCtrAlpha + (bIsHovered ? 0.1f : -0.02f), 0.0f, 1.0f);
            tick++;
            if (bInit) {
                if (tick > 120) {
                    tick = 0;
                    reloadPythonModule();
                }
                if (this->audioAnalyzer)
                    this->audioAnalyzer->onTick();
            }
        }
        void reloadPythonModule() {
            fnList.clear();
#ifdef USE_PYTHON
            try {
                if (DAW::IsPythonInitialized()) {
                    py::module moduleImpl = py::module::import(moduleNamePathGen);
                    moduleImpl.reload();
                    getPyFunctionList(moduleNamePathGen, fnList);
                }
                functionBrowser.setFunctions(fnList);
            } catch (std::exception& e) {
                log_printf("%s\n", e.what());
            }
#endif
        }

        static constexpr float MTR_FLOOR = -70.0f;
        static constexpr float MTR_CEIL  = 0.0f;
        static float scaledRange(float db, float lvlFloor, float lvlCeil) {
            if (db < lvlFloor)
                return 1.0f;
            float lvlRange = lvlFloor - lvlCeil;
            return (math::max(lvlFloor, math::min(db, lvlCeil)) - lvlCeil) / lvlRange;
        }
        void renderPass(int w, int h, int oversample) {
            float millisCont = phaseCont;
            DrawVBO& vbo     = *vboFullscreenQuad;
            mat4x4 matProj   = glm::ortho(0.0f, (float) w, (float) h, 0.0f, -1.0f, 1.0f);
            this->framebuffers[0]->bind();

            if (clearFrame) {
                if (clearFrame == 2) {
                    glClearColor(1, 1, 1, 1);
                } else {
                    glClearColor(0, 0, 0, 0);
                }
                clearFrame = 0;
                glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
            } else {


                if (curPreset.fFadeOutStrength < 1.0f) {
                    // darken previous frames content in tempFrameBuffer
                    // multiplies tempFrameBuffer color att0 by fFadeOutStrength
                    // does not read from any textures
                    
                    glBlendFuncSeparate(GL_DST_COLOR, GL_ZERO, GL_DST_ALPHA, GL_ZERO);
                    glBindVertexArray(vbo.vaoId);
                    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo.vboIdxId);
                    auto pipeDarken = pipeShaderDarken.get();
                    glUseProgram(pipeDarken->program);
                    pipeDarken->setUniforms(w, h, millisCont, curPreset.fFadeOutStrength);
                    glUniformMatrix4fv(pipeDarken->u_mvp, 1, GL_FALSE, value_ptr(matProj));
                    glBindTexture(GL_TEXTURE_2D, 0);
                    glDrawElements(GL_TRIANGLES, vbo.nIndices, GL_UNSIGNED_INT, nullptr);
                    glBindVertexArray(0);
                }
            }

            Uniforms bakeOpt;
            bakeOpt.linecaps    = vec2(LineCaps::round, LineCaps::round);
            bakeOpt.linejoin    = LineJoin::round;
            bakeOpt.miter_limit = 10.0f * oversample;


            const auto col = rgbaToNvg(curPreset.rgba);
            bakeOpt.color  = { col.r, col.g, col.b, col.a };
            if (math::abs(curPreset.fCycleColor) > 0) {
                auto hsl      = rgbToHSL(col.r, col.g, col.b);
                float hue     = hsl.r + (sinf(phase01 * M_PI * 2.0)) * curPreset.fCycleColor;
                auto newCol   = nvgHSL(hue, hsl.g, hsl.b);
                bakeOpt.color = { newCol.r, newCol.g, newCol.b, col.a };
            }

            bakeOpt.linewidth = curPreset.lineWidth / 10.0f * oversample;
            bakeOpt.antialias = curPreset.fAntialias * oversample;
            bakeOpt.scale     = 1;

            DAW::AudioIO::AudioStream* stream    = optionalAudioHost ? optionalAudioHost->getStream(0) : nullptr;
            auto tNow      = getTimeMicros();
            if (timeLastBlock > 0 && stream) {
                double timeSince = (tNow - timeLastBlock) / 1000000.0;
                waveformWindowOffset2 += math::floordS32(timeSince * (double) stream->getSampleRate());
            }
            timeLastBlock            = tNow;
            BakeGLPath* ptrBakedPath = nullptr;
            if (curPreset.mode == AUDIO_WAVEFORM && stream) {

                auto nSampleSize      = bufferProcessed[0].size();
                ivec2 sizeOversampled = sizeViewInner * oversample;
                audioclip_texture_t waveform;
                waveform.pos  = { 0, 0 };
                waveform.size = sizeOversampled;
                dbgassert(waveform.size.x > 0);
                waveform.sampleBegin       = waveformWindowOffset;
                waveform.sampleBeginOffset = waveformWindowOffset2;
                waveform.sampleEnd         = nSampleSize;
                waveform.samplesPerPx      = (nSampleSize / 2.0) / sizeOversampled.x;
                waveform.linewidth         = 3.0f;
                waveform.quality           = 1;
                waveform.method            = SampleMethod::sample_straight;
                waveform.audioId           = 0;

                curPath               = nextPathIdx;
                BakeGLPath& bakedPath = this->bakedPaths[this->nextPathIdx++];
                if (this->nextPathIdx >= this->bakedPaths.size()) {
                    this->nextPathIdx = 0;
                }
                audiosample_t sample;
                sample.nChannels     = 2;
                sample.nSamples      = nSampleSize;
                sample.bitsPerSample = 32;
                sample.sampleRate    = stream->getSampleRate();
                sample.samples.resize(2);
                for (int n = 0; n < 2; n++) {
                    auto& ch = sample.samples[n];
                    sample.samples[n].insert(ch.begin(), bufferProcessed[n].begin(), bufferProcessed[n].end());
                }
                SampleMethod method = SampleMethod::sample_straight;
                std::vector<std::vector<glm::vec2>> tesselatedWaveForms;
                tesselateWaveform(&sample, 0, 0, &waveform, method, tesselatedWaveForms);
                std::vector<path_t> paths;
                paths.reserve(tesselatedWaveForms.size());
                for (auto& waveform : tesselatedWaveForms) {
                    paths.push_back(path_t{std::move(waveform), bakeOpt});
                }
                curRenderer->bakePaths(paths, bakedPath);

                ptrBakedPath = &bakedPath;

            } else if (curPreset.mode == FFT_SPECTRUM) {
                flatCtrlPtsRender.clear();
                float aspect   = math::max(1.0f, w / (float) h);
                float totalLvl = 0;
                for (int band = 0; band < spectrum.numBands; band++) {
                    float levelRms = 0;
                    for (int ch = 0; ch < audio_spectrum::NUM_CHANNELS; ch++) {
                        levelRms += spectrum.bands[ch][band];
                    }
                    levelRms /= (float) audio_spectrum::NUM_CHANNELS;
                    totalLvl += levelRms;
                    double scale = levelRms;
                    vec2 v       = { (-1.0f + 2.0f * (band / (float) (spectrum.numBands - 1))) * aspect, 1.0f - 2.0f * scale };
                    flatCtrlPtsRender.push_back(v);
                }
                totalLvl /= (float) spectrum.numBands;
                double scaleTotal = 1.0 - totalLvl;

                bakeOpt.color  = { col.r, col.g, col.b, col.a };
                if (math::abs(curPreset.fCycleColor) > 0 && audioAnalyzer) {
                    auto hsl      = rgbToHSL(col.r, col.g, col.b);
                    float hue     = hsl.r + (scaleTotal + sinf(audioAnalyzer->processedTime * 0.5f)) * curPreset.fCycleColor;
                    auto newCol   = nvgHSL(hue, hsl.g, hsl.b);
                    bakeOpt.color = { newCol.r, newCol.g, newCol.b, col.a };
                }
                bakeOpt.linewidth += scaleTotal * 8.0;
                vecVecCtrlPtsRender.resize(1);
                vecVecCtrlPtsRender[0] = flatCtrlPtsRender;
                ptrBakedPath           = &getPath(vecVecCtrlPtsRender, bakeOpt, oversample);
            } else if (curPreset.mode == PATH_SCRIPT) {
                vecVecCtrlPtsRender.clear();
#ifdef PROFILE_PATHGEN
                size_t numCtrlPts = 0;
                static int a = 0;
                hires_timer_t timer;
#endif
#ifdef USE_PYTHON
                if (DAW::IsPythonInitialized()) {
                    if (StrStartsWith(this->curPreset.pythonFunc, "pathgen_fl32_")) {
                        try {
#ifdef PROFILE_PATHGEN
                            timer.reset();
#endif
                            vecVecCtrlPtsRaw = callPyFunction<decltype(vecVecCtrlPtsRaw)>(moduleNamePathGen, StringAsCStr(this->curPreset.pythonFunc), spectrum.bands, phaseCont);
#ifdef PROFILE_PATHGEN
                            auto tookB1      = timer.getTimeReset();
#endif
                            vecVecCtrlPtsRender.resize(vecVecCtrlPtsRaw.size());
                            auto itA = vecVecCtrlPtsRaw.cbegin();
                            auto itB = vecVecCtrlPtsRender.begin();
                            auto end = vecVecCtrlPtsRender.end();
                            for (; itB != end;) {
                                auto& vecIn  = *itA++;
                                auto& vecOut = *itB++;
                                vecOut.resize(vecIn.size() / 2);
                                auto itPt = vecIn.cbegin();
                                for (auto& vec : vecOut) {
                                    vec[0] = *itPt++;
                                    vec[1] = *itPt++;
                                }
                            }
                            // callPyFunction<void>(moduleNamePathGen, "testFastPath", spectrum.bands, phaseCont);

#ifdef PROFILE_PATHGEN
                            auto tookB2 = timer.getTimeReset();
                            if (a % 100 == 0) {
                                log_printf("tookC 1 %zd\n", tookB1);
                                log_printf("tookC 2 %zd\n", tookB2);
                            }
#endif
                        } catch (std::exception& e) {
                            vecVecCtrlPtsRender.resize(1);
                            vecVecCtrlPtsRender[0] = ctrlPts;
                            log_lf(Log::L_ERROR, "%s\n", e.what());
                        }
                    } else {
                        try {
                            vecVecCtrlPtsRender = callPyFunction<decltype(vecVecCtrlPtsRender)>(moduleNamePathGen, StringAsCStr(this->curPreset.pythonFunc), spectrum.bands, phaseCont);
#ifdef PROFILE_PATHGEN
                            for (auto& v : vecVecCtrlPtsRender)
                                numCtrlPts += v.size();
                            auto tookB = timer.getTimeReset();
                            if (a % 100 == 0) {
                                log_printf("tookB %zd (%zu pts)\n", tookB, numCtrlPts);
                            }
#endif
                        } catch (std::exception& e) {
                            vecVecCtrlPtsRender.resize(1);
                            vecVecCtrlPtsRender[0] = ctrlPts;
                            log_lf(Log::L_ERROR, "%s\n", e.what());
                        }
                    }
                } else {
                    vecVecCtrlPtsRender.resize(1);
                    vecVecCtrlPtsRender[0] = ctrlPts;
                }
#else
                    vecVecCtrlPtsRender.resize(1);
                    vecVecCtrlPtsRender[0] = ctrlPts;
#endif
                flatCtrlPtsRender.clear();
                // for (auto& v : vecVecCtrlPtsRender)
                //     addAll(flatCtrlPtsRender, v);
                ptrBakedPath = &getPath(vecVecCtrlPtsRender, bakeOpt, oversample);
#ifdef PROFILE_PATHGEN
                auto tookC = timer.getTimeReset();
                if (a%100==0) {
                  log_printf("tookD %zd\n", tookC);
                }
                a++;
#endif
            } else if (curPreset.mode == SIMD_COS_TEST) {
                vecVecCtrlPtsRender.resize(0);
                flatCtrlPtsRender.clear();
                ptrBakedPath      = &getPath(vecVecCtrlPtsRender, bakeOpt, oversample);
            } else {
                vecVecCtrlPtsRender.resize(1);
                vecVecCtrlPtsRender[0] = ctrlPts;
                if (!bPause) {
                    // modifyPointOperation(ctrlPtsCopy, OP_GEN_CIRCLE);
                    modifyPointOperation(vecVecCtrlPtsRender[0], OP_RAND_ALL);
                    modifyPointOperation(vecVecCtrlPtsRender[0], OP_RAND_EACH);
                    // modifyPointOperation(ctrlPtsCopy, OP_MULTIPLY_ALL);
                }
                flatCtrlPtsRender = vecVecCtrlPtsRender[0];
                ptrBakedPath      = &getPath(vecVecCtrlPtsRender, bakeOpt, oversample);
            }


            if (ptrBakedPath) {
                glEnable(GL_DEPTH_TEST);
                glClear(GL_DEPTH_BUFFER_BIT);
                glFrontFace(GL_CW);
                if (curPreset.blendMode == 0) {
                    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ONE, GL_ONE);
                } else {
                    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                }
                curRenderer->render(*ptrBakedPath, matProj, mat4x4(1.0), mat4x4(1.0));
                glFrontFace(GL_CCW);
                glDisable(GL_DEPTH_TEST);
            }
            FrameBuffer::unbindFramebuffer();
            auto fb0Texture = framebuffers[0]->colorAttTextures[0];

            // blur darkened image from tempFrameBuffer to blurFB
            blur_tex_shader::rendercontext_t blurCtxt;
            blurCtxt.w            = w;
            blurCtxt.h            = h;
            blurCtxt.timeAbs      = millisCont;
            blurCtxt.inputTexture = fb0Texture;
            blurCtxt.outputTexture = fb0Texture;
            pipeFbBlur->render(&blurCtxt, static_cast<blur_num_passes>(curPreset.nBlurPasses), 1.0f);

            glBindVertexArray(vbo.vaoId);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo.vboIdxId);

            framebuffers[1]->bindAndClear();
            // glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ONE, GL_ONE);

            // blit blurred image from blurFB using normal blend mode
            {
                auto pipeBloom = pipeShaderBloom.get();
                glUseProgram(pipeBloom->program);
                glBindTexture(GL_TEXTURE_2D, blurCtxt.outputTexture);
                pipeBloom->setUniforms(w, h, millisCont, curPreset.fBloomStrength);
                glUniformMatrix4fv(pipeBloom->u_mvp, 1, GL_FALSE, value_ptr(matProj));
                glDrawElements(GL_TRIANGLES, vbo.nIndices, GL_UNSIGNED_INT, nullptr);
            }


            // blit rendered image over blurred image using additive blend mode
            {
                auto pipeBlit = pipeShaderBlit.get();
                glUseProgram(pipeBlit->program);
                glBindTexture(GL_TEXTURE_2D, fb0Texture);
                pipeBlit->setUniforms(w, h, millisCont, curPreset.fFadeOutStrength);
                glUniformMatrix4fv(pipeBlit->u_mvp, 1, GL_FALSE, value_ptr(matProj));
                glDrawElements(GL_TRIANGLES, vbo.nIndices, GL_UNSIGNED_INT, nullptr);
            }

            if (curPreset.fBlurFeedbackStrength != 0.0f) {
                framebuffers[0]->bind();
                auto pipeBloom = pipeShaderBloom.get();
                glUseProgram(pipeBloom->program);
                glBindTexture(GL_TEXTURE_2D, blurCtxt.outputTexture);
                pipeBloom->setUniforms(w, h, millisCont, curPreset.fBlurFeedbackStrength);
                glUniformMatrix4fv(pipeBloom->u_mvp, 1, GL_FALSE, value_ptr(matProj));
                glDrawElements(GL_TRIANGLES, vbo.nIndices, GL_UNSIGNED_INT, nullptr);
            }
        }
        void prerender(NVGcontext* vg) override {
            if (!bEarlyInit) {
                earlyInit();
                bEarlyInit = true;
            }
            if (bIsIdle && ((daw&&daw->getAudioHost()&&daw->getAudioHost()->isStreaming())||optionalAudioHost)) {
                bIsIdle = false;
            }
            if (bIsIdle) {
                return;
            }
            if (!bInit) {
                init(daw->getHost()->getSampleFormatInternal());
                bInit = true;
            }
            if (bHasPresetToLoad) {
                bHasPresetToLoad = false;
                setPreset(this->presetToLoad);
                auto it = std::find_if(this->presets.begin(), this->presets.end(), [&](const settings_preset_t& preset) {
                    return preset.name == curPreset.name;
                });
                if (it != this->presets.end()) {
                    this->currentPresetIdx = static_cast<int32_t>(std::distance(this->presets.begin(), it));
                }
                bHasPresetToLoad = false;
            }
#ifdef PROFILE_PATHGEN
            static int a = 0;
            hires_timer_t timer;
#endif

            float fPhaseTick  = 0.0f;
            double tmSecondsD = getTimeSecondsD();
            phaseCont         = static_cast<float>(tmSecondsD);
            float millisCont  = static_cast<float>(tmSecondsD * 1000.0);
            phase01           = modff(phaseCont, &fPhaseTick);

            if (sizeView.x <= 0 || sizeView.y <= 0) {
                return;
            }

            ivec2 cs       = sizeView;
            int w          = cs.x * curPreset.nOversample;
            int h          = cs.y * curPreset.nOversample;

            dbgassert(vboFullscreenQuad.get());
            DrawVBO& vbo = *vboFullscreenQuad;
            glClearColor(0, 0, 0, 0);

            glViewport(0, 0, w, h);
            preGLState();

            if (vboSize.x != w || vboSize.y != h) {
                // allocate framebuffer here instead of layout() to assure we are in the right context
                // (gl context present, post startup)
                for (auto& fb : framebuffers) {
                    fb.reset();
                    fb = std::make_shared<FrameBuffer>(w, h, GL_RGBA16F, true, vec4(0.0f));
                    fb->setup();
                    fb->bindAndClear();
                }
                if (vg) {
                    if (nvgFramebuffer) {
                        nvgluDeleteFramebuffer(nvgFramebuffer);
                    }
                    nvgFramebuffer = nvgluCreateFramebuffer(vg, w, h, NVG_IMAGE_16BIT | NVG_IMAGE_PREMULTIPLIED);
                    nvgluBindFramebuffer(nullptr);
                }
                tess2d tess(0);
                tess2d::fullscreenQuad(tess, w, h);
                glBindVertexArray(vbo.vaoId);
                tess2d::uploadVBO(tess, vbo);
                bindVertexAttributes(pipeShaderBlit->attributes);
                glBindVertexArray(0);
                vboSize = { w, h };
            }
            if (bReinitBlurFB) {
                this->pipeFbBlur->setupFramebuffers(w, h, curPreset.nBlurDownsample);
                bReinitBlurFB = false;
            }
            if (iNextInitRenderer != -1) {
                curPreset.nRenderer = iNextInitRenderer;
                curRenderer         = pathRenderers[iNextInitRenderer];
                iNextInitRenderer   = -1;
                reinitPathRenderer();
            }

            // render path into fb0
            int nR        = 0;
            bool rendered = false;
            const bool renderAudio = curPreset.mode == FFT_SPECTRUM || curPreset.mode == PATH_SCRIPT;
            DAW::AudioIO::AudioStream* stream = nullptr;
            if (optionalAudioHost) {
                stream = optionalAudioHost->getStream(0);
            }
            constexpr static bool RENDER_ALL_FRAMES = false;
            auto processAudioBuffer = [&](AudioBuffer* bufInput) {
                if (bufInput->output->channels) {
                    audioAnalyzer->processBlock(bufInput->output, curPreset.fGain);
                    mixDbfsScaleBands(audioAnalyzer->analyzerLf.get(), audioAnalyzer->analyzerHf.get(), spectrum);
                    if (curPreset.mode == AUDIO_WAVEFORM) {
                        if (buffer.feed(bufInput->output, bufferProcessed)) {
                            waveformWindowOffset  = waveformWindowOffset2 - (WAVEFORM_BUFFER_SIZE / 2);
                            waveformWindowOffset2 = 0;
                        }
                    }
                }
                if (renderAudio && (!rendered || RENDER_ALL_FRAMES)) {
                    renderPass(w, h, curPreset.nOversample);
                    rendered = true;
                }
                nR++;
            };
            AudioBuffer* bufInput = nullptr;
            if (stream && audioAnalyzer) {
                while (stream->try_dequeueInput(bufInput)) {
                    processAudioBuffer(bufInput);
                    bufInput->inUse = false;
                    if (nR >= 3) 
                        break;
                }
            } else if (audioAnalyzer) {
                auto getFirstLatencyEffect = [&](DAW::Host::Host*) -> effectbase*
                {
                    std::vector<effectbase *> effects;
                    daw->getHost()->getAllInstances(effects);
                    for (auto e : effects) {
                        if (e->getModuleType() == PLUGIN_TYPE_VISUALIZER) {
                            return e;
                        }
                    }
                    return nullptr;
                };
                effectbase* eff = getFirstLatencyEffect(daw->getHost());
                if (eff) {
                    auto* moduleAudioVisualizer = static_cast<PluginVisualizer::module_visualizer*>(eff);
                    if (moduleAudioVisualizer->getOutputQueueSize() > 0) {
                        // auto lock = daw->lockPlayThread();
                        while (moduleAudioVisualizer->try_dequeue(bufInput)) {
                            processAudioBuffer(bufInput);
                            bufInput->inUse = false;
                            // if (nR >= 3) 
                            //     break;
                        }
                    }
                }
            }
            // log_printf("processed %d blocks\n", nR);
            if (!renderAudio || !rendered) {
                renderPass(w, h, curPreset.nOversample);
                rendered = true;
            }
            FrameBuffer::unbindFramebuffer();

            // blit blur+rendered into nvg context framebuffer using final shading
            if (rendered && vg)
            {
                // glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ONE, GL_ONE);
                nvgluBindFramebuffer(nvgFramebuffer);
                const mat4x4 matProj = glm::ortho(0.0f, (float) w, (float) h, 0.0f, -1.0f, 1.0f);
                glClearColor(0, 0, 0, 0);
                glClear(GL_COLOR_BUFFER_BIT);
                auto pipeBlitFinal = pipeShaderBlitFinal.get();
                glUseProgram(pipeBlitFinal->program);
                glBindTexture(GL_TEXTURE_2D, framebuffers[1]->colorAttTextures[0]);
                pipeBlitFinal->setUniforms(w, h, millisCont, curPreset.fFadeOutStrength);
                glUniformMatrix4fv(pipeBlitFinal->u_mvp, 1, GL_FALSE, value_ptr(matProj));
                glDrawElements(GL_TRIANGLES, vbo.nIndices, GL_UNSIGNED_INT, nullptr);
                nvgluBindFramebuffer(nullptr);
            }


            glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glBindVertexArray(0);

            checkGLError("postrender");

            if (vg) {

                for (auto* gui : guis) {
                    gui->prerender(vg);
                }
            }
#ifdef PROFILE_PATHGEN
            auto tprerender = timer.getTimeReset();
            if (a%100==0) {
                log_printf("prerender %zd\n", tprerender);
            }
            a++;
#endif
        }
        void render(NVGcontext* vg) override {
            if (!vg) {
                const ivec2 cs = sizeView;
                const int w    = cs.x * curPreset.nOversample;
                const int h    = cs.y * curPreset.nOversample;

                const float millisCont = phaseCont;
                const DrawVBO& vbo   = *vboFullscreenQuad;
                const mat4x4 matProj = glm::ortho(0.0f, (float) w, (float) h, 0.0f, -1.0f, 1.0f);
                auto pipeBlitFinal   = pipeShaderBlitFinal.get();
                glClearColor(0, 0, 0, 0);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
                glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ONE, GL_ONE);
                glBindVertexArray(vbo.vaoId);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo.vboIdxId);
                glUseProgram(pipeBlitFinal->program);
                glBindTexture(GL_TEXTURE_2D, framebuffers[1]->colorAttTextures[0]);
                pipeBlitFinal->setUniforms(w, h, millisCont, curPreset.fFadeOutStrength);
                glUniformMatrix4fv(pipeBlitFinal->u_mvp, 1, GL_FALSE, value_ptr(matProj));
                glDrawElements(GL_TRIANGLES, vbo.nIndices, GL_UNSIGNED_INT, nullptr);
                glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glBindVertexArray(0);
                return;
            }
            nFrame++;

            if (nFrame > 200) {
                nFrame = 0;
                // triggerReloadShaders = 1;
            }
            if (triggerReloadShaders) {
                triggerReloadShaders = 0;
                reloadShaders();
            }
            if (isBackgroundRendered()) {
                // renderBackground(vg);
                nvgBeginPath(vg);
                nvgRect(vg, pos.x, pos.y, size.x, size.y);
                nvgFillColor(vg, NVGcolor{ 0, 0, 0, 1 });
                nvgFill(vg);
            }
            if (!setScissorTransform(vg)) {
                return;
            }
            int w = sizeView.x;
            int h = sizeView.y;
            if (this->nvgFramebuffer)
            {
                NVGpaint img = nvgImagePattern(vg, INSET_OUTER, INSET_OUTER, w, h, 0, this->nvgFramebuffer->image, 1);
                nvgBeginPath(vg);
                nvgRect(vg, INSET_OUTER, INSET_OUTER, w, h);
                nvgFillPaint(vg, img);
                nvgFill(vg);
            }
            if (!vecAxisLabels.empty()) {
                for (auto& label : vecAxisLabels) {
                    renderTextLabel(vg,
                                    label.pos,
                                    vec2(size),
                                    label.label,
                                    theme,
                                    26,
                                    theme->getColor(GuiColor::COL_TEXT),
                                    NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
                }
            }
            if (fCtrAlpha <= 0.001f) {
                return;
            }
            nvgGlobalAlpha(vg, fCtrAlpha);

            if (!flatCtrlPtsRender.empty()) {

                nvgBeginPath(vg);
                for (vec2& pt : flatCtrlPtsRender) {
                    vec2 ptView = ctrlPtToScreen(pt);
                    nvgCircleFast(vg, ptView.x, ptView.y, 8);
                }
                nvgFillColor(vg, rgbaToNvg(0x66222222));
                nvgStrokeColor(vg, rgbaToNvg(0xffababab));
                nvgStrokeWidth(vg, 3.0f);
                nvgFill(vg);
                nvgStroke(vg);
            }
            auto cs = getSizeContent();
            if (exprError.length()) {
                vec2 pos = vec2(this->pos) + vec2(cs) * 0.5f;
                renderTextLabel(vg,
                                pos,
                                vec2(size),
                                exprError,
                                theme,
                                26,
                                theme->getColor(GuiColor::COL_TEXT),
                                NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            }
            setFont(vg, 26, THEMECOL_TEXT, NVG_ALIGN_BOTTOM | NVG_ALIGN_LEFT);
            float lineh = 0.0f;
            nvgTextMetrics(vg, nullptr, nullptr, &lineh);
            if (audioAnalyzer) {
                nvgText(vg, 5, cs.y - lineh - 4, StringAsCStr(StringFormat("Blocks processed HF: %d LF: %d", audioAnalyzer->analyzerHf->blocksProcessed, audioAnalyzer->analyzerLf->blocksProcessed)), nullptr);
            }
            for (auto* gui : guis) {
                if (gui->isVisible()) {
                    nvgSave(vg);
                    gui->render(vg);
                    nvgRestore(vg);
                }
            }
        }
        void buttonClicked(guibase* button) override {
            if (button && button->parent == &functionBrowser) {
                if (button == &functionBrowser.buttonReload) {
                    reloadPythonModule();
                    return;
                }
            }
            if (button && button->parent == &functionBrowser.list) {
                auto fnEntry = dynamic_cast<guictr_functions::guictr_functions_entry*>(button);
                setPythonFunction(fnEntry->functionName);
                return;
            }
            if (button && button->parent == &presetBrowser.list) {
                auto presetBtn = dynamic_cast<guictr_presets::gui_linetess_preset_list_entry*>(button);
                auto& preset   = this->presets.at(presetBtn->presetIdx);
                setPreset(preset);
                this->currentPresetIdx = presetBtn->presetIdx;
                return;
            }
            if (button && button->parent == &presetBrowser) {
                bool triggerSave = false;
                if (button == &presetBrowser.buttonAdd) {
                    String s = presetBrowser.textFieldName.value();
                    if (!s.length()) {
                        s = StringFormat("Preset %zu", presets.size()+1);
                    }
                    if (s.length()) {
                        presetBrowser.textFieldName.setValue("");
                        this->curPreset.name = s;
                        settings_preset_t preset = this->curPreset;
                        presets.push_back(preset);
                        presetBrowser.setPresets(presets);
                        triggerSave = true;
                    }
                }
                if (button == &presetBrowser.buttonRemove) {
                    int32_t idx = presetBrowser.list.getSelectedIdx();
                    if (idx > -1) {
                        if (idx < CtrSize(presets)) {
                            presets.erase(presets.begin() + idx);
                            presetBrowser.setPresets(presets);
                        }
                    }
                }
                if (button == &presetBrowser.buttonSave) {
                    triggerSave = true;
                }
                if (triggerSave) {
                    try {
                        for (auto& preset : this->presets) {
                            if (this->curPreset.name == preset.name) {
                                preset = this->curPreset;
                                break;
                            }
                        }
                        settings_t settings1{ this->currentPresetIdx, this->presets };
                        CreateDirectoryIfNotExists(App::Platform::toUserdataPath("presets/Visualizer"));
                        auto path = App::Platform::toUserdataPath("presets/Visualizer/presets.json");
                        saveSettings(settings1, path);
                    } catch (std::exception& e) {
                        log_lf(Log::L_ERROR, "Exception: %s\n", e.what());
                    }
                }
            }
        }
        bool handleKeyInput(KeyEvent& kevt) override {
            if (kevt.type != KeyboardState::K_RELEASE && audioAnalyzer) {
                auto key         = kevt.keyCode;
                auto& analyzerLf = audioAnalyzer->analyzerLf;
                auto& analyzerHf = audioAnalyzer->analyzerHf;
                int newNumBands  = curPreset.nBands;
                if (key == KeyboardKey::DAW_KB_1) {
                    newNumBands = 3;
                }
                if (key == KeyboardKey::DAW_KB_2) {
                    newNumBands = 16;
                }
                if (key == KeyboardKey::DAW_KB_3) {
                    newNumBands = 32;
                }
                if (key == KeyboardKey::DAW_KB_4) {
                    newNumBands = 64;
                }
                if (key == KeyboardKey::DAW_KB_W || key == KeyboardKey::DAW_KB_S) {
                    if (analyzerHf) {
                        int incr    = key == KeyboardKey::DAW_KB_S ? -1 : 1;
                        newNumBands = std::max<int>(1, analyzerHf->numBands + incr);
                    }
                }
                curPreset.nBands = newNumBands;
                if (curPreset.nBands != analyzerHf->numBands) {
                    analyzerHf->setNumBands(newNumBands);
                    analyzerLf->setNumBands(newNumBands);
                    analyzerHf->updateBands();
                    analyzerLf->updateBands();
                    spectrum = *audioAnalyzer->analyzerLf;
                }
            }
            if (kevt.type == K_PRESS) {
                if (isKC(KeyCombo{ 0, KeyboardKey::DAW_KB_SPACE }, kevt)) {
                    bPause = !bPause;
                    return true;
                }
                if (isKC(KeyCombo{ 0, KeyboardKey::DAW_KB_DELETE }, kevt)) {
                    clearFrame = 1;
                    return true;
                }
                if (isKC(KeyCombo{ 0, KeyboardKey::DAW_KB_BACKSPACE }, kevt)) {
                    clearFrame = 2;
                    return true;
                }
                if (isKC(KeyCombo{ 0, KeyboardKey::DAW_KB_F5 }, kevt)) {
                    triggerReloadShaders = 1;
                    return true;
                }
            }
            return false;
        }
        vec2 lastMousePos = vec2(0);

        bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
            if (this->contains(mpos)) {
                lastMousePos = this->toContainerSpace(mpos);
            }
            return guictr_base::mouseHitTest(mpos, evt);
        }
        void handleRightClick(MouseEvent& evt) override {
            if (hasControlHandles()) {
                lastMousePos = evt.relMousepos;
                vec2 local = screenToCtrl(evt.relMousepos);
                int minPt  = getMinPt(local);
                if (minPt > -1) {
                    ctrlPts.erase(ctrlPts.begin() + minPt);
                }
            }
        }
        void handleDraggedBegin(MouseEvent& evt) override {

            if (hasControlHandles()) {
                vec2 local   = screenToCtrl(evt.relMousepos);
                ptDraggedIdx = getMinPt(local);
                if (evt.type == MouseEventType::M_EVT_DOUBLECLICK) {
                    ctrlPts.push_back(local);
                }
            }
        }

        void handleDraggedMove(MouseEvent& evt) override {
            lastMousePos = evt.relMousepos;
            if (hasControlHandles()) {
                if (ptDraggedIdx > -1 && ptDraggedIdx < CtrSize(ctrlPts)) {
                    log_lf(Log::L_DEBUG, "mousepos %d %d\n", evt.relMousepos.x, evt.relMousepos.y);
                    vec2 local = screenToCtrl(evt.relMousepos);
                    log_lf(Log::L_DEBUG, "screenToCtrl %f %f\n", local.x, local.y);
                    auto& pt   = ctrlPts[ptDraggedIdx];
                    pt         = local;
                }
            }
        }
        void handleDraggedRelease(MouseEvent& evt) override {
            if (hasControlHandles()) {
                if (ptDraggedIdx >= 0 && ptDraggedIdx < CtrSize(ctrlPts)) {

                    auto& pt = ctrlPts[ptDraggedIdx];
                    pt.x     = math::clamp(pt.x, -1.0f, 1.0f);
                    pt.y     = math::clamp(pt.y, -1.0f, 1.0f);
                }
                ptDraggedIdx = -1;
            }
        }
        void layout() override {

            int32_t h          = 24;
            ivec2 sizeFields   = { h * 8, h };
            ivec2 sizeControls = { h * 8, h };

            std::array<guibase*, 6> controlsMode = { &dropdownRenderer, &dropdownMode, &dropdownSplineType, &dropdownBlendMode, &dropdownEdit, &textFieldExpression };
            std::array<guibase*, 2> controlsBlur = { &dropdownBlurDownsample, &dropdownBlurPasses };
            std::vector<guibase*> allControls;

            allControls.reserve(controlsMode.size() + controlsParameters.size());
            allControls.insert(allControls.end(), controlsMode.begin(), controlsMode.end());
            allControls.insert(allControls.end(), controlsBlur.begin(), controlsBlur.end());
            allControls.insert(allControls.end(), controlsParameters.begin(), controlsParameters.end());

            for (auto* input : controlsMode) {
                input->size = sizeControls;
            }
            for (auto* input : controlsBlur) {
                input->size = sizeControls;
            }
            for (auto* input : controlsParameters) {
                input->size = sizeFields;
            }
            ivec2 inputPos = { 0, 0 };
            for (auto* input : allControls) {
                input->pos = inputPos;
                if (input->isVisible()) {
                    inputPos.y += input->size.y + 5;
                }
            }

            ivec2 cs          = getSizeContent();
            if (functionBrowser.isVisible()) {
                presetBrowser.size = { cs.x / 6, cs.y * 1 / 3 };
            } else {
                presetBrowser.size = { cs.x / 6, cs.y * 2 / 3 };
            }
            presetBrowser.pos = { cs.x - presetBrowser.size.x, 0 };
            functionBrowser.size = { cs.x / 6, cs.y * 1 / 3 };
            functionBrowser.pos  = { cs.x - presetBrowser.size.x, presetBrowser.bottom() + 5 };

            colorPick.size = { presetBrowser.size.x, presetBrowser.size.x/2 };
            colorPick.pos  = { cs.x - colorPick.size.x, cs.y - colorPick.size.y };
            sizeView       = cs - INSET_OUTER * 2;
            sizeViewInner  = sizeView - INSET_INNER * 2;
            float aspectX  = sizeViewInner.x / (float) sizeViewInner.y;
            aspectView     = vec2(aspectX < 1.0f ? 1.0f : sizeViewInner.y / (float) sizeViewInner.x, aspectX < 1.0f ? aspectX : 1.0f);

            for (auto* gui : guis) {
                gui->layout();
            }
            if (lastCs != cs) {
                lastCs = cs;
                resizeFramebuffers();
            }
        }
        void setColorSelector() {
            colorPick.setU32(curPreset.rgba);
            colorPick.setRefU32(&curPreset.rgba);
        }
        void setControl(BaseCtrl* parentCtrl) override {
            guictr_base::setControl(parentCtrl);
            setColorSelector();
        }

    private:
        vec2 toScaledPt(const vec2& ctrlPt) {
            return ((ctrlPt*vec2(1.0f, -1.0f) * aspectView) * 0.5f + 0.5f);
        }
        vec2 ctrlPtToView(const vec2& ctrlPt) {
            auto scaledPt = toScaledPt(ctrlPt);//(vec2(ctrlPt) * aspectView) + 1.0f;
            return vec2(sizeViewInner) * scaledPt + vec2(INSET_INNER);
        }
        vec2 ctrlPtToScreen(const vec2& ctrlPt) {
            return ctrlPtToView(ctrlPt) + vec2(INSET_OUTER);
        }
        vec2 viewToCtrlPt(const vec2& pt) {
            vec2 relPt  = pt - vec2(INSET_INNER);
            vec2 ctrlPt = vec2(relPt / vec2(sizeViewInner) - 0.5f) * 2.0f / aspectView;
            ctrlPt *= vec2(1.0f, -1.0f);
            return ctrlPt;
        }
        vec2 screenToCtrl(const vec2& pt) {
            return viewToCtrlPt(pt - vec2(INSET_OUTER));
        }
        int getMinPt(vec2 local, float minCollisionDistance = 32.0f) {
            int minIdx     = -1;
            double minDist = 0;
            const auto ctrSize = CtrSize(ctrlPts);
            for (int i = 0; i < ctrSize; i++) {
                auto dist = glm::distance(local, ctrlPts[i]);
                if (minIdx < 0 || minDist > dist) {
                    minIdx  = i;
                    minDist = dist;
                }
            }
            if (minIdx > -1) {
                auto v = glm::distance(ctrlPtToScreen(ctrlPts[minIdx]), ctrlPtToScreen(local));
                if (v > minCollisionDistance) {
                    return -1;
                }
            }
            return minIdx;
        }
    };
}// namespace lineplot

CEREAL_CLASS_VERSION(lineplot::settings_preset_t, 3);

namespace DAW::UI {

guictr_base* MakeAudioVisualizer(DawInstance* daw) {
    auto ctr = new lineplot::guictr_audiovis(daw);
    return ctr;
}

} // namespace DAW::UI


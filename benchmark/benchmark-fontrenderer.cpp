#include "color_util.h"
#include "glheaders.h"
#include "hires_timer.h"
#include "math/seq_math.h"
#include "math/vec.h"
#include <GLFW/glfw3.h>
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif
#include <nanovg.h>
#include <nanovg_gl.h>
#include <benchmark/benchmark.h>
#include <array>
#include "types.h"
#include <cstdio>
#include <functional>
#include <memory>
#ifdef _WIN32
#include <windows.h>
#endif
#include "logging.h"
#include "str_util.h"
#include "exceptions.h"
#include "platform.h"
#include "renderresources.h"
#include "mousecursor.h"
#include "guifonts.h"


namespace RenderResources {
    void initResources(NVGcontext* vg);// renderresources.cpp
}

namespace MouseCursors {
    void initCursors();// mousecursor.cpp
}

extern volatile bool fatalError;

static void glfw_error_callback(int error, const char* description) {
    log_printf("glfw-error %d: %s\n", error, description);
}
struct membuf : std::streambuf
{
    template<typename T>
    membuf(T* data, size_t len) {
        auto const begin = reinterpret_cast<char*>(data);
        auto const end = reinterpret_cast<char*>(data + len);
        this->setg(begin, begin, end);
    }
};

static void loadStrings(std::vector<String>& strings) {
    const auto filepath = TEST_PATH("word_dict.txt");
    try {
        std::vector<uint8_t> vec;
        ReadFileVector(filepath, vec);
        if (vec.empty()) {
            log_printf("%s is empty\n", filepath);
            return;
        }
        membuf sbuf(vec.data(), vec.size());
        std::istream in(&sbuf);
        std::string line;
        while (strings.size() < 25 && std::getline(in, line)) {
            strings.push_back(line);
        }
        while (true) {
            String concat;
            if (!std::getline(in, line)) break;
            concat += line;
            concat += " ";
            if (!std::getline(in, line)) break;
            concat += line;
            concat += " ";
            if (!std::getline(in, line)) break;
            concat += line;
            strings.push_back(concat);
        }
    } catch (const std::exception& e) {
        log_printf("While reading %s: %s\n", filepath, e.what());
    }
}

int main(int argc, char** argv) {
    std::vector<String> args(&argv[0], &argv[argc]);
    App::Platform::initPlatformEnvironment(BuildInfo::PRODUCT_NAME_LOWER);
    try {
        glfwSetErrorCallback(glfw_error_callback);
        glfwInitHint(GLFW_CONTEXT_KEEPCURRENT, 1);
        if (!glfwInit()) {
            exit(EXIT_FAILURE);
        }
        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifndef NDEBUG
        // debug context
        glfwWindowHint(GLFW_CONTEXT_DEBUG, GL_TRUE);
#else
        glfwWindowHint(GLFW_CONTEXT_DEBUG, GL_FALSE);
#endif
        glfwWindowHint(GLFW_VISIBLE, 1);

        glfwWindowHint(GLFW_SAMPLES, 0);
        glfwWindowHint(GLFW_STENCIL_BITS, 8);
        glfwWindowHint(GLFW_DEPTH_BITS, 24);
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
        auto glfw = glfwCreateWindow(1400, 700, "Benchmark", nullptr, nullptr);
        if (!glfw)
            throw appexception("Couldn't create window");
        glfwMakeContextCurrent(glfw);
        if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
            throw appexception("Required OpenGL extensions not present.\nConsider updating graphics drivers");
        }

        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
        glEnable(GL_BLEND);
        glEnable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_STENCIL_TEST);
        glDisable(GL_MULTISAMPLE);
        glDepthFunc(GL_LEQUAL);
        glClearColor(0, 0, 0, 0);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBindVertexArray(0);
        glActiveTexture(GL_TEXTURE0);
        glfwSwapInterval(-1);
        int flags = NVG_ANTIALIAS;
#ifndef NDEBUG
        flags |= NVG_DEBUG;
#endif
        auto vg = nvgCreateGL3(flags);
        if (!vg) {
            throw appexception("Couldn't initialize nanovg");
        }
        nvgShapeAntiAlias(vg, USE_NANOVG_AA);
        RenderResources::initResources(vg);
        
        UIFont::font_instance fontInstance{"Roboto-Regular.ttf", -1};

        int winwidth = 0, winheight = 0;
        int fbwidth = 0, fbheight = 0;
        glfwGetWindowSize(glfw, &winwidth, &winheight);
        glfwGetFramebufferSize(glfw, &fbwidth, &fbheight);
        float pxratio = fbwidth / (float) winwidth;
        nvgBeginFrame(vg, winwidth, winheight, pxratio);
        glViewport(0, 0, fbwidth, fbheight);

        const float fSize = 48.0f;
        ivec2 size(fbwidth, fbheight);
        ivec2 strPos = size/2;
        static const auto str = "Test String Benchmark";

        std::vector<String> strings;
        loadStrings(strings);
        hires_timer_t timer;
        auto tmHrNow = timer.getTime();
        auto tmHrBegin = tmHrNow;
        while((tmHrNow - tmHrBegin) < 5'000'000LL) {
            tmHrNow = timer.getTime();
            glfwWaitEventsTimeout(0.001);
            if (glfwWindowShouldClose(glfw)) {
                break;
            }
            double tmMs = static_cast<double>((tmHrNow - tmHrBegin)/100)/10.0;
            size_t idx = math::clamp<size_t>(strings.size()/2+math::rounddS64(tmMs/100.0), 0U, strings.size()-1);
            float angle = sin(tmMs/800.0)*M_PI*0.5;
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
            nvgBeginPath(vg);
            nvgRect(vg, 0, 0, size.x, size.y);
            nvgFillColor(vg, rgbToNvg(0x1f071f));
            nvgFill(vg);
            nvgSave(vg);
            nvgTranslate(vg, size.x * 0.5f, size.y * 0.5f);
            nvgRotate(vg, angle);
            nvgTranslate(vg, size.x * -0.5f, size.y * -0.5f);
            UIFont::bindFont(vg, fontInstance);
            nvgFontSize(vg, fSize);
            nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            nvgFillColor(vg, rgbToNvg(0xffffff));
            nvgText(vg, strPos.x, strPos.y, StringAsCStr(strings[idx]), nullptr);
            nvgRestore(vg);
            nvgEndFrame(vg);
            glfwSwapBuffers(glfw);
        }


        UIFont::bindFont(vg, fontInstance);
        nvgFontSize(vg, fSize);
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, rgbToNvg(0xffffff));
        const float maxWidth = size.x*0.5f;

        benchmark::RegisterBenchmark("nvgTextBreakLines Advance Kerning Enabled", [&](benchmark::State& state) {
          nvgFontKerningAdvance(vg, 1);
	        NVGtextRow rows[2]{};
            size_t idx = 0;
            for (auto _ : state) {
                nvgTextBreakLines(vg, StringAsCStr(strings[idx++]), nullptr, maxWidth, rows, 2);
                if (idx >= strings.size()) {
                    idx = 0;
                }
            };
        });
        benchmark::RegisterBenchmark("nvgTextBreakLines Advance Kerning Disabled", [&](benchmark::State& state) {
          nvgFontKerningAdvance(vg, 0);
	        NVGtextRow rows[2]{};
            size_t idx = 0;
            for (auto _ : state) {
                nvgTextBreakLines(vg, StringAsCStr(strings[idx++]), nullptr, maxWidth, rows, 2);
                if (idx >= strings.size()) {
                    idx = 0;
                }
            };
        });
        benchmark::RegisterBenchmark("glfwMakeContextCurrent", [&](benchmark::State& state) {
            for (auto _ : state) {
                glfwMakeContextCurrent(nullptr);
                glfwMakeContextCurrent(glfw);
            };
        });

        benchmark::RegisterBenchmark("glfwSwapBuffers", [&](benchmark::State& state) {
            for (auto _ : state) {
                glfwSwapBuffers(glfw);
            };
        });

        benchmark::Initialize(&argc, argv);
        if (::benchmark::ReportUnrecognizedArguments(argc, argv))
            return 1;
        benchmark::RunSpecifiedBenchmarks();
        benchmark::Shutdown();
        glfwDestroyWindow(glfw);
    } catch (std::exception& e) {
        log_lf(Log::L_ERROR, "exception %s\n", e.what());
    }
    return 0;
}

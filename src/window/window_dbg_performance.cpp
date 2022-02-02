#include "glheaders.h"
#include <nanovg.h>
#include <nanovg_gl.h>
#include <nanovg_gl_utils.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <utility>
#include "math/vec.h"
#include "math/mat.h"
#include "fileio.h"
#include "str_util.h"
#include "gl/gl_util.h"
#include "gl/gl_attr.h"
#include "gl/gl_vbo.h"
#include "gl/gl_tess2d.h"
#include "renderresources.h"
#include "guicolors.h"
#include "color_util.h"
#include "rand.h"
#include "platform.h"
#include "host/profiling_impl.h"

namespace windowdebug_performance {
#define DBG_PERF_HIST_SIZE 512
#define DBG_PERF_INSTANCE_CHANNELS 32
    GLuint program2dTexture = 0;
    GLint u_mvp             = 0;
    GLint u_renderColor     = 0;
    GLint u_renderInfo      = 0;
    int64_t tmLastUpdate    = 0;
    int64_t tmLastReload    = 0;
    float quadSize          = 1024;
    seq_rand rnd;
    const int texW = DBG_PERF_HIST_SIZE;
    std::vector<float> texData;
    int nCall = 0;
    std::vector<VertexAttr> attributes{
        { "in_position", 2, GL_FLOAT },
        { "in_texcoord", 2, GL_FLOAT },
    };
    DrawVBO vbo;
    int loadShader() {
        tmLastReload = getTimeMillis();
        String srcVertex;
        String srcFragment;
        int64_t ret = ReadFileText("textured.vsh", srcVertex);
        if (ret <= 0) {
            printf("Cannot read file shader.vert\n");
            return 1;
        }
        ret = ReadFileText("perfgraph.fsh", srcFragment);
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
        u_mvp         = glGetUniformLocation(program, "mvp");
        u_renderInfo  = glGetUniformLocation(program, "renderInfo");
        u_renderColor = glGetUniformLocation(program, "renderColor");
        glUniform1i(glGetUniformLocation(program, "tex0"), 0);
        for (auto & attribute : attributes) {
            attribute.bindingPt = glGetAttribLocation(program, attribute.name);
        }
        checkGLError("glGetAttribLocation");
        glDeleteProgram(program2dTexture);
        program2dTexture = program;
        return 0;
    }
    GLuint generateTexture() {
        GLuint tex0 = 0;
        glGenTextures(1, &tex0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex0);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        checkGLError("glTexImage2D");
        glBindTexture(GL_TEXTURE_2D, 0);
        return tex0;
    }


    using ImgData = std::shared_ptr<uint8_t>;

    void setColor(uint8_t* b, int i) {
        b[0] = i & 0xFF;
        i    = i >> 8;
        b[1] = i & 0xFF;
        i    = i >> 8;
        b[2] = i & 0xFF;
        i    = i >> 8;
        b[3] = i & 0xFF;
    };
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
    //RenderResources::NvgImageTexture imgQuad;


    struct ProfilingDataChannelBase {
        int32_t texChannel = -1;
        String name;
        String unit;
        int64_t valueLast  = 0;
        int64_t valueMax   = 0;
        int64_t valueMin   = 0;
        int64_t valueAvg   = 0;
        std::array<float, DBG_PERF_HIST_SIZE> valuesRaw{};
        std::array<float, DBG_PERF_HIST_SIZE> valuesNormalized{};
        vec2 graphPos{};
        vec2 graphSize{};
    };
    void setChannel(ProfilingDataChannelBase* ch, String name, String unit, int32_t texChannel) {
        dbgassert(ch->valuesNormalized.size() == DBG_PERF_HIST_SIZE);
        ch->name       = std::move(name);
        ch->unit       = std::move(unit);
        ch->texChannel = texChannel;
    }
    void normalizeData(ProfilingDataChannelBase* ch) {
        dbgassert(ch->valuesRaw.size() == DBG_PERF_HIST_SIZE);
        dbgassert(ch->valuesNormalized.size() == DBG_PERF_HIST_SIZE);
        const float* rawData  = &ch->valuesRaw[0];
        const float minFl     = 0;
        const float maxFl     = math::max(1.0, ch->valueMax * 0.2 + ch->valueAvg * 0.8);
        const float sc        = (maxFl - minFl);
        float* normalizedData = &ch->valuesNormalized[0];
        if (sc < 1.0f / 1024.0f) {
            for (int i = 0; i < DBG_PERF_HIST_SIZE; ++i) {
                *normalizedData++ = 0.5f;
            }
            return;
        }
        for (int i = 0; i < DBG_PERF_HIST_SIZE; ++i) {
            *normalizedData++ = (*rawData++ - minFl) / sc;
            //dbgassert(*(normalizedData-1) <= 1.0f);
        }
    }
    void setSample(ProfilingDataChannelBase* ch, int32_t pos, int64_t value) {
        dbgassert(ch->valuesRaw.size() == DBG_PERF_HIST_SIZE);
        if (pos + 1 == DBG_PERF_HIST_SIZE) {
            ch->valueMax  = value;
            ch->valueMin  = value;
            ch->valueAvg  = value;
            ch->valueLast = value;
        } else {
            ch->valueMax = math::max(ch->valueMax, value);
            ch->valueMin = math::min(ch->valueMin, value);
            ch->valueAvg += value;
        }
        dbgassert(pos >= 0 && pos < ch->valuesRaw.size());
        dbgassert(pos >= 0 && pos < ch->valuesNormalized.size());
        ch->valuesRaw[pos]        = value;
        ch->valuesNormalized[pos] = value;
        if (pos == 0) {
            ch->valueAvg /= DBG_PERF_HIST_SIZE;
        }
    }
    struct ProfilingDataRenderInstance {
        void* instancePtr = nullptr;
        std::vector<std::shared_ptr<ProfilingDataChannelBase>> channels;
        int32_t lastWriteIdx = -1;
        GLuint tex0          = 0;
        std::vector<ProfilingDataChannelBase*> allChannels;
        vec2 instancePos{};
        String name;
        int32_t nextFreeChannelIdx = 0;
    };
    std::vector<ProfilingDataRenderInstance> instancesRenderStats;
    void updateProfilingData() {
        using namespace ProfilingImpl;
        profiled_instances<render_stats_t>* renderWindowStatsVec;
        profilingGetData(&renderWindowStatsVec);
        for (auto& renderWindowStats : *renderWindowStatsVec) {
            ProfilingDataRenderInstance* windowInstance = nullptr;
            for (ProfilingDataRenderInstance& prevChannel : instancesRenderStats) {
                if (prevChannel.instancePtr == renderWindowStats.instancePtr) {
                    windowInstance = &prevChannel;
                }
            }
            bool isFirstInvocation = false;
            if (!windowInstance) {
                instancesRenderStats.push_back(ProfilingDataRenderInstance{});
                windowInstance              = &instancesRenderStats.back();
                windowInstance->instancePtr = renderWindowStats.instancePtr;
                windowInstance->tex0        = generateTexture();
                windowInstance->name        = renderWindowStats.name;
                isFirstInvocation           = true;
            }
            auto& statsArray            = renderWindowStats.stats;
            const int32_t statsArrayLen = statsArray.size();

            auto& chs = windowInstance->channels;
            if (chs.empty()) {
                chs.resize(8);
                for (auto& ch : chs) {
                    ch = std::make_shared<ProfilingDataChannelBase>();
                }
                setChannel(chs[0].get(), "tm ctrl::render", "us", windowInstance->nextFreeChannelIdx++);
                setChannel(chs[1].get(), "tm ctrl::prerender", "us", windowInstance->nextFreeChannelIdx++);
                setChannel(chs[2].get(), "tm editor::render", "us", windowInstance->nextFreeChannelIdx++);
                setChannel(chs[3].get(), "tm track_controls::render", "us", windowInstance->nextFreeChannelIdx++);
                setChannel(chs[4].get(), "tm waveforms::update", "us", windowInstance->nextFreeChannelIdx++);
                setChannel(chs[5].get(), "# waveforms updates", " ", windowInstance->nextFreeChannelIdx++);
                setChannel(chs[6].get(), "# notes rendered", " ", windowInstance->nextFreeChannelIdx++);
                setChannel(chs[7].get(), "# clips rendered", " ", windowInstance->nextFreeChannelIdx++);
            }

            //step thru time backwards
            int32_t idx      = renderWindowStats.writeIdx - 1;
            int32_t dataSize = math::min<int32_t>(texW, statsArrayLen);
            for (; dataSize && idx < statsArrayLen;) {
                if (idx < 0) {
                    idx = statsArray.size() - 1;
                }
                setSample(chs[0].get(), dataSize - 1, statsArray[idx].stats.timeRender);
                setSample(chs[1].get(), dataSize - 1, statsArray[idx].stats.timePrerender);
                setSample(chs[2].get(), dataSize - 1, statsArray[idx].stats.timeRenderEditor);
                setSample(chs[3].get(), dataSize - 1, statsArray[idx].stats.timeRenderTrackControls);
                setSample(chs[4].get(), dataSize - 1, statsArray[idx].stats.timeUpdateWaveforms);
                setSample(chs[5].get(), dataSize - 1, statsArray[idx].stats.numWaveFormsRendered);
                setSample(chs[6].get(), dataSize - 1, statsArray[idx].stats.notesRendered);
                setSample(chs[7].get(), dataSize - 1, statsArray[idx].stats.clipsRendered);


                idx--;
                dataSize--;
            }
            for (auto& channel : chs) {
                normalizeData(channel.get());
            }
            texData.resize(texW * texW);
            memset(texData.data(), 0, sizeof(float) * texData.size());
            for (int j = 0; j < texW; ++j) {
                texData[j] = 0.5f;
            }
            for (auto& channel : chs) {
                memcpy(&texData[channel->texChannel * texW], channel->valuesNormalized.data(), sizeof(float) * texW);
                //if (i % 2 == 1) {
                //    for (int j = 0; j < texW; ++j) texData[i * texW + j] = 0.5f;
                //}
                //if (i % 4 == 3) {
                //    for (int j = 0; j < texW; ++j) texData[i * texW + j] = 0.75f;
                //}
                //if (i == 0) {
                //    for (int j = 0; j < texW; ++j) texData[i * texW + j] = 0.25f;
                //}
                //i++;
            }
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, windowInstance->tex0);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, texW, texW, 0, GL_RED, GL_FLOAT, texData.data());
        }
        profiled_instances<application_stats_t>* appStatsVec;
        profilingGetData(&appStatsVec);
        for (auto& appStats : *appStatsVec) {
            ProfilingDataRenderInstance* windowInstance = nullptr;
            for (ProfilingDataRenderInstance& prevChannel : instancesRenderStats) {
                if (prevChannel.instancePtr == appStats.instancePtr) {
                    windowInstance = &prevChannel;
                }
            }
            bool isFirstInvocation = false;
            if (!windowInstance) {
                instancesRenderStats.push_back(ProfilingDataRenderInstance{});
                windowInstance              = &instancesRenderStats.back();
                windowInstance->instancePtr = appStats.instancePtr;
                windowInstance->tex0        = generateTexture();
                windowInstance->name        = appStats.name;
                isFirstInvocation           = true;
            }
            auto& statsArray            = appStats.stats;
            const int32_t statsArrayLen = statsArray.size();

            auto& chs = windowInstance->channels;
            if (chs.empty()) {
                chs.resize(4);
                for (auto& ch : chs) {
                    ch = std::make_shared<ProfilingDataChannelBase>();
                }
                setChannel(chs[0].get(), "tm tick delay", "us", windowInstance->nextFreeChannelIdx++);
                setChannel(chs[1].get(), "#window msgs", "msgs", windowInstance->nextFreeChannelIdx++);
                setChannel(chs[2].get(), "#WM_PAINT msgs", "msgs", windowInstance->nextFreeChannelIdx++);
                setChannel(chs[3].get(), "#redraw req", "req", windowInstance->nextFreeChannelIdx++);
            }

            //step thru time backwards
            int32_t idx      = appStats.writeIdx - 1;
            int32_t dataSize = math::min<int32_t>(texW, statsArrayLen);
            for (; dataSize && idx < statsArrayLen;) {
                if (idx < 0) {
                    idx = statsArray.size() - 1;
                }
                setSample(chs[0].get(), dataSize - 1, statsArray[idx].stats.tickTimerDelay);
                setSample(chs[1].get(), dataSize - 1, statsArray[idx].stats.numMessagesProcessed);
                setSample(chs[2].get(), dataSize - 1, statsArray[idx].stats.numMessagesWmPaint);
                setSample(chs[3].get(), dataSize - 1, statsArray[idx].stats.numRedrawReq);


                idx--;
                dataSize--;
            }
            for (auto& channel : chs) {
                normalizeData(channel.get());
            }
            texData.resize(texW * texW);
            memset(texData.data(), 0, sizeof(float) * texData.size());
            for (int j = 0; j < texW; ++j) {
                texData[j] = 0.5f;
            }
            for (auto& channel : chs) {
                memcpy(&texData[channel->texChannel * texW], channel->valuesNormalized.data(), sizeof(float) * texW);
                //if (i % 2 == 1) {
                //    for (int j = 0; j < texW; ++j) texData[i * texW + j] = 0.5f;
                //}
                //if (i % 4 == 3) {
                //    for (int j = 0; j < texW; ++j) texData[i * texW + j] = 0.75f;
                //}
                //if (i == 0) {
                //    for (int j = 0; j < texW; ++j) texData[i * texW + j] = 0.25f;
                //}
                //i++;
            }
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, windowInstance->tex0);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, texW, texW, 0, GL_RED, GL_FLOAT, texData.data());
        }
    }
}// namespace windowdebug_performance

using namespace windowdebug_performance;

int initDebugWindowPerformance(NVGcontext* vg) {
    glBindVertexArray(0);
    int ret = loadShader();
    if (ret)
        return ret;
    tess2d tess;
    tess.add(quadSize, 0.0f, 1, 1);
    tess.add(0.0f, 0.0f, 0, 1);
    tess.add(0.0f, quadSize, 0, 0);
    tess.add(quadSize, quadSize, 1, 0);
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
void drawDebugWindowPerformance(NVGcontext* vg, int winW, int winH, float pxratio) {

    nCall++;
    auto tmNow = getTimeMillis();
    if (tmNow - tmLastUpdate >= 250) {
        tmLastUpdate = tmNow;
        updateProfilingData();
    }
    if (tmNow - tmLastReload >= 1600) {
        //if (loadShader()) {
        //    //return;
        //}
    }

    glUseProgram(program2dTexture);


    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo.vboIdxId);
    glm::mat4 matProj = glm::ortho(0.f, (float) winW, (float) winH, 0.f, 1.0f, -1.0f);
    int32_t cols      = 4;
    ivec2 renderSize  = ivec2(winW - 32, winH - 32);
    ivec2 renderPos   = ivec2(16);
    vec2 graphSize    = vec2(renderSize.x / cols);
    graphSize.y /= 4;
    tess2d tess;
    vec2 grphInset      = vec2(2.0f);
    vec2 graphSizeInset = graphSize - grphInset * 2.0f;
    {
        tess.setOffset(grphInset);
        tess.add(graphSizeInset.x, 0.0f, 1, 1);
        tess.add(0.0f, 0.0f, 0, 1);
        tess.add(0.0f, graphSizeInset.y, 0, 0);
        tess.add(graphSizeInset.x, graphSizeInset.y, 1, 0);
    }
    tess2d::uploadVBO(tess, vbo);


    // note that we have to call the next 2 lines every frame when not on OpenGL 3.0 or higher contexts.
    // OpenGL documentation does not mention this directly
    glBindVertexArray(vbo.vaoId);
    bindVertexAttributes(attributes);
    glBindBuffer(GL_ARRAY_BUFFER, vbo.vboVertId);

    glm::mat4 mvp;

    vec2 graphPos = vec2(renderPos);
    for (ProfilingDataRenderInstance& renderInstance : instancesRenderStats) {
        renderInstance.instancePos = graphPos;
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, renderInstance.tex0);
        for (int pass = 0; pass < renderInstance.channels.size(); pass++) {
            auto channel = renderInstance.channels[pass];
            auto color   = rgbToNvg(colorOnlyPalette[(pass * 4 + 2) % colorOnlyPaletteLen]);
            color.a      = 0.77f;
            mvp          = matProj * glm::translate(glm::mat4(1.0), glm::vec3(graphPos.x, graphPos.y, 0));
            glUniformMatrix4fv(u_mvp, 1, GL_FALSE, value_ptr(mvp));
            glUniform4f(u_renderColor, color.r, color.g, color.b, color.a);
            glUniform4f(u_renderInfo, tmNow * 0.001f, pass, graphSizeInset.x, graphSizeInset.y);
            glDrawElements(GL_TRIANGLES, vbo.nIndices, GL_UNSIGNED_INT, nullptr);
            channel->graphPos  = graphPos;
            channel->graphSize = graphSize;
            if (pass % cols == cols - 1) {
                graphPos.y += graphSize.y;
                graphPos.x = renderPos.x;
            } else {
                graphPos.x += graphSize.x;
            }
        }
        graphPos.y += graphSize.y;
        graphPos.x = renderPos.x;
    }

    glBindVertexArray(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glStencilMask(~0U);
    glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    nvgBeginFrame(vg, winW, winH, pxratio);


    nvgSave(vg);
    nvgShapeAntiAlias(vg, 1);

    //UIFont::font_instance instance = theme->getFont(UIFont::FONT_DEFAULT);
    //UIFont::bindFont(vg, instance);
    float lineh;
    nvgTextMetrics(vg, nullptr, nullptr, &lineh);
    int TEXT_FONT_SIZE = 18;
    nvgFontSize(vg, TEXT_FONT_SIZE);
    int inset = 0;

    for (ProfilingDataRenderInstance& renderInstance : instancesRenderStats) {
        for (const auto& channel : renderInstance.channels) {
            nvgBatchedRect(vg, channel->graphPos.x, channel->graphPos.y + lineh, (channel->graphSize.x - inset * 2) / 3.5, channel->graphSize.y - inset - (lineh));
        }
    }
    NVGpaint paint{};
    paint.image      = -1;
    paint.innerColor = rgbaToNvg(0x7F333333);
    nvgFillPaint(vg, paint);
    nvgBatchedRender(vg);
    nvgFillColor(vg, rgbaToNvg(0xFFFFFFFF));
    nvgTextAlign(vg, NVG_ALIGN_TOP | NVG_ALIGN_LEFT);
    for (ProfilingDataRenderInstance& renderInstance : instancesRenderStats) {
        nvgText(vg, renderInstance.instancePos.x + 4, renderInstance.instancePos.y - TEXT_FONT_SIZE - inset / 2, StringAsCStr(renderInstance.name), nullptr);
        for (const auto& channel : renderInstance.channels) {
            float y      = channel->graphPos.y;
            nvgText(vg, channel->graphPos.x + 4, y + inset / 2, StringAsCStr(channel->name), nullptr);
            y += lineh;
        }
    }
    TEXT_FONT_SIZE -= 4;
    nvgTranslate(vg, 6, 3);
    nvgFontSize(vg, TEXT_FONT_SIZE);
    nvgFillColor(vg, rgbaToNvg(0xFFAAAAAA));
    nvgTextAlign(vg, NVG_ALIGN_TOP | NVG_ALIGN_LEFT);
    nvgTextMetrics(vg, nullptr, nullptr, &lineh);
    for (ProfilingDataRenderInstance& renderInstance : instancesRenderStats) {
        for (const auto& channel : renderInstance.channels) {
            float y      = channel->graphPos.y + TEXT_FONT_SIZE + inset;
            {
                String strFormatted = StringFormat("%d%s", channel->valueLast, StringAsCStr(channel->unit));
                nvgText(vg, channel->graphPos.x + inset / 2, y + inset / 2, StringAsCStr(strFormatted), nullptr);
            }
            y += lineh;
            {
                String strFormatted = StringFormat("Avg: %d%s", channel->valueAvg, StringAsCStr(channel->unit));
                nvgText(vg, channel->graphPos.x + inset / 2, y + inset / 2, StringAsCStr(strFormatted), nullptr);
            }
            y += lineh;
            {
                String strFormatted = StringFormat("Max: %d%s", channel->valueMax, StringAsCStr(channel->unit));
                nvgText(vg, channel->graphPos.x + inset / 2, y + inset / 2, StringAsCStr(strFormatted), nullptr);
            }
            y += lineh;
            {
                String strFormatted = StringFormat("Min: %d%s", channel->valueMin, StringAsCStr(channel->unit));
                nvgText(vg, channel->graphPos.x + inset / 2, y + inset / 2, StringAsCStr(strFormatted), nullptr);
            }
            y += lineh;
        }
    }
    nvgRestore(vg);


    nvgEndFrame(vg);
}

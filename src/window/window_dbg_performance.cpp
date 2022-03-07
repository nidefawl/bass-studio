#include "glheaders.h"
#include <nanovg.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <utility>
#include "logging.h"
#include "math/vec.h"
#include "math/mat.h"
#include "fileio.h"
#include "profiling.h"
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

struct ProfilingDataChannelBase {
    std::array<float, DBG_PERF_HIST_SIZE> valuesRaw{};
    std::array<float, DBG_PERF_HIST_SIZE> valuesNormalized{};
    int64_t valueLast     = 0;
    int64_t valueMax      = 0;
    int64_t valueMin      = 0;
    int64_t valueAvg      = 0;
    size_t offsetStMember = 0;
    size_t texChannel     = 0;
    vec2 graphPos{};
    vec2 graphSize{};
    float scaleClampMax = 1.0f;
    float scaleMin      = 0.0f;
    String name;
    String unit;
};

struct ProfilingDataRenderInstance {
    const void* instancePtr = nullptr;
    std::vector<std::shared_ptr<ProfilingDataChannelBase>> channels;
    GLuint tex0 = 0;
    vec2 instancePos{};
    String name;
    int32_t nextFreeChannelIdx = 0;
};


void normalizeData(ProfilingDataChannelBase* ch) {
    dbgassert(ch->valuesRaw.size() == DBG_PERF_HIST_SIZE);
    dbgassert(ch->valuesNormalized.size() == DBG_PERF_HIST_SIZE);
    memcpy(ch->valuesNormalized.data(), ch->valuesRaw.data(), sizeof(float) * DBG_PERF_HIST_SIZE);
    const float minFl = 0;
    float maxFl = 10;
    int64_t tmpMax = ch->valueMax;
    while (tmpMax > 10) {
        maxFl *= 10;
        tmpMax /= 10;
    }
    const float sc        = 1.0f / (maxFl - minFl);
    const float* rawData  = ch->valuesRaw.data();
    float* normalizedData = ch->valuesNormalized.data();
    for (int i = 0; i < DBG_PERF_HIST_SIZE; ++i) {
        *normalizedData++ = (*rawData++ - minFl) * sc;
    }
}

void setSamples(ProfilingDataChannelBase* const ch,
                const int64_t* const arrBase,
                const size_t arrLen,
                const size_t stride,
                size_t readIdx,
                const size_t offsetMember) {
    dbgassert(ch->valuesRaw.size() == DBG_PERF_HIST_SIZE);
    dbgassert(readIdx < arrLen);
    int64_t valSample   = *(arrBase + readIdx * stride + offsetMember);
    ch->valueLast = valSample;
    int64_t valueMin    = valSample;
    int64_t valueMax    = valSample;
    int64_t valueAvg    = 0;
    // step thru time backwards
    for (size_t pos = DBG_PERF_HIST_SIZE - 1; pos < DBG_PERF_HIST_SIZE; --pos) {
        valSample = *(arrBase + readIdx * stride + offsetMember);
        ch->valuesRaw[pos] = valSample;
        valueMax  = math::max(valueMax, valSample);
        valueMin  = math::min(valueMin, valSample);
        valueAvg += valSample;
        if (--readIdx > arrLen - 1)
            readIdx = arrLen - 1;
    }
    ch->valueAvg  = valueAvg / static_cast<int64_t>(DBG_PERF_HIST_SIZE);
    ch->valueMin  = valueMin;
    ch->valueMax  = valueMax;
}

static GLuint generateTexture() {
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

class window_impl {
    const size_t texW = DBG_PERF_HIST_SIZE;
    std::vector<VertexAttr> attributes {
        { "in_position", 2, GL_FLOAT },
        { "in_texcoord", 2, GL_FLOAT },
    };
    GLuint program2dTexture = 0;
    GLint u_mvp             = 0;
    GLint u_renderColor     = 0;
    GLint u_renderInfo      = 0;
    DrawVBO vbo;
    std::vector<float> texData;
    
    int64_t tmLastUpdate    = 0;
    int64_t tmLastReload    = 0;
    seq_rand rnd;
    int nCall = 0;

    std::vector<ProfilingDataRenderInstance> profDataInstances;
    int loadShader() {
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
            printf("Link error: %s\n", StringAsCStr(log));
            return 1;
        }
        if (!log.empty()) {
            printf("Link log: %s\n", StringAsCStr(log));
        }
        checkGLError("linkProgram");
        glUseProgram(program);
        u_mvp         = glGetUniformLocation(program, "mvp");
        u_renderInfo  = glGetUniformLocation(program, "renderInfo");
        u_renderColor = glGetUniformLocation(program, "renderColor");
        glUniform1i(glGetUniformLocation(program, "tex0"), 0);
        for (auto& attribute : attributes) {
            attribute.bindingPt = glGetAttribLocation(program, attribute.name);
        }
        checkGLError("glGetAttribLocation");
        glDeleteProgram(program2dTexture);
        program2dTexture = program;
        return 0;
    }

    template<typename T>
    ProfilingDataRenderInstance* getOrInitProfInstance(
        const ProfilingImpl::profiling_channel_descs* const channelDesc, 
        const ProfilingImpl::profiling_entry_t<T>& instance)
    {
        for (ProfilingDataRenderInstance& prevChannel : profDataInstances) {
            if (prevChannel.instancePtr == instance.instancePtr) {
                return &prevChannel;
            }
        }
        profDataInstances.push_back(ProfilingDataRenderInstance{instance.instancePtr});
        ProfilingDataRenderInstance* windowInstance = &profDataInstances.back();
        // windowInstance->instancePtr = instance.instancePtr;
        windowInstance->tex0        = generateTexture();
        windowInstance->name        = instance.name;
        auto& chs                   = windowInstance->channels;
        chs.reserve(channelDesc->size());
        int32_t texChannel = 0;
        for (auto& entry : *channelDesc) {
            dbgassert(entry.offsetStMember % 8 == 0);
            auto ch = std::make_shared<ProfilingDataChannelBase>();
            if (entry.unit == "us")
                ch->scaleClampMax = 10000.0f;
            ch->name           = entry.name;
            ch->unit           = entry.unit;
            ch->offsetStMember = entry.offsetStMember >> 3;
            ch->texChannel     = texChannel++;
            chs.push_back(ch);
        }
        return windowInstance;
    }
    template<typename T>
    void updateProfilingData(ProfilingImpl::profiling_data_t<T>& profData) {
        static_assert(sizeof(T) % 64 == 0, "Type must provide 64 byte size");
        static_assert(alignof(T) % 64 == 0, "Type must provide 64 byte alignment");
        static_assert((sizeof(T) >> 3) % 8 == 0, "Stride expected to be multiple of 8");
        using ProfilingImpl::profiling_data_t;
        dbgassert(profData.channelDesc);
        dbgassert(profData.instanceList);
        for (auto& profDataInstance : *profData.instanceList) {
            auto const windowInstance = this->getOrInitProfInstance(profData.channelDesc, profDataInstance);

            auto& statsArray    = profDataInstance.stats;
            const size_t stride = sizeof(statsArray[0]) >> 3;
            const auto basePtr  = reinterpret_cast<int64_t*>(statsArray.data());
            const auto dataSize = statsArray.size();
            const auto readIdx  = profDataInstance.writeIdx ? profDataInstance.writeIdx - 1 : dataSize - 1;
            auto& channels      = windowInstance->channels;
            for (auto& channel : channels) {
                setSamples(channel.get(), basePtr, dataSize, stride, readIdx, channel->offsetStMember);
            }
            for (auto& channel : channels) {
                normalizeData(channel.get());
            }
            // memset(texData.data(), 0, sizeof(float) * texData.size());
            for (auto& channel : channels) {
                memcpy(&texData[channel->texChannel * texW], channel->valuesNormalized.data(), sizeof(float) * texW);
            }
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, windowInstance->tex0);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, texW, texW, 0, GL_RED, GL_FLOAT, texData.data());
        }
    }
public:
    window_impl() {
        tmLastReload = getTimeMillis();
        texData.resize(texW * texW);
    }

    int init() {
        glBindVertexArray(0);
        int ret = loadShader();
        if (ret)
            return ret;
        tess2d tess;
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

    void render(NVGcontext* vg, int winW, int winH, float pxratio) {
        nCall++;
        auto tmNow = getTimeMillis();
        if (tmNow - tmLastUpdate >= 20) {
            tmLastUpdate = tmNow;

            auto tmStart = getTimeMicros();
            ProfilingImpl::profiling_data_t<application_stats_t> profDataApp;
            profilingGetData(&profDataApp);
            updateProfilingData(profDataApp);
            ProfilingImpl::profiling_data_t<render_stats_t> profDataRenderStats;
            profilingGetData(&profDataRenderStats);
            updateProfilingData(profDataRenderStats);
            auto tmEnd = getTimeMicros();
            if (nCall++ % 100 == 0) {
                log_printf("took %zd\n", tmEnd - tmStart);
            }
        }
        // if (tmNow - tmLastReload >= 1600) {
        //     if (loadShader()) {
        //        //return;
        //     }
        // }

        glUseProgram(program2dTexture);


        const int FONTSIZE_TITLE  = 18;
        const int FONTSIZE_GRAPH  = 16;
        const int FONTSIZE_LEGEND = 14;
        const int WND_PADDING     = 6;
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo.vboIdxId);
        const glm::mat4 matProj = glm::ortho(0.f, (float) winW, (float) winH, 0.f, 1.0f, -1.0f);
        const int32_t cols      = 4;
        const auto renderSize   = vec2(winW, winH) - vec2(WND_PADDING * 2.0f);
        const vec2 graphSize    = vec2(renderSize.x / static_cast<float>(cols)) * vec2(1.0, 1.0 / 3.0f);
        const vec2 grphInset    = vec2(8.0f);
        const auto legendSize   = vec2(graphSize.x * 1.0f/3.0f, FONTSIZE_LEGEND*4.0f + grphInset.x*0.5f * 2.0f);
        tess2d tess;
        const vec2 graphSizeInset = graphSize - grphInset * 2.0f;
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

        vec2 graphPos = vec2(WND_PADDING);
        for (ProfilingDataRenderInstance& renderInstance : profDataInstances) {
            renderInstance.instancePos = graphPos;
            graphPos.y += (FONTSIZE_TITLE + grphInset.y*2.0f);
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
                    graphPos.x = WND_PADDING;
                } else {
                    graphPos.x += graphSize.x;
                }
            }
            graphPos.y += graphSize.y;
            graphPos.y += WND_PADDING;
            graphPos.x = WND_PADDING;
        }

        glBindVertexArray(0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glStencilMask(~0U);
        glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        nvgBeginFrame(vg, winW, winH, pxratio);


        nvgSave(vg);
        nvgShapeAntiAlias(vg, 1);

        for (ProfilingDataRenderInstance& renderInstance : profDataInstances) {
            for (const auto& channel : renderInstance.channels) {
                auto legendPos = channel->graphPos + vec2(0, channel->graphSize.y - legendSize.y);
                nvgBatchedRect(vg, legendPos.x, legendPos.y, legendSize.x, legendSize.y);
            }
        }
        NVGpaint paint{};
        paint.image      = -1;
        paint.innerColor = rgbaToNvg(0x7F333333);
        nvgFillPaint(vg, paint);
        nvgBatchedRender(vg);
        nvgFillColor(vg, rgbaToNvg(0xFFFFFFFF));
        nvgTextAlign(vg, NVG_ALIGN_MIDDLE | NVG_ALIGN_LEFT);
        nvgFontSize(vg, FONTSIZE_TITLE);
        for (ProfilingDataRenderInstance& renderInstance : profDataInstances) {
            auto titlePos = renderInstance.instancePos + vec2(0, FONTSIZE_TITLE) * 0.5f + grphInset;
            nvgText(vg, titlePos.x, titlePos.y, StringAsCStr(renderInstance.name), nullptr);
        }
        nvgFillColor(vg, rgbaToNvg(0xFFCCCCCC));
        nvgFontSize(vg, FONTSIZE_GRAPH);
        for (ProfilingDataRenderInstance& renderInstance : profDataInstances) {
            for (const auto& channel : renderInstance.channels) {
                auto subTitlePos = channel->graphPos + vec2(0, FONTSIZE_GRAPH) * 0.5f;
                nvgText(vg, subTitlePos.x, subTitlePos.y, StringAsCStr(channel->name), nullptr);
            }
        }
        nvgFillColor(vg, rgbaToNvg(0xFFAAAAAA));
        nvgFontSize(vg, FONTSIZE_LEGEND);
        for (ProfilingDataRenderInstance& renderInstance : profDataInstances) {
            for (const auto& channel : renderInstance.channels) {
                auto legendPos = channel->graphPos + vec2(0, channel->graphSize.y - legendSize.y);
                vec2 textPos = legendPos + grphInset * 0.5f + vec2(0,  FONTSIZE_LEGEND * 0.5);
                {
                    String strFormatted = StringFormat("%d%s", channel->valueLast, StringAsCStr(channel->unit));
                    nvgText(vg, textPos.x, textPos.y, StringAsCStr(strFormatted), nullptr);
                    textPos.y += FONTSIZE_LEGEND;
                }
                {
                    String strFormatted = StringFormat("Avg: %d%s", channel->valueAvg, StringAsCStr(channel->unit));
                    nvgText(vg, textPos.x, textPos.y, StringAsCStr(strFormatted), nullptr);
                    textPos.y += FONTSIZE_LEGEND;
                }
                {
                    String strFormatted = StringFormat("Max: %d%s", channel->valueMax, StringAsCStr(channel->unit));
                    nvgText(vg, textPos.x, textPos.y, StringAsCStr(strFormatted), nullptr);
                    textPos.y += FONTSIZE_LEGEND;
                }
                {
                    String strFormatted = StringFormat("Min: %d%s", channel->valueMin, StringAsCStr(channel->unit));
                    nvgText(vg, textPos.x, textPos.y, StringAsCStr(strFormatted), nullptr);
                    textPos.y += FONTSIZE_LEGEND;
                }
            }
        }
        nvgRestore(vg);


        nvgEndFrame(vg);
    }
};
}// namespace windowdebug_performance

static std::shared_ptr<windowdebug_performance::window_impl> window;
int initDebugWindowPerformance(NVGcontext* vg) {
    if (window) {
        return 1;
    }
    window = std::make_shared<windowdebug_performance::window_impl>();
    return window->init();
}
void drawDebugWindowPerformance(NVGcontext* vg, int winW, int winH, float pxratio) {
    if (!window) {
        return;
    }
    window->render(vg, winW, winH, pxratio);
}

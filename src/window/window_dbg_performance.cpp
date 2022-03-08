#include "assert_dbg.h"
#include "glheaders.h"
#include <nanovg.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <utility>
#include "logging.h"
#include "math/seq_math.h"
#include "math/vec.h"
#include "math/mat.h"
#include "fileio.h"
#include "profiling.h"
#include "str_util.h"
#include "gl/gl_util.h"
#include "gl/gl_attr.h"
#include "gl/gl_vbo.h"
#include "gl/gl_tess2d.h"
#include "gl/gl_shader.h"
#include "renderresources.h"
#include "guicolors.h"
#include "color_util.h"
#include "rand.h"
#include "platform.h"
#include "host/profiling_impl.h"

namespace windowdebug_performance {

#define DBG_PERF_HIST_SIZE PROFILING_MAX_LEN

struct ProfilingDataChannelBase {
    std::array<float, DBG_PERF_HIST_SIZE> valuesRaw{};
    std::array<float, DBG_PERF_HIST_SIZE> valuesNormalized{};
    int64_t valueLast     = 0;
    int64_t valueMax      = 0;
    int64_t valueMin      = 0;
    int64_t valueAvg      = 0;
    int64_t fixedScale    = -1;
    size_t offsetStMember = 0;
    size_t texChannel     = 0;
    vec2 graphPos{};
    vec2 graphSize{};
    String name;
    String unit;
};

struct ProfilingDataRenderInstance {
    const void* instancePtr = nullptr;
    std::vector<std::shared_ptr<ProfilingDataChannelBase>> channels;
    GLuint texActive = 0;
    GLuint texUpload = 0;
    int64_t dataFrameNum = -1;
    vec2 instancePos{};
    String name;
    int32_t nextFreeChannelIdx = 0;
};


void normalizeData(ProfilingDataChannelBase* ch) {
    dbgassert(ch->valuesRaw.size() == DBG_PERF_HIST_SIZE);
    dbgassert(ch->valuesNormalized.size() == DBG_PERF_HIST_SIZE);
    // memcpy(ch->valuesNormalized.data(), ch->valuesRaw.data(), sizeof(float) * DBG_PERF_HIST_SIZE);
    const float minFl = 0;
    const float valAvg = ch->valueAvg;
    float maxFl = 10;
    if (ch->fixedScale > 0 && valAvg >= ch->fixedScale * 0.05) {
        maxFl = static_cast<float>(ch->fixedScale);
    }
    while (maxFl > 100 && valAvg < maxFl * 0.05) {
        maxFl /= 10;
    }
    while (valAvg > maxFl * 0.85) {
        maxFl *= 10;
    }
    const float sc        = 1.0f / (maxFl - minFl);
    const float* rawData  = ch->valuesRaw.data();
    float* normalizedData = ch->valuesNormalized.data();
    for (int i = 0; i < DBG_PERF_HIST_SIZE; ++i) {
        *normalizedData++ = (*rawData++ - minFl) * sc;
        // *normalizedData++ = i / (float)(DBG_PERF_HIST_SIZE-1);
    }
}

template<size_t stride, size_t arrLen>
void setSamples(ProfilingDataChannelBase* const ch,
                const int64_t* const arrBase,
                size_t readIdx,
                const size_t offsetMember) {
    dbgassert(ch->valuesRaw.size() == DBG_PERF_HIST_SIZE);
    dbgassert(readIdx < arrLen);
    int64_t valSample   = *(arrBase + readIdx * stride + offsetMember);
    ch->valueLast = valSample;
    int64_t valueMin    = valSample;
    int64_t valueMax    = valSample;
    int64_t valueAvg    = 0;
    auto itOut = ch->valuesRaw.rbegin();
    // step thru time backwards
    for (size_t pos = DBG_PERF_HIST_SIZE - 1; pos < DBG_PERF_HIST_SIZE; --pos) {
        valSample = *(arrBase + readIdx * stride + offsetMember);
        *itOut++ = valSample;
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


struct gl_shader_perfgraph : gl_shader_pipeline {
    bool isValid = false;
    GLint u_renderColor     = 0;
    GLint u_renderInfo      = 0;
    gl_shader_perfgraph() {
        attributes = {
            { "in_position", 2, GL_FLOAT },
            { "in_texcoord", 2, GL_FLOAT },
        };
    }
    ~gl_shader_perfgraph() {
    }
    void setUniforms(int32_t w, int32_t h, float fTime) {
        if (u_viewport >= 0)
            glUniform2f(u_viewport, w, h);
        if (u_time >= 0)
            glUniform1f(u_time, fTime);
    }
    template<typename T>
    int load(T* srcParser) {
        const char* fnameVsh = "textured.vsh";
        const char* fnameFsh = "perfgraph.fsh";
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
        u_renderInfo  = glGetUniformLocation(program, "u_renderInfo");
        u_renderColor = glGetUniformLocation(program, "u_renderColor");
        glGenVertexArrays(1, &vbo.vaoId);
        glBindVertexArray(vbo.vaoId);
        vbo.genBuffers();
        glBindBuffer(GL_ARRAY_BUFFER, vbo.vboVertId);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo.vboIdxId);
        bindVertexAttributes(attributes);
        glBindVertexArray(0);
        if (checkGLError("glGenVertexArrays and genBuffers"))
            return -1;
        isValid = true;
        return 0;
    };
};
// static constexpr uint64_t nextPowerOfTwo64 (uint64_t x) { return 1ULL<<(sizeof(uint64_t) * 8 - __builtin_clzll(x)); }

class window_impl {
    static constexpr size_t TEXTURE_WIDTH  = DBG_PERF_HIST_SIZE;
    static constexpr size_t TEXTURE_HEIGHT = 16;
    std::vector<float> texData;
    
    int64_t tmLastUpdate    = 0;
    int64_t tmLastReload    = 0;
    seq_rand rnd;
    int nCall = 0;

    std::vector<ProfilingDataRenderInstance> profDataInstances;
    std::shared_ptr<gl_shader_perfgraph> pipePerfShader;

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
        dbgassert(channelDesc->size() <= TEXTURE_HEIGHT);
        profDataInstances.push_back(ProfilingDataRenderInstance{instance.instancePtr});
        ProfilingDataRenderInstance* windowInstance = &profDataInstances.back();
        std::array<GLuint, 2> textures{};
        glGenTextures(textures.size(), textures.data());
        glActiveTexture(GL_TEXTURE0);
        for (GLuint tex : textures) {
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        }
        checkGLError("glTexImage2D");
        glBindTexture(GL_TEXTURE_2D, 0);

        windowInstance->texActive = textures[0];
        windowInstance->texUpload = textures[1];
        windowInstance->name        = instance.name;
        auto& chs                   = windowInstance->channels;
        chs.reserve(channelDesc->size());
        int32_t texChannel = 0;
        for (auto& entry : *channelDesc) {
            dbgassert(entry.offsetStMember % 8 == 0);
            auto ch = std::make_shared<ProfilingDataChannelBase>();
            if (entry.unit == "us")
                ch->fixedScale = 20000;
            ch->name           = entry.name;
            ch->unit           = entry.unit;
            ch->offsetStMember = entry.offsetStMember >> 3;
            ch->texChannel     = texChannel++;
            chs.push_back(ch);
        }
        return windowInstance;
    }
    
    template<typename T>
    int32_t updateProfilingData(ProfilingImpl::profiling_data_t<T>& profData) {
        static_assert(sizeof(T) % 64 == 0, "Type must provide 64 byte size");
        static_assert(alignof(T) % 64 == 0, "Type must provide 64 byte alignment");
        static_assert((sizeof(T) >> 3) % 8 == 0, "Stride expected to be multiple of 8");
        using ProfilingImpl::profiling_data_t;
        dbgassert(profData.channelDesc);
        dbgassert(profData.instanceList);
        int32_t numUpdated = 0;
        for (auto& profDataInstance : *profData.instanceList) {
            auto const windowInstance = this->getOrInitProfInstance(profData.channelDesc, profDataInstance);
            if (windowInstance->dataFrameNum == profDataInstance.frameNum)
                continue;
            windowInstance->dataFrameNum = profDataInstance.frameNum;
            std::swap(windowInstance->texActive, windowInstance->texUpload);

            static constexpr size_t STRIDE = sizeof(T) >> 3;
            const auto basePtr  = reinterpret_cast<int64_t*>(profDataInstance.stats.data());
            const auto readIdx  = profDataInstance.writeIdx ? profDataInstance.writeIdx - 1 : PROFILING_MAX_LEN - 1;
            auto& channels      = windowInstance->channels;
            for (auto& channel : channels) {
                setSamples<STRIDE, PROFILING_MAX_LEN>(channel.get(), basePtr, readIdx, channel->offsetStMember);
                normalizeData(channel.get());
                memcpy(&texData[channel->texChannel * TEXTURE_WIDTH], channel->valuesNormalized.data(), sizeof(float) * TEXTURE_WIDTH);
            }
            glBindTexture(GL_TEXTURE_2D, windowInstance->texUpload);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, TEXTURE_WIDTH, TEXTURE_HEIGHT, 0, GL_RED, GL_FLOAT, texData.data());
            numUpdated++;
        }
        return numUpdated;
    }
public:
    window_impl() {
        texData.resize(TEXTURE_WIDTH * TEXTURE_HEIGHT);
    }

    int init() {
        tmLastReload = getTimeMillis();
        pipePerfShader = std::make_shared<gl_shader_perfgraph>();
        struct gl_srcparser_perfgraph {
            void preprocessSources(std::vector<glshader_src>& srcList) {
                for (auto& src : srcList) {
                    if (src.stage == GL_FRAGMENT_SHADER) {
                        src.source.insert(0, StringFormat("#define TEXTURE_HEIGHT %zu.0\n", TEXTURE_HEIGHT));
                        src.source.insert(0, StringFormat("#define TEXTURE_WIDTH %zu.0\n", TEXTURE_WIDTH));
                        src.source.insert(0, "#version 150 core\n");
                    }
                }
            }
        };
        gl_srcparser_perfgraph parser;
        int ret = pipePerfShader->load(&parser);
        updateProfilingData();
        return ret;
    }
    int32_t updateProfilingData() {
        int32_t numUpdated = 0;
        ProfilingImpl::profiling_data_t<prof_stats_applicaton_t> profDataApp;
        profilingGetData(&profDataApp);
        numUpdated += updateProfilingData(profDataApp);
        ProfilingImpl::profiling_data_t<prof_stats_render_t> profDataRender;
        profilingGetData(&profDataRender);
        numUpdated += updateProfilingData(profDataRender);
        ProfilingImpl::profiling_data_t<prof_stats_window_t> profDataWindow;
        profilingGetData(&profDataWindow);
        numUpdated += updateProfilingData(profDataWindow);
        return numUpdated;
    }
    int render(NVGcontext* vg, int winW, int winH, float pxratio) {
        float zoom = 1.0f;
        float fbWidth = winW * zoom;
        float fbHeight = winH * zoom;
        auto tmMillis = getTimeMicros() / 1000UL;
        nCall++;
        int32_t numUpdated = updateProfilingData();
        if (!numUpdated) {
            return 0;
        }
        if (tmMillis - tmLastReload >= 1600) {
            if (init()) {
               return 0;
            }
        }
        auto const pipeline = pipePerfShader.get();
        if (!pipeline->isValid)
            return 0;
        auto tmEndUpload = getTimeMicros();

        const int FONTSIZE_TITLE  = 16;
        const int FONTSIZE_GRAPH  = 14;
        const int FONTSIZE_LEGEND = 12;
        const int WND_PADDING     = 6;
        const auto renderSize   = vec2(fbWidth, fbHeight) - vec2(WND_PADDING * 2.0f);
        int layoutCols = math::floorfS32(math::min(6.0f, fbWidth/280.0f));
        vec2 layoutSize(0);
        do {
            layoutSize = vec2(renderSize.x / static_cast<float>(layoutCols)) * vec2(1.0, 1.0 / 4.0f);
        } while(layoutSize.x > fbWidth/2.0 && layoutCols++);
        while(layoutSize.y > 100.0) {
            layoutSize.y *= 0.75;
        } 
        const int32_t cols      = layoutCols;
        const vec2 graphSize    = layoutSize;
        const vec2 grphInset    = vec2(4.0f);
        const auto legendSize   = vec2(FONTSIZE_LEGEND*8.0f, FONTSIZE_LEGEND*4.0f + grphInset.x*0.5f * 2.0f);
        tess2d tess;
        const auto graphSizeInset = ivec2(graphSize - grphInset * 2.0f);
        {
            tess.setOffset(ivec2(grphInset));
            tess.add(graphSizeInset.x, 0.0f, 1, 1);
            tess.add(0.0f, 0.0f, 0, 1);
            tess.add(0.0f, graphSizeInset.y, 0, 0);
            tess.add(graphSizeInset.x, graphSizeInset.y, 1, 0);
        }

        const auto gr = 15/256.f;
        glClearColor(gr, gr, gr, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        glUseProgram(pipeline->program);

        auto& vbo = pipeline->vbo;
        pipeline->bindBuffer(vbo);
        tess2d::uploadVBO(tess, vbo);
        const glm::mat4 matProj = glm::ortho(0.f, (float) fbWidth, (float) fbHeight, 0.f, 1.0f, -1.0f);
        pipeline->setUniforms(fbWidth, fbHeight, getTimeMillisF());
        glUniformMatrix4fv(pipeline->u_mvp, 1, GL_FALSE, value_ptr(matProj));
        glm::mat4 mvp;

        vec2 graphPos = vec2(WND_PADDING);
        int colFill = 0;
        int prevEntryNumRows = 1;
        for (ProfilingDataRenderInstance& renderInstance : profDataInstances) {
            const auto channelsSize = renderInstance.channels.size();
            if (prevEntryNumRows > 1 || (colFill > 0 && colFill + channelsSize > cols)) {
                graphPos.y += graphSize.y;
                graphPos.y += WND_PADDING;
                graphPos.x = WND_PADDING;
                colFill = 0;
            } else if (colFill) {
                graphPos.y -= (FONTSIZE_TITLE + grphInset.y*2.0f);
            }
            renderInstance.instancePos = graphPos;
            graphPos.y += (FONTSIZE_TITLE + grphInset.y*2.0f);
            glBindTexture(GL_TEXTURE_2D, renderInstance.texActive);
            prevEntryNumRows = 1;
            for (int pass = 0; pass < channelsSize; pass++) {
                auto channel = renderInstance.channels[pass];
                auto color   = rgbToNvg(colorOnlyPalette[(pass * 4 + 2) % colorOnlyPaletteLen]);
                color.a      = 0.77f;
                mvp          = glm::translate(matProj, glm::vec3(glm::ivec3(graphPos.x, graphPos.y, 0)));
                glUniformMatrix4fv(pipeline->u_mvp, 1, GL_FALSE, value_ptr(mvp));
                glUniform4f(pipeline->u_renderColor, color.r, color.g, color.b, color.a);
                glUniform4f(pipeline->u_renderInfo, tmMillis * 0.001f, pass, graphSizeInset.x, graphSizeInset.y);
                glDrawElements(GL_TRIANGLES, vbo.nIndices, GL_UNSIGNED_INT, nullptr);
                channel->graphPos  = ivec2(graphPos);
                channel->graphSize = ivec2(graphSize);
                colFill++;
                if (pass % cols == cols - 1 && channelsSize > pass + 1) {
                    graphPos.y += graphSize.y + grphInset.y;
                    graphPos.x = WND_PADDING;
                    colFill = 0;
                    prevEntryNumRows++;
                } else {
                    graphPos.x += graphSize.x;
                }
            }
        }

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
        auto tmEndGraphs = getTimeMicros();
        if (nCall % 50 == 0) {
            log_printf("tmEndGraphs %zd\n", tmEndGraphs - tmEndUpload);
        }
        nvgBeginFrame(vg, fbWidth, fbHeight, pxratio);


        nvgSave(vg);
        // nvgShapeAntiAlias(vg, 1);

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
        nvgFillColor(vg, rgbaToNvg(0xFF00FF7F));
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
        auto tmEndNvg = getTimeMicros();
        if (nCall % 50 == 0) {
            log_printf("tmEndNvg %zd\n", tmEndNvg - tmEndGraphs);
        }
        return 1;
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
int drawDebugWindowPerformance(NVGcontext* vg, int winW, int winH, float pxratio) {
    if (!window) {
        return 0;
    }
    return window->render(vg, winW, winH, pxratio);
}

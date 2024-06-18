#pragma once
#include "fileio.h"
#include "gl/gl_context.hpp"
#include "gl/gl_shader.h"
#include "glheaders.h"
#include "logging.h"
#include "platform.h"
#include "str_util.h"
#include "types.h"
#include <GLFW/glfw3.h>
#include <cstdint>
#include <variant>

namespace DAW::GPU {

struct gpu_program_definitions_t {
    samplecount_t blocksize1024Fixed = 0;
    samplecount_t channels = 2;
    samplecount_t polyVoices = 16;
    samplecount_t unisonVoices = 4;
};

struct gpu_program_desc_t {
    int32_t programNr = 0;
    String name = "";
    String def = "";
};

struct gpu_program : public gpu_program_definitions_t {
    static constexpr size_t MAX_PROGRAMS = 32;
    std::array<gpu_program_desc_t, MAX_PROGRAMS> programDescs{};
    std::array<GLuint, MAX_PROGRAMS> programs{};
    std::array<GLuint, MAX_PROGRAMS> programsWaveform{};
    int32_t numPrograms = 0;
    bool is_valid() const {
        return numPrograms > 0 && !!programs[0];
    }
    void destroy() {
        for (auto& program : programs) {
            if (program) {
                glDeleteProgram(program);
                program = 0;
            }
        }
        programDescs = {};
    }
};

inline std::variant<gpu_program, String> compileGPUProgram(const glshader_src& src, const gpu_program_definitions_t& defs) {
    auto compileProgram = [](const auto& src, size_t programNr, bool isWaveformSampler) ->  std::variant<GLuint, String> {
        String sourceCopy = src;
        GLuint shader1 = glCreateShader(GL_COMPUTE_SHADER);
        dbgassert(shader1);
        StrUtil::StringReplace(sourceCopy, "#define N_PROGRAM 0", "#define N_PROGRAM " + std::to_string(programNr));
        if (isWaveformSampler) {
            StrUtil::StringReplace(sourceCopy, "#define IS_WAVEFORM_SAMPLER 0", "#define IS_WAVEFORM_SAMPLER 1");
        }
#if 0
        if (!isWaveformSampler) {
            log_lf(Log::L_DEBUG, "compileGPUProgram %d:\n%s\n", int(programNr), StringAsCStr(sourceCopy));
        }
#endif
        std::array<GLchar*, 1> sources = {(GLchar*)sourceCopy.c_str()};
        glShaderSource(shader1, 1, sources.data(), NULL);
        glCompileShader(shader1);
        String log = getLog(0, shader1);
        if (getStatus(shader1, GL_COMPILE_STATUS) != 1) {
            glGetError();
            glDeleteShader(shader1);
            return StringFormat("Compile error: %s\n", StringAsCStr(log));
        }
        GLuint program = glCreateProgram();
        glAttachShader(program, shader1);
        glLinkProgram(program);
        glDeleteShader(shader1);
        String log2 = getLog(1, program);
        if (getStatus(program, GL_LINK_STATUS) != 1) {
            glGetError();
            glDeleteProgram(program);
            return StringFormat("Link error: %s\n", StringAsCStr(log2));
        }
        return program;
    };
    gpu_program result;
    auto& glslSourceCode = src.source;
    // find #define PROGRAM_NAME_.* ".*"\n using string find, not regex
    size_t pos = 0;
    size_t programNr = 0;
    size_t posLastProcessedDefine = 0;
    while ((pos = glslSourceCode.find("#define PROGRAM_NAME_", pos)) != String::npos) {
        size_t start = pos + 21;
        size_t lineEnd = glslSourceCode.find('\n', start);
        size_t beginName = glslSourceCode.find('\"', start);
        if (beginName+1 > lineEnd || beginName == String::npos) {
            break;
        }
        beginName++;
        size_t endName = glslSourceCode.find('\"', beginName);
        if (endName > lineEnd || endName == String::npos) {
            break;
        }
        pos = endName + 1;
        posLastProcessedDefine = pos;
        String defName = glslSourceCode.substr(start, beginName - start - 1);
        String name = glslSourceCode.substr(beginName, endName - beginName);
        gpu_program_desc_t desc;
        desc.programNr = int32_t(programNr);
        desc.name = name;
        desc.def = defName;
        log_lf(Log::L_DEBUG, "Found program %zu %s %s\n", programNr, StringAsCStr(name), StringAsCStr(defName));
        result.programDescs[programNr] = desc;
        programNr++;
        if (programNr >= gpu_program::MAX_PROGRAMS) {
            break;
        }
    }
    for (;programNr < gpu_program::MAX_PROGRAMS; programNr++) {
        gpu_program_desc_t desc;
        desc.programNr = int32_t(programNr);
        desc.name = "Reserved program";
        desc.def = "RESERVED_" + std::to_string(programNr);
        result.programDescs[programNr] = desc;
    }
    if (posLastProcessedDefine == 0 || programNr == 0) {
        return String("No program names found in shader source");
    }
    /* place a #define PROGRAM_<NAME> <NR> in the source code */
    String defBlock = "";
    for (size_t i = 0; i < programNr; i++) {
        defBlock += "#define PROGRAM_" + result.programDescs[i].def + " " + std::to_string(i) + "\n";
    }
    defBlock += "#define N_PROGRAMS " + std::to_string(programNr) + "\n";
    /* insert after posLastProcessedDefine */
    posLastProcessedDefine++;
    if (posLastProcessedDefine >= glslSourceCode.size()) {
        return String("Failed to insert program defines");
    }
    auto processedSrc = glslSourceCode.substr(0, posLastProcessedDefine) + defBlock + glslSourceCode.substr(posLastProcessedDefine);
    // log_lf(Log::L_DEBUG, "Processed source code:\n%s\n", StringAsCStr(processedSrc));
    for (size_t i = 0; i < programNr; i++) {
        auto programSynth = compileProgram(processedSrc, i, false);
        if (std::holds_alternative<String>(programSynth)) {
            return std::get<String>(programSynth);
        }
        auto programWaveform = compileProgram(processedSrc, i, true);
        if (std::holds_alternative<String>(programWaveform)) {
            return std::get<String>(programWaveform);
        }
        result.programs[i] = std::get<GLuint>(programSynth);
        result.programsWaveform[i] = std::get<GLuint>(programWaveform);
    }
    result.numPrograms = gpu_program::MAX_PROGRAMS;
    result.blocksize1024Fixed = defs.blocksize1024Fixed;
    result.channels = defs.channels;
    result.polyVoices = defs.polyVoices;
    result.unisonVoices = defs.unisonVoices;
    return result;
}

inline std::variant<gpu_program, String> loadshader(const gpu_program_definitions_t& defs, gpu_program& previous) {
    static int64_t lastModTimeGpuSoundShader = 0;
    static int64_t lastModTimeShaderToyShader = 0;
    String filenameGpuSoundShader = "shaders/gpu_sound.glsl";
    // String filenameShaderToyShader = "shaders/shadertoy_wtdSRN_lullaby.glsl";
    // String filenameShaderToyShader = "shaders/shadertoy_MdGGWd_fuer_elise.glsl";
    // String filenameShaderToyShader = "shaders/shadertoy_NddSzl_Synthwave_Song.glsl";
    String filenameShaderToyShader = "shaders/athibaul Techno Song.glsl";
    // String filenameShaderToyShader = "shaders/shadertoy_3sXyDr_riff180320.glsl";
    // String filenameShaderToyShader = "shaders/shadertoy_test.glsl";
    auto glSourceLoader = std::make_unique<glshader_srcloader>();
    
    // check time of last modification
    using App::Platform::toResourcePath;
    int64_t timeDiskGpuSoundShader = FileTimeGetter(toResourcePath(filenameGpuSoundShader)).getWriteTimeI64();
    int64_t timeDiskShaderToyShader = FileTimeGetter(toResourcePath(filenameShaderToyShader)).getWriteTimeI64();
    if (previous.programs[0] != 0) {
        // if the file has not been modified, return the previous shader
        if (timeDiskGpuSoundShader == lastModTimeGpuSoundShader && timeDiskShaderToyShader == lastModTimeShaderToyShader) {
            return previous;
        }
    }
    lastModTimeGpuSoundShader = timeDiskGpuSoundShader;
    lastModTimeShaderToyShader = timeDiskShaderToyShader;
    if (!glSourceLoader->addStageSrc(GL_COMPUTE_SHADER, filenameGpuSoundShader.c_str(), 0) || glSourceLoader->sources.empty()) {
        log_lf(Log::L_ERROR, "Failed to load compute shader source file %s", StringAsCStr(filenameGpuSoundShader));
        return previous;
    }
    if (!glSourceLoader->addStageSrc(GL_COMPUTE_SHADER, filenameShaderToyShader.c_str(), 0)) {
        log_lf(Log::L_ERROR, "Failed to load compute shader source file %s", StringAsCStr(filenameShaderToyShader));
        return previous;
    }
    auto& sourcefiles = glSourceLoader->sources;
    auto& file0Source = sourcefiles[0].source;
    StrUtil::StringReplace(file0Source, "#define N_CHANNELS 0", "#define N_CHANNELS " + std::to_string(defs.channels));
    StrUtil::StringReplace(file0Source, "#define N_SAMPLES 0", "#define N_SAMPLES " + std::to_string(defs.blocksize1024Fixed));
    sourcefiles[0].source += sourcefiles[1].source;
    log_lf(Log::L_DEBUG, "Source code:\n%s\n", StringAsCStr(sourcefiles[0].source));
    auto newShader = compileGPUProgram(sourcefiles[0], defs);
    if (std::holds_alternative<gpu_program>(newShader)) {
        previous.destroy();
    }
    return newShader;
}
struct gpu_program_loadresult {
    enum class Type {
        PROGRAM_LOAD_ERROR,
        PROGRAM_LOAD_SUCCESS,
        PROGRAM_LOAD_NO_CHANGE
    } type = Type::PROGRAM_LOAD_ERROR;
    String strError;
    std::optional<gpu_program> gpuProgram = std::nullopt;
};
inline gpu_program_loadresult loadGPUProgram(gpu_program_definitions_t defs, gpu_program& previous) {
    static int64_t lastModTimeGpuSoundShader = 0;
    String filenameGpuSoundShader = "shaders/gpu_synth.glsl";
    // check time of last modification
    using App::Platform::toResourcePath;
    int64_t timeDiskGpuSoundShader = FileTimeGetter(toResourcePath(filenameGpuSoundShader)).getWriteTimeI64();
    if (previous.is_valid()) {
        // if the file has not been modified, return the previous shader
        if (timeDiskGpuSoundShader == lastModTimeGpuSoundShader) {
            return {gpu_program_loadresult::Type::PROGRAM_LOAD_NO_CHANGE, "", previous};
        }
    }
    lastModTimeGpuSoundShader = timeDiskGpuSoundShader;
    auto glSourceLoader = std::make_unique<glshader_srcloader>();
    if (!glSourceLoader->addStageSrc(GL_COMPUTE_SHADER, filenameGpuSoundShader.c_str(), 0) || glSourceLoader->sources.empty()) {
        auto errMessage = StringFormat("Failed to load compute shader source file %s", StringAsCStr(filenameGpuSoundShader));
        return {gpu_program_loadresult::Type::PROGRAM_LOAD_ERROR, errMessage, previous};
    }
    auto& sourcefiles = glSourceLoader->sources;
    auto& file0Source = sourcefiles[0].source;
    StrUtil::StringReplace(file0Source, "#define N_CHANNELS 0", "#define N_CHANNELS " + std::to_string(defs.channels));
    StrUtil::StringReplace(file0Source, "#define N_SAMPLES 0", "#define N_SAMPLES " + std::to_string(defs.blocksize1024Fixed));
    StrUtil::StringReplace(file0Source, "#define N_POLY_VOICES 0", "#define N_POLY_VOICES " + std::to_string(defs.polyVoices));
    StrUtil::StringReplace(file0Source, "#define N_UNISON_VOICES 0", "#define N_UNISON_VOICES " + std::to_string(defs.unisonVoices));
    auto res = compileGPUProgram(sourcefiles[0], defs);
    if (std::holds_alternative<gpu_program>(res)) {
        previous.destroy();
        return {gpu_program_loadresult::Type::PROGRAM_LOAD_SUCCESS, "", std::get<gpu_program>(res)};
    }
    return {gpu_program_loadresult::Type::PROGRAM_LOAD_ERROR, std::get<String>(res), previous};
}

template<size_t N>
struct ssbo_ringbuffer_t : public OpenGLResource {
    constexpr size_t size() const {
        return N;
    }
    ~ssbo_ringbuffer_t() override {
        destroy();
    }
    void destroy() override {
        if (makeContextCurrent()) {
            glDeleteBuffers(ssbo.size(), &ssbo[0]);
        }
        ssbo.fill(0);
        glfwWindowHandle = nullptr;
    }
    std::array<GLuint, N> ssbo{};
    uint64_t frameIndex = 0;
    void genBuffers() {
        storeGlContext();
        glGenBuffers(ssbo.size(), &ssbo[0]);
        checkGLError("glGenBuffers");
    }

    void allocate(size_t len, GLenum usage) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, current());
        checkGLError("glBindBuffer");
        glBufferData(GL_SHADER_STORAGE_BUFFER, len, nullptr, usage);
        checkGLError("glBufferData");
    }
    void uploadBuffer(void* buffer, size_t len) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, current());
        checkGLError("glBindBuffer");
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, len, buffer);
        checkGLError("glBufferSubData");
    }
    GLuint current() const {
        return ssbo[frameIndex % N];
    }
    bool downloadBufferDelayed(void* buffer, size_t len) {
        size_t delay = N - 1;
        if (frameIndex < delay) {
            // memset 0
            memset(buffer, 0, len);
            return false;
        }
        auto delayed = frameIndex - delay;
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo[delayed % N]);
        checkGLError("glBindBuffer");
        GLvoid* p = glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
        checkGLError("glMapBuffer");
        if (!p) {
            return false;
        }
        checkGLError("glMapBuffer");
        memcpy(buffer, p, len);
        checkGLError("memcpy");
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        checkGLError("glUnmapBuffer");
        return true;
    }
    void incrementFrame() {
        frameIndex++;
        if (frameIndex > 2 * N) {
            frameIndex -= N;
        }
    }
};

struct gpu_compute_context_t {
    double one_over_samplerate;
    double time_sample_phase_reset;
    double bpm;
    double time_seconds;
    double time_samples;
    double time_beats;
    double osc1_unison_voice_count;
    double osc1_filter;
    double osc1_pw;
    double osc1_pw_mod_rate;
    double osc1_pw_mod_depth;
    double osc1_width_keytrack;
};

    
class GPUAudioProcessor {
protected:
    struct host_buffer_t {
        ssbo_ringbuffer_t<2> ssbo{};
        std::vector<float> buffer;
        void downloadBuffer() {
            ssbo.downloadBufferDelayed(buffer.data(), buffer.size() * sizeof(float));
        }
        void uploadBuffer() {
            ssbo.uploadBuffer(buffer.data(), buffer.size() * sizeof(float));
        }
        void clearBuffer() {
            std::fill(std::begin(buffer), std::end(buffer), 0.0f);
        }
        void incrementFrame() {
            ssbo.incrementFrame();
        }
    };
    GLFWwindow* window = nullptr;
    gpu_program gpuProgram{};
    gpu_compute_context_t gpuContext{};
    int32_t currentProgramId = 0;
    int32_t allocatedVoiceCount = 0;
    host_buffer_t ssboInputSynthState{};
    host_buffer_t ssboInputVoiceStates{};
    host_buffer_t ssboOutput{};
    host_buffer_t ssboOutputWaveform{};
    GLuint ubo = 0;
    std::array<host_buffer_t*, 4> hostBuffers{&ssboInputSynthState, &ssboInputVoiceStates, &ssboOutput, &ssboOutputWaveform};
    int64_t timeLastShaderError = 0;

    //TODO: make these user runtime options
    bool bUseGlFinish = true;
    bool bUseMemoryBarriers = true;

    /* Set ssbo to size of host_buffer_t::buffer */
    void reallocateSSBOs() {
        for (auto* buffer : hostBuffers) {
            buffer->clearBuffer();
            for (size_t i = 0; i < buffer->ssbo.size(); i++) {
                buffer->ssbo.allocate(buffer->buffer.size() * sizeof(float), GL_DYNAMIC_DRAW);
                buffer->uploadBuffer();
                buffer->incrementFrame();
            }
        }
        checkGLError("allocateForBlockSize");
    }
public:
    static constexpr size_t GPU_BLOCK_SIZE = 1024;
    GPUAudioProcessor() = default;
    virtual ~GPUAudioProcessor()
    {
        GlfwContextSwitch ctxSwitch(window);
        if (window) {
            releaseGlResources();
            glfwDestroyWindow(window);
        }
    }
    bool initComputeContext(bool bInitGL = false) {
        
        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);

        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        window = glfwCreateWindow(512, 512, "GPU Synth", NULL, NULL);
        if (!assert_expr(window != nullptr)) {
            log_lf(Log::L_ERROR, "Failed creating glfw window\n");
            return false;
        }
        GlfwContextSwitch ctxSwitch(window);
        if (!glad_glDispatchCompute && !gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
            log_lf(Log::L_ERROR, "glad GL loader failed\n");
            return false;
        }
        if (!glad_glDispatchCompute) {
            log_lf(Log::L_ERROR, "GL compute is not supported on this platform\n");
            return false;
        }

        int work_grp_cnt[3]{};
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &work_grp_cnt[0]);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &work_grp_cnt[1]);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &work_grp_cnt[2]);
        log_lf(Log::L_INFO, "max global (total) work group counts x:%i y:%i z:%i\n",
        work_grp_cnt[0], work_grp_cnt[1], work_grp_cnt[2]);

        int work_grp_size[3]{};
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &work_grp_size[0]);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &work_grp_size[1]);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &work_grp_size[2]);
        log_lf(Log::L_INFO, "max local (in one shader) work group sizes x:%i y:%i z:%i\n",
        work_grp_size[0], work_grp_size[1], work_grp_size[2]);

        int work_grp_inv = 0;
        glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &work_grp_inv);
        log_lf(Log::L_INFO, "max local work group invocations %i\n", work_grp_inv);
        glGenBuffers(1, &ubo);

        glBindBuffer(GL_UNIFORM_BUFFER, ubo);
        checkGLError("glBindBuffer");
        glBufferData(GL_UNIFORM_BUFFER, sizeof(gpu_compute_context_t), nullptr, GL_STREAM_DRAW);
        checkGLError("glBufferData");
        for (auto* buffer : hostBuffers) {
            buffer->ssbo.genBuffers();
        }
        checkGLError("genBuffers");

        return true;
    }
    int32_t currentProgram() const { return currentProgramId; }
    void reloadShader(gpu_program_definitions_t defs) {
        auto res = loadGPUProgram(defs, this->gpuProgram);
        checkGLError("loadGPUProgram");
        switch (res.type) {
            case gpu_program_loadresult::Type::PROGRAM_LOAD_ERROR:
                log_lf(Log::L_ERROR, "%s\n", res.strError.c_str());
                timeLastShaderError = getTimeMillis();
                break;
            case gpu_program_loadresult::Type::PROGRAM_LOAD_SUCCESS:
                this->gpuProgram = *res.gpuProgram;
                updateProgramList();
                break;
            case gpu_program_loadresult::Type::PROGRAM_LOAD_NO_CHANGE:
                break;
        }
    }
    virtual void updateProgramList() {

    }
    void releaseGlResources() {
        for (auto* buffer : hostBuffers) {
            buffer->ssbo.destroy();
        }
        glDeleteBuffers(1, &ubo);
        gpuProgram.destroy();
        gpuProgram = {};
        ubo = 0;
    }
};

} // namespace DAW::GPU

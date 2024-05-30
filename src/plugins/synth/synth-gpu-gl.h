#pragma once

#include "fileio.h"
#include "gl/gl_shader.h"
#include "glheaders.h"
#include "logging.h"
#include "platform.h"
#include "str_util.h"
#include "types.h"

namespace PluginSynth::GPU {


struct shader_gpu_compute_defs_t {
    samplecount_t blocksize = 512;
    samplecount_t channels = 2;
    samplecount_t polyVoices = 16;
    samplecount_t unisonVoices = 4;
};

struct shader_gpu_compute : public shader_gpu_compute_defs_t {
    GLuint programSynth = 0;
    GLuint programSampleWaveform = 0;
    bool is_valid() const {
        return programSynth != 0;
    }
    void destroy() {
        if (programSampleWaveform) {
            glDeleteProgram(programSampleWaveform);
            programSampleWaveform = 0;
        }
        if (programSynth) {
            glDeleteProgram(programSynth);
            programSynth = 0;
        }
    }
};

inline std::variant<shader_gpu_compute, String> compile_shader(const glshader_src& src, const shader_gpu_compute_defs_t& defs) {
    auto compileProgram = [&src](int n) ->  std::variant<GLuint, String> {
        String sourceCopy = src.source;
        GLuint shader1 = glCreateShader(GL_COMPUTE_SHADER);
        StrUtil::StringReplace(sourceCopy, "#define N_PROGRAM 0", "#define N_PROGRAM " + std::to_string(n));
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
        String log2 = getLog(1, program);
        if (getStatus(program, GL_LINK_STATUS) != 1) {
            glGetError();
            glDeleteProgram(program);
            glDeleteShader(shader1);
            return StringFormat("Link error: %s\n", StringAsCStr(log2));
        }
        return program;
    };
    auto programSynth = compileProgram(1);
    if (std::holds_alternative<String>(programSynth)) {
        return std::get<String>(programSynth);
    }
    auto programWaveform = compileProgram(0);
    if (std::holds_alternative<String>(programWaveform)) {
        return std::get<String>(programWaveform);
    }
    
    shader_gpu_compute result;
    result.programSynth = std::get<GLuint>(programSynth);
    result.programSampleWaveform = std::get<GLuint>(programWaveform);
    result.blocksize = defs.blocksize;
    result.channels = defs.channels;
    result.polyVoices = defs.polyVoices;
    result.unisonVoices = defs.unisonVoices;
    return result;
}

inline std::variant<shader_gpu_compute, String> loadshader(const shader_gpu_compute_defs_t& defs, shader_gpu_compute& previous) {
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
    if (previous.programSynth != 0) {
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
    StrUtil::StringReplace(file0Source, "#define N_CHANNELS 2", "#define N_CHANNELS " + std::to_string(defs.channels));
    StrUtil::StringReplace(file0Source, "#define N_SAMPLES 512", "#define N_SAMPLES " + std::to_string(defs.blocksize));
    sourcefiles[0].source += sourcefiles[1].source;
    auto newShader = compile_shader(sourcefiles[0], defs);
    if (std::holds_alternative<shader_gpu_compute>(newShader)) {
        previous.destroy();
    }
    return newShader;
}

inline std::variant<shader_gpu_compute, String> loadshader_synth(shader_gpu_compute_defs_t defs, shader_gpu_compute& previous) {
    static int64_t lastModTimeGpuSoundShader = 0;
    String filenameGpuSoundShader = "shaders/gpu_synth.glsl";
    auto glSourceLoader = std::make_unique<glshader_srcloader>();
    // check time of last modification
    using App::Platform::toResourcePath;
    int64_t timeDiskGpuSoundShader = FileTimeGetter(toResourcePath(filenameGpuSoundShader)).getWriteTimeI64();
    if (previous.programSynth != 0) {
        // if the file has not been modified, return the previous shader
        if (timeDiskGpuSoundShader == lastModTimeGpuSoundShader) {
            return previous;
        }
    }
    int work_grp_cnt[3];

    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &work_grp_cnt[0]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &work_grp_cnt[1]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &work_grp_cnt[2]);

    log_lf(Log::L_INFO, "max global (total) work group counts x:%i y:%i z:%i\n",
    work_grp_cnt[0], work_grp_cnt[1], work_grp_cnt[2]);
    int work_grp_size[3];

    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &work_grp_size[0]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &work_grp_size[1]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &work_grp_size[2]);

    log_lf(Log::L_INFO, "max local (in one shader) work group sizes x:%i y:%i z:%i\n",
    work_grp_size[0], work_grp_size[1], work_grp_size[2]);
    int work_grp_inv;
    glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &work_grp_inv);
    log_lf(Log::L_INFO, "max local work group invocations %i\n", work_grp_inv);
    lastModTimeGpuSoundShader = timeDiskGpuSoundShader;
    if (!glSourceLoader->addStageSrc(GL_COMPUTE_SHADER, filenameGpuSoundShader.c_str(), 0) || glSourceLoader->sources.empty()) {
        log_lf(Log::L_ERROR, "Failed to load compute shader source file %s", StringAsCStr(filenameGpuSoundShader));
        return previous;
    }
    auto& sourcefiles = glSourceLoader->sources;
    auto& file0Source = sourcefiles[0].source;
    StrUtil::StringReplace(file0Source, "#define N_CHANNELS 0", "#define N_CHANNELS " + std::to_string(defs.channels));
    StrUtil::StringReplace(file0Source, "#define N_SAMPLES 0", "#define N_SAMPLES " + std::to_string(defs.blocksize));
    StrUtil::StringReplace(file0Source, "#define NUM_POLY_VOICES 0", "#define NUM_POLY_VOICES " + std::to_string(defs.polyVoices));
    StrUtil::StringReplace(file0Source, "#define NUM_UNISON_VOICES 0", "#define NUM_UNISON_VOICES " + std::to_string(defs.unisonVoices));
    auto res = compile_shader(sourcefiles[0], defs);
    if (std::holds_alternative<shader_gpu_compute>(res)) {
        previous.destroy();
    }
    return res;
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
    double unison_voice_count;
    double unison_detune;
    double bleb_duration;
};

} // namespace PluginSynth::GPU
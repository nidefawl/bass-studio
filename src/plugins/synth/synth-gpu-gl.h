#pragma once

#include "fileio.h"
#include "gl/gl_shader.h"
#include "glheaders.h"
#include "logging.h"
#include "platform.h"
#include "str_util.h"
#include "types.h"
#include <cstdint>
#include <variant>

namespace PluginSynth::GPU {

struct shader_gpu_compute_defs_t {
    samplecount_t blocksize = 512;
    samplecount_t channels = 2;
    samplecount_t polyVoices = 16;
    samplecount_t unisonVoices = 4;
};

struct shader_gpu_program_desc_t {
    int32_t programNr;
    String name;
    String def;
};
#define MAX_PROGRAMS 1024
struct shader_gpu_compute : public shader_gpu_compute_defs_t {
    std::array<shader_gpu_program_desc_t, MAX_PROGRAMS> programDescs{};
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

inline std::variant<shader_gpu_compute, String> compile_shader(const glshader_src& src, const shader_gpu_compute_defs_t& defs) {
    auto compileProgram = [](const auto& src, int32_t programNr, bool isWaveformSampler) ->  std::variant<GLuint, String> {
        String sourceCopy = src;
        GLuint shader1 = glCreateShader(GL_COMPUTE_SHADER);
        StrUtil::StringReplace(sourceCopy, "#define N_PROGRAM 0", "#define N_PROGRAM " + std::to_string(programNr));
        if (isWaveformSampler) {
            StrUtil::StringReplace(sourceCopy, "#define IS_WAVEFORM_SAMPLER 0", "#define IS_WAVEFORM_SAMPLER 1");
        }
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
    shader_gpu_compute result;
    auto& glslSourceCode = src.source;
    // find #define PROGRAM_NAME_.* ".*"\n using string find, not regex
    size_t pos = 0;
    int32_t programNr = 0;
    size_t posLastProcessedDefine = 0;
    while ((pos = glslSourceCode.find("#define PROGRAM_NAME_", pos)) != String::npos) {
        size_t start = pos + 21;
        size_t lineEnd = glslSourceCode.find("\n", start);
        size_t beginName = glslSourceCode.find("\"", start);
        if (beginName+1 > lineEnd || beginName == String::npos) {
            break;
        }
        beginName++;
        size_t endName = glslSourceCode.find("\"", beginName);
        if (endName > lineEnd || endName == String::npos) {
            break;
        }
        pos = endName + 1;
        posLastProcessedDefine = pos;
        String defName = glslSourceCode.substr(start, beginName - start - 1);
        String name = glslSourceCode.substr(beginName, endName - beginName);
        shader_gpu_program_desc_t desc;
        desc.programNr = programNr;
        desc.name = name;
        desc.def = defName;
        log_lf(Log::L_DEBUG, "Found program %d %s %s\n", programNr, StringAsCStr(name), StringAsCStr(defName));
        result.programDescs[programNr] = desc;
        programNr++;
        if (programNr >= MAX_PROGRAMS) {
            break;
        }
    }
    if (posLastProcessedDefine == 0 || programNr == 0) {
        return String("No program names found in shader source");
    }
    /* place a #define PROGRAM_<NAME> <NR> in the source code */
    String defBlock = "";
    for (int32_t i = 0; i < programNr; i++) {
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
    for (int32_t i = 0; i < programNr; i++) {
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
    result.numPrograms = programNr;
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
    if (previous.programs[0]) {
        // if the file has not been modified, return the previous shader
        if (timeDiskGpuSoundShader == lastModTimeGpuSoundShader) {
            return previous;
        }
    }
    lastModTimeGpuSoundShader = timeDiskGpuSoundShader;
    if (!glSourceLoader->addStageSrc(GL_COMPUTE_SHADER, filenameGpuSoundShader.c_str(), 0) || glSourceLoader->sources.empty()) {
        log_lf(Log::L_ERROR, "Failed to load compute shader source file %s", StringAsCStr(filenameGpuSoundShader));
        return previous;
    }
    auto& sourcefiles = glSourceLoader->sources;
    auto& file0Source = sourcefiles[0].source;
    StrUtil::StringReplace(file0Source, "#define N_CHANNELS 0", "#define N_CHANNELS " + std::to_string(defs.channels));
    StrUtil::StringReplace(file0Source, "#define N_SAMPLES 0", "#define N_SAMPLES " + std::to_string(defs.blocksize));
    StrUtil::StringReplace(file0Source, "#define N_POLY_VOICES 0", "#define N_POLY_VOICES " + std::to_string(defs.polyVoices));
    StrUtil::StringReplace(file0Source, "#define N_UNISON_VOICES 0", "#define N_UNISON_VOICES " + std::to_string(defs.unisonVoices));
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
    double osc1_unison_detune;
    double osc1_filter;
    double osc1_stereo;
    double osc1_pw;
    double osc1_pw_mod_rate;
    double osc1_pw_mod_depth;
    double osc1_filter_keytrack;
    double osc1_detune_keytrack;
    double osc1_width_keytrack;
};

/* keep in sync with shader defines */
static constexpr size_t NUM_VOICE_INPUT_PARAMETERS = 3;
static constexpr size_t NUM_SYNTH_INPUT_PARAMETERS = 2;
/*

struct voice_state_input_t {
    float velocity[N_SAMPLES];
    float pitch[N_SAMPLES];
};

struct synth_state_input_t {
    float param_bleb[N_SAMPLES];
    float param_stereo[N_SAMPLES];
    float param_pw[N_SAMPLES];
    float param_filter_keytrack[N_SAMPLES];
    float param_detune_keytrack[N_SAMPLES];
    float param_width_keytrack[N_SAMPLES];
};

*/

} // namespace PluginSynth::GPU
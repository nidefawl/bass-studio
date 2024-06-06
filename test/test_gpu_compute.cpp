#include "TestBase.hpp"
#include "appsettings.h"
#include "fileio.h"
#include "gl/gl_shader.h"
#include "gl/gl_util.h"
#include "host/audiobuffer/audiobuffer.h"
#include "host/audiohost/audio_host.h"
#include "logging.h"
#include "math/mat.h"
#include "math/seq_math.h"
#include "platform.h"
#include "rand.h"
#include "samplerate.h"
#include "str_util.h"
#include "thread.h"
#include "buildinfo.h"
#include "types.h"
#include "appconfig.h"
#include "assert_dbg.h"
#include "event.h"
#include "glheaders.h"
#include "hires_timer.h"
#include "host/plugin/modules.h"
#include "platform/linux/windowsize.h"
#include "tls.h"
#include "util/profiling.h"
#include <GLFW/glfw3.h>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <glm/gtc/type_ptr.hpp>
#include "types.h"
#include "plugins/synth/synth-gpu-gl.h"



#ifdef _WIN32
static GLFWwindow* window = nullptr;
static BOOL WINAPI CtrlCHandler(DWORD dwType) {
    if (window) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
    return true;
}
#endif

static void error_callback(int error, const char* description)
{
    fprintf(stderr, "Error: %s\n", description);
    if (window) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}
namespace PluginSynth::GPU {
class gpu_compute_test {
public:
    static constexpr uint16_t NUM_POLY_VOICES   = 32;
    static constexpr uint16_t NUM_UNISON_VOICES = 32;
    sampleformat_t sampleFormat;
    gpu_compute_context_t gpuContext{};

    std::vector<float> inputBuffer;
    std::vector<float> outputBuffer;
    std::vector<float> outputBufferWaveform;

    int32_t currentProgramId = 0;
    int32_t currentProgram() {
        return currentProgramId;
    }

    GLuint ubo = 0;
    ssbo_ringbuffer_t<4> ssboInput{};
    ssbo_ringbuffer_t<4> ssboOutput{};
    ssbo_ringbuffer_t<4> ssboOutputWaveform{};
    PluginSynth::GPU::gpu_program gpuProgram{};

    hires_timer_t perfTimer;
    int64_t timePerfLog = 0;
    int64_t timeCheckShader = 0;
    int64_t timeLastShaderError = 0;
    double timeComputeAvg = -2.0;
public:
    gpu_compute_test(const sampleformat_t& format, double tempo100, double unisonVoiceCount, double unisonDetune, double blebDuration) 
        : sampleFormat(format)
    {
        gpuContext.bpm = tempo100 / 100.0;
        gpuContext.unison_voice_count = unisonVoiceCount;
        gpuContext.unison_detune = unisonDetune;
        gpuContext.one_over_samplerate = 1.0 / sampleFormat.sampleRate;
    }
    
    void initGlResources() {
        glGenBuffers(1, &ubo);
        glBindBuffer(GL_UNIFORM_BUFFER, ubo);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(gpu_compute_context_t), &gpuContext, GL_STREAM_DRAW);
        checkGLError("glBufferData");
        ssboInput.genBuffers();
        ssboOutput.genBuffers();
        ssboOutputWaveform.genBuffers();
        checkGLError("genBuffers");

        auto res = loadGPUProgram(gpu_program_definitions_t{currentProgram(), sampleFormat.blockSize, 2, NUM_POLY_VOICES, NUM_UNISON_VOICES}, this->gpuProgram);
        checkGLError("loadGPUProgram");
        if (std::holds_alternative<String>(res)) {
            log_lf(Log::L_ERROR, "%s\n", std::get<String>(res).c_str());
        } else {
            this->gpuProgram = std::get<gpu_program>(res);
        }
        timeCheckShader = getTimeMillis();
        timePerfLog = getTimeMillis();

        inputBuffer.resize(gpuProgram.blocksize * NUM_POLY_VOICES * NUM_VOICE_INPUT_PARAMETERS);
        outputBuffer.resize(gpuProgram.blocksize * gpuProgram.channels);
        outputBufferWaveform.resize(gpuProgram.blocksize);

        for (size_t i = 0; i < ssboInput.size(); ++i) {
            ssboInput.allocate(inputBuffer.size() * sizeof(float), GL_DYNAMIC_DRAW);
            ssboInput.incrementFrame();
        }
        for (size_t i = 0; i < ssboOutput.size(); ++i) {
            ssboOutput.allocate(outputBuffer.size() * sizeof(float), GL_DYNAMIC_DRAW);
            ssboOutput.incrementFrame();
        }
        for (size_t i = 0; i < ssboOutputWaveform.size(); ++i) {
            ssboOutputWaveform.allocate(outputBufferWaveform.size() * sizeof(float), GL_DYNAMIC_DRAW);
            ssboOutputWaveform.incrementFrame();
        }
        checkGLError("glBufferData");

        glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssboInput.current());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssboOutput.current());
        checkGLError("glBindBufferBase");
    }
    void releaseGlResources() {
        glUseProgram(0);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, 0);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
        ssboInput.destroy();
        ssboOutput.destroy();
        ssboOutputWaveform.destroy();
        gpuProgram.destroy();
        glDeleteBuffers(1, &ubo);
        ubo = 0;
    }

    void processGpuSynth(AudioBlock* audioblock) {
        if (!glad_glDispatchCompute) {
            return;
        }
        if (gpuProgram.blocksize != audioblock->samples || !gpuProgram.is_valid() || gpuProgram.channels != audioblock->channels) {
            gpuProgram = {};
        }
        auto curContext = glfwGetCurrentContext();
        if (curContext != window)
            glfwMakeContextCurrent(window);
        checkGLError("glfwMakeContextCurrent");
        auto tmNow_ms = getTimeMillis();
        if (!gpuProgram.is_valid() || tmNow_ms - timeCheckShader > 1000) {
            if (tmNow_ms - timeLastShaderError > 333) {
                timeCheckShader = tmNow_ms;
                auto res = loadGPUProgram(gpu_program_definitions_t{currentProgram(), audioblock->samples, audioblock->channels, NUM_POLY_VOICES, NUM_UNISON_VOICES}, gpuProgram);
                if (std::holds_alternative<String>(res)) {
                    log_lf(Log::L_ERROR, "%s\n", std::get<String>(res).c_str());
                    timeLastShaderError = tmNow_ms;
                } else {
                    gpuProgram = std::get<gpu_program>(res);
                }
            }
        }
        perfTimer.reset();
        audioblock->clear();
        inputBuffer.resize(gpuProgram.blocksize * NUM_POLY_VOICES * NUM_VOICE_INPUT_PARAMETERS);
        outputBuffer.resize(gpuProgram.blocksize * gpuProgram.channels);
        outputBufferWaveform.resize(gpuProgram.blocksize);

        for (int s = 0; s < gpuProgram.blocksize; s++) {
            auto absTime = gpuContext.time_samples + s;
            for (size_t i = 0; i < NUM_POLY_VOICES; i++) {
                auto note_base = 60;
                auto tmSecs = (absTime * gpuContext.one_over_samplerate) + i * 0.3;
                tmSecs = fmod(tmSecs, 6.0);
                boolean bIsNoteActive = tmSecs < 2.0;
                auto idx_is_active = i * (NUM_VOICE_INPUT_PARAMETERS * gpuProgram.blocksize) + s;
                auto idx_velocity = idx_is_active + gpuProgram.blocksize;
                auto idx_pitch = idx_velocity + gpuProgram.blocksize;
                auto idx_bleb = idx_pitch + gpuProgram.blocksize;
                inputBuffer[idx_is_active] = float(bIsNoteActive);
                inputBuffer[idx_pitch] = 1000.0 + i * 100.0;
                inputBuffer[idx_velocity] = 0.5;
                inputBuffer[idx_bleb] = 2.0;
            }
        }

        ssboInput.uploadBuffer(inputBuffer.data(), inputBuffer.size() * sizeof(float));

        checkGLError("glBindBuffer");
        glBindBuffer(GL_UNIFORM_BUFFER, ubo);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(gpu_compute_context_t), &gpuContext, GL_STREAM_DRAW);
        checkGLError("glBufferData");
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssboInput.current());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssboOutput.current());
        checkGLError("glBindBufferBase");
        if (gpuProgram.programSynth) {
            glUseProgram(gpuProgram.programSynth);
            checkGLError("glUseProgram");
            glDispatchCompute(1, 1, 1);
            checkGLError("glDispatchCompute");
        }

        glBindBuffer(GL_UNIFORM_BUFFER, ubo);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(gpu_compute_context_t), &gpuContext, GL_STREAM_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssboOutputWaveform.current());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);


        if (gpuProgram.programSampleWaveform) {
            glUseProgram(gpuProgram.programSampleWaveform);
            checkGLError("glBufferData");
            glDispatchCompute(1, 1, 1);
        }

        ssboOutput.downloadBufferDelayed(outputBuffer.data(), outputBuffer.size() * sizeof(float));
        ssboOutputWaveform.downloadBufferDelayed(outputBufferWaveform.data(), outputBufferWaveform.size() * sizeof(float));


        ssboInput.incrementFrame();
        ssboOutput.incrementFrame();
        ssboOutputWaveform.incrementFrame();

        // copy nFrames to outputs
        for (samplecount_t ch = 0; ch < audioblock->channels; ch++) {
            for (samplecount_t sampleIdx = 0; sampleIdx < audioblock->samples; sampleIdx++) {
                float val = outputBuffer[sampleIdx + ch * audioblock->samples];
                if (fp_math::isNanOrInfd(val)) {
                    val = 0;
                } else if (val < -1.5) {
                    val = -1.5;
                } else if (val > 1.5) {
                    val = 1.5;
                }
                audioblock->buf[ch][sampleIdx] = val;
            }
        }

        tmNow_ms = getTimeMillis();
        auto tmTotal_ms = perfTimer.getTimeDoubleReset() * 1000.0;
        if (tmNow_ms - timePerfLog >= 5000 || (tmTotal_ms > timeComputeAvg * 4.0 && timeComputeAvg > 0.3)) {
            if (timeComputeAvg < 0.0 && tmNow_ms - timePerfLog > 1500) {
                timeComputeAvg = tmTotal_ms;
            }
            timePerfLog = tmNow_ms;
            log_lf(Log::L_WARN, "gpu_compute_test: %f ms (avg: %f ms)\n", tmTotal_ms, timeComputeAvg);
            // print sample 0
            auto sample0 = outputBuffer[0];
            log_lf(Log::L_WARN, "sample 0: %f\n", sample0);
        }
        timeComputeAvg = 0.9 * timeComputeAvg + 0.1 * tmTotal_ms;
        if (curContext != window)
            glfwMakeContextCurrent(curContext);
    }
    void setTime(double samplePos) {
        gpuContext.time_samples = samplePos;
        gpuContext.time_seconds = samplePos * gpuContext.one_over_samplerate;
        gpuContext.time_beats = samplePos * gpuContext.one_over_samplerate * gpuContext.bpm / 60.0;
    }
};
} // namespace PluginSynth::GPU

int main(int, char*[]) {
    seqthreads::registerThread("mainthread", seqthreads::ThreadType::MainThread);
    try {
        App::Platform::initPlatformEnvironment(BuildInfo::PRODUCT_NAME_LOWER);
        auto& tls = daw_tls::initNewTls();
        loadSettings(*tls.settings);
        auto& settings = *tls.settings;

        std::vector<float> outputBuffer;
        glfwSetErrorCallback(error_callback);

        glfwInitHint(GLFW_COCOA_MENUBAR, GLFW_FALSE);

        if (!glfwInit())
            exit(EXIT_FAILURE);

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        // glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

        window = glfwCreateWindow(640, 480, "gpu sound test", NULL, NULL);
        if (!window)
        {
            glfwTerminate();
            exit(EXIT_FAILURE);
        }
        glfwMakeContextCurrent(window);
        if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
            throw appexception("Required OpenGL extensions not present.\nConsider updating graphics drivers");
        }
        // enableGlDebugCallback();
#ifdef _WIN32
        if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE) CtrlCHandler, TRUE)) {
            fprintf(stderr, "Unable to install handler!\n");
            return EXIT_FAILURE;
        }
#endif

        PluginSynth::GPU::gpu_compute_test test(sampleformat_t{
                                                    .sampleRate = 96000,
                                                    .blockSize = 1024,
                                                    .sampleformat = sampleformat_bits_t::FLOAT_32
                                                },
                                                120.0, // tempo100
                                                32.0,   // unisonVoiceCount
                                                -0.2,   // unisonDetune
                                                2.0    // blebDuration
                                                );
        test.initGlResources();
        auto timeStart = getTimeSecondsD();
        auto timeRenderStart = timeStart;
        auto timeLastPerfLog = timeStart;
        int32_t warmupBlocks = 0;
        double tmComputeAvg = -1.0;
        AudioBlock audioblock(2, test.sampleFormat.blockSize);
        samplecount_t samplePos = 0;
        samplecount_t numIterations = 0;
        double tStart = getTimeSecondsD();
        while(window && !glfwWindowShouldClose(window)) {
            glfwPollEvents();
            test.setTime(samplePos);
            test.processGpuSynth(&audioblock);
            samplePos += audioblock.samples;
            numIterations++;
            if (numIterations >= 22) {
                double tSince = getTimeSecondsD() - tStart;
                if (tSince > 1.5) {
                    tStart = getTimeSecondsD();
                    double blocksPerSeconds = numIterations / tSince;
                    double samplesPerSeconds = blocksPerSeconds * audioblock.samples;
                    log_lf(Log::L_WARN, "gpu_compute_test: %.0f samplesPerSeconds/s\n", samplesPerSeconds);
                    numIterations = 0;
                }
            }
        }
        test.releaseGlResources();

        if (window)
            glfwDestroyWindow(window);

    } catch (std::exception& e) {
        log_printf("exception %s\n", e.what());
        return 1;
    }
    glfwTerminate();
    exit(EXIT_SUCCESS);
}



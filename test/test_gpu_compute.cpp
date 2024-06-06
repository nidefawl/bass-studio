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


namespace DAW::GPU {
static constexpr uint16_t NUM_AUDIO_CHANNELS = 2;
static constexpr uint16_t NUM_POLY_VOICES   = 32;
static constexpr uint16_t MAX_UNISON_VOICES   = 32;

/* keep in sync with shader defines */
static constexpr size_t NUM_VOICE_INPUT_PARAMETERS = 3;
static constexpr size_t NUM_SYNTH_INPUT_PARAMETERS = 2;

class gpu_compute_test : public GPUAudioProcessor {
public:
    sampleformat_t sampleFormat;
    hires_timer_t perfTimer;
    int64_t timePerfLog = 0;
    int64_t timeCheckShader = 0;
    double timeComputeAvg = -2.0;
public:
    gpu_compute_test(const sampleformat_t& format, double tempo100) 
        : sampleFormat(format)
    {
        gpuContext.bpm = tempo100 / 100.0;
        gpuContext.one_over_samplerate = 1.0 / sampleFormat.sampleRate;
        gpuProgram.blocksize = 0;
        
    }
    void setBlocksize(blocksize_t blockSize) {
        sampleFormat.blockSize = blockSize;
        if (gpuProgram.blocksize != blockSize) {
            reloadShader({blockSize, NUM_AUDIO_CHANNELS, NUM_POLY_VOICES, MAX_UNISON_VOICES});
        }
        ssboInputSynthState.buffer.resize(blockSize * NUM_SYNTH_INPUT_PARAMETERS);
        ssboInputVoiceStates.buffer.resize(blockSize * NUM_POLY_VOICES * NUM_VOICE_INPUT_PARAMETERS);
        ssboOutput.buffer.resize(blockSize * gpuProgram.channels);
        ssboOutputWaveform.buffer.resize(blockSize);
        GPUAudioProcessor::reallocateSSBOs();
    }
    GLFWwindow* getWindow() {
        return window;
    }
    void processGpuSynth(AudioBlock* audioblock) {
        if (!glad_glDispatchCompute) {
            return;
        }
        TEST_ASSERT_EQUAL(audioblock->samples, sampleFormat.blockSize);
        GlfwContextSwitch ctxSwitch(window);
        checkGLError("GlfwContextSwitch");
        if (!gpuProgram.is_valid()) {
            gpuProgram.destroy();
            gpuProgram = {};
        }
        auto tmNow_ms = getTimeMillis();
        if (!gpuProgram.is_valid() || tmNow_ms - timeCheckShader > 1000) {
            if (tmNow_ms - timeLastShaderError > 2000) {
                timeCheckShader = tmNow_ms;
                reloadShader({sampleFormat.blockSize, NUM_AUDIO_CHANNELS, NUM_POLY_VOICES, MAX_UNISON_VOICES});
            }
        }

        audioblock->clear();

        const int nOversample = 1;
        // const auto bpm100 = host->prjGlobals.tempo100;

        const int32_t programId = currentProgramId % gpuProgram.programs.size();
        const auto sampleRate = sampleFormat.sampleRate;

        gpuContext.one_over_samplerate = 1.0 / sampleRate;
        // gpuContext.time_samples = hostInfo->m_vstTimeInfo.samplePos;
        // gpuContext.time_seconds = hostInfo->m_vstTimeInfo.samplePos * gpuContext.one_over_samplerate;
        // gpuContext.time_beats = hostInfo->m_vstTimeInfo.ppqPos;
        gpuContext.osc1_unison_voice_count = 32;
        gpuContext.osc1_unison_detune = 0.3f;
        gpuContext.osc1_filter = 0.8f;
        gpuContext.osc1_stereo = 0.5f;
        gpuContext.osc1_pw = 0.5f;
        gpuContext.osc1_pw_mod_rate = 0.002f;
        gpuContext.osc1_pw_mod_depth = 0.002f;
        gpuContext.osc1_filter_keytrack = 0.5f;
        gpuContext.osc1_detune_keytrack = 0.5f;
        gpuContext.osc1_width_keytrack = 0.5f;


        auto& inputBufferSynthState = ssboInputSynthState.buffer;
        auto& inputBufferVoiceStates = ssboInputVoiceStates.buffer;
        ssboInputSynthState.clearBuffer();
        ssboInputVoiceStates.clearBuffer();
        double osc1_filter = 0.7;
        double osc1_filter_keytrack = 0.0;
        double osc1_stereo = 0.0;
        for (int s = 0; s < gpuProgram.blocksize; s++) {
            auto absTime = gpuContext.time_samples + s;
            inputBufferSynthState[s + gpuProgram.blocksize * 0] = osc1_filter_keytrack;
            inputBufferSynthState[s + gpuProgram.blocksize * 1] = osc1_stereo;
            for (size_t i = 0; i < NUM_POLY_VOICES; i++) {
                const auto idx_base = i * (NUM_VOICE_INPUT_PARAMETERS * gpuProgram.blocksize);
                const auto idx_velocity = idx_base + s;
                float velocity = -1.0;
                auto note_base = 60;
                auto tmSecs = (absTime * gpuContext.one_over_samplerate) + i * 0.3;
                tmSecs = fmod(tmSecs, 6.0);
                bool bIsNoteActive = tmSecs < 2.0;
                if (bIsNoteActive) {
                    velocity = 0.5;
                    const double osc1Frequency = 440.0 * pow(2.0, (note_base - 69) / 12.0);
                    const auto idx_pitch    = idx_base + gpuProgram.blocksize * 1 + s;
                    const auto idx_filter   = idx_base + gpuProgram.blocksize * 2 + s;
                    inputBufferVoiceStates[idx_pitch]    = osc1Frequency;
                    inputBufferVoiceStates[idx_filter]   = 1.0-osc1_filter;
                }
                inputBufferVoiceStates[idx_velocity] = velocity;
            }
        }

        perfTimer.reset();
        ssboInputSynthState.uploadBuffer();
        ssboInputVoiceStates.uploadBuffer();
        checkGLError("glBufferData");

        glBindBuffer(GL_UNIFORM_BUFFER, ubo);
        checkGLError("glBindBuffer");
        glBufferData(GL_UNIFORM_BUFFER, sizeof(gpuContext), &gpuContext, GL_STREAM_DRAW);
        checkGLError("glBufferData");
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssboInputSynthState.ssbo.current());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssboInputVoiceStates.ssbo.current());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ssboOutput.ssbo.current());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, ssboOutputWaveform.ssbo.current());
        glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);

        checkGLError("glBindBufferBase");
        if (gpuProgram.programs[programId]) {
            glUseProgram(gpuProgram.programs[programId]);
            checkGLError("glUseProgram");
            glDispatchCompute(1, 1, 1);
            checkGLError("glDispatchCompute");
        }

        if (gpuProgram.programsWaveform[programId]) {
            glUseProgram(gpuProgram.programsWaveform[programId]);
            checkGLError("glBufferData");
            glDispatchCompute(1, 1, 1);
        }

        glFinish();
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT|GL_BUFFER_UPDATE_BARRIER_BIT);
        if (gpuProgram.programs[programId]) {
            ssboOutput.downloadBuffer();
        } else {
            ssboOutput.clearBuffer();
        }
        if (gpuProgram.programsWaveform[programId]) {
            ssboOutputWaveform.downloadBuffer();
        } else {
            ssboOutputWaveform.clearBuffer();
        }

        for (auto* buffer : hostBuffers) {
            buffer->incrementFrame();
        }

        const auto& outputBuffer = ssboOutput.buffer;
        // copy nFrames to outputs
        for (samplecount_t ch = 0; ch < NUM_AUDIO_CHANNELS; ch++) {
            for (samplecount_t sampleIdx = 0; sampleIdx < audioblock->samples; sampleIdx++) {
                float val = outputBuffer[sampleIdx + ch * audioblock->samples];
                float hardClipAt = 2.5;
                if (fp_math::isNanOrInfd(val)) {
                    val = 0;
                } else if (val < -hardClipAt) {
                    val = -hardClipAt;
                } else if (val > hardClipAt) {
                    val = hardClipAt;
                }
                audioblock->buf[ch][sampleIdx] = val;
            }
        }

        tmNow_ms = getTimeMillis();
        auto tmTotal_ms = perfTimer.getTimeDoubleReset() * 1000.0;
        if (tmNow_ms - timePerfLog >= 10000 || tmTotal_ms > timeComputeAvg * 10.0) {
            if (timeComputeAvg < 0.0) {
                if (tmNow_ms - timePerfLog > 1500)
                    timeComputeAvg = tmTotal_ms;
            } else {
                log_lf(Log::L_WARN, "gpu_compute_test: %f ms (avg: %f ms)\n", tmTotal_ms, timeComputeAvg);
                // print sample 0
                auto sample0 = outputBuffer[0];
                log_lf(Log::L_WARN, "sample 0: %f\n", sample0);
            }
            timePerfLog = tmNow_ms;
        }
        timeComputeAvg = 0.95 * timeComputeAvg + 0.05 * tmTotal_ms;
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

        glfwInitHint(GLFW_COCOA_MENUBAR, GLFW_FALSE);

        if (!glfwInit())
            exit(EXIT_FAILURE);

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        // glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

        auto sampleFormat = sampleformat_t{
            .sampleRate = 96000,
            .blockSize = 1024,
            .sampleformat = sampleformat_bits_t::FLOAT_32
        };
        DAW::GPU::gpu_compute_test test(sampleFormat, 120.0);
        test.initComputeContext(true);
        auto window = test.getWindow();
        GlfwContextSwitch ctxSwitch(window);
        test.setBlocksize(sampleFormat.blockSize);
        auto timeStart = getTimeSecondsD();
        auto timeRenderStart = timeStart;
        auto timeLastPerfLog = timeStart;
        int32_t warmupBlocks = 0;
        double tmComputeAvg = -1.0;
        AudioBlock audioblock(2, test.sampleFormat.blockSize);
        samplecount_t samplePos = 0;
        samplecount_t numIterations = 0;
        double tStart = getTimeSecondsD();
        TEST_ASSERT_THROW(window != nullptr);
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
    } catch (std::exception& e) {
        log_printf("exception %s\n", e.what());
        return 1;
    }
    glfwTerminate();
    exit(EXIT_SUCCESS);
}



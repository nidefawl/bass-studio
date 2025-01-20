#include "TestBase.hpp"
#include "appsettings.hpp"
#include "gl/gl_util.hpp"
#include "logging.hpp"
#include "math/seq_math.hpp"
#include "platform.hpp"
#include "samplerate.hpp"
#include "str_util.hpp"
#include "thread.hpp"
#include "buildinfo.h"
#include "types.hpp"
#include "glheaders.h"
#include "hires_timer.hpp"
#include "tls.hpp"
#include <GLFW/glfw3.h>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <glm/gtc/type_ptr.hpp>
#include "types.hpp"
#include "plugins/synth/synth-gpu-gl.hpp"
#include "plugins/synth/synth-gpu-parameters.hpp"
#include "host/audiobuffer/audiobuffer.hpp"


namespace DAW::GPU {
using namespace PluginSynth::GPU;
class gpu_compute_test : public GPUAudioProcessor {
public:
    sampleformat_t sampleFormat;
    int32_t userLimitPolyVoices;
    int32_t userLimitUnisonVoices;
    hires_timer_t perfTimer;
    int64_t timePerfLog = 0;
    int64_t timeCheckShader = 0;
    double timeComputeAvg = -2.0;
    size_t numActiveVoicesBlock = 0;
    size_t numActiveVoicesMax = 0;
    AudioBlock audioOutputBuffer;
public:
    gpu_compute_test(const sampleformat_t& format, double tempo100, int32_t maxPolyVoices, int32_t maxUnisonVoices) 
        : sampleFormat(format),
        userLimitPolyVoices(math::min<int32_t>(MAX_POLY_VOICES, maxPolyVoices)),
        userLimitUnisonVoices(math::min<int32_t>(MAX_UNISON_VOICES, maxUnisonVoices)),
        audioOutputBuffer(NUM_AUDIO_CHANNELS, format.blockSize)
    {
        gpuContext.bpm = tempo100 / 100.0;
        gpuContext.one_over_samplerate = 1.0 / sampleFormat.sampleRate;
        gpuProgram.blocksize1024Fixed = 0;
        
    }
    void setBlocksize(samplecount_t blockSize) {
        sampleFormat.blockSize = blockSize;
        audioOutputBuffer.realloc(blockSize);
    }
    GLFWwindow* getWindow() {
        return window;
    }
    void processGpuSynth() {
        if (!glad_glDispatchCompute) {
            return;
        }
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
                reloadShader({sampleFormat.blockSize, NUM_AUDIO_CHANNELS, MAX_POLY_VOICES, MAX_UNISON_VOICES});
            }
        }

        const int32_t programMax = math::max(1, CtrSize(gpuProgram.programs));
        const int32_t programId = currentProgramId % programMax;
        const auto sampleRate = sampleFormat.sampleRate;

        gpuContext.one_over_samplerate = 1.0 / sampleRate;
        gpuContext.osc1_unison_voice_count = 16;
        gpuContext.osc1_filter = 0.8f;
        gpuContext.osc1_pw = 0.5f;
        gpuContext.osc1_pw_mod_rate = 0.002f;
        gpuContext.osc1_pw_mod_depth = 0.002f;
        gpuContext.osc1_width_keytrack = 0.5f;

        auto& inputBufferSynthState = ssboInputSynthState.buffer;
        auto& inputBufferVoiceStates = ssboInputVoiceStates.buffer;
        ssboInputSynthState.clearBuffer();
        ssboInputVoiceStates.clearBuffer();
        double osc1_filter = 0.7;
        double osc1_filter_keytrack = 0.0;
        double osc1_stereo = 0.0;
        ssize_t minVoiceIdx = -1;
        ssize_t maxVoiceIdx = -1;
        for (int s = 0; s < userLimitPolyVoices && s < allocatedVoiceCount; s++) {
            auto absTime = gpuContext.time_samples + s;
            inputBufferSynthState[s + gpuProgram.blocksize1024Fixed * 0] = float(osc1_filter_keytrack);
            inputBufferSynthState[s + gpuProgram.blocksize1024Fixed * 1] = float(osc1_stereo);
            for (ssize_t i = 0; i < MAX_POLY_VOICES; i++) {
                const auto idx_base = i * (NUM_VOICE_INPUT_PARAMETERS * gpuProgram.blocksize1024Fixed);
                float velocity = -1.0;
                auto note_base = 60;
                auto tmSecs = (absTime * gpuContext.one_over_samplerate) + i * 1.789;
                tmSecs = fmod(tmSecs, 5.5);
                bool bIsNoteActive = tmSecs < 0.6;
                if (bIsNoteActive) {
                    if (minVoiceIdx < 0 || i < minVoiceIdx) {
                        minVoiceIdx = i;
                    }
                    if (maxVoiceIdx < 0 || i > maxVoiceIdx) {
                        maxVoiceIdx = i;
                    }
                    velocity = 0.5;
                    const double osc1Frequency = 440.0 * pow(2.0, (note_base - 69) / 12.0);
                    const auto idx_velocity = idx_base + s;
                    const auto idx_pitch    = idx_base + gpuProgram.blocksize1024Fixed * 1 + s;
                    const auto idx_filter   = idx_base + gpuProgram.blocksize1024Fixed * 2 + s;
                    const auto idx_detune   = idx_base + gpuProgram.blocksize1024Fixed * 3 + s;
                    const auto idx_detune_keytrack = idx_base + gpuProgram.blocksize1024Fixed * 4 + s;
                    inputBufferVoiceStates[idx_velocity] = velocity;
                    inputBufferVoiceStates[idx_pitch]    = float(osc1Frequency);
                    inputBufferVoiceStates[idx_filter]   = float(1.0 - osc1_filter);
                    inputBufferVoiceStates[idx_detune]   = 1.0;
                    inputBufferVoiceStates[idx_detune_keytrack]   = 0.5;
                }
            }
        }
        size_t numActiveVoicesBlock = minVoiceIdx == -1 ? 0 : maxVoiceIdx - minVoiceIdx + 1;
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

        TEST_ASSERT_EQUAL(audioOutputBuffer.samples, sampleFormat.blockSize);
        const auto& outputBuffer = ssboOutput.buffer;
        // copy nFrames to outputs
        for (samplecount_t ch = 0; ch < NUM_AUDIO_CHANNELS; ch++) {
            for (samplecount_t sampleIdx = 0; sampleIdx < audioOutputBuffer.samples; sampleIdx++) {
                auto idx = size_t(sampleIdx + ch * audioOutputBuffer.samples);
                TEST_ASSERT_THROW(idx < outputBuffer.size());
                float val = outputBuffer[idx];
                float hardClipAt = 2.5;
                if (fp_math::isNanOrInfd(val)) {
                    val = 0;
                } else if (val < -hardClipAt) {
                    val = -hardClipAt;
                } else if (val > hardClipAt) {
                    val = hardClipAt;
                }
                audioOutputBuffer.buf[ch][sampleIdx] = val;
            }
        }

        tmNow_ms = getTimeMillis();
        auto tmTotal_ms = perfTimer.getTimeDoubleReset() * 1000.0;
        if (tmNow_ms - timePerfLog >= 10000 || tmTotal_ms > timeComputeAvg * 10.0) {
            if (timeComputeAvg < 0.0) {
                if (tmNow_ms - timePerfLog > 1500)
                    timeComputeAvg = tmTotal_ms;
            } else {
                log_lf(Log::L_WARN, "gpu_compute_test: %f ms (avg: %f ms) (active voices: %zu)\n", tmTotal_ms, timeComputeAvg, numActiveVoicesBlock);
                // print sample 0
                auto sample0 = outputBuffer[0];
                log_lf(Log::L_WARN, "sample 0: %f\n", sample0);
            }
            timePerfLog = tmNow_ms;
        }
        timeComputeAvg = 0.95 * timeComputeAvg + 0.05 * tmTotal_ms;
    }
    void setTime(samplecount_t samplePos) {
        double dSamplePos = double(samplePos);
        gpuContext.time_samples = dSamplePos;
        gpuContext.time_seconds = dSamplePos * gpuContext.one_over_samplerate;
        gpuContext.time_beats = dSamplePos * gpuContext.one_over_samplerate * gpuContext.bpm / 60.0;
    }
    void reloadProgram() {
        numActiveVoicesMax = 0;
        GlfwContextSwitch ctxSwitch(window);
        if (!gpuProgram.is_valid()) {
            reloadShader({ GPU_BLOCK_SIZE, NUM_AUDIO_CHANNELS, userLimitPolyVoices, userLimitUnisonVoices });
            timeCheckShader = getTimeMillis();
        }
        allocatedVoiceCount = userLimitPolyVoices;
        audioOutputBuffer.realloc(gpuProgram.blocksize1024Fixed);
        ssboInputSynthState.buffer.resize(gpuProgram.blocksize1024Fixed * size_t(NUM_SYNTH_INPUT_PARAMETERS));
        ssboInputVoiceStates.buffer.resize(gpuProgram.blocksize1024Fixed * size_t(userLimitPolyVoices) * size_t(NUM_VOICE_INPUT_PARAMETERS));
        ssboOutput.buffer.resize(gpuProgram.blocksize1024Fixed * gpuProgram.channels);
        ssboOutputWaveform.buffer.resize(gpuProgram.blocksize1024Fixed);
        GPUAudioProcessor::reallocateSSBOs();
    }

};
} // namespace PluginSynth::GPU

int main(int, char*[]) {
    seqthreads::registerThread("mainthread", seqthreads::ThreadType::MainThread);
    try {
        App::Platform::initPlatformEnvironment(BuildInfo::PRODUCT_NAME_LOWER);
        auto& tls = daw_tls::initNewTls();
        loadSettings(*tls.settings);
        std::vector<float> outputBuffer;

        glfwInitHint(GLFW_COCOA_MENUBAR, GLFW_FALSE);

        if (!glfwInit())
            return 1;

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        // glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        auto sampleFormat = sampleformat_t{
            .sampleRate = 44100,
            .blockSize = 1024,
            .sampleformat = sampleformat_bits_t::FLOAT_32
        };
        DAW::GPU::gpu_compute_test test(sampleFormat, 120.0, PluginSynth::GPU::MAX_POLY_VOICES, PluginSynth::GPU::MAX_UNISON_VOICES);
        test.initComputeContext(true);
        test.reloadProgram();

        auto window = test.getWindow();
        GlfwContextSwitch ctxSwitch(window);
        samplecount_t samplePos = 0;
        samplecount_t numIterations = 0;
        double tmStart = getTimeSecondsD();
        double tmStats = getTimeSecondsD();
        TEST_ASSERT_THROW(window != nullptr);
        TEST_ASSERT_EQUAL(test.audioOutputBuffer.samples, sampleFormat.blockSize);
        while(window && !glfwWindowShouldClose(window)) {
            TEST_ASSERT_EQUAL(test.audioOutputBuffer.samples, sampleFormat.blockSize);
            glfwPollEvents();
            test.setTime(samplePos);
            test.processGpuSynth();
            samplePos += sampleFormat.blockSize;
            numIterations++;
            auto tmNow = getTimeSecondsD();
            if (numIterations >= 22) {
                double tSince = tmNow - tmStats;
                if (tSince > 1.5) {
                    tmStats = tmNow;
                    double blocksPerSeconds = numIterations / tSince;
                    double samplesPerSeconds = blocksPerSeconds * sampleFormat.blockSize;
                    log_lf(Log::L_WARN, "gpu_compute_test: %.0f samples/second\n", samplesPerSeconds);
                    numIterations = 0;
                }
            }
            if (tmNow - tmStart >= 10.0) {
                log_lf(Log::L_INFO, "END: Time: %.3f s\n", tmNow);
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }
        }
        test.releaseGlResources();
    } catch (std::exception& e) {
        log_printf("exception %s\n", e.what());
        return 1;
    }
    glfwTerminate();
    return 0;
}



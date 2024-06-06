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

int main(int, char*[]) {
    seqthreads::registerThread("mainthread", seqthreads::ThreadType::MainThread);
    try {
        App::Platform::initPlatformEnvironment(BuildInfo::PRODUCT_NAME_LOWER);
        auto& tls = daw_tls::initNewTls();
        loadSettings(*tls.settings);
        auto& settings = *tls.settings;
        auto audioHost = std::make_unique<audiohost>();

        std::vector<float> outputBuffer;
        glfwSetErrorCallback(error_callback);

        glfwInitHint(GLFW_COCOA_MENUBAR, GLFW_FALSE);

        if (!glfwInit())
            exit(EXIT_FAILURE);

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

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

        struct context {
            float samplerate;
            float bpm;
            float time_seconds;
            float time_samples;
            float time_beats;
        } ctxt;

        GLuint ubo = 0;
        GLuint ssbo = 0;
        GLuint ssbo2 = 0;
        glGenBuffers(1, &ubo);
        glBindBuffer(GL_UNIFORM_BUFFER, ubo);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(context), nullptr, GL_STREAM_DRAW);
        glGenBuffers(1, &ssbo);
        glGenBuffers(1, &ssbo2);
        GLuint query;
        glGenQueries(1, &query);

        checkGLError("glBindBuffer");

        audioHost->startAudio(settings.iosettings);
        DAW::GPU::gpu_program shader{};
        {
            auto stream =audioHost->getStreamSharedPtr(0);
#ifdef _WIN32
            if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE) CtrlCHandler, TRUE)) {
                fprintf(stderr, "Unable to install handler!\n");
                return EXIT_FAILURE;
            }
#endif
            sampleformat_t sampleFormat = {
                .sampleRate = stream->getSampleRate(),
                .blockSize = stream->getBlockSize(),
                .sampleformat = sampleformat_bits_t::FLOAT_32
            };
            channelnum_t channels = stream->getNumOutputChannels();
            seq_rand noiseRng;
            constexpr size_t LOCAL_RING_BUF_SIZE = 128;
            struct local_ringbuffer_t {
                uint32_t writePos = 0;
                AudioBuffer* buffers[LOCAL_RING_BUF_SIZE] = { 0 };
            } ringbuffer;
            for (auto& buffer : ringbuffer.buffers) {
                buffer = allocateBuffer(channels);
            }
            ctxt.samplerate = sampleFormat.sampleRate;
            ctxt.bpm = 120;
            ctxt.time_samples = 0;
            ctxt.time_seconds = 0;
            ctxt.time_beats = 0;
            ctxt.time_samples = sampleFormat.blockSize * 4444;
            ctxt.time_seconds = ctxt.time_samples / ctxt.samplerate;
            ctxt.time_beats = ctxt.time_samples / (ctxt.samplerate * 60 / ctxt.bpm);
            DAW::GPU::gpu_program_definitions_t defs = {
                .blocksize = sampleFormat.blockSize,
                .channels = channels,
                .polyVoices = 32,
                .unisonVoices = 32
            };
            auto res = DAW::GPU::loadshader(defs, shader);
            if (std::holds_alternative<String>(res)) {
                log_lf(Log::L_ERROR, "%s\n", std::get<String>(res).c_str());
            } else {
                shader = std::get<DAW::GPU::gpu_program>(res);
            }
            auto timeStart = getTimeSecondsD();
            auto timeRenderStart = timeStart;
            auto timeLastPerfLog = timeStart;
            int32_t warmupBlocks = 0;
            double tmComputeAvg = -1.0;
            while(window && !glfwWindowShouldClose(window)) {
                glfwPollEvents();
                do {
                    if (stream->getOutputQueueSize() < LOCAL_RING_BUF_SIZE / 2)
                        break;
                    seqthreads::threadSleepMicros(200);
                } while(true);

                AudioBuffer* ptrExternalInputs = nullptr;
                while (stream->try_dequeueInput(ptrExternalInputs)) {
                    TEST_ASSERT_THROW(ptrExternalInputs != nullptr);
                    ptrExternalInputs->inUse = false;
                }

                auto buf = ringbuffer.buffers[ringbuffer.writePos%LOCAL_RING_BUF_SIZE];
                dbgassert(!buf->inUse);
                buf->inUse = true;
                ringbuffer.writePos++;
                auto audioblock = buf->output;
                audioblock->realloc(sampleFormat.blockSize);
                audioblock->clear();
                audioblock->fillNoise(noiseRng, 0.05);

                outputBuffer.resize(audioblock->samples * channels);
                // audioblock->SubChannelsBlock(0, 1).clear();
                for (samplecount_t i = 0; i < channels; i++) {
                    memcpy(&outputBuffer[i * audioblock->samples], audioblock->buf[i], audioblock->samples * sizeof(float));
                }
                audioblock->clear();
                glBeginQuery(GL_TIME_ELAPSED, query);
                glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
                glBufferData(GL_SHADER_STORAGE_BUFFER, outputBuffer.size() * sizeof(float), outputBuffer.data(), GL_STATIC_READ);
                glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo2);
                glBufferData(GL_SHADER_STORAGE_BUFFER, outputBuffer.size() * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
                checkGLError("glBufferData");
                glFinish();
                checkGLError("glFinish");
                glBindBuffer(GL_UNIFORM_BUFFER, ubo);
                checkGLError("glBindBuffer");
                // glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(context), &ctxt);
                glBufferData(GL_UNIFORM_BUFFER, sizeof(context), &ctxt, GL_STREAM_DRAW);
                ctxt.time_samples += audioblock->samples;
                ctxt.time_seconds = ctxt.time_samples / ctxt.samplerate;
                // calc from samples
                ctxt.time_beats = ctxt.time_samples / (ctxt.samplerate * 60 / ctxt.bpm);

                checkGLError("glBufferData");
                glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo);
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssbo2);
                checkGLError("glBindBufferBase");
                glUseProgram(shader.programs[0]);
                checkGLError("glUseProgram");
                glMemoryBarrier(GL_UNIFORM_BARRIER_BIT|GL_SHADER_STORAGE_BARRIER_BIT);
                glDispatchCompute(1, 1, 1);
                glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
                checkGLError("glDispatchCompute");
                glFinish();
                checkGLError("glFinish");
                glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo2);
                GLvoid* p = glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
                checkGLError("glMapBuffer");
                memcpy(outputBuffer.data(), p, outputBuffer.size() * sizeof(float));
                glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
                glEndQuery(GL_TIME_ELAPSED);

                checkGLError("glUnmapBuffer");
                audioblock->clear();
                for (samplecount_t i = 0; i < channels; i++) {
                    memcpy(audioblock->buf[i], &outputBuffer[i * audioblock->samples], audioblock->samples * sizeof(float));
                }
                if (warmupBlocks >= 0) {
                    warmupBlocks++;
                    if (warmupBlocks >= LOCAL_RING_BUF_SIZE/4) {
                        for (int i = 0; i < warmupBlocks; i++) {
                            auto ringBufIdx = ringbuffer.writePos - warmupBlocks + i;
                            stream->enqueue(ringbuffer.buffers[ringBufIdx%LOCAL_RING_BUF_SIZE]);
                        }
                        warmupBlocks = -1;
                    }
                } else {
                    stream->enqueue(buf);
                }

                auto tmNow_ms = getTimeSecondsD();
                GLuint64 result;
                glGetQueryObjectui64v(query, GL_QUERY_RESULT, &result);
                auto ms = result*1.e-6;
                
                if (tmNow_ms - timeLastPerfLog > 5.0 || ms > tmComputeAvg * 4.0) {
                    if (tmComputeAvg < 0.0 && tmNow_ms - timeLastPerfLog > 1.5) {
                        tmComputeAvg = ms;
                    }
                    timeLastPerfLog = tmNow_ms;
                    std::cout << ms << " ms (avg: " << tmComputeAvg << " ms)" << std::endl;
                }
                tmComputeAvg = 0.9 * tmComputeAvg + 0.1 * ms;
            }
        }

        if (window)
            glfwDestroyWindow(window);

        audioHost->stopAudio();
    } catch (std::exception& e) {
        log_printf("exception %s\n", e.what());
        return 1;
    }
    glfwTerminate();
    exit(EXIT_SUCCESS);
}



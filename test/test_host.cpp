#include "TestBase.hpp"
#include "appsettings.h"
#include "audiocache.h"
#include "common/test_common.h"
#include "host/mainctrl.h"
#include "logging.h"
#include "math/seq_math.h"
#include "project.h"
#include "samplerate.h"
#include "seq_time.h"
#include "tls.h"
#include "track_types.h"
#include "types.h"
#include "util/testing_environment.h"
#include <cmath>
#include <memory>
#include <utility>
#include <vector>
#include "audioblock.h"
#include "clip.h"
#include "host/host.h"

namespace test_host {
    std::shared_ptr<DawInstance> initDaw(const sampleformat_t sampleformat) {
        auto dawInstance = std::make_shared<DawInstance>();
        log_out("Testing Samplerate %uHz at Blocksize %u\n", sampleformat.sampleRate, sampleformat.blockSize);
        auto& settings = *daw_tls::getTls().settings;
        settings.saveOnExit = false;
        settings.iosettings.midiconfigs.clear();
        settings.iosettings.configs.clear();
        settings.iosettings.asioConfig = {};
        settings.iosettings.internalSamplerate = sampleformat.sampleRate;
        settings.iosettings.internalBlocksize = sampleformat.blockSize;
        settings.iosettings.blocksize = sampleformat.blockSize;
        settings.dawsettings.audioEnabled = false;

        dawInstance->initDaw();
        dbgassert(dawInstance->getHost()->m_sampleFormatInternal == sampleformat);
        dawInstance->startDaw();
        dawInstance->initProcessingResources();
        return dawInstance;
    }

    template<typename Functor>
    audiofile_t* createSample(audiocache* cache, const sampleformat_t& sf, const channelnum_t numChannels, samplecount_t numSamples, Functor&& functor) {
        auto csr = create_sample_req_t{sf, numChannels, true, "", -1};
        auto samplefile = cache->createSample(csr);
        TEST_ASSERT_THROW(!!samplefile);
        store_sample_req_t ssr;
        ssr.id = samplefile->id;
        ssr.format = csr.format;
        ssr.length = numSamples;
        ssr.channels.resize(csr.numChannels);
        for (auto& ch : ssr.channels) {
            ch.resize(numSamples);
            std::memset(ch.data(), 0, numSamples * sizeof(float));
        }
        auto& chs = ssr.channels;
        for (samplecount_t pos = 0; pos < numSamples; ++pos) {
            chs[0][pos] = functor(pos, numSamples);
            chs[1][pos] = 1.0f - chs[0][pos];
        }
        cache->updateSample(ssr);
        return samplefile;
    }
    
    clip_t* createClipFromSample(tick_t tickBegin, const project_globals_t& prjGlobals, audiofile_t* sampleFile) {
        auto sample = sampleFile->getSample();
        tick_t lenClipTicks = sampleToTickConvert<tick_t, roundmode::round>(sample->nSamples, prjGlobals.tempo100, sample->sampleRate);
        auto clip = new clip_t(tickBegin, lenClipTicks, CLIP_AUDIO);
        clip->audio.id = sampleFile->id;
        clip->lenSamples = tickToSampleConvert<samplecount_t, roundmode::floor>(lenClipTicks, prjGlobals.tempo100, sample->sampleRate);
        clip->len += TICKS_BAR+1232;
        return clip;
    }
    float maximumErrorSeen = 0.0f;
    void testFillAudioFromClips(DawInstance* daw, const sampleformat_t& sf, const channelnum_t numChannels, int32_t testMode, float maxError) {
        TEST_BEGIN("testFillAudioFromClips");
        auto& prjGlobals = daw->getGlobals();
        auto cache = daw->getAudioCache();
        TEST_ASSERT_THROW(!!cache);


        std::vector<clip_t *> clips;
        auto block = AudioBlock(numChannels, sf.blockSize);
        auto sampleTestData = [sf](samplecount_t pos, samplecount_t numSamples) -> float {
            auto samplerate = sf.sampleRate;
            // 440 Hz sine wave
            auto freq = 440.0f;
            auto phase = 2.0f * FLOAT_PI * freq * pos / samplerate;
            return ::sinf(phase);
        };
        tick_t tickBegin = 0;
        if (testMode == 0) {
            tickBegin = 1;
        }
        for (int32_t iClip = 0; iClip < 32; ++iClip) {
            samplecount_t numSamples = 512 + iClip * 13*17*55;
            auto samplefile = createSample(cache, sf, numChannels, numSamples, sampleTestData);
            auto audioClip = createClipFromSample(tickBegin, prjGlobals, samplefile);
            audioClip->audio.fadeIn.durationMs = 0;
            audioClip->audio.fadeOut.durationMs = 0;
            clips.push_back(audioClip);
            block.clear();
            auto samplePos = tickToSampleConvert<samplecount_t, roundmode::floor>(tickBegin, prjGlobals.tempo100, sf.sampleRate);
            DAW::Host::FillAudioBlockFromClips(daw->getAudioCache(), daw->getProjectGlobals(), clips, sf, samplePos, block);
            float maxErrorClip = 0.0f;
            auto sampleLen = math::min(audioClip->lenSamples, block.samples);
            for (samplecount_t pos = 0; pos < sampleLen; ++pos) {
                float expected[2];
                expected[0] = sampleTestData(pos, numSamples);
                expected[1] = 1.0f - expected[0];
                for (channelnum_t ch = 0; ch < block.channels; ++ch) {
                    float* blockChannel = block.buf[ch];
                    float valAbsDiff = math::abs(blockChannel[pos] - expected[ch]);
                    maxErrorClip = math::max(maxErrorClip, valAbsDiff);
                    if ((iClip==0&&ch==0&&pos<10) || pos == block.samples>>1 || valAbsDiff >= maxError*0.7f) {
                        log_out("channel %u pos %u: expected %f, actual %f, diff %f\n", ch, pos, expected[ch], blockChannel[pos], valAbsDiff);
                    }
                    TEST_ASSERT_THROW(valAbsDiff < maxError);
                }
            }
            maximumErrorSeen = math::max(maximumErrorSeen, maxErrorClip);
            log_out("Pass Test %d Clip %u Max Error %f\n", testMode, iClip, maxErrorClip);
            if (testMode == 0) {
                tickBegin += TICKS_BAR*100;
            } else {
                tickBegin = audioClip->end()+32;
            }
        }
        for (auto clip : clips) {
            delete clip;
        }
        TEST_END();
    }
    void testClipFades(DawInstance* daw, const sampleformat_t& sf, const channelnum_t numChannels) {
        TEST_BEGIN("testClipFades");
        auto& prjGlobals = daw->getGlobals();
        auto cache = daw->getAudioCache();
        TEST_ASSERT_THROW(!!cache);

        std::vector<clip_t *> clips;
        auto sampleTestData = [sf](samplecount_t pos, samplecount_t numSamples) -> float {
            auto samplerate = sf.sampleRate;
            // 440 Hz sine wave
            auto freq = 440.0f;
            auto phase = 2.0f * FLOAT_PI * freq * pos / samplerate;
            return ::sinf(phase);
        };
        samplecount_t sampleLenClip = 100000;
        samplecount_t samplePosClip = sampleLenClip;
        auto samplefile = createSample(cache, sf, numChannels, sampleLenClip, sampleTestData);
        auto tickPosClip = sampleToTickConvert<tick_t, roundmode::floor>(samplePosClip, prjGlobals.tempo100, sf.sampleRate);
        auto audioClip = createClipFromSample(tickPosClip, prjGlobals, samplefile);
        audioClip->len /= 2;
        audioClip->offsetStart = audioClip->len/3;
        auto off = tickToSampleConvert<samplecount_t, roundmode::floor>(audioClip->offsetStart, prjGlobals.tempo100, sf.sampleRate);
        clips.push_back(audioClip);
        auto block = AudioBlock(numChannels, sampleLenClip*3);
        block.clear();
        DAW::Host::FillAudioBlockFromClips(daw->getAudioCache(), daw->getProjectGlobals(), clips, sf, 0, block);

        for (auto clip : clips) {
            delete clip;
        }
        TEST_END();
    }
}// namespace

int main() {
    setExceptionHandler();
    App::Platform::initPlatformEnvironment("daw");
    seqthreads::registerThread("mainthread", seqthreads::ThreadType::MainThread);
    daw_tls::initNewTls();
    auto sf = sampleformat_t{44100, 512};
    auto daw = test_host::initDaw(sf);
    const channelnum_t numChannels = 2;
    test_host::testFillAudioFromClips(daw.get(), sf, numChannels, 0, 0.001f);
    test_host::testFillAudioFromClips(daw.get(), sf, numChannels, 1, 0.001f);
    log_out("Max Error %f\n", test_host::maximumErrorSeen);
    test_host::testClipFades(daw.get(), sf, numChannels);
    daw->unloadProject();
    daw->destroy();
    daw = nullptr;
    return 0;
}

#include "TestBase.hpp"
#include "fileio.h"
#include "host/audiobuffer/audioblock.h"
#include "host/audiocache/audiocache.h"
#include "logging.h"
#include "samplerate.h"
#include "tls.h"
#include "types.h"
#include <dsp/rates.h>
#include <vector>

namespace {

    void test_oversampling() {
        TEST_BEGIN("test_oversampling");
        auto fileTypes = std::array{ SUPPORTED_AUDIO_FILE_TYPES };
        std::vector<FileFound> files;
        for (const auto& fileType : fileTypes) {
            findFilesWithExt(TEST_PATH("samples"), fileType, false, files);
        }
        log_printf(TEST_PATH("audiofiles: %zu files\n"), files.size());
        sampleformat_t format{44100, 128, sampleformat_bits_t::FLOAT_32};
        audiocache cache(format.sampleRate);
        for (const FileFound& file : files) {
            audiofile_t* audiofile = cache.loadFile(file.path, -1, "", nullptr, nullptr);
            if (audiofile) {
                auto* sampleIn = audiofile->sample.get();
                samplecount_t numSamplesOut = sampleIn->nSamples * 2;
                log_lf(Log::L_INFO, "loaded audiofile: %s - %u channels, %zd samples\n", audiofile->name.c_str(), sampleIn->nChannels, sampleIn->nSamples);
                {
                    const create_sample_req_t ssr = {
                        .format = sampleformat_t{sampleIn->sampleRate * 2, 512, sampleformat_bits_t::FLOAT_32},
                        .numChannels = sampleIn->nChannels,
                        .isTemporarySample = false,
                        .path = audiofile->name + "_oversampled_2x.wav",
                        .id = -1,
                        .preAllocate = numSamplesOut
                    };
                    audiofile_t* sampleOut = cache.createSample(ssr);
                    TEST_ASSERT_THROW(sampleOut != nullptr);
                    sampleOut->getSample()->nSamples = ssr.preAllocate;
                    signalsmith::rates::Oversampler2xFIR<float> oversampler(sampleIn->nChannels, format.blockSize);
                    const samplecount_t latency = oversampler.latency();
                    //append latency silent samples to input
                    sampleIn->nSamples += latency;
                    for (auto& channel : sampleIn->samples) {
                        channel.resize(sampleIn->nSamples);
                    }

                    const samplecount_t numBlocks = (sampleIn->nSamples + format.blockSize - 1) / format.blockSize;
                    std::vector<float*> channelsIn;
                    channelsIn.resize(sampleIn->nChannels);
                    samplecount_t inputOffset = 0;
                    samplecount_t outputOffset = 0;
                    for (samplecount_t i = 0; i < numBlocks; i++) {
                        for (channelnum_t c = 0; c < sampleIn->nChannels; c++) {
                            channelsIn[c] = sampleIn->samples[c].data() + inputOffset;
                        }
                        oversampler.up(channelsIn, format.blockSize);
                        auto numSamplesCopy = samplecount_t(format.blockSize * 2);
                        // skip first latency samples
                        if (inputOffset == 0) {
                            numSamplesCopy -= latency;
                        }
                        if (inputOffset + format.blockSize > sampleIn->nSamples) {
                            numSamplesCopy = sampleIn->nSamples - inputOffset;
                        }
                        for (channelnum_t c = 0; c < sampleIn->nChannels; c++) {
                            auto* channelOversampledOut = oversampler[c];
                            auto* channelSampleOut = sampleOut->getSample()->samples[c].data() + outputOffset;
                            if (inputOffset == 0) {
                                channelOversampledOut += latency;
                            }
                            if (numSamplesCopy > 0) {
                                std::memcpy(channelSampleOut, channelOversampledOut, numSamplesCopy * sizeof(float));
                            }
                        }
                        outputOffset += numSamplesCopy;
                        inputOffset += format.blockSize;
                    }

                    cache.saveSamples({sampleOut->id});
                    cache.unloadSampleId(sampleOut->id);
                }
                cache.unloadSampleId(audiofile->id);
            } else {
                log_lf(Log::L_ERROR, "Failed to load audiofile: %s\n", file.path.c_str());
            }
        }
        TEST_END();
    }

}// namespace

int main() {
    setExceptionHandler();
    daw_tls::initNewTls();
    test_oversampling();
    return 0;
}

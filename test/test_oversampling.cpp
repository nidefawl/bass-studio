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

    void test_oversampling_wavefile() {
        TEST_BEGIN("test_oversampling_wavefile");
        auto fileTypes = std::array{ SUPPORTED_AUDIO_FILE_TYPES };
        std::vector<FileFound> files;
        for (const auto& fileType : fileTypes) {
            findFilesWithExt(TEST_PATH("samples"), fileType, false, files);
        }
        log_printf(TEST_PATH("audiofiles: %zu files\n"), files.size());
        const samplerate_t sampleRate = 44100;
        for (const FileFound& file : files) {
            String sampleNameDownsampled;
            String sampleNameUpsampled;
            {
                sampleformat_t format{sampleRate, 128, sampleformat_bits_t::FLOAT_32};
                audiocache cache(format.sampleRate);
                std::shared_ptr<audiofile_t> out;
                TEST_ASSERT_THROW(cache.loadFile(out, file.path, ""));
                audiofile_t* audiofile = out.get();
                TEST_ASSERT_THROW(audiofile != nullptr);
                sampleNameDownsampled = audiofile->name + "_downsampled.wav";
                sampleNameUpsampled = audiofile->name + "_upsampled.wav";
                auto* sampleIn = audiofile->sample.get();
                samplecount_t numSamplesOut = sampleIn->nSamples * 2;
                log_lf(Log::L_INFO, "loaded audiofile: %s - %u channels, %zd samples\n", audiofile->name.c_str(), sampleIn->nChannels, sampleIn->nSamples);
                const create_sample_req_t ssr = {
                    .format = sampleformat_t{format.sampleRate * 2, 512, sampleformat_bits_t::FLOAT_32},
                    .numChannels = sampleIn->nChannels,
                    .isTemporarySample = false,
                    .path = sampleNameUpsampled,
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
                    auto numSamplesIn = math::min<samplecount_t>(sampleIn->nSamples - inputOffset, format.blockSize);
                    oversampler.up(channelsIn, numSamplesIn);
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
                cache.unloadSampleId(audiofile->id);
            }
            {
                sampleformat_t format{sampleRate * 2, 128, sampleformat_bits_t::FLOAT_32};
                audiocache cache(format.sampleRate);
                // load upsampled file and downsample it
                format = sampleformat_t{sampleRate * 2, 128, sampleformat_bits_t::FLOAT_32};
                cache.setSamplerate(format.sampleRate);
                std::shared_ptr<audiofile_t> out;
                TEST_ASSERT_THROW(cache.loadFile(out, sampleNameUpsampled, "", false, nullptr, nullptr));
                const auto audiofile = out.get();
                TEST_ASSERT_THROW(audiofile != nullptr);
                const auto sampleIn = audiofile->sample.get();
                log_lf(Log::L_INFO, "loaded audiofile: %s - %u channels, %zd samples\n", audiofile->name.c_str(), sampleIn->nChannels, sampleIn->nSamples);
                {
                    signalsmith::rates::Oversampler2xFIR<float> oversampler(sampleIn->nChannels, format.blockSize);
                    const samplecount_t latency = oversampler.latency();
                    // append latency silent samples to input
                    sampleIn->nSamples += latency;
                    const create_sample_req_t ssr = {
                        .format = sampleformat_t{format.sampleRate / 2, 512, sampleformat_bits_t::FLOAT_32},
                        .numChannels = sampleIn->nChannels,
                        .isTemporarySample = false,
                        .path = sampleNameDownsampled,
                        .id = -1,
                        .preAllocate = sampleIn->nSamples / 2 + latency
                    };
                    audiofile_t* sampleOut = cache.createSample(ssr);
                    TEST_ASSERT_THROW(sampleOut != nullptr);
                    sampleOut->getSample()->nSamples = ssr.preAllocate;
                    for (auto& channel : sampleIn->samples) {
                        channel.resize(sampleIn->nSamples);
                    }

                    const samplecount_t numBlocks = (sampleIn->nSamples + format.blockSize - 1  + latency * 2) / format.blockSize;
                    std::vector<float*> channelsIn;
                    channelsIn.resize(sampleIn->nChannels);
                    std::vector<float*> channelsOut;
                    channelsOut.resize(sampleIn->nChannels);
                    samplecount_t inputOffset = 0;
                    samplecount_t outputOffset = 0;
                    for (samplecount_t i = 0; i < numBlocks; i++) {
                        for (channelnum_t c = 0; c < sampleIn->nChannels; c++) {
                            channelsIn[c] = sampleIn->samples[c].data() + inputOffset;
                        }
                        auto numSamplesCopy = samplecount_t(format.blockSize);
                        numSamplesCopy = math::min<samplecount_t>(numSamplesCopy, math::max<samplecount_t>(0, sampleIn->nSamples - inputOffset));
                        if (numSamplesCopy > 0) {
                            for (channelnum_t c = 0; c < sampleIn->nChannels; c++) {
                                auto* channelOversampledOut = oversampler[c];
                                std::memcpy(channelOversampledOut, channelsIn[c], numSamplesCopy * sizeof(float));
                            }
                        }
                        for (channelnum_t c = 0; c < sampleIn->nChannels; c++) {
                            channelsOut[c] = sampleOut->getSample()->samples[c].data() + outputOffset;
                        }
                        oversampler.down(channelsOut, int32_t(numSamplesCopy / 2));
                        outputOffset += numSamplesCopy / 2;
                        inputOffset += format.blockSize;
                    }
                    // shift sampleOut by latency samples to left
                    for (channelnum_t c = 0; c < sampleIn->nChannels; c++) {
                        auto* channelSampleOut = sampleOut->getSample()->samples[c].data();
                        sampleOut->getSample()->samples[c].assign(channelSampleOut + latency / 2, channelSampleOut + sampleOut->getSample()->nSamples - latency / 2);
                        //resize to actual size
                        sampleOut->getSample()->samples[c].resize(sampleOut->getSample()->nSamples - latency);
                    }
                    sampleOut->getSample()->nSamples -= samplecount_t(latency * 1.5);
                    cache.saveSamples({sampleOut->id});
                    cache.unloadSampleId(sampleOut->id);
                }
                cache.unloadSampleId(audiofile->id);
            }
        }
        TEST_END();
    }

    void test_oversampling_synthezised() {
        TEST_BEGIN("test_oversampling_synthezised");
        const channelnum_t numChannels = 2;
        const samplecount_t blockSizeDown = 512;
        const samplecount_t blockSizeUp = blockSizeDown * 2;
        const samplecount_t testLength = blockSizeDown * 10;
        signalsmith::rates::Oversampler2xFIR<float> oversampler;
        oversampler.resize(numChannels, blockSizeDown);
        std::vector<float> samplesOut(numChannels * testLength);
        for (samplecount_t total = 0; total < testLength; total += blockSizeDown) {
            float* inputs[numChannels] = {};
            for (channelnum_t c = 0; c < numChannels; c++) {
                inputs[c] = oversampler[c];
            }
            for (samplecount_t i = 0; i < blockSizeUp; i++) {
                for (channelnum_t c = 0; c < numChannels; c++) {
                    inputs[c][i] = float(fmod(i / double(blockSizeUp) + 0.5 , 1.0) * 2.0 - 1.0);
                }
            }
            float* outputs[numChannels] = {};
            for (channelnum_t c = 0; c < numChannels; c++) {
                outputs[c] = samplesOut.data() + total + testLength * c;
            }
            oversampler.down(outputs, blockSizeDown);
        }
        audiocache cache(44100);
        const create_sample_req_t ssr = {
            .format = sampleformat_t{44100, 512, sampleformat_bits_t::FLOAT_32},
            .numChannels = numChannels,
            .isTemporarySample = false,
            .path = "test_oversampling_synthezised.wav",
            .id = -1,
            .preAllocate = testLength
        };
        audiofile_t* sampleOut = cache.createSample(ssr);
        TEST_ASSERT_THROW(sampleOut != nullptr);
        sampleOut->getSample()->nSamples = ssr.preAllocate;
        for (channelnum_t c = 0; c < numChannels; c++) {
            auto* channelSampleOut = sampleOut->getSample()->samples[c].data();
            std::memcpy(channelSampleOut, samplesOut.data() + testLength * c, testLength * sizeof(float));
        }
        cache.saveSamples({sampleOut->id});
        cache.unloadSampleId(sampleOut->id);
        TEST_END();
    }
}// namespace

int main() {
    setExceptionHandler();
    daw_tls::initNewTls();
    test_oversampling_wavefile();
    test_oversampling_synthezised();
    return 0;
}

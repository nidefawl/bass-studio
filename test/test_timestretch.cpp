#include "TestBase.hpp"
#include "fileio.hpp"
#include "host/audiocache/audiocache.hpp"
#include "logging.hpp"
#include "samplerate.hpp"
#include "tls.hpp"
#include <memory>
#include <signalsmith-stretch.h>
#include <vector>

namespace {

    void test_timestretch() {
        TEST_BEGIN("test_timestretch");
        auto fileTypes = std::array{ SUPPORTED_AUDIO_FILE_TYPES };
        std::vector<FileFound> files;
        for (const auto& fileType : fileTypes) {
            findFilesWithExt(TEST_PATH("samples"), fileType, false, files);
        }
        log_printf(TEST_PATH("audiofiles: %zu files\n"), files.size());
        audiocache cache(44100);
        for (const FileFound& file : files) {
            std::shared_ptr<audiofile_t> out;
            if (cache.loadFile(out, file.path, "")) {
                auto audiofile = out.get();
                log_lf(Log::L_INFO, "loaded audiofile: %s - %u channels, %zd samples\n", audiofile->name.c_str(), audiofile->sample->nChannels, audiofile->sample->nSamples);
                {
                    signalsmith::stretch::SignalsmithStretch<float> stretch;
                    stretch.presetDefault(audiofile->sample->nChannels, audiofile->sample->sampleRate);
                    stretch.setTransposeFactor(2);
                    const create_sample_req_t ssr = {
                        .format = sampleformat_t{audiofile->sample->sampleRate, 512, sampleformat_bits_t::FLOAT_32},
                        .numChannels = audiofile->sample->nChannels,
                        .isTemporarySample = false,
                        .path = audiofile->name + "_pitch+12.wav",
                        .id = -1,
                        .preAllocate = audiofile->sample->nSamples
                    };
                    audiofile_t* sample = cache.createSample(ssr);
                    TEST_ASSERT_THROW(sample != nullptr);
                    sample->getSample()->nSamples = ssr.preAllocate;
                    stretch.process(audiofile->sample->samples, audiofile->sample->nSamples, sample->getSample()->samples, sample->getSample()->nSamples);
                    cache.saveSamples({sample->id});
                }
                {
                    signalsmith::stretch::SignalsmithStretch<float> stretch;
                    stretch.presetDefault(audiofile->sample->nChannels, audiofile->sample->sampleRate);
                    const create_sample_req_t ssr = {
                        .format = sampleformat_t{audiofile->sample->sampleRate, 512, sampleformat_bits_t::FLOAT_32},
                        .numChannels = audiofile->sample->nChannels,
                        .isTemporarySample = false,
                        .path = audiofile->name + "_stretch_x2.wav",
                        .id = -1,
                        .preAllocate = audiofile->sample->nSamples * 2
                    };
                    audiofile_t* sample = cache.createSample(ssr);
                    TEST_ASSERT_THROW(sample != nullptr);
                    sample->getSample()->nSamples = ssr.preAllocate;
                    stretch.process(audiofile->sample->samples, audiofile->sample->nSamples, sample->getSample()->samples, sample->getSample()->nSamples);
                    cache.saveSamples({sample->id});
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
    test_timestretch();
    return 0;
}

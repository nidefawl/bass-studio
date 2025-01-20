#include "TestBase.hpp"
#include "fileio.hpp"
#include "host/audiocache/audiocache.hpp"
#include "logging.hpp"
#include "platform.hpp"
#include "rand.hpp"
#include "str_util.hpp"
#include "tls.hpp"
#include "types.hpp"
#include <cstdint>
#include <vector>

namespace TestAudiofiles {
    void test_dr_wav() {
        TEST_BEGIN("test_dr_wav");
        std::vector<FileFound> files;
        // findFilesWithExt("D:\\Samples2021", "wav", true, files);
        findFilesWithExt(TEST_PATH("samples"), "wav", false, files);
        for (const FileFound& file : files) {
            drwav wav{};
            drwav_init_file(&wav, file.path.c_str(), nullptr);
            auto totalSamples = wav.totalPCMFrameCount;
            auto samplesRead = size_t(0);
            const auto chunkSize = 4096 * 128;
            std::vector<int16_t> buffer(chunkSize * wav.channels);
            while (true) {
                auto samplesReadNow = drwav_read_pcm_frames_s16(&wav, chunkSize, buffer.data());
                samplesRead += samplesReadNow;
                if (samplesReadNow != chunkSize) {
                    break;
                }
            }
            // TEST_ASSERT_EQUAL(totalSamples, samplesRead);
            if (totalSamples != samplesRead) {
                log_lf(Log::L_ERROR, "Failed to read all samples from file: %s\n", file.path.c_str());
                log_lf(Log::L_ERROR, "totalSamples: %zu, samplesRead: %zu\n", totalSamples, samplesRead);
            }
            drwav_uninit(&wav);
        }
        TEST_END();
    }

    void test_audiofile_loading() {
        TEST_BEGIN("test_audiofile_loading");
        auto fileTypes = std::array{ SUPPORTED_AUDIO_FILE_TYPES };
        std::vector<FileFound> files;
        for (const auto& fileType : fileTypes) {
            findFilesWithExt(TEST_PATH("samples"), fileType, false, files);
        }
        log_printf(TEST_PATH("audiofiles: %zu files\n"), files.size());
        for (const FileFound& file : files) {
            audiocache::fileloader loader;
            bool b = loader.resolveFile(file.path, App::Platform::getCurrentWorkingDirectory(), false);
            loader.setTargetSampleRate(44100);
            TEST_ASSERT_EQUAL(b, true);
            TEST_ASSERT_EQUAL(loader.isOk(), true);
            b = loader.preloadFile(nullptr, nullptr);
            TEST_ASSERT_EQUAL(b, true);
            TEST_ASSERT_EQUAL(loader.isOk(), true);
            TEST_ASSERT_NOT_EQUAL(loader.getExpectedNumSamples(), 0);
            while (!loader.isFinished()) {
                bool b = loader.loadFileIncremental();
                TEST_ASSERT_EQUAL(b, true);
            }
            TEST_ASSERT_EQUAL(loader.isOk(), true);
            auto audiofile = loader.getSPFile();
            if (audiofile) {
                log_lf(Log::L_INFO, "loaded audiofile: %s - %u channels, %zd samples\n", audiofile->name.c_str(), audiofile->sample->nChannels, audiofile->sample->nSamples);
            } else {
                log_lf(Log::L_ERROR, "Failed to load audiofile: %s\n", file.path.c_str());
            }
        }
        TEST_END();
    }

    void test_audiofile_writing_riff() {
        TEST_BEGIN("test_audiofile_writing_riff");
        drwav wav{};
        drwav_data_format format;
        //write noise (10 sec 44.1khz 32 bit fp)
        format.container = drwav_container_riff;
        format.format = DR_WAVE_FORMAT_IEEE_FLOAT;
        format.channels = 2;
        format.sampleRate = 44100;
        format.bitsPerSample = 32;
        auto numSamplesToWrite = samplecount_t(format.sampleRate) * 10;
        
        drwav_bool32 ret = drwav_init_file_write_sequential_pcm_frames(&wav, "test_audiofile_writing_riff_noise.wav", &format, numSamplesToWrite, nullptr);
        TEST_ASSERT_THROW(ret == DRWAV_TRUE);

        const auto chunkSize = samplecount_t(4096 * 128);
        std::vector<float> buffer(chunkSize * format.channels);
        auto bufferBytes = buffer.size() * sizeof(float);
        seq_rand rand;
        rand.rng_seed(0xCAFEF00D);
        for (float & i : buffer) {
            i = float(rand.rng_double() - rand.rng_double()) * 0.01f;
        }
        auto samplesWritten = samplecount_t(0);
        while (samplesWritten < numSamplesToWrite) {
            auto samplesToWrite = std::min(chunkSize, numSamplesToWrite - samplesWritten);
            auto writeLenBytes  = size_t(samplesToWrite) * 8;
            TEST_ASSERT_THROW(bufferBytes >= writeLenBytes);
            auto wrote = samplecount_t(drwav_write_pcm_frames(&wav, samplesToWrite, buffer.data()));
            samplesWritten += wrote;
        }
        drwav_uninit(&wav);
        TEST_END();
    }

    void test_audiofile_writing_riff_metadata() {
        TEST_BEGIN("test_audiofile_writing_riff_metadata");
        drwav wav{};
        drwav_data_format format;
        //write noise (10 sec 44.1khz 32 bit fp)
        format.container = drwav_container_riff;
        format.format = DR_WAVE_FORMAT_IEEE_FLOAT;
        format.channels = 2;
        format.sampleRate = 44100;
        format.bitsPerSample = 32;
        auto numSamplesToWrite = samplecount_t(format.sampleRate) * 10;
        
        drwav_metadata metadata;
        metadata.type = drwav_metadata_type_unknown;
        safe_strcpy(metadata.data.unknown.id, "TEST", sizeof(metadata.data.unknown.id));
        metadata.data.unknown.chunkLocation = drwav_metadata_location_top_level;
        // metadata.data.unknown.dataSizeInBytes = 4;
        std::array<uint8_t, 32> dummyData;
        for (size_t i = 0; i < dummyData.size(); i++) {
            dummyData[i] = uint8_t(i);
        }
        metadata.data.unknown.dataSizeInBytes = dummyData.size();
        metadata.data.unknown.pData = dummyData.data();
        drwav_bool32 ret = drwav_init_file_write_pcm_frames_metadata(&wav, "test_audiofile_writing_riff_metadata.wav", &format, numSamplesToWrite, nullptr, &metadata, 1);
        TEST_ASSERT_THROW(ret == DRWAV_TRUE);

        const auto chunkSize = samplecount_t(4096 * 128);
        std::vector<float> buffer(chunkSize * format.channels);
        auto bufferBytes = buffer.size() * sizeof(float);
        seq_rand rand;
        rand.rng_seed(0xCAFEF00D);
        for (float & i : buffer) {
            i = float(rand.rng_double() - rand.rng_double()) * 0.01f;
        }
        auto samplesWritten = samplecount_t(0);
        while (samplesWritten < numSamplesToWrite) {
            auto samplesToWrite = std::min(chunkSize, numSamplesToWrite - samplesWritten);
            auto writeLenBytes  = size_t(samplesToWrite) * 8;
            TEST_ASSERT_THROW(bufferBytes >= writeLenBytes);
            auto wrote = samplecount_t(drwav_write_pcm_frames(&wav, samplesToWrite, buffer.data()));
            samplesWritten += wrote;
        }
        drwav_uninit(&wav);
        TEST_END();
    }
} // namespace TestAudiofiles

int main() {
    setExceptionHandler();
    daw_tls::initNewTls();
    using namespace TestAudiofiles;
    test_dr_wav();
    test_audiofile_loading();
    test_audiofile_writing_riff();
    test_audiofile_writing_riff_metadata();
    return 0;
}

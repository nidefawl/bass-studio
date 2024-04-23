#include "TestBase.hpp"
#include "fileio.h"
#include "host/audiocache/audiocache.h"
#include "logging.h"
#include "platform.h"
#include "tls.h"
#include <vector>

namespace {
    void test_dr_wav() {
        TEST_BEGIN("test_dr_wav");
        std::vector<FileFound> files;
        // findFilesWithExt("D:\\Samples2021", "wav", true, files);
        findFilesWithExt(TEST_PATH("samples"), "wav", false, files);
        for (const FileFound& file : files) {
            drwav wav;
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
                log_lf(Log::L_ERROR, "totalSamples: %u, samplesRead: %u\n", totalSamples, samplesRead);
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

}// namespace

int main() {
    setExceptionHandler();
    daw_tls::initNewTls();
    test_dr_wav();
    test_audiofile_loading();
    return 0;
}

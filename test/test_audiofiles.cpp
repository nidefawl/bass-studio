#include "TestBase.hpp"
#include "fileio.h"
#include "host/audiocache/audiocache.h"
#include "logging.h"
#include "platform.h"
#include "tls.h"
#include <vector>

namespace {

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
    test_audiofile_loading();
    return 0;
}
